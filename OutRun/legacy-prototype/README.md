# OutRun for Aster 4

这是基于 ZgzInfinity/OutRun 重制版的 480×320 横屏可玩首版。首版移植了 Map1 路段曲率数据和上游车辆/棕榈素材，并以 Aster ABI 4 图形与触控 API 重写单车道伪 3D 驾驶循环。

![Aster ABI 4 模拟器最终画面](artifacts/drive-final.png)

## 构建与运行

需要 Windows、Python、Node.js，以及项目配置中的 Aster SDK 4.0.0。构建会先从旁边的 `../vendor/upstream` 转换素材，再编译 Xtensa AOT 包并运行包校验：

```powershell
./tools/build.ps1
```

生成物为 `dist/xtensa/aster.outrun.ast`。SDK 的 AOT 构建已通过，但本次只在 SDK ABI 4 模拟器测试；模拟器运行 WASM 并忽略包内 AOT。设备安装后，将 `sdcard/aster/apps/aster.outrun/data/resources/` 下的文件复制到 SD 卡 `/aster/apps/aster.outrun/data/resources/`。程序会先读 `/data/resources`，资源缺失时回退到包内 `/assets/resources`。

用 SDK 自带的 ABI 4 模拟器运行：

```powershell
./tools/run-sim.ps1 --headless --frames 300 --screenshot ./artifacts/drive-final.png --diagnostics ./artifacts/final-diagnostics.json
```

SDK 自带模拟器会由脚本将外置资源复制到 simulator state 目录。当前构建输出未签名；未连接真实设备，未验证设备安装、触摸面板响应或实际刷新率。

## 操作

- 油门自动保持；触屏下方左侧按钮左半/右半分别向左/右转，按钮画有方向箭头。
- 触屏下方右侧按钮按住刹车，松开即解除。
- 在下方控制条以外触摸屏幕左/右半部也会分别转向。
- 驶出路面会减速；接近同车道交通车时发生碰撞、减速并记录碰撞数。

设备只有单触点时，转向与刹车采用明确的单触点操作；不能同时转向和刹车。

## 目录

- `src/main.c`：Aster 生命周期、道路投影、驾驶状态、触控、交通车、碰撞和 HUD。
- `tools/prepare_assets.py`：从上游 Map1 与素材确定性生成运行资源、清单及包内回退副本。
- `tools/build.ps1`、`tools/run-sim.ps1`：构建/verify 与模拟器资源挂载及启动。
- `sdcard/aster/apps/aster.outrun/data/resources/`：可复制到设备 SD 卡的外置资源。
- `artifacts/`：最终截图、构建/verify 日志、模拟器输入回放、碰撞及外置资源诊断。
- `LICENSE`：保留的上游 GPL-3.0 许可证。

## 实测摘要与限制

本地上游 commit `6882721c99bc79365f815f68d389bf5078129e68`。ABI 4.0 AOT 构建和 AST verify 均退出码 0；SDK simulator 0.2.0、Node v24.14.0 在 480×320 下外置资源运行 300 帧、300 次 present、退出码 0、错误计数 0，并记录交通碰撞。触控 replay 记录了左右转向及刹车按下/释放；另有无输入碰撞回放。详见 `docs/代码移植文档.md` 和 `artifacts/`。

诊断中 300 帧平均宿主执行时间约 31.5 ms、最大 40.2 ms；这不是设备显示刷新率。surface 为 307,200 字节 RGB565，应用配置 1 MiB 初始 guest memory、2 MiB 上限、512 KiB heap。5 张常驻图的未压缩像素约 27 KiB，赛道采样 2 KiB、图形命令缓存 6 KiB、绘制 arena 约 1 KiB；这些是静态估算，不等同于 SDK 全部运行时或真机可用内存。完整 3D、其他赛道/分支、菜单计分、原版 AI、音乐音效均未迁移。公开分发素材前请单独核实上游美术资源权利。
