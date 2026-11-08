> 历史移植记录：以下路径和内存限制包含旧版本信息。当前项目布局与编译入口以 [项目 README](../README.md) 和 [DEVICE.md](DEVICE.md) 为准。

# 仙剑 DOS → Aster 模拟器研究移植

2026-10-08。当前为**包含音乐/音效、3.625 MiB 线性内存的 0.4.0 模拟器研究版**。
使用实际仙剑引擎与实际 DOS 数据；不是原版 EXE、DOSBox、网页游戏或模拟画面。
所有运行时导入均为 SDK ABI 3.7 的公开 `aster_*` 函数。

当前微雪 SDK 上限 2 MiB。此研究包在普通 sim 和 SDK verify 中被拒绝，
不能部署到现有 Aster 固件。没有修改 SDK 契约、target JSON，也没有刷写开发板。
sim 仅在显式传入 `--experimental-memory-mib 4` 时放宽上限，默认仍为 2 MiB。
链接器 map 位于 build/app.map；容量最终以编译产物和真实运行测量为准。
当前构建默认使用 3.625 MiB，启动下限为 3.5625 MiB；配置下已通过剧情、音频、行走及存读档回归。
0.3.0 将常驻预留从 2 MiB 缩到 1.75 MiB，节省 256 KiB。
0.4.0 的音乐由 MP3 文件播放，常驻视图不再复制原版 MUS/RIX；完整资源包保留所有内容。
常驻视图实际使用 1,415,356 B，owner 为 1.375 MiB，比 0.3.0 再节省 384 KiB。
构建前由上游实际 C 资源构造器审核容量，结果写入 build/resident-audit.json；运行时再次检查边界。
模拟器参数仍使用整数上限 4 MiB，但 WASM initial/max 与实际分配均为 3.625 MiB。
编译、链接、打包和启动统一读取 memory-profile.json，避免构建与运行配置不一致。
应用内存与整机 PSRAM 的区别、开发板当前限制见 [内存评估](MEMORY.md)。

## 运行

从工作区根目录执行：

```powershell
cd G:\proj\aster-app\aster
powershell -NoProfile -ExecutionPolicy Bypass -File .\run-pal.ps1
```

屏幕原生 320×200 游戏区域放在 320×480 画布上方；下方是触摸方向键、确认、返回。
键盘：方向键行走/选菜单，Enter 或空格确认，Esc 打开菜单/返回。
游戏内部支持保存和读取进度。启动脚本显式使用 aster-port/sim-state 保存模拟器数据，
按 APP ID 隔离。用模拟器“结束”按钮或终端 Ctrl+C 退出。
在模拟器界面点击一次“启用声音”后播放音乐及音效；这是浏览器的音频播放限制。
启动脚本的 --state 指向音频资源和存档目录，勿省略它单独运行研究包。

## 构建步骤

1. 首次获取引擎：
   ```powershell
   git clone --depth 1 --branch extreme https://github.com/44670/sdlpal-embedded.git sdlpal-aster
   ```
   本次引擎基线：795fc8ae9133b020855f4ef5d44abef23b5f0fce。
2. 本次资源来源是公开镜像 https://github.com/handoing/Legend-of-Sword-and-Fairy ，
   提交 b6945a2a55717a76c7290fded90ad435acf598de，数据目录 pal-resources/pal。
   这是第三方镜像，不能据此称为官方发行或可随代码分发的数据。
   引擎代码许可与原版游戏数据许可是不同的。
   本次获取命令（未运行仓库中的 EXE）：
   ```powershell
   git clone --depth 1 https://github.com/handoing/Legend-of-Sword-and-Fairy.git pal-resources
   ```
