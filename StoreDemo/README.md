# 商城外部资源演示

用于验证“程序 + 独立资源”下载安装，不把测试资源嵌入 AST。需要 SDK ABI 4.3；当前发布配置仅包含 P4 480×480 和模拟器。

在 APPS 根目录运行：

```sh
python StoreDemo/tools/prepare.py
ASTER_WORKSPACE="$PWD/StoreDemo" python ../aster-sdk/tools/build.py StoreDemo --target riscv --aot
python -m StoreServer publish-project StoreDemo --target riscv
```

环境变量写法适用于 Linux/macOS；PowerShell 使用 `$env:ASTER_WORKSPACE=...`。工具链安装要求见 SDK 文档。

`prepare.py` 生成 `dist/resources/message.txt` 和 1 MiB 的 `pattern.bin`。应用通过 `/assets` 读取文字及图案文件首尾，成功后显示结果并输出 `STOREDEMO PASS` 日志。

模拟器版本使用独立输出目录，保持 P4 AOT 包：

```sh
ASTER_WORKSPACE="$PWD/StoreDemo/build/sim" python ../aster-sdk/tools/build.py StoreDemo --target riscv --no-aot
python -m StoreServer publish-project StoreDemo --target wasm
```

修改程序或资源后提高 `app.toml` 中的版本号，再重新发布；服务器禁止覆盖已经发布的版本。
