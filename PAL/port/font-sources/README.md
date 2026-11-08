Fusion Pixel 12px monospaced BDF, release 2026.07.20.
Source: https://github.com/TakWolf/fusion-pixel-font/releases/tag/2026.07.20
Asset: fusion-pixel-font-12px-monospaced-bdf-v2026.07.20.zip
SHA256: aea98326638e138de8583f0ae87db9eb722b9f44519361a32e0ee9577b3c6586
Member: fusion-pixel-12px-monospaced-zh_hans.bdf
License: SIL Open Font License 1.1 (OFL.txt).
The converter validates this exact archive and subsets the converted game corpus.

P4 native UI: aster-ui-18.cpfont, copied from Aster firmware's ui_18.cpfont.
SHA256: fef8631bd559c5b7728966d0da6e531ea10043793bd463cc32aa06017f1317f4
The v4 18px face is based on Droid Sans Fallback (Apache-2.0) and DejaVu Latin
glyphs (Bitstream Vera / DejaVu notices). See ASTER-UI-NOTICE and
DroidSansFallbackFull.LICENSE. tools/prepare_native_font.py validates this
exact input and subsets the game FONT corpus, without consulting host fonts.
Chinese ink retains the 18px grid; wide Latin ink is area-sampled into PAL's
nine-pixel half-width cell. U+2027 uses the face's middle dot; U+2500 is an
explicit centered horizontal stroke. Output ui18.bin contains 4-bit coverage.
