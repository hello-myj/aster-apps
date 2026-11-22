# 现有应用的商城构建

每个工程的 `store.toml` 给出各板型的 AOT 包和独立资源目录。鱼鹰光电 P4C5 和 FAKE POD NANO PRO 均用 `riscv`，立创实战派用 `xtensa`。播放器仅面向 FAKE POD；OpenOMF 与 OpenTyrian 的实机变体仅面向鱼鹰 P4C5。版本号和最低 ABI 来自包内清单。

## 一次性准备

先按各工程文档安装资源转换依赖，并运行原有素材准备工具。WSL/Linux 构建需要 Python 3.11+、SDK 的 WASI/AOT 工具链；设置 `ASTER_WASI_SDK`、`ASTER_WAMRC`，必要时设置 `ASTER_SDK`。默认 SDK 为本仓库同级 `aster-sdk`。

- OutRun：`tools/prepare_assets.py`、`tools/prepare_audio.py`。
- PixelWheels：`tools/prepare_assets.py`（包含音频转换）。
- GoldMiner：`tools/prepare.py`。
- OpenOMF：`tools/prepare-engine-deps.py`、`tools/prepare-engine-assets.py`。先按 `tools/prepare-reference.ps1` 下载并校验官方 0.8.6 参考资源；Linux 不执行桌面程序，只使用其中的数据和 ffmpeg 转换音频。
- OpenTyrian：`tools/prepare.py`、`tools/prepare_audio.py`。Linux 音频转换需要 gcc、SDL2 开发文件、ffmpeg。构建采用校验 SHA-256 的 Binaryen 133，将协程转换为普通 WASM 后生成 RISC-V AOT。
- TouchTest：无需外部素材准备。

在 APPS 根目录构建，例如：

```sh
python tools/build-market.py OutRun PixelWheels GoldMiner OpenOMF OpenTyrian TouchTest
```

未指定架构时上述命令构建 RISC-V 包；立创版本需为支持该板的项目另行传 `--target xtensa` 并准备其资源。输出路径按项目 `store.toml` 选择，一般为 `dist/<target>/<APP ID>.ast` 和 `dist/resources`；GoldMiner 的素材按 CPU 分开，使用 `dist/<target>/resources`。素材准备失败时不要发布残缺目录。模拟器版本单独运行 `--no-aot`，输出到各工程 `build/sim/dist/riscv`，保留实机 AOT。

```sh
python tools/build-market.py OutRun PixelWheels GoldMiner OpenOMF OpenTyrian TouchTest --no-aot
node tools/smoke-market.cjs OutRun PixelWheels GoldMiner OpenOMF OpenTyrian TouchTest
```

检查启动、帧提交和资源加载，截图与日志写入各工程 `artifacts/market`。这些输出及生成资源不提交 Git。

## 发布

```sh
python -m StoreServer publish-project OutRun --target riscv
python -m StoreServer publish-project PixelWheels --target riscv
python -m StoreServer publish-project GoldMiner --target riscv
python -m StoreServer publish-project OpenOMF --target riscv
python -m StoreServer publish-project OpenTyrian --target riscv
python -m StoreServer publish-project TouchTest --target riscv
```

模拟器变体用 `--target wasm`，仅对有对应变体配置的工程发布。同版本内容不可覆盖，资源或程序改变后提高版本号。

## 资源与显示

OutRun、Pixel Wheels 的只读资源从 `/data/resources` 迁到 `/assets/resources`；OpenOMF 从 `/assets/resources` 读原版数据，配置和状态继续使用 `/data/config`、`/data/state`。OpenTyrian 从 `/assets/resources`、`/assets/audio` 读取，配置与存档继续在 `/data`。两者 libc 适配提供独立 `/assets` 只读挂载入口，实际写权限仍由 Host 执行。

GoldMiner 的 `game.gma` 和音频从 AST 拆为外部文件，保持应用内 `/assets/assets/...` 路径；图标仍在 AST 内。TouchTest 不需要独立资源。

