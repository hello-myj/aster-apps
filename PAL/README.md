# 仙剑奇侠传 · Aster PAL

由旧 pal 0.11.x 工程迁入当前 APPS，应用 ID 为 `aster.pal`，版本 0.13.0，ABI 4.3。保留真实 PAL 引擎、完整 DOS 数据、简体文本；P4 使用独立的原生 UI 字体，lckfb 保留 Fusion Pixel 12px 字体。

- P4 / ST7102：480×480、RISC-V AOT；上方场景以最近邻像素风缩放到 480×300，文字用独立 18px 字形在目标像素网格绘制，菜单面板单独绘制，下方独立操作区。
- lckfb / ESP32-S3：横屏 320×240、Xtensa AOT；320×200 游戏区及半透明按键。
- 十字方向键支持按住与滑动换向，确认/返回按触点独立处理，可同时按住方向与确认。暂停清除触摸按键；触摸更新合并到帧回调，避免每次 MOVE 重复提交整帧。
- 资源读取 `/assets/resources`，音乐和音效读取 `/assets/audio`；标准 `N.rpg` 存档位于应用私有 `/data`，不依赖旧共享目录。新 ID 独立保存存档，不覆盖原工程或其他应用的数据。

P4 初始线性内存 5 MiB、上限 6 MiB、SDK 堆预算 768 KiB。0.13.0 的链接 map 中，原生字库固定容量 492,000 字节，绘制记录和复制/裁剪工作区合计 286,720 字节；共 778,720 字节（约 760.5 KiB），放在 APP 线性内存，实机使用 PSRAM，不改固件 SRAM/L2 配置。lckfb 构建关闭该层，仍为初始 3.5 MiB、上限 4 MiB、SDK 堆 256 KiB；RGB565 中间帧缓冲为 P4 450 KiB / S3 150 KiB，两者 C 栈 64 KiB。声明上限不等于实际占用，结果以链接 map、运行时分配和实机日志为准。

0.12.2 将十字键箭头改为直接像素绘制，避免设备字体缺少箭头字形；缩放坐标在初始化时计算，重复采样行直接复制，保持游戏比例和原来的最近邻像素映射。

0.12.3 修复按键闪烁：旧 gfx_exec 会立即刷新文字区域，不能在该调用后淡化文字再 present 整屏。现在先合成游戏、半透明背景及箭头，再通过一次显式整屏 gfx_exec 绘制文字并刷新；确认/返回保持固定亮度，不增加帧缓冲或内存预算。

用户已确认 lckfb 0.12.3 不再闪烁、操作正常。0.12.4 的 P4 原尺寸居中布局已停止采用。0.13.0 恢复场景放大，并分离原生文字与菜单：不把低分辨率文字先画到底图再放大；绘制记录跟随 SDL 备份、恢复和菜单像素缓冲，重绘时裁剪对应覆盖区域。字体离线生成，运行时无通用分配器或字体栅格化。

## 构建

依赖当前仓库旁的 `aster-sdk`、WASI Clang、匹配 CPU 的 WAMRC、Node.js 和 Binaryen 123。SDK 可通过 `ASTER_SDK` 指定，工具链遵循 SDK 的 `ASTER_WASI_SDK`、`ASTER_WAMRC`，Node 通过 `ASTER_NODE` 指定。在 `port` 执行 npm install 或 pnpm install，依赖由 package.json / pnpm-lock.yaml 管理。

从本项目执行：

```sh
python tools/build.py --target riscv
python tools/build.py --target xtensa
ASTER_WORKSPACE=$PWD/build/sim python tools/build.py --target riscv --no-aot
python tools/prepare_store.py
```

Windows 入口为 `build.ps1 -Target riscv` 或 `-Target xtensa`，需要该平台可用的 SDK 编译器和 Python。当前工作区也支持在 WSL 使用 Linux WASI/WAMRC，并通过 ASTER_NODE 调用 Windows node.exe。

设备包为 `dist/riscv/aster.pal.ast`、`dist/xtensa/aster.pal.ast`。store.toml 按板型、CPU 和分辨率限制分发；从 APPS 根目录执行 `python -m StoreServer publish-project PAL --target riscv` 或 `--target xtensa`。设备安装必须同时获取程序与资源。

## 从原始资源重建

引擎在 sdlpal-aster，原始数据在 pal-resources/pal；来源、固定提交和许可见 [UPSTREAM.md](UPSTREAM.md)。引擎快照已包含迁移时完整 Aster 补丁，副本保存在 patches/engine-aster.patch。没有把原工程 Git 元数据或构建产物当作源码。

