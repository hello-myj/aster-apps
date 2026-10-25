# OpenOMF → Aster：基础对战预览 0.1.1

使用原版 One Must Fall 2097 的机器人精灵、竞技场、招式数据、音乐和音效，在 Aster 模拟器运行的基础对战适配。独立工程完整放在此文件夹。当前只完成模拟器预览，未部署或评估微雪实机性能。

**当前不是完整桌面 OpenOMF 引擎移植，也未达到原版完整手感。** 上游 SDL/OpenGL、桌面依赖无法直接用于 Aster，当前 C 运行层只接入部分机制。

## 当前范围

- 11 台原版机器人、5 个原版竞技场背景，选机选场。
- 读取原 AF 动画帧顺序/时长、RLE 精灵、位移标签、招式输入、伤害与碰撞点；碰撞点测试目标精灵像素。
- 拳脚、前后移动、跳跃、下蹲、后退防御、部分招式生成物、受击、血条和回合结算；三局两胜，AI 或演示对战。
- 原 PSM 音乐离线转换为完整 MP3；原 SOUNDS.DAT 生成 PCM16 短音效，使用四个句柄缓存。
- 原生320×200场景按最近邻放大到480×300，在480×320横屏内居中，透明触摸按键分slot处理。

AI、受击硬直、投技、连招判定与跳跃处理是基础近似；动画脚本只接入部分标签。已取消自动挑选必杀技的SPECIAL捷径，特殊招式需要按原AF方向指令加拳/脚。场景只加载背景，未接入竞技场动态物件/危险机关。驾驶员选择/完整属性、锦标赛升级/存档、拆解与毁灭终结技、完整音频时序、网络和桌面菜单仍待移植。11台资源均已转换，不等同于逐台完整招式验收。

## 操作

先点击模拟器的“启用声音”，再点游戏画面获得键盘焦点。

- 菜单左右选玩家机器人，上下选竞技场；CPU机器人用右侧箭头选择。
- Enter / START FIGHT：开始；空格 / DEMO FIGHT：AI演示。
- 方向键或WASD移动/跳跃/下蹲；后退可防御。
- Z 拳、X 脚、C 同时按下拳脚；特殊招式使用方向序列加拳/脚。
- Esc / MENU：暂停；RESUME继续，ROBOT & ARENA SELECT返回选机。
- 回合结束按Enter或NEXT ROUND继续；两胜后REMATCH重开。R重开比赛。
- 触摸使用画面下方透明按键；鼠标只有单指；电脑按键生成独立虚拟触点，走相同的touch处理函数并同步高亮，支持斜方向与攻击组合。

## 0.1.1 输入与动画改进

- 方向历史与拳脚缓冲分开，按原AF招式顺序匹配；拳脚缓冲3个28ms逻辑tick，方向历史使用约9tick（252ms）的时间窗口。
- 输入事件立即记录方向和拳脚，匹配时按朝向镜像；接入位置限制、空中一次攻击及jg/jl/jm/jh/jj/jp/jz/jn连招许可。
- 接入低段/空中攻击与站防/蹲防区别，以及AF格挡伤害、格挡硬直数据。
- x±/y±按每逻辑tick执行；跳跃从原动画约110tick处进入，斜跳动画步进7，后跳反向播放；左右墙界改为20/300。
- 键盘映射为6个保留虚拟触点slot，方向键/WASD同义键聚合，松开其中一键不会错误释放仍由另一键按住的按钮。暂停、页面切换清空触点；Esc暂停/恢复。
- 转换资源格式升级为OMF2，Move64字节、Frame36字节，旧包不能混用新资源；tools/build.ps1一起重建。

尚未对齐：完整动画脚本和extra脚本、投技/击倒/受击状态机、驾驶员完整属性与耐力、原AI、竞技场动态物件与危险机关、终结技和原菜单。上述改进不等于完整原引擎移植。

## 构建与运行

1. SDK位于../../aster-sdk，或设置ASTER_SDK。
2. Python安装tools/requirements.txt，设置OPENOMF_PYTHON可指定解释器。
3. 安装ffmpeg用于原PSM音乐转换；支持FFMPEG或PATH。Windows也可使用WSL Ubuntu-22.04中的ffmpeg。
4. tools/build.ps1：必要时从上游官方链接获取数据，转换资源、构建Xtensa AOT及WASM包并校验。
5. tools/run-sim.ps1：Node.js22+，默认8847端口；OPENOMF_NODE指定解释器。

构建包dist/xtensa/aster.openomf.ast；模拟器外部SD挂载sdcard/aster/apps，Guest读取/data/resources。build/dist/artifacts/sdcard均为可重建输出，继承仓库忽略规则；原始游戏数据单独下载且不入Git。

## 内存与移植边界

单机器人包约0.34–0.70MiB，包含原压缩精灵，常驻两台机器人缓冲各733,802B。只解码当前姿势，包含4个生成物槽，总计6个65,536B姿势缓存；逻辑图像320×200 RGB565。Guest配置初始3MiB/上限4MiB，模拟器普通表面另约300KiB Host缓存。AOT已构建不代表已经能在实机维持可用帧率。

## 来源与许可证

运行层和转换器GPL-3.0-or-later，见LICENSE。OpenOMF上游MIT，启动图标CC-BY-4.0；原OMF游戏数据保留原权利，不能由仓库GPL替其授权。详细出处见vendor/UPSTREAM.md，原许可保留在vendor/upstream。移植记录见docs/移植记录.md。

## 官方完整引擎电脑版对照（0.8.6）

用户要求完整原版体验后，原src/main.c简化战斗层不再作为完整移植的交付路线。完整Aster原引擎适配尚未完成。当前原引擎的对照版本为官方Windows发布0.8.6，源码版本e389b0b5f96c9eda5f2894cacd19202952cdc8a3；本目录vendor/upstream的开发版源码与该发布版须区分。

运行tools/run-reference.ps1，会准备并打开独立桌面窗口；首次下载由tools/prepare-reference.ps1完成并校验官方SHA256。保留官方配置和游戏默认速度。它不是Aster模拟器，也不能代表微雪上已经运行完整引擎。

- 官方来源：https://github.com/omf2097/openomf/releases/tag/0.8.6
- Windows压缩包SHA256：16e737154b61683d9674de978f48f13e493c6b590ab7237269490d31b6a2fb13。
- 游戏、资源、配置、日志及存档都位于artifacts/reference-0.8.6/openomf，继承artifacts忽略规则，不重复提交发布二进制。
- 官方默认P1方向键移动，Enter拳，右Shift脚；Esc返回菜单。可在游戏设置里修改操作。
- 启动记录确认OpenGL3初始化、SDL_mixer音频设备打开、原SOUNDS.DAT与语言资源加载、Engine initialization successful。当前工具没有桌面画面读取能力，未声明已观察到菜单或完成实战验证。

后续Aster适配要直接复用上游har.c、player.c/object.c、intersect.c、arena.c、game_state.c、控制器/AI与原格式读取器，并替换平台层：SDL事件/时钟→Aster事件，桌面OpenGL→软件调色板绘制，文件与配置→Aster文件系统，音频→Aster音频接口，有界内存分配器→APP自有内存池。战斗规则、完整动画标签/extra脚本、投技、击倒、连招与AI不再用简化实现代替。当前没有完成这一适配，真机内存和帧率也尚未确定。