# License scope and third-party notices

Copyright (C) 2026 hello-myj and contributors.

Unless a file or component states otherwise, the original application code,
porting code, tools and documentation authored for this repository are licensed
under the GNU General Public License, version 3 or (at your option) any later
version (SPDX-License-Identifier: GPL-3.0-or-later). See LICENSE for the complete
license text.

This default does not replace any third-party copyright, license, attribution,
trademark or asset notice. Third-party components retain their original terms;
file-level notices take precedence for those files. Generated resources retain
the terms applicable to their source material. Inclusion in this repository does
not grant ownership of third-party names, characters, logos or other marks.

## OutRun

- Upstream: https://github.com/ZgzInfinity/OutRun
- Revision: 6882721c99bc79365f815f68d389bf5078129e68
- Upstream distribution includes GNU GPL version 3 in
  OutRun/vendor/upstream/LICENSE; preserve the upstream copyright and other
  notices, including those applicable to artwork and audio.
- Aster port code: GPL-3.0-or-later, as marked in its source files.
- Source and resource provenance: OutRun/vendor/UPSTREAM.md and the upstream
  README. This repository does not independently relicense upstream assets.

## Pixel Wheels

- Upstream: https://github.com/agateau/pixelwheels
- Revision: 0ed0376a6b1e4ba0f055b788fb84cb08d20cf8ea
- Author: Aurélien Gâteau and contributors.
- Original game code and Aster port: GPL-3.0-or-later.
- Original utility code: Apache-2.0, as identified by the upstream notices.
- Original and converted artwork/audio: CC-BY-SA-4.0, subject to any applicable
  file-specific notices supplied upstream.
- License texts: PixelWheels/vendor/upstream/LICENSE.*.
- Source and resource provenance: PixelWheels/vendor/UPSTREAM.md.

## TouchTest

The original Aster multitouch diagnostic application and its tools are licensed
under GPL-3.0-or-later. See TouchTest/LICENSE and the repository LICENSE.

## Other notices

Keep the third-party license files and individual source notices when copying
or redistributing these components. For dependencies, fonts and other files with
separate notices inside vendor directories, those supplied notices remain
applicable. The shared Aster SDK is installed separately and is not relicensed by
this repository.

## OpenOMF

OpenOMF upstream source is MIT; its icon is CC-BY-4.0. Original One Must Fall2097
game data retains its original terms and is fetched separately, not licensed by
this repository. Aster adapters and tools are GPL-3.0-or-later. See
OpenOMF/vendor/UPSTREAM.md and retained upstream license files for provenance.
The original-engine build also statically links wasi-libc and LLVM compiler
builtins, and uses declaration-only SDL2/ENet headers; preserve their applicable
component notices when distributing a build.


## GoldMiner

GoldMiner-Rebirth code is MIT, copyright Lazy_V. Original assets are described by upstream as sourced from the Internet and modified; the code licence does not establish asset redistribution rights. Aster C adapters and tools are GPL-3.0-or-later. See GoldMiner/vendor/UPSTREAM.md and vendor/upstream/LICENSE. WASI libc/compiler runtime dependency notices also apply to linked code.

GoldMiner small UI text uses DejaVu Sans. The font and its full licence notices are retained in GoldMiner/vendor/fonts/DejaVuSans.ttf and LICENSE-DejaVu.txt. Upstream Kurland titles remain unchanged in style.

## OpenTyrian

OpenTyrian engine code is GPL-2.0-or-later; offline OPL synthesis retains its
LGPL-2.1-or-later notices. Aster adapters/tools are GPL-3.0-or-later. Original
Tyrian 2.1 data and converted audio retain their freeware terms, are downloaded
separately, and are not relicensed by the repository GPL. See
OpenTyrian/vendor/UPSTREAM.md, upstream/COPYING and
upstream/doc/tyrian-freeware-license.txt. SDL headers, wasi-libc, LLVM builtins
and Binaryen retain their own licenses.

OpenTyrian Chinese UI uses a bitmap subset of Noto Sans CJK SC 2.004,
licensed under SIL Open Font License 1.1. Preserve
OpenTyrian/vendor/licenses/Noto-OFL.txt with this font data.

# PAL / 仙剑奇侠传

`PAL/sdlpal-aster` 保存 sdlpal-embedded 的固定源码快照（`795fc8ae9133b020855f4ef5d44abef23b5f0fce`）及 Aster 兼容补丁，保留原许可证和作者记录。`PAL/pal-resources` 为用户旧项目所用 DOS 数据镜像的固定快照（`b6945a2a55717a76c7290fded90ad435acf598de`）；游戏数据来源与引擎代码许可分别处理。字体为 Fusion Pixel Font，OFL-1.1 文件和固定版本压缩包保存在项目内。详细来源及许可边界见 `PAL/UPSTREAM.md`。
PAL 的 P4 原生 UI 字体由固定版本 Aster 18px cpfont 子集生成，基于 Droid Sans Fallback（Apache-2.0）与 DejaVu 拉丁字形（Bitstream Vera / DejaVu notices）。原始 bitmap 快照、摘要及许可保留在 `PAL/port/font-sources`。