```sh
python -m pip install --target port/python-deps -r port/requirements.txt
python tools/prepare.py --pack --audio
```

该步骤需要 FFmpeg 和 Node 项目依赖。主机先构建完整原生数据包，转换简体文字和字形，再离线生成 RIX/OPL 音乐与 WAV 音效，最后整理商城资源。完整数据包约 50.1 MB，以 3 MiB 分片；加上 86 首音乐、237 个音效片段和元数据共约 81.5 MB，每个文件均在设备 4 MiB 上限内。

图标来自上游 Icon.png，以 python tools/prepare_icon.py 重建为 48×48 ASTI。字体原始 ZIP、OFL 和固定摘要在 port/font-sources。port/resources、生成音频、build、dist、测试存档和 tmp_ui 截图不提交。port 中旧 PowerShell 脚本为历史入口，当前构建与资源准备以上述 tools 为准。

## 模拟验证

构建便携版本并准备资源后，在项目目录执行：

```sh
PAL_TEST_RESOLUTION=480x480 node tests/current-flow.cjs
PAL_TEST_RESOLUTION=320x240 node tests/current-flow.cjs
```

测试从真实游戏开场操作，覆盖对话、方向和菜单、普通存档、行走、普通读档恢复位置、音乐音效及暂停恢复。测试存档在独立 port/build/smoke-state-*，不读取玩家存档；同时检查确认松手不释放另一触点的方向键、滑动换向和暂停取消触点。

`node tests/native-layout.cjs` 对比两种尺寸的真实游戏截图，检查场景仍按最近邻缩放、P4 文字不再使用缩放的低分辨率位图。`tests/native-ui-test.c` 检查独立文字层的裁剪、备份恢复、自重叠复制、绘制顺序和容量失败；该主机检查不替代游戏截图或实机验收。

`python tests/audio-coverage.py` 核对所有非空音频编号与商城文件，并逐样本比较音效转换结果；当前为 86 首音乐、211 个非空音效编号，共 237 个 WAV 片段。空编号保留原始语义，不虚构替代音效。主回归另检查每张合成画面仅通过一次显式整屏 gfx_exec 发布，不额外 surface_present。

run.ps1 -Resolution 480x480（或 320x240）启动当前 aster-sim。项目内 sim-assets.cjs 为模拟器及其工作进程挂载外部资源，不更改 SDK 或模拟器的包格式及限制。玩家存档位于 build/player-state，截图由真实游戏循环生成到 tmp_ui。

## 验证范围

2026-11-07：0.13.0 的 P4 AOT 与两种尺寸的实际游戏模拟回归通过，每种尺寸覆盖 6,656 个事件帧、1,766 次完整合成发布。截图对比中 90,090 个场景像素保持最近邻映射，菜单文字区域出现 2,015 个独立字形像素差异；已检查开场、对话、主菜单、系统、存档和恢复后的实际游戏截图。原生文字层裁剪/复制/容量主机检查、字库覆盖及笔画定位检查通过。

P4 已安装并通过 AOT 启动 0.13.0，日志确认 480×300 游戏区、18px 独立 UI、完整资源和音乐加载。商城已更新为正常权限的 0.1.8，修复成功 EOF 后状态暂忙导致“下载尚未完成”的误判，回归测试能在 0.1.7 复现且 0.1.8 通过。新版实屏文字及菜单恢复待用户确认；未宣称战斗、全部菜单及全剧情验收。lckfb 实机保持已确认正常的 0.12.3，当前源码的 S3 模拟回归也通过，独立 UI 层在该目标不分配工作区。

两种尺寸的真实游戏模拟回归、两种架构 AOT 构建及当前 SDK 校验已通过；另以每次最多写入 997 字节的模拟文件接口验证普通存读档。2026-11-06 实机记录：P4 和 lckfb 均已安装正式版 0.12.2，通过 AOT 启动并加载完整数据和音乐。lckfb 已进入开场剧情，实机截图确认中文、画面和像素箭头可显示；串口触摸注入验证方向与确认独立按住、释放，检查结束无残留触点。截图后应用恢复 RUNNING，last_err=0；该板运行时报告线性内存峰值 3,932,160 字节（3.75 MiB）。

旧 S3 共用配置曾因线性内存分配失败无法启动，已通过目标专用配置修复；失败日志保留在 artifacts，验证用临时应用已卸载。开场阶段两板曾测得约 9 次画面提交/秒，尚未完成性能优化，不能据此宣称流畅运行所有场景。实体触摸手感及扬声器听感仍待用户确认，未把模拟器回归或串口注入当成用户验收，也未宣称全剧情、战斗和通关验收。
