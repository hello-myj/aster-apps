# SDK 4.1 连续音频适配（OutRun 0.3.0）

日期：2026-10-22。

## SDK 与兼容性

本次使用 E:\个人项目资料\aster\aster-sdk 中的 sdk-v4.1.0-rc.1；ABI 4.1.0，SDK commit 02ae2945433d4235770e65eb23654646283b59db。参考该 SDK 的 docs/continuous-audio.md 和 app-development.md，没有改动 SDK/固件契约。

app.toml 版本0.3.0，abi=4.1.0、abi_min=4.1，新增 audio-effects 必需能力。保持 Xtensa AOT、480×320、初始3 MiB/上限4 MiB配置及大按键/菜单。新导入无法在4.0固件加载，不能仅靠feature检测兼容；旧0.2.3包备份在 artifacts/audio-4.1/before/dist/xtensa/aster.outrun.ast。

工具目录中的构建、部署、模拟器和采样路径均已迁移至新 SDK，部署脚本检查设备ABI4.1。tools/prepare_audio.py生成新音效；tools/build.ps1会在原资源转换后调用它。

## APP 改动

音频实现从 game.c 分离为 src/port/audio.c。

- 去掉每19个逻辑步重播0.315秒片段的处理。
- 起跑先启动MP3背景会话，再通过effect_play_ex启动引擎循环；没有每帧重播引擎。
- 引擎使用0.65–1.75倍播放速率范围内的转速代理映射，考虑速度、档位、油门、刹车、路外与碰撞。升档时音高回落，再随速度上升；这是一版驾驶调校，并非对原版全部声音控制公式的逐项复刻。
- 用set_params更新音高和音量，120 ms平滑过渡，不改变播放位置；帧间状态最多每50 ms提交一次，目标变化过小时不提交。后台样本持续推进，即使APP没有新的FRAME。
- 刹车音为独立循环，开始制动时启动，松开时用effect_stop淡出80 ms，不停止背景音乐或引擎。
- 引擎淡入80 ms，循环接缝交叉淡化160源采样帧（10 ms）；刹车交叉淡化220帧（约10 ms）。循环区间覆盖完整样本。
- 起跑/碰撞为独立一次性play_ex，降低增益并加5 ms淡入，为混音留余量。
- 音频状态更新移到每帧固定步进结束后，避免同一帧多次发送过渡参数。菜单和系统暂停沿用4.1的会话暂停/恢复，不重启循环相位。
- 重试、换地图、退出时，先独立停止并卸载四个句柄，再结束背景会话；清理幂等。
- API错误计数并有限输出日志；循环启动失败有间隔和次数限制，不用高频重试淹没音频线程。

## 新资源

保留原完整音效，转换为PCM16单声道，并由4.1固件按当前输出采样率重采样：

- engine-loop.wav：16000 Hz，26081帧，1.630秒，PCM数据52162 B。
- brake-loop.wav：22050 Hz，12508帧，0.567秒，PCM数据25016 B。
- crash-once.wav：22050 Hz，15450帧，0.701秒，PCM数据30900 B。
- start-once.wav：22050 Hz，10242帧，0.464秒，PCM数据20484 B。

每个PCM data均小于64 KiB，四项共128562 B，文件头另计。引擎原22050 Hz全长转为16000 Hz，避免完整立体声音效超限；另三项保持22050 Hz。转换脚本只使用Python标准库，来源与校验值保存在 SD 资源目录 audio/continuous-audio.json。

SD位置仍为 /aster/apps/aster.outrun/data/resources/audio/。仅补传四个新文件，音乐与画面资源不变。旧变体文件可保留以回退，当前版本不再加载它们。

## 构建与运行记录

使用新SDK执行：

    E:\个人项目资料\aster\aster-sdk\aster.exe build E:\个人项目资料\aster\APPS\OutRun --target xtensa --aot

构建及包校验通过：5个源文件，22个aster导入可解析，含play_ex、set_params和effect_stop；产物 dist/xtensa/aster.outrun.ast。

已核实微雪设备abi=4.1.0，安装0.3.0成功；四个新音效传输CRC OK。实机AOT启动，状态RUNNING、err=0，Guest peak=2711552 B（约2.59 MiB）。启动后开场预热窗口约9.85 FPS；该值不代表驾驶时多路混音帧率。

原始启动日志：artifacts/device/deploy.log；音频运行采样：artifacts/fps/audio-4.1/device.log。用户已确认引擎连续正常且音高会变化；未新增或运行自动化测试套件。

## 已知边界

新SDK仍是RC1第一阶段。start_file/流式音乐会话交接可能停止音效；APP按文档先开BGM再开循环，音乐会话重开后重新武装循环，不能承诺切歌无缝。自然EOF后的背景重开仍依据公开会话状态；不伪造独立BGM播放状态。

固件尚未保证SD阻塞、MP3解码繁忙时不欠载。APP避免了逐帧重播造成的必然间隙，但不能仅凭API成功声称硬件永无断音或爆音。更改这些边界需要继续固件侧工作。

## 实机试听结果

2026-10-22，用户确认“连续正常，音高会变化”。已采集到四个音效句柄加载成功，BGM rc=0，引擎循环首次启动后继续高速行驶，刹车循环在制动操作时启动。采样期间没有 AUDIO41 API 错误日志，Map1/Map2 行驶日志 errors=0、assetErrors=0，Guest peak仍为2711552 B。

这确认了本次引擎循环与平滑变调的真实听感。没有据此宣称切歌、长时间SD压力、所有暂停入口或完整通关均已验收；这些RC1边界仍保留。
