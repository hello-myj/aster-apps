> 旧版 0.11.x 及更早的历史记录；当前 P4/lckfb 的配置、构建和验证以 [根 README](../README.md) 为准。

> 历史内存评估。当前 0.7.0 使用 3.125 MiB 线性内存与真机 AOT；现状以 [DEVICE.md](DEVICE.md) 为准。

# 当前仙剑移植版的内存下限

更新日期：2026-10-08。当前为有声音的 0.4.0、3.625 MiB 研究版；没有修改开发板固件。

## 实测结果

| 应用线性内存 | 结果 |
| --- | --- |
| 56 页，3,670,016 B，3.5 MiB | 失败：No event buffer memory |
| 57 页，3,735,552 B，3.5625 MiB | 启动成功：当前编译布局的启动下限 |
| 58 页，3,801,088 B，3.625 MiB | 默认配置；开场、触摸对话、行走、菜单、存读档、BGM/音效、循环、暂停/恢复回归通过 |

WASM 页为 65,536 B。当前模块 __heap_base=3,731,280 B，
事件缓冲分配后 hostAllocationEnd=3,731,344 B，因此当前编译布局至少需要 57 页。
3.625 MiB 版本运行了 6476 帧、1742 次画面提交，没有退出或越界错误。
在默认配置中，上述分配末端距内存上限还剩 69,744 B，约 68.1 KiB。
完整战斗、所有地图、长剧情和通关未验证；这里是当前构建及已验证路线的下限，
不是经过全面优化后的理论最低值。

0.2.0 的启动下限是 4.1875 MiB，默认配置为 6 MiB。
0.3.0 将常驻 owner 从 2 MiB 缩到 1.75 MiB，节省 262,144 B（256 KiB），默认配置为 4 MiB。
0.4.0 不再将未被运行时音乐后端使用的原版 MUS/RIX 复制进常驻视图，
owner 缩到 1.375 MiB，再节省 393,216 B（384 KiB）；相对 0.2.0 共减少 640 KiB。
上游实际 C 资源构造器以当前数据审核出 residentBytes=1,415,356 B，
capacityBytes=1,441,792 B，保留 26,436 B 余量；运行时再次检查容量。
完整资源包仍为 50,125,017 B。音乐保持 MP3 播放；场景/玩家/战斗 arena 与战斗缓冲没有改变。
没有把运行 Windows/Node/浏览器的主机内存计作开发板 PSRAM。

## 主要固定内存

| 对象 | 容量 |
| --- | --- |
| 常驻资源视图 owner | 1408 KiB（当前数据实际使用 1,415,356 B） |
| 战斗 arena | 512 KiB |
| 场景 arena | 256 KiB |
| 玩家 arena | 128 KiB |
| 战斗效果 + 召唤缓冲 | 128 KiB |
| 存档结构 | 185.61 KiB |
| 事件对象表 | 171.875 KiB |
| Asyncify continuation | 128 KiB |
| 跨帧 C 栈副本 | 128 KiB |
| C 调用栈（链接器预留） | 128 KiB |
| 游戏两张索引画面 | 125 KiB |
| 游戏 RGB565 转换缓冲 | 125 KiB |
| 资源临时块 | 64 KiB |
| 资源目录表 | 40 KiB |

其余包括状态、字体/菜单缓存和数据对齐。大小以 build/app.map 与
build/memory-audit.json 为准。上述 owner 的容量已包含在应用线性内存里，不能再重复相加。

## PSRAM 容量判断

应用线性内存不等于整机 PSRAM。还有 Aster/WAMR 元数据和运行时、LVGL、
屏幕/表面、文件系统与音频缓冲；部分内存位于内部 SRAM，具体放置应以固件布局为准。
320×480 RGB565 表面本身需要 307,200 B（300 KiB），不含管理对象。
当前 sim 的该表面使用主机图形画布，未占用上述 WASM 线性内存，不能用它替代真机测量。

按常见 4/8/16 MiB PSRAM 档位选：
- 4 MiB 放不下当前应用本体，当前版本不适合。
- 8 MiB 是值得继续适配的最小实际目标档位；你的板子已有此容量。
- 16 MiB 有更大余量，但当前没有证据表明必须升级。

8 MiB 整机是否能可靠运行仍需重新配置固件并真机测峰值，尚未确认。
不要从 simulator 通过推导真机必然够用。

## 开发板当前限制（COM10 只读查询）

- 外部 RAM：8,388,608 B。
- 当前 SDK 单应用最大线性内存：2 MiB。
- 当前固件 WAMR 池：2,621,440 B，2.5 MiB。
- LVGL 池：1,045,476 B，查询时 used=515,236 B，peak=518,856 B。
- 内部 DRAM 堆：65,536 B，查询时 free=49,996 B；固件提示 AOT exec text 使用此堆。

因此当前首先要解决 APP 限额、WAMR 池和 AOT 可执行内存，而不是先购买更大 PSRAM。
现有 `sys mem` 的 PSRAM 95% 行包含池/布局预留；它还同时报告外部堆可分配多个块，
不能把该行直接解释为游戏只能获得剩余 5%。

## 重复测量

```powershell
cd G:\proj\aster-app\aster\apps\pal\sdlpal-aster
node aster-port/memory-audit.cjs --write-minimum
$env:PAL_TEST_MINIMUM='1'
node aster-port/smoke.cjs
Remove-Item Env:PAL_TEST_MINIMUM
```

下限测试仅生成 build/minimum.wasm 和测试 manifest，没有替换正式研究包。
报告：build/memory-audit.json；详细日志：memory-smoke.log。
