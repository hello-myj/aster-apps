# Upstream and resource provenance

- Engine: https://github.com/omf2097/openomf
- Revision: da5f5ad4b9df63300ae6e8e6ad446924dc11b668
- Engine license: MIT; original text retained in upstream/LICENSE.
- Original One Must Fall 2097 by Diversions Entertainment / Epic MegaGames.
- Data archive: https://www.omf2097.com/pub/files/omf/openomf-assets.zip
  (the upstream BUILD.md supplies this link). Downloaded game data retains its
  original terms and is excluded from Git at vendor/game-data/.
- Launcher icon: original OpenOMF icon, CC-BY-4.0, notice in
  upstream/resources/icons/LICENSE.

The Aster runtime/converter are GPL-3.0-or-later, with format and behaviour
adaptations attributed to OpenOMF's MIT sources. The current default target compiles the upstream offline gameplay, AI, menus,
animation scripts and tournament code. Aster platform adapters replace desktop
services; network, external mods/PNG, desktop controller services and some audio
features remain unsupported. It is not a claim of full desktop feature parity. The original
copyright and other notices in the upstream files remain applicable.

All project-specific source, raw data, generated SD resources, tools and docs
are kept inside this OpenOMF folder. Original upstream Git metadata is backed
up under APPS/.git/vendor-backups/openomf-upstream.git, outside tracked content.

## Original-engine build dependencies

prepare-engine-deps.py pins URLs and SHA256 for wasi-sdk24 libc/sysroot and
compiler builtins, SDL2 2.32.10 and ENet1.3.18. SDL/ENet are declaration-only;
their implementations are not linked. Their license files are extracted alongside
headers in ignored artifacts/engine-deps. WASI libc is statically linked with its
Host-call member removed; port/wasi_adapter.c supplies only documented Aster I/O.
Upstream runtime sources and applicable notices: https://github.com/WebAssembly/wasi-libc
and https://github.com/llvm/llvm-project/tree/main/compiler-rt. Preserve component
notices when redistributing the compiled runtime.

Vanilla resources come from the official OpenOMF0.8.6 Windows package:
https://github.com/omf2097/openomf/releases/download/0.8.6/openomf_0.8.6_windows.zip
SHA256: 16e737154b61683d9674de978f48f13e493c6b590ab7237269490d31b6a2fb13.
Original PSM tracks are converted in full to128kbps stereo44100Hz MP3 without
changing their original ownership or licensing.
