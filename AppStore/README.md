# Aster 商城

原生 C 应用，使用 Aster SDK 的绘图、触摸、HTTP(S)、文件系统和应用管理接口。需要 ABI 4.3 固件，权限为 `net`、`fs`、`kv`、`appmgr`。支持目录、详情、权限与体积提示、下载进度、取消、失败重试、打开已安装应用和服务地址设置。

0.1.2 在列表和详情页显示应用图标，商城自身也带有 Launcher 图标。缩略图来自服务端对安装包 ASTI 图标的提取，不需要重新发布现有游戏。只加载可见条目，翻页或离开时释放固件图片句柄；480×480 当前最多四张、320×240 最多两张，RGB565 像素约占 18/9 KiB PSRAM（另有少量句柄开销）。无图标、网络失败或 SHA-256 不匹配时显示名称首字占位图。后台加载图标可以被点击操作中断，不阻塞应用下载安装。

`assets/icon.svg` 是商城启动图标的可编辑源，`assets/icon.bin` 是配套 96×96 ASTI RGB565A8 产物，构建时一起复制到配置目录。

## 构建与运行

SDK 默认位于 APPS 的同级目录 `aster-sdk`；也可用 `--sdk` 指定。先按 SDK 文档安装 WASI SDK 和对应 AOT 编译器。在 APPS 根目录运行：

```sh
python AppStore/tools/build.py --target riscv --server-url http://192.168.110.154:9080
python ../aster-sdk/tools/deploy.py AppStore/dist/riscv/aster.store.ast --target riscv --port COM11 --run
```

P4 用 `riscv`，S3 用 `xtensa`，分别构建和部署。0.1.1 同时适配 480×480 和 lckfb 的 320×240 横屏，后者使用紧凑列表、详情、进度及地址键盘。构建脚本给 Xtensa 配置 384 KiB 客户堆，P4 使用 768 KiB。服务端的板白名单仍须逐板确认，不能把 P4 应用直接提供给 S3。lckfb 的运行时板 ID 是 `aster.esp32s3.lckfb`。

```sh
python AppStore/tools/build.py --target xtensa --server-url http://192.168.110.154:9080
```

lckfb 需要先升级到 ABI 4.3 固件。串口部署时分别选择该板的数据口与控制台，不要误选同时连接的 P4。

默认地址是 `http://127.0.0.1:9080`，仅适合电脑上的模拟器。实机填写电脑局域网 IP，商城“设置”中可以修改并保存；保存的设置优先于构建默认值。Wi-Fi 应先在系统设置中连接。

构建脚本在 `build/configured` 生成配置，不修改源码配置。输出在 `dist/<target>`。`--no-aot` 可生成用于模拟器的 WASM 包。商城自身目前通过 SDK 引导安装，不支持在商城内自更新。

## 下载与安装

设备报告真实板 ID、CPU/AOT 目标、ABI、能力位及绘图尺寸。服务器过滤后，商城再次检查兼容元数据。下载分块写 SD 卡，每个文件都校验长度、Range 响应和 SHA-256；资源不会整体载入 WASM 内存。

程序和资源先放入商城私有 `/data` 暂存目录。下载完整后调用 `aster_appmgr_install`，固件先销毁商城 VM，再执行安装，随后重启商城显示结果。资源随代码版本切换到目标应用 `/assets`，更新保留其 `/data`。未完成下载可以取消；重启商城会清理遗留暂存目录。

## 开发验证

P4 实机已完成商城获取目录、下载程序与独立资源、安装、打开 StoreDemo 1.0.1 的验证；演示应用 AOT 日志确认文字和 1 MiB 外部资源校验成功。

lckfb 实机已验证：ABI 4.3、商城 0.1.1 Xtensa AOT 启动、Wi-Fi 目录请求，以及黄金矿工 0.2.0 的程序与 13 个独立资源下载、安装和自动返回商城。固件升级保留了原有 SD 应用和 Wi-Fi 配置。

`tests/codec_test.c` 覆盖 JSON 边界和 SHA-256；`tests/store-flow.cjs` 使用真实商城 WASM、真实本地 HTTP 服务和模拟器 Host，检查目录、下载、安装后重启、外部资源及演示应用校验日志。

`tests/layout-flow.cjs SMALL.ast LARGE.ast` 使用分别按 Xtensa/P4 堆配置构建的便携包，检查 320×240、480×480 界面，以及小屏地址输入、删除和无效地址拦截。需启动包中配置的 HTTP 服务；界面截图输出到 `artifacts/layout`。

`tests/icon-flow.cjs SMALL.ast LARGE.ast` 使用本地 HTTP 测试服务验证真实图标解码、校验失败回退，以及慢速图标请求期间的按钮操作，截图输出到 `artifacts/icons`。

运行流程测试前需发布 `StoreDemo --target wasm`，启动 9080 服务，并将无 AOT 的商城输出到 `AppStore/build/sim/dist/riscv/aster.store.ast`：

```sh
ASTER_WORKSPACE="$PWD/AppStore/build/sim" python ../aster-sdk/tools/build.py AppStore --target riscv --no-aot
node AppStore/tests/store-flow.cjs
```

上述环境变量语法用于 Linux/macOS；Windows PowerShell 用 `$env:ASTER_WORKSPACE=...`。需要先在同级 `aster-sim` 安装 npm 依赖。模拟器安装目录当前仅在会话内保留，实机安装持久化到 SD 卡。
