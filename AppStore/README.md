# Aster 商城

原生 C 应用，使用 Aster SDK 的绘图、触摸、HTTP(S)、文件系统和应用管理接口。需要 ABI 4.3 固件，权限为 `net`、`fs`、`kv`、`appmgr`。支持目录、详情、权限与体积提示、下载进度、取消、失败重试、打开已安装应用和服务地址设置。

配套固件已修复嵌套资源复制时的工作线程栈压力：递归目录状态放 PSRAM，2 KiB 复制缓冲仅在复制文件时申请内部内存并释放；工作线程常驻栈大小不变。OutRun 44,764,586 字节资源已完整安装，实测安装阶段栈余量约 8 KiB。

0.1.7 修复竖屏商城下载横屏应用时的客户端二次校验：320×240 和 240×320 按同一面板识别，其他分辨率、板型及架构仍拒绝。`tests/orientation-download.cjs` 使用真实商城 WASM、HTTP 清单、安装包及资源，覆盖两种方向完整下载/安装和不兼容元数据拒绝。

0.1.6 将 lckfb 商城改为 240×320 竖屏：三行图标列表、详情、下载进度和八列地址键盘。P4 继续使用 480×480 布局。配套 Zephyr 固件接入原生安装进度页，显示阶段、复制大小、进度条和用时，“查看详情”可在复制时展开当前文件；安装事务完成后回到商城。服务器按屏幕两边尺寸匹配兼容版本，商城竖屏仍能下载同面板的横屏游戏，板型、AOT 架构、ABI 与能力过滤保持有效。

0.1.5 在列表底部显示当前页/总页数。页数按实际屏幕可见行数计算，涵盖服务端每批 8 条的目录分页；空列表为 0/0，加载时显示等待状态。跨服务端分页点击“上页”会落到上一批的最后一屏，刷新回到第一页。`tests/pages-flow.cjs` 覆盖 17 条应用的双向翻页、首尾、空列表和刷新。

0.1.4 在读取兼容目录后查询已安装版本，按与服务端一致的数字版本规则比较 `major.minor[.patch]`，每段 0..255；`1.10` 大于 `1.9`，`1.2` 等于 `1.2.0`。列表未安装项显示“下载”，有新版显示“更新”，同版或本机较新显示“打开”；详情显示本机与商城版本，并阻止同版重装/降级。版本或注册表读取异常时不提供安装操作。

列表图标、标题和行内空白均可进入详情，右侧操作按钮直接下载、更新或打开；行间空隙不触发操作。320×240 与 480×480 使用相同规则。刷新或安装后重新进入商城时会重新计算状态；更新仍使用原安装事务并保留 `/data` 存档。

0.1.3 将下载写卡缓冲改为 8 KiB：每次 HTTP 读取仍遵守 ABI 的 2 KiB 上限，累计后批量写入，并在分段 EOF 时写完尾块；保留原 SHA-256、ETag 和长度校验。比旧数组多 6 KiB APP 线性内存，商城退出后释放，声明的初始/最大内存不变。资源仍按完整版本下载与安装，本轮没有引入资源复用或增量更新。

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

lckfb 需要先升级到 ABI 4.3 固件；安装进度页需要包含 `ui/install_progress.c` 的本轮固件。`tests/portrait-flow.cjs` 覆盖竖屏列表/详情/设置及 17 条目录的分页，并回归 320×240 与 P4 480×480。串口部署时分别选择该板的数据口与控制台，不要误选同时连接的 P4。

默认地址是 `http://127.0.0.1:9080`，仅适合电脑上的模拟器。实机填写电脑局域网 IP，商城“设置”中可以修改并保存；保存的设置优先于构建默认值。Wi-Fi 应先在系统设置中连接。

构建脚本在 `build/configured/<target>` 生成配置，不修改源码配置。输出在 `dist/<target>`。`--no-aot` 可生成用于模拟器的 WASM 包，输出在 `build/sim/<target>/dist/<target>`，与实机 AOT 包分开。商城自身目前通过 SDK 引导安装，不支持在商城内自更新。

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

包含安装进度支持的 P4 固件会在商城 VM 退出后的安装阶段显示原生进度界面。下载暂存目录位于商城私有 `/data/s.../assets`，安装时复制到目标应用的未启用版本目录，成功后才切换有效记录；因此下载与安装是独立的 SD 写入过程。进度界面显示阶段、累计大小、耗时和可点击的文件详情。

`tests/update-flow.cjs PORTABLE.ast` 检查两种分辨率的版本状态、整行详情入口、直接操作、同版/高版本防降级及刷新。`tests/store-flow.cjs` 还覆盖实际下载旧版本升级、注册表替换和存档保留。