Pixel Wheels 按屏幕原生尺寸绘制。鱼鹰 P4C5 将转向和油门放在游戏区下方；立创实战派使用横屏触控和自动油门。OpenOMF 与 OpenTyrian 在鱼鹰 P4C5 的 480×480 屏幕上保留 480×300 游戏视口，底部放置独立摇杆和动作键。各板型的布局与操作见 [Pixel Wheels](../PixelWheels/README.md)、[OpenOMF](../OpenOMF/README.md) 和 [OpenTyrian](../OpenTyrian/README.md)。

GoldMiner 0.2.0 取消底部虚拟按键，保留右上角透明 MENU。lckfb 用 320×240 原尺寸与 Xtensa AOT；P4 用 RISC-V AOT，将游戏等比放大为 480×360 居中。点击游戏放钩、点击炸药图标使用、达标后点击目标金额跳关、点击商品购买、点击店主继续。主指抬手确认，拖动、多指与留白不触发游戏操作。

GoldMiner 的 1 倍/1.5 倍资源直接从原图生成，以 GMA2 记录资源比例，分别存放 `dist/xtensa/resources` 和 `dist/riscv/resources`。S3 初始线性内存 2 MiB、总上限 3 MiB，自有 C 堆 1 MiB、图片缓存上限 768 KiB；P4 初始 3 MiB、总上限 4 MiB，自有 C 堆 2 MiB、图片缓存上限 1.5 MiB，L2 仍为 128 KiB。OpenOMF/OpenTyrian 初始 4 MiB，总上限 5 MiB。

GoldMiner 两种 AOT 编译及 320×240 / 480×480 模拟器触摸流程均通过；炸药、购买和店主继续由真实状态机 C 测试覆盖。P4 用户已确认 0.2.0 正常。lckfb 在 ABI 4.1 固件上已安装并以 Xtensa AOT 启动，原生 320×240 截图已核对，稳定菜单区间约 19.9 FPS；触摸和关卡仍待用户确认。

OutRun 在鱼鹰上用方向盘和踏板，在立创实战派上用 IMU 转向，左、右半屏分别控制刹车和油门。

原工程中针对旧 SD 部署布局的说明适用于历史构建；本次版本请通过商城安装，或同时把程序和 `dist/resources` 按新版资源布局安装。仅复制 AST 将缺少数据。

## 发布与验证记录

OBS 快照 `obs-20261122-store033` 中，鱼鹰光电 P4C5 有八款应用，立创实战派有六款，FAKE POD NANO PRO 的扩展目录有 TouchTest 和 Aster 音乐 0.4.27。各项目的实机验证以具体版本和板型的记录为准。[发布记录](https://github.com/hello-myj/aster-releases/blob/main/docs/obs-distribution.md)

OpenTyrian 的协程入口使用独立函数边界，防止引擎被合并进直接控制 Asyncify 的 Host 回调；Asyncify 同时处理间接调用。否则可能在暂停/恢复后破坏局部变量及文件句柄。最终 AST 不包含内部 `asyncify.*` 或 `tyrian.*` 导入。

历史 0.1.6 优化不透明矩形/图块和文字缩放的逐像素运算。`GoldMiner/tests/render-equivalence.cjs` 对比旧版 AST 和新版 AST：菜单、目标页、关卡、抓取和暂停的五帧像素完全一致；固定流程的资源读取调用从 2684 次降到 71 次。此为模拟器回归证据，P4 新版帧率仍待测。

P4 固件现已编译关闭解释器，安装与运行均强制匹配 AOT，不再默默回退。AOT 代码通过验证后的 PSRAM 指令映射执行，旧版 GoldMiner 0.1.5 和商城已完成实机执行验证；之前的内部 SRAM 分配限制会导致 AOT 加载失败。

GoldMiner 0.2.0 也已发布 `xtensa` / `aster.esp32s3.lckfb` / 320×240 白名单变体；商城不会向 S3 提供 RISC-V 包。对应资源必须与程序一起安装。

## 公共分发

完成各项目素材准备并构建 `store.toml` 声明的实机包后，可用 `tools/prepare-publication.py --catalog ../aster-releases/publication.json --skip-build` 选择完整商城；还需指定此次导出的 `--tag`、`--output` 和目标源。默认源使用华为云 OBS，GitHub 工作流独立构建 TouchTest 镜像。新的板型即使复用已有架构，也需要自己的首次开机资源配置。[导出与上传](https://github.com/hello-myj/aster-releases/blob/main/docs/obs-distribution.md)
