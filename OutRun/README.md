# OutRun 重制版 → Aster 4.1，0.3.1

由当前模型重新移植，参照 ZgzInfinity/OutRun 重制版。本版保留原赛道逐段高程与曲率、双路车道宽度、场景对象布置和轴点、车辆方向/坡度/制动/碰撞帧，并将绘图替换为 Aster RGB565 软件渲染。0.1 试版保留在 legacy-prototype/。

![起跑场景](artifacts/v2/start.png)
![赛道驾驶](artifacts/v2/drive-900.png)

## 项目目录

APPS 下每个应用对应一个完整项目文件夹。本项目的源码、原版参考、资源、工具与文档全部放在 OutRun 内：

```text
APPS/
  OutRun/
    app.toml          应用配置
    src/              Aster 移植代码
    assets/           应用图标
    vendor/upstream/  原版源码、素材及许可证（保留原仓库历史）
    sdcard/           SD 卡资源
    tools/            资源转换、构建、模拟和部署工具
    docs/             移植与优化文档
    dist/             构建产物
    artifacts/        历史截图和运行记录
    legacy-prototype/ 早期移植版本
```

添加其他项目时，在 APPS 下新建与 OutRun 平级的项目文件夹。备份、复制或整合 OutRun 时，请保留整个文件夹；资源转换依赖内部的 vendor/upstream，SDK 仍由外部公共安装提供。

## 运行

在项目目录执行：

    ./tools/run-sim.ps1

打开 SDK 自带 ABI 4 模拟器，480×320 横屏。启动器直接挂载本项目的 sdcard/aster/apps 作为虚拟 SD 数据目录。点击 START RACE 开始三秒起跑；初始 CIRCUIT 01 是原 Map1，点击两侧大号箭头选择其他地图。

- 左下两个半透明按钮：左转、右转。
- 右下 BRAKE：按住制动，松开解除。
- 右上 AUTO GAS：点击切换自动油门；关闭后出现独立 GAS 按钮。
- 电脑键盘：左右方向转向，上方向/左 Ctrl 加速，下方向/左 Alt 制动，Enter 开始或重试，Esc 打开/关闭游戏菜单；菜单内上下方向选择、Enter 确认。
- 初始地图选择时，键盘左右方向切换地图。
- 只有单触点的设备采用自动油门配合转向/刹车。键盘支持同时按方向和油门。
- 模拟器中点击“启用声音”后试听音乐与音效。

## SD 卡与构建

完整外部资源位于 sdcard/aster/apps/aster.outrun/data/resources/。将 sdcard/ 内的内容复制到实际 SD 卡根目录；运行时通过 /data/resources 访问。应用包不内嵌这套大资源，只有 AST 无法运行。

    ./tools/build.ps1

构建会从项目内的 vendor/upstream 转换资源并生成 Xtensa AOT 包，再运行 verify。产物为 dist/xtensa/aster.outrun.ast。SDK 路径在 tools/build.ps1 和 run-sim.ps1 中配置。Python 依赖在 tools/requirements.txt；音频使用 imageio-ffmpeg 自带工具转换。

SDK 的 Windows 包装器有时会打印 reader thread UnicodeDecodeError，但工具仍生成包；本版以包校验和实际运行确认结果，日志位于 artifacts/v2-build-verify.log。

## 已恢复

15 张完整地图、原逐段地形和分道规则、约 8.5 万处原地图对象布置、背景滚动、按深度缩放及坡道遮挡、1156 张所需图片（含玩家148帧与轮胎28帧、17种交通车、原HUD与终点素材）；原加速/自动档/转向阈值/离心力公式、路外减速和恢复、物体轴点碰撞、车辆旋转及恢复、20辆交通车、超车分数、原式五层路线选择、计时、超时/重试、终点观众与冲线减速；原音乐与起跑/引擎/刹车/碰撞音效。

## 仍与完整桌面版有差距

设置、车型颜色选择和排行榜菜单尚未完整移植；当前使用红色 Ferrari。交通 AI 保留三类行为意图与原横移步幅，未逐项复刻原 AI 的全部返回计时与随机调度。部分人物、碰撞、终点和分岔过场的编排仍有差异；场景切换尚未实现原版全部滑动/渐变效果。当前只提供一首原音乐，短音效受 SDK 每个64KiB上限截为0.315秒循环片段，没有原 SFML 完整混音与音高变化。

本版是核心体验的重新移植，仍需继续对照桌面版细节。已在微雪 waveshare35b 真机运行首关，转向/刹车事件正常，用户确认横屏和背景音乐正常；0.2.2相同开场实测约9.8 FPS，驾驶帧率随场景变化，仍需优化。详见 docs/实机部署记录.md。当前配置3MiB初始WASM内存、4MiB上限、1,350,000字节图像缓存，不能仅凭模拟器通过认定设备能达到相同帧率。

## 验证记录

- tools/validate-maps.js：通过实际菜单依次加载并渲染15张地图，截图位于 artifacts/v2/maps/。
- tools/validate-sim.js：实际WASM、SD资源、固定输入时钟运行约70秒，记录转向、制动、碰撞恢复、音频事件与画面，位于 artifacts/v2/。
- tools/validate-lifecycle.js：验证超时后重试。使用150ms事件步长验证生命周期，不能用其宿主运行耗时估计实际帧率。
- tools/validate-boundaries.ps1：独立夹具使用生产游戏/绘图/资源代码验证关卡边界、左右路线和终点。它从边界状态起测，并非实驾五关通关记录。

详见 docs/代码移植文档.md。保留上游 GPL-3.0 LICENSE 与代码来源；资源逐文件来源见 SD 资源目录的 catalog.json。


早期0.2.0包在电脑模拟器上的开场菜单测试为300帧约13.67秒，约22 FPS（见 artifacts/v2/live-timing.json），尚未达到请求的60Hz；固定时钟回放的每帧耗时不能当作实际刷新率。


## 0.2.2 帧率优化

保持480×320与原素材，优化精灵透明边界、采样、道路色带、HUD与图片缓存。实机相同开场从约5 FPS提升到约9.8 FPS，初始3 MiB/上限4 MiB未增加。驾驶结果与计时口径见 docs/APP帧率优化记录.md。


## 0.2.3 大按键与游戏菜单

方向按钮84×64，刹车96×64，绘制范围与触摸判定共用 src/port/ui.h。右上 MENU 暂停并打开菜单：RESUME 继续，RESTART 重开当前地图，SELECT MAP 返回当前地图的选择界面，再用左右箭头选择并 START RACE。手动油门模式有独立 GAS 按钮。操作菜单的手势会等松手后再接受驾驶输入，避免误转向；暂停菜单静止时不重复重绘。


## 0.3.0 连续音频（SDK 4.1）

使用本地 aster-sdk 的4.1.0-rc.1。引擎改为后台连续循环，随速度、档位平滑变调，刹车独立淡出停止；四个音效使用较完整的单声道素材。需要ABI4.1固件。源码为src/port/audio.c，详见docs/SDK4.1连续音频适配.md。下文早期0.2版的0.315秒短音效限制已经由本节替代。


## 图标与简介

启动器使用上游 Resources/Icon/Outrun.png 的红色跑车像素图标，转换为48×48 RGB565A8 ASTI；源图在assets/icon.png，打包图标为assets/icon.bin。中文简介：载着你，驶向海风与落日。
