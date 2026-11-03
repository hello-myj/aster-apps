# 多指触控测试

当前商城版本的 P4 构建、资源路径和发布步骤见 [商城应用构建](../docs/market-apps.zh-CN.md)。无需独立资源文件。

独立项目，APP ID `aster.touchtest`，SDK ABI 4.1，Xtensa AOT，480×320 横屏优先，按运行时实际尺寸绘制。不依赖 SD 外部资源，不需要网络、音频或文件权限。

## 使用

- 多根手指同时按住并移动：显示彩色圆圈、硬件事件 slot 编号、坐标和最近 24 个位置的轨迹。
- 当前：仍按住的触点数；最高：本次运行或重置以来最高同时触点数；设备上限：SDK 查询到的 max_points。
- 同时两指时显示连线及两点距离，可观察捏合、分开和交叉时编号是否稳定。
- 抬起后圆圈和轨迹保留约 700ms，再消失；暂停和恢复清空触点，避免残留按下状态。
- 所有屏幕区域都记录触点，包括标题、底部和重置按钮。单指点按右上角重置，在抬起后清除计数；多指操作不会触发重置。
- DOWN/MOVE/UP 为事件累计数。异常为重复 DOWN、缺失 DOWN 的 MOVE 或无对应活跃点的 UP；它是诊断线索，不自动等同于硬件故障。
- 超容量：同时超出 APP 的 10 个跟踪槽位时丢弃的事件数。设备能力受固件和面板限制，APP 不会把单指模拟成多指。
- 键盘 Esc 退出，空格重置计数。

当前 SDK 手册记录两板驱动最多两点，实际以设备上限和真机输入为准。轮询接口没有 slot，因此本 APP 使用独立 TOUCH 事件跟踪身份，不用轮询顺序推断手指。

## 构建与运行

在项目目录运行 `./tools/build.ps1`，生成 `dist/xtensa/aster.touchtest.ast`。

电脑模拟器：`./tools/run-sim.ps1`。鼠标只能提供普通单指输入，真实多指需在开发板操作。

真机：公共 SDK 的 `aster.exe deploy dist/xtensa/aster.touchtest.ast --target xtensa --port <实际串口> --run`。

### ESP32-P4

配套 SDK 的 `riscv` 目标可生成 P4 的 RV32 ILP32F AOT，原有 Xtensa 默认目标保留。
在项目目录使用支持 P4 的 SDK：

```text
python ../../aster-sdk/tools/aster.py build . --target riscv
python ../../aster-sdk/tools/deploy.py dist/riscv/aster.touchtest.ast --target riscv --port COM11 --run
```

P4 的 480×480 RGB565 画布需要 460800 字节。应用内存上限改为 2 MiB，避免原
1 MiB 预算在 P4 运行时保留初始内存后，无足够的托管堆创建画布。初始内存和栈不变。
新版 P4 固件支持 ST7123 五点触摸，应用应显示 `reported_max=5`，并按稳定槽位绘制各指轨迹。
新版已烧录 P4 v3.2，实机 AOT 启动日志确认 `reported_max=5`、状态 RUNNING。
固件五指事件链路已通过 WASM/AOT 桥接回归；实际五指同时拖动、交叉和独立抬起效果待用户确认。
已在 P4 v3.2 上通过共享 UART 的 ASTERFS 3 上传、安装并以 AOT 启动，实际画布
480×480、状态 RUNNING；用户确认文字、彩色触点和单指拖动轨迹正常。
本机记录保存在忽略目录 `artifacts/p4/serial-deploy.log` 和 `artifacts/p4/running.log`。

项目源码、工具和产物均在此目录；SDK 共用 `../../aster-sdk`。构建通过不代表已完成多指真机验收。
## 本次交付记录

已完成 AOT 构建和 SDK 包校验，包大小 50,912 字节；已在微雪 waveshare35b、ABI 4.1.0 固件安装并启动，状态 RUNNING (err=0)。初始化确认实际画布 480×320，设备 reported_max=2。部署记录见 artifacts/deploy.log。真实双指交叉、连续移动、独立抬起效果待用户操作确认。

部署时固件多次报告 repeated invalid AXS15231B touch frames (-22)，发生在 APP 启动之前。如果存在不响应或漏点，请同时排查触摸驱动/屏模组；本 APP 只显示固件交付的事件。

## 开源许可证

GPL-3.0-or-later，完整文本见 [LICENSE](LICENSE)；仓库许可范围见 [第三方声明](../THIRD_PARTY_NOTICES.md)。