3. 已生成资源，可重复转换或指定其他同版本完整 DOS 数据：
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\port\prepare-resources.ps1
   ```
   字体使用上游仓库内固定版本，转换器验证其哈希。
   输出 pal_full.pak 为 50,125,017 字节；开机派生常驻视图为 1,747,784 字节。
4. 安装构建用 Binaryen 和离线音频编码工具（已完成）：
   ```powershell
   cd G:\proj\aster-app\aster\port
   & 'C:\Users\23266\.cache\codex-runtimes\codex-primary-runtime\dependencies\bin\fallback\pnpm.cmd' install
   ```
5. 首次转换音频（已完成，后续通常无需重做）：
   ```powershell
   cd G:\proj\aster-app\aster
   powershell -NoProfile -ExecutionPolicy Bypass -File .\port\prepare-audio.ps1
   ```
   离线使用上游 RIX sequencer 与固定 MAME OPL2 核心生成 PCM，再用 FFmpeg 编码。
   已生成 86 首 MP3 和 237 个 WAV 片段，放在 sim-state/shared/music/pal。
   长音效分成 <=64 KiB 的 8192 Hz、双声道 S16LE WAV，运行时顺序续播，
   不通过截断或加速缩短原音效。片段交接受 FRAME 调度影响，可能有短间隙。
6. 编译、链接、跨帧转换、打研究包并运行：
   ```powershell
   cd G:\proj\aster-app\aster
   powershell -NoProfile -ExecutionPolicy Bypass -File .\run-pal.ps1 -Build
   ```
   可拆分为 compile-engine.ps1、link.ps1、asyncify.mjs、package.mjs 四步。
   使用 SDK 自带 clang 与 runtime C 文件；不依赖 demo-bounce 的中间文件。

## 实现边界

- 上游固定内存引擎、SDL shim、预解码资源和剧情/战斗逻辑保持使用。
- platform.c 连接 Aster 文件、显示、时钟、键盘、触摸、生命周期。
- audio.c 通过公开 SDK 文件播放/音效接口接入音乐切换、循环、暂停/恢复和游戏音效。
  音频从 /music/pal 读取；未给 SDK 添加 PCM 流接口，运行时不合成 OPL。
  当前音乐切换立即生效，原引擎请求的淡入淡出尚未实现。
- libc.c 提供这次链接所需的精简兼容函数，不是完整 POSIX/标准 libc。
- 没有 game-owned malloc/free；对象为命名固定容量数组与上游生命周期 arena。
- Asyncify 在构建时转换阻塞等待；转换后的模块内部保存/恢复 Wasm 调用状态及 C 栈。
  运行时不需要新增 ABI import，Aster FRAME 回调会在 yield 后返回。
- main.c 的 PAL_ASTER 条件分支替换原来的 setjmp/longjmp 退出路径。
- 当前保留 MEM_LEVEL2 的场景、玩家及战斗 arena，缩小常驻资源预留，尚未达到微雪 2 MiB APP 预算。
- 存在原版引擎与此嵌入式分支自身的兼容差异；尚未完成全剧情/战斗回归。

## 验证

```powershell
cd G:\proj\aster-app\aster\sdlpal-aster
node aster-port/smoke.cjs
```

此脚本以确定性时钟驱动真实引擎，通过普通键盘/触摸输入打开新游戏、推进开场、
行走、菜单与存读档；只读导出用于检查当前菜单、场景和位置，不注入游戏状态。
游戏验证共运行 6474 帧、1741 次画面提交，存档文件确实生成；移动后读档，
场景及 viewport 均恢复保存时的值。完整战斗、长剧情路线和通关尚未验证。
音频版另行验证了游戏正常触发的非空 MP3 背景流、PCM 音效、播放结束后的 BGM 循环，
以及模拟器暂停/恢复同步音频状态。原版 86 首 RIX 都完成解码并具有非零 PCM 样本。
截图来自实际游戏循环，放在 tmp_ui/，不能作为完整通关证据。
需要人工试玩确认视觉与操作体验。

模拟器基础与内存选项测试：
```powershell
cd G:\proj\aster-app\aster\aster-sim
node --test test/*.test.js
```
本次结果：29 通过、3 个外部示例集成测试跳过、0 失败。

SDK 验证研究包会报 invalid initial memory/stack for target，这是当前真实限制：
```powershell
aster verify .\port\build\pal-research.ast --target waveshare-35b
```

下一步若要求真机：首先缩减常驻资源/固定工作区，验证跨帧机制在 WAMR/AOT 中的行为，
评估 AOT 可执行内存；必要时调整真实固件内存布局。不能只放宽 SDK target 校验值。
