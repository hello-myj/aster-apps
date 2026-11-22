# Aster APPS

Aster 应用项目集合：一个应用对应一个独立文件夹，项目代码、素材原件、工具与文档放在各自项目内。

## 应用与支持设备

商城按设备上报的板型、AOT 架构、ABI、能力和屏幕筛选版本。鱼鹰光电 P4C5 与 FAKE POD NANO PRO 同为 RISC-V，商城仍使用各自的板型 ID。

- [Aster 音乐](HiFiPlayer/README.md) 支持鱼鹰光电 P4C5 和 FAKE POD NANO PRO 的方屏、本地音乐和三种全屏动态歌词。鱼鹰在歌词页双击切换，FAKE POD 支持摇一摇和蓝牙音频。需要固件 ABI 4.5 和 `media-player` 服务。
- [楚河·中国象棋](Xiangqi/README.md)、[黄金矿工](GoldMiner/README.md) 和 [仙剑奇侠传](PAL/README.md) 提供鱼鹰 P4C5 和立创 ESP32-S3 实战派两个架构的商城包。
- [OutRun](OutRun/README.md) 和 [Pixel Wheels](PixelWheels/README.md) 同时支持这两块板。OutRun 在立创板可用倾斜转向，Pixel Wheels 使用屏幕按键。
- [OpenOMF](OpenOMF/README.md) 和 [OpenTyrian](OpenTyrian/README.md) 的实机商城版本面向鱼鹰 P4C5，分别针对方屏设计格斗和射击操作区。
- [TouchTest](TouchTest/README.md) 可在以上三款开发板查看设备上报的触点与轨迹。

## 构建、安装与分发

商城程序由首次开机安装，当前为 0.3.3。各游戏和播放器独立更新，同一项目的两款板也可能处于不同版本，例如鱼鹰上的仙剑为 0.13.0、立创上的仙剑为 0.12.3。

每个应用目录有独立的 `app.toml`；`store.toml` 记录上架变体、资源目录及适用设备。应用需要的素材转换工具和第三方资源在各工程 README 中说明。已安装 SDK 的开发者可从本仓库根目录执行：

```sh
aster build HiFiPlayer --target riscv --aot
aster verify HiFiPlayer/dist/riscv/aster.hifi.ast
aster deploy HiFiPlayer/dist/riscv/aster.hifi.ast --target riscv --port PORT --run
```

将 `PORT` 换成实际串口。播放器的歌词艺术字和试听资源包含在包内。带独立资源树的游戏需要将对应版本的程序与资源一起安装。游戏资源准备、构建和上架命令见 [商城应用构建](docs/market-apps.zh-CN.md)。

[AppStore](AppStore/README.md) 是设备端商城；[StoreServer](StoreServer/README.md) 提供本地目录和下载服务。从仓库根运行 `python -m StoreServer serve` 可启动本地服务。[StoreDemo](StoreDemo/README.md) 是带独立资源的安装测试工程。公网目录由 [发布配置](https://github.com/hello-myj/aster-releases/blob/main/publication.json) 选择应用。协议、断点续传及版本兼容规则见 [应用与资源分发](docs/app-store-plan.zh-CN.md)。

## 第三方来源

OutRun 原版源码和素材完整存放于 `OutRun/vendor/upstream`，按普通文件纳入本仓库，不使用子模块。上游为 ZgzInfinity/OutRun，基准提交 `6882721c99bc79365f815f68d389bf5078129e68`，许可证及原始说明保留在该目录。各项目和第三方内容按各自许可证使用。

## 开源许可证

本仓库自行编写的应用代码、移植代码、工具及文档，除另有声明外，采用 **GNU GPL v3.0 或更新版本（GPL-3.0-or-later）**，完整文本见 [LICENSE](LICENSE)。

该许可证允许使用、修改、商用和再分发；对外分发受 GPL 约束的程序或修改版时，需要按其条款提供对应源代码并保留版权及许可声明。

第三方代码与素材保留原许可证：Pixel Wheels 游戏代码为 GPL-3.0-or-later，部分工具代码为 Apache-2.0，图像和音频素材为 CC BY-SA 4.0；OutRun 继续遵循上游及各文件的许可声明。仓库根许可证不替换这些第三方条款。详细来源、署名和范围见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
