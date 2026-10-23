# Pixel Wheels → Aster：移植版 0.1.6

基于 Aurélien Gâteau / agateau/pixelwheels 的原图、原 TMX 地图和车辆定义实现 C/WASM 运行层。480×320 横屏，在官方 Aster 模拟器中运行，保留未来 Xtensa AOT 构建路径。一个项目对应此完整文件夹。

## 运行

1. SDK 放在 `../../aster-sdk`，或设置 `ASTER_SDK`。
2. Python 安装 `tools/requirements.txt`；可用 `PIXELWHEELS_PYTHON` 指定解释器。音频转换还需要 ffmpeg（PATH 或 FFMPEG；Windows 可使用 WSL Ubuntu-22.04 中的 ffmpeg）。
3. 执行 `./tools/build.ps1`，转换原始资源并构建包。
4. 执行 `./tools/run-sim.ps1`，点击 START RACE。Node.js 22+；可用 `PIXELWHEELS_NODE` 指定 Node。

构建包：`dist/xtensa/aster.pixelwheels.ast`；模拟器数据：`sdcard/aster/apps/aster.pixelwheels/data/resources`。这些为可重新生成的输出，不入 Git。运行必须通过启动脚本挂载资源；仅打开 AST 包不会带入外部地图。

## 操作

- 选图：左右方向键或地图两侧箭头。
- 选车：上下方向键或车辆两侧箭头。
- Enter / START RACE 开始；A / DEMO DRIVE 演示驾驶，驾驶时 A 切换自动驾驶。
- 方向键左右转向，上 / Ctrl 加速，下 / Alt 制动，低速继续制动可倒车。
- 默认自动油门，可点 AUTO GAS 切换手动；手动时显示 GAS。
- 空格 / TURBO 消耗一个加速道具，初始一个，拾取原地图礼盒可补充，最多三个。
- Esc / MENU 暂停；菜单 RESUME 继续，TRACK & CAR SELECT 返回选图选车。
- R / RESET 重置当前比赛；暂停时也可按 R。
- 透明触摸按钮按 slot 独立跟踪，可同时转向和制动/加速。电脑鼠标本身只有一指。
- 模拟器点“启用声音”后可试听原版音乐、倒计时、碰撞、漂移、涡轮、拾取和连续变调引擎。

## 当前范围

- 9 张原版地图、15 辆车、原版车轮、原 Logo 和原音乐。
- 原地图背景/前景分层、Tiled 翻转、路面材质、碰撞形状、航点与礼盒位置。
- 六辆车、AI 路线跟随、三圈比赛、排名与计时、车间碰撞、倒车、漂移痕迹、镜头前视和小地图。
- 连续引擎参考原版五段录音的相邻混合与 pow2Out 音高曲线，使用 ABI 4.1 play_ex 循环及 set_params 更新。最多四个音效句柄：两段引擎、一个漂移/涉水循环、一个按优先级复用的提示音。背景音乐全曲保留，转换时降低 9dB 以改善引擎与提示音的相对清晰度；未增加游戏内音乐开关。

这是可玩的移植首版。C 车辆求解器、圆形近似车间碰撞与栅格障碍近似替代原 Java/libGDX/Box2D，驾驶手感和 AI 尚未完整复刻；目前只实现涡轮道具，未实现导弹、枪、地雷等完整道具系统。原赛季解锁、完整菜单、成绩存档、多人分屏、桥下/跳跃/救援过场和动画车辆效果尚未完整适配。演示驾驶可能在复杂路段卡住，不能代表原版 AI。九图都已转换，不等同于九图实驾通关验收。

## 资源与内存

地图按分层 32×32 图块去重，原 64×64 图块按最近邻缩到一半；未重新设计赛道。单图约 0.36–1.60 MiB，常驻图缓冲 1,703,936 B。初始线性内存 3 MiB / 上限 4 MiB，0.1.6 已在微雪板以 AOT 部署并启动；实机声音听感与驾驶性能待用户确认。每帧固定 60 Hz 逻辑子步，最多追赶 100ms，绘制请求 30 Hz；这不是实机帧率承诺。

## 许可证和来源

上游：https://github.com/agateau/pixelwheels

基准提交：`0ed0376a6b1e4ba0f055b788fb84cb08d20cf8ea`。

游戏逻辑 GPL-3.0-or-later，原版其他代码 Apache-2.0，原始和转换后素材 CC BY-SA 4.0，均保留于 `vendor/upstream/LICENSE.*`；本运行层及转换器按 GPL-3.0-or-later 提供。图像只做解码、原版车轮组合、图块去重与尺寸/像素格式转换，素材署名 Aurélien Gâteau 及原项目贡献者。
调试：驾驶时 C 在 APP 日志输出当前位置、速度、目标航点、路面和碰撞状态，便于后续继续调校。

## 实机部署

本次构建为 ABI 4.1 / Xtensa AOT，连续音频需 audio-out 和 audio-effects。原始素材与模拟器数据保持原位置；设备资源另生成到 dist/device-resources。

在有 ffmpeg 的 Python 环境运行 `python tools/prepare_device_assets.py`，完整保留曲目并将音乐转换为 44.1kHz 双声道 128kbps MP3；每个资源文件限制为小于 4MiB，地图/车图等二进制按字节原样复制。本机使用 WSL Ubuntu-22.04 的 Python3 和 ffmpeg。

然后运行 `python tools/deploy_device.py --port <实际串口>`，验证设备、上传全部资源并 CRC 校验，再安装并启动 APP。真机路径为 `/SD:/aster/apps/aster.pixelwheels/data/resources`，游戏内仍使用 `/data/resources`。断点继续可加 `--resume`，它只复用本机记录中此前已上传且 CRC 通过、内容哈希未变的文件；SD 卡内容被更换或删除后请不用此参数。

部署不包含固件烧录。运行结果和资源上传状态分别保存在 artifacts/device/deploy.log 与 upload-status.json。
本次实机部署：已在微雪 waveshare35b / ABI4.1.0 以AOT安装并启动0.1.6，首图加载成功，新增音频资源已同步SD。详细结果见 docs/实机部署记录.md；声音和双指体验待用户确认。
