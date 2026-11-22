# Aster 音乐

公开商城版本 0.4.27，仅对 `fakepod-s31` 且提供 `media-player` 的兼容设备显示。界面按 FAKE POD NANO PRO 的 460×460 方屏设计，最低固件 ABI 4.5。MP3 播放需要带 MP3 解码的固件。在系统下拉设置连接蓝牙音箱后，支持系统输出切换的固件会将当前歌曲从有线耳机转到音箱，保留播放进度。

- 在播放页从封面或曲目信息区域向右滑进入歌词，向左滑进入曲库。反向滑动回到播放页。
- FAKE POD NANO PRO 上，在播放页、歌词页或曲库摇一摇，会随机进入「剪纸换幕」「折面换向」或「整句推镜」。再摇一次回到刚才的页面。连续两次进入会选到不同效果。每种效果有六组逐句构图，左滑可随时回播放页。
- 三种动效都整句显示英文，按单词换行。英文长句沿用艺术字，中英混排保留英文单词。增强 LRC 按时间戳推进高亮，普通 LRC 在句内估算；手机蓝牙发来的歌词按句换幕。艺术字库缺字时使用系统字体。
- 播放页等比完整显示封面。深海蓝背景和冰蓝按钮在所有歌曲间保持一致，封面周围的光晕少量取色。右下角是上一首、下一首和大号播放／暂停键，音量和进度位于左侧。
- 将同名的 JPG、JPEG、PNG 或 BMP 放在歌曲旁边即可指定封面。也支持读取 MP3 和 WAV 的 ID3 封面、FLAC 的内嵌封面。找不到可用封面时显示同色系的抽象图案。
- 本地歌曲旁边可以放同名的 `.lrc`。例如 `music/稻香-周杰伦.mp3` 配 `music/稻香-周杰伦.lrc`。支持 UTF-8、UTF-16 和 GBK，支持毫秒时间戳、同一行多个时间戳以及 `[offset]`。当前歌词按字宽换行，拖动进度条时预览目标句，暂停时保留当前句。GBK 映射表随包安装，显示使用系统字库。

- FLAC 也可以直接使用内嵌歌词。播放器优先读取同名 LRC，其次查找 FLAC 标签里的带时间戳歌词，再回退到纯文本歌词。带时间戳的歌词随播放、拖动进度及摇一摇动效同步；纯文本可在歌词页上下滑动翻阅，在动态歌词页上下滑动换句。
- 手机接收：开启系统蓝牙后，手机连接 `Aster-S31 Audio`。播放页自动显示手机提供的曲名及歌手或专辑，可播放、暂停及切歌。
- 手机通过蓝牙播放时，播放页和歌词页同步显示已播时间和曲目时长，未知时间显示为 `--:--`。要跳到指定时间，请在手机上拖动进度条。在 FAKE POD 上播放 SD 卡歌曲时，可直接拖动设备上的进度条。
- 手机支持 AVRCP Cover Art 时，播放页使用手机提供的专辑缩略图。在网易云音乐中开启「外接设备蓝牙时显示歌词」，右滑可查看手机当前发送的文字，摇一摇进入整句动态歌词。只发送歌名的播放器会一直显示歌名。
- 本地音乐：曲库读取 SD 卡 `music` 目录中的 MP3、WAV 和 FLAC。MP3 支持 32、44.1 及 48 kHz。曲库最多显示 64 首，可翻页选择，也可播放随包附带的原创试听旋律。
- 播放器打开时，本地曲目自然结束后按曲库顺序续播，末曲接回第一首。曲库只有一首或播放试听示例时，播完即停。退出到桌面后，系统播完当前曲目；手机蓝牙播放由手机管理队列。
- 蓝牙输出：用系统蓝牙的“搜索设备”连接耳机或音箱。打开本地曲目时，音乐使用当前已连接的系统输出。播放页用一行显示音源的格式、采样率和位深。
- 本地播放保留源采样率和位深（以服务协商结果为准）。蓝牙当前为 SBC，属于有损编码。
- 返回桌面保留后台音乐；切换模式会停止前一种源。当前不提供发送模式的耳机按键遥控或 AAC。
- 曲库为目录当前层，不递归子目录。安装包声明支持 `fakepod-s31` 并要求 `media-player`。支持板型检查的固件会在启动时核对这两项。

构建：在配置好 SDK 的终端运行 `aster build HiFiPlayer --target riscv --aot`，串口安装使用 `aster deploy PACKAGE --target riscv --port COM14 --run`，实际端口以设备识别结果为准。

`audio/demo.flac` 是 48 kHz/24 位双声道原创试听旋律，`art/moonlight.bmp` 是原创默认封面，两者均采用 MIT 授权。

基线 JPEG 封面使用 ChaN 的 TJpgDec R0.03，播放器分帧读取和解码，内嵌 JPEG 直接从歌曲文件读取。源码及许可随工程保留。[TJpgDec 来源与许可](https://github.com/hello-myj/aster-firmware/blob/6a54e3e2294ac9a91ec4aecc3445fbf4cd405c81/modules/imgdec/vendor/tjpgd.c)

本地播放时，进度条支持整条触摸区域按下与拖动，拖动时预览目标时间，松手提交跳转；音乐自然结束后仍可拖到目标位置继续播放。手机蓝牙播放时，设备显示手机回报的时间，在手机播放器内定位。

## 动态歌词的设计参考与字体

「剪纸换幕」「折面换向」「整句推镜」的视觉设计参考了 Folia（chthollyphile/folia-major）的「商籁」和「凝彩」，尤其是随歌词切句改变画面构图、让文字参与转场的做法。Aster 针对 460×460 方屏用 C 独立实现了整句排版、逐词高亮和动效绘制。感谢 Folia 作者和贡献者。[Folia 项目](https://github.com/chthollyphile/folia-major) · [Folia 许可证](https://github.com/chthollyphile/folia-major/blob/main/LICENSE)

随 APP 分发的艺术字轮廓来自 Noto Serif SC（`paper-serif.bin`）和 Noto Sans SC（`paper-bold.bin`），均采用 SIL Open Font License 1.1。两份字体的版权声明和许可证随安装包提供。[Noto Serif SC 来源](https://github.com/google/fonts/tree/main/ofl/notoserifsc) · [衬线字体许可](text/paper-serif-OFL.txt) · [Noto Sans SC 来源](https://github.com/google/fonts/tree/main/ofl/notosanssc) · [粗体许可](text/paper-bold-OFL.txt)

重新生成字库时，构建脚本使用 fontTools 提取字体轮廓；设备端由播放器直接绘制这些轮廓。[字库生成脚本](tools/build_paper_font.py) · [fontTools 项目](https://github.com/fonttools/fonttools)
