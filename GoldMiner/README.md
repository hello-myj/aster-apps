# 黄金矿工 / GoldMiner 0.1.1

摇钩拾金，重温旧日午后的闪光。

基于 GoldMiner-Rebirth 5ace977fb5770004009ec7b089c95ffa95240efa 的原生 C 移植。运行代码是 C → WASM → Xtensa AOT，没有 Lua 虚拟机或 LÖVE 引擎。vendor/upstream 中的 Lua 仅作来源与行为对照。

原作320×240画面保持比例，绘制为384×288并居中于480×320原生画布，底部32像素操作栏不遮挡矿石。图片离线以预乘alpha Lanczos采样到最终尺寸，小字使用12像素DejaVu Sans，标题保留Kurland风格，文字不再经过整帧缩放。提供原图片、动画及完整音效；不把该复刻称为官方 Flash 原版源码。

## 操作

- 菜单：上下选择，Enter 或 Z 确认；触摸 Start Game/High Score 可直接进入。
- 挖矿：Down / S / Enter / Z 放钩，也可点击游戏区域或 DROP。
- 炸药：Up / W / X；必须拥有炸药且正在拉回物品。
- 达标提前过关：Space / NEXT。
- 商店：左右 / A、D 选择，Enter / Z / BUY 购买，Space / NEXT 继续。
- Esc / P / MENU 暂停；暂停页可继续、回菜单或退出。
- 支持触摸 slot 事件；一次 DOWN 触发一次操作，不将两个手指合并为一个状态。

## 构建和模拟器

SDK默认为相邻的 ../../aster-sdk，也可指定 ASTER_SDK。资源准备需要 Python3、Pillow 和 ffmpeg；Windows未找到ffmpeg时使用WSL Ubuntu-22.04中的ffmpeg。GOLDMINER_PYTHON和GOLDMINER_NODE可指定解释器。

```powershell
./tools/build.ps1
./tools/run-sim.ps1
```

默认端口8850，使用命令行输出的完整带会话令牌的URL；进入后点击“启用声音”。模拟器运行WASM，不能代表实机AOT帧率。

包在 dist/xtensa/aster.goldminer.ast，资源都包含在包里，安装时落在APP的SD资源目录，无需手工搬资源。原始资源在vendor/upstream，离线转换输出在assets；配置/最高分在APP独立/data/savedata.bin。

## 内存与范围

当前固定线性内存1310720B（1.25MiB），其中655360B为APP私有C池；Host管理堆393216B、调用栈32768B。图片/字形LRU缓存上限393216B，精灵表按帧拆分读取。约3.35MB资源包放SD，不要求整包常驻RAM；C池、Guest线性预算、Host/固件总占用不是同一个数。

最低ABI4.1，当前包目标ABI4.2.0。只使用公开的文件音效、显示、触摸和文件接口，没有私有导入，没有Lua/WASI/SDL Host导入。2026-10-25已部署微雪3.5B，使用现有ABI4.2固件直接运行AOT；约三分钟菜单观察结束RUNNING/err0，0.1.0菜单实测约15.3FPS。用户已确认横屏、触摸及声音正常；已记录第三关L3_2的实际抓取、计分及道具效果，0.1.0稳定关卡样本约13.3–13.4FPS（菜单约15.3FPS）。

## 结构及对齐范围

- port/game.c：对应上游GameStates.lua、Entities.lua、props.lua的状态机、60Hz固定步进、钩子/碰撞、TNT连锁、道具、计分、商店和最高分。
- port/levels.h：prepare.py从levels.lua的字面数据直接生成，共31套布局（含上游调试和未被当前循环选中的布局），不手工重新摆放。
- port/platform.c：有界RGB565/alpha绘图、最终尺寸字形、音频四句柄LRU、触摸/键盘和Aster生命周期。
- port/allocator.c、wasi_adapter.c：复用本仓库的有界C池和libc I/O适配。构建验证最终无WASI Host导入。
- tools/prepare.py：转换上游图片/字体/音效与关卡，校验固定WASI依赖哈希。其他APP仅可作为下载缓存，缺失时会独立下载；运行时不依赖其他项目。

保留原关卡坐标、质量/价值、65度每秒摆钩、100单位每秒出钩、61秒关卡、目标递增、神秘袋、鼹鼠、钻石鼹鼠、骨头、TNT及五种商店道具。C版随机数算法与Lua不同，随机商店/后续关卡抽样不保证逐种子一致。字体栅格和动画相位可能与LÖVE略有不同；没有声称逐帧等同Flash或已经全关卡验收。

## 许可

移植适配与工具为GPL-3.0-or-later，见LICENSE；上游代码MIT及各依赖声明保留。上游明确代码MIT，素材来自互联网并经修改；素材原有权利不由代码许可证覆盖。来源见vendor/UPSTREAM.md。

## 0.1.1 清晰度调整

原生480×320输出，HUD加深颜色并调整金额间距；触摸坐标同时逆变换，游戏逻辑和碰撞坐标保持320×240。字体和图片在资源准备阶段转换，APP仍固定1.25MiB线性内存。模拟器1400帧真实输入回放完成第一关、购买道具并进入第二关，无trap。实机验证结果见docs/移植记录.md。

新增小字号字体来自DejaVu Sans，字体及授权在vendor/fonts/；授权独立于项目代码许可证。
