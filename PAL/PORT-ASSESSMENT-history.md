# 微雪开发板剧情游戏移植评估

日期：2026-10-07。结论：硬件具备运行经过嵌入式适配的传统 2D RPG 的潜力。
当前 Aster SDK 不支持直接编译运行桌面 SDLPAL；完整仙剑运行、性能和通关均未验证。

## 实际设备读取

仅执行只读探测与系统查询，没有安装应用或刷写固件。

```powershell
cd G:\proj\aster-app\aster
. .\env.ps1
aster ports
aster probe --port COM10 --target waveshare-35b
aster shell --console COM10 aster sys info
aster shell --console COM10 aster sys mem
aster shell --console COM10 aster sys features
```

握手：product=aster target=waveshare-35b abi=3.7 serial=shared sd=1。
系统：Zephyr，aster.esp32s3.waveshare35b，ESP32-S3，240 MHz，2 核，
外部 RAM 8388608 B，Flash 16777216 B，SD 已挂载，11 个已安装应用。
当前系统报告内部 RAM 327680 B；这不是芯片宣传的 SRAM 总量，也不是空闲量。

本次内存查询：
- dram heap size=65536 peak=35660 used=15316 free=49996；固件明确注释 AOT exec text 使用此堆。
- WAMR pool=2621440 B。
- PSRAM static=113040，heap=7864320，used=7977360/8388608 (95%)。
- 外部堆探测：largest free block >=1048576 B，8×150 KB 块可同时分配。

95% 与池预留/布局有关，不能从该行推导只有 5% 可用于 APP；结合运行时池和堆探测判断。
也不能把 8 MiB 物理 PSRAM 全部算入单 APP 预算。

## 当前 Aster 边界

SDK targets/waveshare-35b.json：320×480 RGB565，横竖屏，最大线性内存 2097152 B，
target status=experimental。硬件/固件具备显示、触摸、文件、音频等能力。
APP 是 C→WASM/AOT、init/event 生命周期。没有桌面 SDL、标准 POSIX、线程、永久主循环约定；
公开精简 libc 不承诺通用 malloc/free。AOT 仍受运行时内存限制，并不是绕开 WASM 内存预算。
音频公开接口以文件播放和有限音效为主，没有通用 PCM 流写入接口。

320×200 8-bit 索引画面 64000 B；两份 128000 B；320×200 RGB565 128000 B。
480×320 RGB565 307200 B。只说明帧缓冲预算，不代表整个仙剑运行内存。
完整剧情数据和常驻 RAM 大小并非正比：地图、素材、文本可以从 SD 按需读入。

## 可参考的代码

- SDLPAL：https://github.com/sdlpal/sdlpal
  仙剑经典版本的跨平台实现，以 C 为主，依赖 SDL 及部分 C++ 音频组件。
  game.c 有永久主循环；global.c 使用 malloc/fread/fseek；video.c 有 320×200 索引画面。
  不是仅改 include 就可成为 Aster APP。
- ESP32 分支：https://github.com/44670/sdlpal-embedded
- 当前移植文档：https://github.com/44670/sdlpal-embedded/blob/extreme/esp32s3/README.md
  MEM_LEVEL1 面向约 500 KB SRAM 无 PSRAM，MEM_LEVEL2 面向至少 4 MB 直接映射 PSRAM。
  预处理资源、固定内存和按需 SD 读取值得借鉴。
  现成配置涉及 Cardputer/CoreS3/小喵，没有已确认的微雪 Aster APP 目标。
  LEVEL1 还使用 NOR 缓存视图，而 Aster APP 没有同等公开映射接口；LEVEL2 也不能直接塞进 2 MiB。
  上游文档明确说明资源覆盖和测试不能替代自然剧情路线/真机验收。

原版游戏资源不随引擎提供，用户需要提供合法取得的 DOS 或其他选定版本资源。
嵌入式分支当前资源工具针对 DOS 数据，不应混用 98/Win95 数据。

## 建议路线

保留 Aster：先从嵌入式分支借鉴资源处理和低内存架构，实现 Aster 文件、显示、输入后端，
把剧情/菜单/战斗中的阻塞等待转换为可暂停状态，预转换音乐为 SDK 能播放的文件。
先做完整一张地图、NPC 对话、一次战斗、存读档的验证场景，再逐步覆盖完整内容。
先 sim 验证功能，再在 COM10 真机测加载、AOT 大小、WAMR 峰值、帧时和 SD 延迟。
若加载或预算不通过，应修改真实固件内存布局/执行模型，而不是仅放宽 target JSON。

原生固件：使用 ESP-IDF/Zephyr 原生版本能避免 APP 沙箱和事件执行模型约束，
但需重新适配微雪屏幕、触摸、SD、音频，并替换/整合现有 Aster 固件。
本次没有执行此路线。

原创内容：C 实现地图、任务、对话分支、回合战斗、背包和存档，剧情/素材作为 SD 数据，
适合从当前 Aster API 设计。可以做丰富完整的游戏；内容制作和游戏设计仍是主要工作。

验收必须区分：构建成功、sim 运行、真机局部场景运行、完整流程与通关验证。
