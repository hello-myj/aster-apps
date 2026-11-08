> 旧版 0.11.x 及更早的历史记录；当前 P4/lckfb 的配置、构建和验证以 [根 README](../README.md) 为准。

# 微雪开发板安装与传输

> 2026-10-12 更新：0.9.0 使用 SDK 0a954691e053，目标统一为 `xtensa`，声明横屏。全屏游戏、左下透明摇杆和右下确认/返回，已通过立创 320×240 和微雪 480×320 模拟器回归与 AOT 包校验；线性内存 3.375 MiB，尚未部署新版到两块真机。下面是 0.7.0 的历史真机记录，旧 `--target waveshare-35b` 命令在新 SDK 下应改为 `--target xtensa`。两块板使用相同 `port/build/pal-device.ast`，分别使用实际串口部署；资源与音频需存在各自 SD 卡。更换开发板或 SD 卡时必须单独保存传输 journal，避免误用另一台设备的完成记录。

2026-10-08，设备 COM10；应用版本 0.7.0。使用用户更新后的 SDK/固件。
首轮资源及音频传输完成：371/371 文件、81,524,531 B，SDK CRC 检查成功，耗时 2696.6 秒。应用更新会替换 assets 目录，因此游戏资源改放更新时保留的共享 `/music/pal/resources/`；48 个分片补传完成，共 50,125,017 B，耗时 1213.7 秒。音频仍保留。详细状态以 `build/device-transfer-progress.json` 为准。

## 当前配置

- WASM 初始/最大线性内存 3,276,800 B（3.125 MiB）。
- 0.6.0 设备使用 WASM 解释器。0.7.0 包含 Xtensa ESP32-S3 AOT，真机启动日志确认 `847 KB, AOT`。编译使用 `--size-level=0`；小代码模型会报 target address out of range 并回退到 WASM。
- 像素转换使用 256 色 RGB565 查表；触摸按钮高亮并保留至少 250 ms 反馈，显示“已接收确认/返回”。该反馈表示输入已入队，剧情动作的结果仍以游戏画面为准。
- 输入队列保证快速按下/松开跨两次引擎采样，防止同一次排空队列后 held-key 状态已松开而漏掉点击。模拟器快速点击菜单回归、存读档和音频回归通过：6516 帧、2159 次提交。
- C 栈 64 KiB，跨帧栈副本 64 KiB，Asyncify continuation 128 KiB。
- 常驻资源 owner 1 MiB，实际使用 960,936 B，剩余 87,640 B。
- 原版 MUS 不常驻；RGM 头像按需读入已有临时块。资源包内容未删减。
- 模拟器的分片模式回归通过：6476 帧、1742 次画面提交，覆盖开场、对话、行走、菜单、存读档和音频。完整战斗、全剧情和通关未验证。
- 新 SDK 的微雪目标上限为 7.5 MiB；设备有 8 MiB 物理 PSRAM。AOT、系统及显示也占用内存，SDK 上限不等于设备实际可分配量。

## 命令（在工作区根目录逐条执行）

```powershell
cd G:\proj\aster-app\aster
. .\env.ps1
aster doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\pal\port\build-device.ps1
aster verify .\apps\pal\port\build\pal-device.ast --target waveshare-35b
aster deploy .\apps\pal\port\build\pal-device.ast --target waveshare-35b --port COM10
```

应用包由 SDK 安装。资源与音频单独传输，避免将大资源放入安装包；游戏数据使用共享音乐目录下的 resources 子目录，以免后续应用更新清空 assets。
原资源文件 50,125,017 B，分成 48 个不超过 1 MiB 的文件，游戏按逻辑偏移读取，跨片读取由平台层完成。
另有 86 首 MP3、237 个 WAV 片段。传输总计 371 文件、81,524,531 B（77.75 MiB）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\pal\port\deploy-payload.ps1 -Port COM10
```

仅补传游戏数据可添加 `-OnlyGameResources`。主程序同时支持包内 assets 和共享 resources 读取。

SDK 每个文件传输成功后才登记已完成状态；失败可重新运行相同命令，已登记且本地哈希一致的文件会跳过。
此恢复记录仅适用于同一张未清空的 SD 卡，重新格式化或更换卡后须先移走 `build/device-transfer-journal.json`。
传输期间勿同时运行占用 COM10 的其他命令。

另开 PowerShell 终端查看进度：

```powershell
Get-Content .\apps\pal\port\build\device-transfer-progress.json
Get-Content .\apps\pal\port\device-transfer.log -Tail 20
```

传输结束后检查并启动：

```powershell
aster shell --console COM10 aster app info com.example.palresearch
aster shell --console COM10 aster app start com.example.palresearch
```

日志：`device-transfer.log`。完成记录：`build/device-transfer-journal.json`。
0.6.0 真机卡在第一个回调，停止超时。通过只读 esptool 芯片查询后的 hard-reset 恢复设备，未刷写固件或清空 SD：

```powershell
& "$env:ASTER_SDK\runtime\python.exe" -m esptool --chip esp32s3 --port COM10 --after hard-reset chip-id
```

0.7.0 已确认 AOT 加载、完整数据读取（full=50125017 resident=960936 segmented=1）、标题/菜单显示和确认进入开场剧情。真机截图在 `../tmp_ui/aster-device-aot*.png`。开场动画的稳定测量为 53–54 次提交/约 5 秒（10.6–10.8 次/秒），单次游戏回调峰值 76–80 ms；开场对话测到 33 次/5 秒。FRAME 回调数和画面提交数不同，配置 fps=60 不能当实测游戏 60 FPS。剧情加载窗口峰值 831 ms，首次资源装载峰值 2325 ms，仍有优化空间。

功放仍报告 `aster_audio: pa gpio not ready`，不能声称真机声音已修复；完整剧情、战斗和长期稳定性尚未验证。
