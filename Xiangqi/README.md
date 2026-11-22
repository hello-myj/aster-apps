# 楚河 · 中国象棋

Aster ABI 4.3 游戏，共用一份源码，分别构建 ESP32-P4 和 ESP32-S3 AOT 包。

- P4：480×480，居中棋盘、木色棋子、两侧题字。
- lckfb：240×320 竖屏，紧凑棋盘和底部操作栏。
- 点击己方棋子，再点击绿色提示位置落子；支持悔棋、自动存档、退出续局。
- 设置中选择人机或双人对弈、执红或执黑、入门／进阶／挑战。执黑时自动翻转棋盘。
- AI 使用分帧迭代加深 Alpha-Beta 搜索；每帧计算预算 P4 约 24 ms、lckfb 约 16 ms，各等级同时受深度、节点数和总时间限制，设备算力会影响实际搜索深度。入门／进阶／挑战的总时间上限约 0.4／1.5／3 秒；加入静态搜索、置换表、杀手走法、历史排序和位置／子力活动度评估。
- 将死、困毙或和棋后显示结束页，可再来一局、查看终局或返回桌面。
- 包内包含图标和原创选子／落子、吃子、将军及结束音效（落子轻微变调，选子和悔棋有独立反馈），无需额外复制游戏资源；使用设备现有系统字库。

实现常用棋子走法、将军限制、将帅照面、将死及困毙。和棋采用三次重复或连续 120 步未吃子且未动兵卒的简化规则，不实现比赛中的长将、长捉裁决。

## 构建

```powershell
aster build . --target riscv --aot
aster build . --target xtensa --aot
```

输出为 `dist/riscv/aster.xiangqi.ast` 和 `dist/xtensa/aster.xiangqi.ast`。
用 SDK 串口安装工具安装对应架构的包，或通过模拟器 `--profile osptek-p4 --app <工程目录>` 或 `--profile lckfb-s3 --app <工程目录>` 启动。

## 验证

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/rules.c src/xiangqi.c -o rules
./rules
```

规则测试包括初始局面深度 2/3 的 1920/79666 个节点、蹩马腿、塞象眼、炮架、过河兵、九宫、照面限制、随机合法对局，以及分帧 AI 的预算和落子合法性。可加 `-DXQ_TEST_REFERENCE -fsanitize=address,undefined` 检查快速将军检测与参考实现是否一致，并检查内存。

`tools/assets.py` 使用 Pillow 生成图标和落子声；运行时无需 Python。

许可证沿用仓库 GPL-3.0。

