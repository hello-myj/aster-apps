# 来源与版本

- 引擎： https://github.com/44670/sdlpal-embedded.git ，extreme 分支，基线 `795fc8ae9133b020855f4ef5d44abef23b5f0fce`。许可证保留在引擎目录。完整 Aster 入口、字体度量和资源兼容补丁保存为 patches/engine-aster.patch；自有适配代码位于 port/。
- DOS 数据： https://github.com/handoing/Legend-of-Sword-and-Fairy.git ，提交 `b6945a2a55717a76c7290fded90ad435acf598de`。来源为第三方镜像；游戏数据与引擎代码许可分别处理，不代表官方授权发行。
- 字体：引擎 third_party/fusion-pixel-font 中的固定版本 BDF ZIP 和许可；用于离线转换。
- P4 独立文字层：port/font-sources/aster-ui-18.cpfont 为 Aster 18px 字库的固定快照，基于 Droid Sans Fallback / DejaVu，摘要及 Apache-2.0、Bitstream Vera 许可见同目录说明和 notices。游戏原始数据包不因 P4 字体变化而修改。
- 音频：由原始游戏资源离线生成 MP3/WAV，脚本位于 port，依赖版本由 package.json / pnpm-lock.yaml 管理。

本次迁移将上述固定提交作为普通源码快照存放在项目内，保留许可证。引擎快照保留原工作区的五处修改，随后加入独立 UI 层挂钩；相对固定上游的七个文件完整差异已保存为 patches/engine-aster.patch。上游符号链接在 Windows 可解析时物化为文件，未将旧 Git 元数据、编译产物或存档作为源码。旧资源与字体保持一致，没有拼接另一套 DOS 数据。
