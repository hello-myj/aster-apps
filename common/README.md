# P4 游戏公共控件

`p4_gamepad.h` 提供 480×480 屏幕的游戏区、控制区、圆盘摇杆、圆形动作键及按压反馈。只依赖 Aster SDK 的像素绘制接口；游戏输入和规则仍由各自工程实现，不引入新的虚拟机或固件 API。

当前用于 OpenOMF 0.3.1 和 OpenTyrian 0.2.3。触点在按下时确定归属，摇杆和按钮可以同时操作，拖动不跨区触发其他动作。两款应用都使用原生方屏画布，游戏视口保持 480×300 原比例。

验证命令（APPS 根目录，需先准备依赖、资源和便携包）：

```sh
python common/tests/test_p4_controls.py
node common/tests/p4-games.cjs OpenOMF OpenTyrian
node common/tests/omf-selection.cjs
node common/tests/tyrian-menus.cjs
```

前者编译真实 SDL 输入适配器，验证斜向、多指独立释放、触点归属和中文菜单行高；后者运行完整 WASM 引擎，检查画布几何、摇杆反馈、松手回中、暂停恢复并输出截图。Linux 下执行 C 测试需要 `cc` 和各游戏已准备好的 SDL 头文件。

`omf-selection.cjs` 从新配置以纯触屏确认走完驾驶员、机器人与 VS，要求真正进入竞技场并继续运行。`tyrian-menus.cjs` 走通新游戏菜单并保存截图供核对翻译与颜色；不会把无 trap 等同于所有页面视觉验收。
