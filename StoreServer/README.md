# 本地应用分发服务器

在 APPS 仓库根目录运行，使用 Python 3.11 或更新版本，无第三方依赖。服务端代码可用于 Windows、Linux、macOS；Windows 和 WSL/Linux 已通过 HTTP 接口测试，macOS 尚未实际验证。

```sh
python -m StoreServer publish-project StoreDemo --target riscv
python -m StoreServer serve --host 0.0.0.0 --port 9080
```

先按 [StoreDemo](../StoreDemo/README.md) 编译程序、生成资源。设备端服务地址填写电脑的局域网地址，例如 `http://192.168.110.154:9080`；`127.0.0.1` 在开发板上指向开发板自身。电脑和设备须互通，防火墙允许该端口。健康检查为 `/healthz`。

默认数据目录是 `StoreServer/data`，可在子命令前指定 `--data /path/to/data`。运行期间发布新版本后，目录接口会重新载入。退出服务按 Ctrl+C。

## 发布约定

每个应用工程保存 `app.toml`、源码和素材转换工具；生成的程序在 `dist/<target>`，资源在 `dist/resources`。工程内 `store.toml` 声明已验证的变体，示例见 [StoreDemo/store.toml](../StoreDemo/store.toml)。资源目录的相对路径会映射到应用的 `/assets`。存档写 `/data`。

也可直接发布：

```sh
python -m StoreServer publish StoreDemo/dist/riscv/aster.store.demo.ast --board osptek-p4-st7102 --screen 480x480 --resources StoreDemo/dist/resources
```

必须明确开发板白名单；相同 CPU 不代表屏幕、内存和外设都适配。 板 ID 应以设备上报为准，例如 lckfb 实机为 `aster.esp32s3.lckfb`，不是硬件配置文件的短名 `lckfb-s3` 或 CPU 目标 `xtensa`。`--board`、`--screen` 可重复指定。服务器读取 AST 中的 ID、版本、ABI、AOT 后端及能力要求，不凭文件名猜测。已有项目需完成资源路径迁移和目标板验证后再添加发布配置。

发布会复制程序与资源，生成 SHA-256 摘要定位的不可变快照。相同 ID/版本/目标/重叠开发板不能发布不同内容；修改程序或资源后应更新应用版本。最多 4096 个资源文件、总计 1 GiB；拒绝符号链接、路径穿越和跨平台不安全文件名。生成的数据不提交 Git。

## HTTP 接口

设备参数包含 `board`、`target`、打包整数 `abi`、能力位掩码 `features`、`width`、`height`。列表、发布详情、文件下载都会检查兼容性；未知板不会得到任意版本。

- `GET /v1/catalog?...&page=0`：兼容应用目录，单页最多 8 个，同一应用仅显示最新兼容版本。
- `GET /v1/releases/<摘要>?...&page=0`：程序及资源文件清单，含大小和 SHA-256，单页最多 8 个。
- `GET /v1/releases/<摘要>/files/<序号>?...`：文件内容，支持 HEAD、Range、If-Range、ETag。

设备商城复核元数据并流式校验文件，下载完整后请求固件安装。代码与资源一起写入非活动版本目录，成功后切换活动版本。失败保留原应用；用户数据不随更新删除。

HTTP 服务只读，没有远程上传或发布入口。当前用于可信局域网，SHA-256 用于完整性校验；硬件参数匹配也不等于设备身份认证。公网阶段应部署生产 HTTP 前端、HTTPS、可信签名和发布权限策略，并配置限流。Python 官方也明确说明 [http.server 不适合生产部署](https://docs.python.org/3/library/http.server.html)。

## 验证

```sh
python -m unittest StoreServer.test_server -v
```

覆盖硬件过滤、资源发布、不可变版本、热加载、Range/HEAD 和摘要校验。商城完整下载、安装及外部资源读取另见 [AppStore](../AppStore/README.md)。
