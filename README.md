# Aster APPS

Aster 应用项目集合：一个应用对应一个独立文件夹，项目代码、素材原件、工具与文档放在各自项目内。

## 当前项目

- [OutRun](OutRun/README.md)：OutRun 重制版移植，微雪 480×320 横屏、虚拟按键、地图选择、连续引擎音，ABI 4.1 / Xtensa AOT。
- [TouchTest](TouchTest/README.md)：多指触控测试，显示触点编号、彩色轨迹、坐标、双指距离与事件统计，ABI 4.1 / Xtensa AOT。

- [PixelWheels](PixelWheels/README.md)：Pixel Wheels 俯视赛车移植版 0.1.6，原版 9 赛道与 15 车型，480×320 横屏、连续引擎与比赛音效，ABI 4.1 / Xtensa AOT，支持模拟器与微雪实机。

## 目录约定

```text
aster/
  aster-sdk/      共用 SDK（独立安装，不提交到本仓库）
  APPS/          本仓库
    OutRun/
    TouchTest/
    <新项目>/
```

新项目直接放在 APPS 下，与现有项目平级；不把项目依赖的原始素材拆到 APPS 的另一个项目文件夹。

## 构建

安装兼容的 Aster SDK 后，进入各项目执行 `./tools/build.ps1`；模拟运行执行 `./tools/run-sim.ps1`，具体要求见项目 README。OutRun 的部分脚本仍记录当前开发机 SDK 路径，更换开发机时需同步配置。

编译输出 build/dist、运行记录 artifacts、Python 缓存和生成的 sdcard 资源不入库。OutRun 的 SD 资源由项目内 `vendor/upstream` 和资源转换工具重新生成；新克隆后先安装 `OutRun/tools/requirements.txt` 中的 Python 依赖并运行构建。真机部署还需按 OutRun 文档打包并上传外部 SD 资源，仅安装 AST 包不够。

## 第三方来源

OutRun 原版源码和素材完整存放于 `OutRun/vendor/upstream`，按普通文件纳入本仓库，不使用子模块。上游为 ZgzInfinity/OutRun，基准提交 `6882721c99bc79365f815f68d389bf5078129e68`，许可证及原始说明保留在该目录。各项目和第三方内容按各自许可证使用。
