# WSM Player

Wii homebrew by Dimok, giantpune and samu. This is an **altered version** of
[Wii System Menu Player](https://github.com/giantpune/wii-system-menu-player),
not Nintendo's System Menu and not an official Nintendo product.

WSM Player renders Wii resources from your own console's NAND. It includes
channel customization, themes, controller and mouse support, developer camera
tools and signed application updates. Required Nintendo artwork, fonts,
channel dumps and settings image packs are **not distributed** here.
Do not share your NAND.

This repository starts with a reviewed source-only snapshot; no private backup
history, personal configuration, logs or signing secrets are included.
See [REVIEW.md](REVIEW.md), [provenance](LICENSES/PROVENANCE.md) and the SHA-256
inventory. PREPARATION.json records copy-time checks, not hardware certification.

## Build

Install devkitPro's devkitPPC and Wii development libraries. The Makefile links
libogc, libfat, ASND, wiikeyboard, wiiuse, bte and the Wii portlibs providing
gd, JPEG, PNG and zlib. These SDK dependencies are external, not bundled.
Set `DEVKITPRO` and `DEVKITPPC` as required by devkitPro, then run:

```sh
make -j4
```

The output is `boot.dol`. A complete clean build of this snapshot was checked
with devkitPPC r46.1 and libogc 2.10.0 on Windows/MSYS2. Platform setup and
hardware behavior can differ; a build test is not a real-Wii test.

The old object-only `MagicPatches` dependency is replaced with readable,
behavior-preserving C++ in `source/utils/magicpatcher.cpp`. This is a marked
reconstruction, not the recovered original source. It was compared with the
legacy PowerPC implementation on 2,912 synthetic cases; see REVIEW.md.

## Download and updates

SD-ready builds and the signed update feed are maintained separately in
[WSM-Player-Updates](https://github.com/samu123368/WSM-Player-Updates).
Application updates replace the DOL only; they do not install IOS or WADs.
The public verification key is included; the private signing seed is not.
A fork should use its own signing key and update feed.

## License

The combined source distribution is provided under **GNU GPL version 2**;
see [COPYING.txt](COPYING.txt). Individual files retain their original
copyright and compatible permissive notices, which must also be preserved.
The original renderer has its own permissive notice; this does not relicense
the GPL portions. Monocypher, TinyXML, sigslot, RSA MD5, SHA-1, Apple AIFF,
ASH reference and external libogc notices are recorded in source/`LICENSES/`.
The Noto-derived fallback font is separately covered by the SIL OFL.

Project-original sample weather/news records and the generic homebrew card/icon
are included. There is no warranty. Review file-specific notices when
redistributing or modifying components.
