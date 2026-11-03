# 黄金矿工 / GoldMiner 0.2.0

同一份原生 C 游戏源码支持立创 ESP32-S3（320×240）与当前 OSPTEK ESP32-P4（480×480）。设备分别使用 Xtensa、RISC-V AOT；安装包不跨 CPU 通用。没有 Lua 虚拟机或 LÖVE 运行时。

游戏保持原来的 320×240 逻辑坐标：lckfb 原尺寸显示，P4 等比放大到 480×360 并居中，上下留白每帧清空。无底部虚拟按键栏，只保留右上角半透明 MENU。素材分别从原图直接生成 1 倍和 1.5 倍版本，字体按目标尺寸栅格化，不再把 1.2 倍图片二次放大。

## 触摸操作

- 直接点 Start Game / High Score。
- 点游戏画面放钩；拖动、第二根手指及画面留白不会误触放钩。
- 有炸药并抓住物品时，点顶部的炸药图标使用。
- 达标后目标文字变绿，点左上角的目标金额提前过关。
- 商店中点商品购买，点右侧店主（或 Continue 文字）继续。
- 目标页、过关页可以点画面继续。
- MENU 打开暂停菜单，点 RESUME、MAIN MENU、EXIT GAME；再次点 MENU 也可恢复。

操作在抬手时确认。键盘操作仍保留，设备不需要虚拟方向键、DROP、TNT、NEXT、BUY 等按钮。

## 构建

需要 Python 3.11+、Pillow、ffmpeg，以及同级 `aster-sdk` 的 WASI SDK 和 AOT 编译器。`ASTER_SDK` 可指定 SDK 路径；Linux/WSL 按 SDK 文档设置 `ASTER_WASI_SDK` 与 `ASTER_WAMRC`。

从 APPS 根目录执行：

```sh
python GoldMiner/tools/build.py --target riscv
python GoldMiner/tools/build.py --target xtensa
```

构建工具准备固定哈希依赖、生成素材，再编译配套 AOT。P4 输出为 `dist/riscv/aster.goldminer.ast` 和 `dist/riscv/resources`；S3 输出为 `dist/xtensa/aster.goldminer.ast` 和 `dist/xtensa/resources`（均在 GoldMiner 目录内）。两款资源目录相互独立，构建 S3 不会覆盖 P4 的素材。

资源目录映射到应用的 `/assets`，包含 `assets/game.gma` 和 `assets/audio`；用户存档继续写 `/data/savedata.bin`。设备安装必须同时包含 AST 和相同变体的资源树。

P4 初始线性内存 3 MiB、上限 4 MiB，自有 C 堆 2 MiB、图片缓存上限 1.5 MiB。S3 初始 2 MiB、上限 3 MiB，自有 C 堆 1 MiB、图片缓存上限 768 KiB。帧缓冲及 Host 对象使用独立预留预算；应用数据位于 PSRAM，不是增加片上 L2 缓存。

在 GoldMiner 目录使用 PowerShell 也可运行：

```powershell
./tools/build.ps1 -Target xtensa
./tools/build.ps1 -Target riscv
./tools/build.ps1 -Target xtensa -SimulatorOnly
./tools/run-sim.ps1 -Target xtensa
```

模拟器构建会额外生成 `build/sim/standalone/<target>/aster.goldminer.ast`，其中内嵌资源以便独立运行；此副本不用于商城发布。设备包和商城 WASM 变体仍分离资源。模拟器默认打开 320×240（xtensa）或 480×480（riscv），需要同级 `aster-sim` 安装 npm 依赖。

## 商城发布

在 APPS 根目录运行：

```sh
python -m StoreServer publish-project GoldMiner --target riscv
python -m StoreServer publish-project GoldMiner --target xtensa
```

`store.toml` 对 P4 480×480（`osptek-p4-st7102`）和 lckfb 320×240（实机 ID `aster.esp32s3.lckfb`）分别设置白名单；服务端按 CPU、开发板、屏幕、ABI 和能力选择。商城安装流程需要 ABI 4.3 固件；游戏本身最低 ABI 为 4.1。模拟器发布使用 `--target wasm`，先构建 `--target riscv --no-aot`。

## 验证范围

```sh
python GoldMiner/tests/test_touch_rules.py -v
node GoldMiner/tests/touch-flow.cjs
```

C 测试覆盖真实游戏状态机中的炸药、达标跳关、购买、店主继续、暂停和退出。模拟器分别加载两个目标的实际安装包及对应资源，在 320×240 和 480×480 下检查开始、比分、拖动取消、多指取消、暂停恢复和放钩；截图与日志位于 `artifacts/touch`。P4 用户已确认 0.2.0 显示、MENU 和触摸正常。lckfb 已在现有 ABI 4.1 固件上安装并以 Xtensa AOT 启动，实机截图为 320×240，菜单稳定区间约 19.9 FPS；原图素材按需读取后无重复读卡。lckfb 的触摸与关卡试玩仍待用户确认，不能把菜单帧率当成关卡帧率。设备日志与截图在 `artifacts/lckfb`。

`tests/render-equivalence.cjs` 是历史 0.1.5→0.1.6 绘制优化检查，0.2.0 有意改变布局与素材，当前应运行触摸流程测试。历史微雪实机记录在 `docs/移植记录.md`，其中旧的 480×320 底栏、内存值及 `tools/deploy-device.py` 部署方式不适用于本版。

## 来源与许可

基于 GoldMiner-Rebirth `5ace977fb5770004009ec7b089c95ffa95240efa`。原版 Lua 保留作来源与行为对照，31 套关卡从原数据转换；保留钩子/碰撞、质量与价值、TNT 连锁、随机道具、计分和存档。未宣称逐帧等同 Flash 或已完成全关卡验收。

移植适配与工具为 GPL-3.0-or-later，见 LICENSE；上游代码 MIT 及各依赖声明保留。上游素材来自互联网并经修改，素材权利不由代码许可证覆盖；来源见 `vendor/UPSTREAM.md`。DejaVu Sans 字体和许可在 `vendor/fonts`。

旧固件串口验证可临时生成带 ASTR 资源的离线包，由原安装器解包成独立资源文件；本次 lckfb 使用此方式，无须刷固件。商城发布的程序和资源仍各自独立，未改变发布结构。
