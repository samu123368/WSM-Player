# Component provenance and notices

WSM Player is an altered version of Dimok and giantpune's Wii System Menu
Player, with fork changes by samu and contributors. The combined distribution
uses GPL v2; per-file compatible permissive notices and asset licenses remain.
Original headers are not replaced with a blanket permissive license.

| Component | Primary origin / notice |
| --- | --- |
| Original project and renderer | [giantpune/wii-system-menu-player](https://github.com/giantpune/wii-system-menu-player/tree/9b15635763e9f0929f905e5d3a50663fcc46e336); `wiibrew_page.txt` labels the project GPL; `Original-project-license.txt`, `Renderer.txt` and original headers |
| GPL utilities | `source/utils/char16.cpp`, `lz77.c` and `lz77.h`; original GPL v2 notices; root `COPYING.txt` |
| Readable MagicPatches | Altered reconstruction of the original GPL-labelled project's object from first commit `db100b636d5aa6526e896195010dafab5093f149`; reference Git blob `ad7af87958f5c668c8157e9785488602871101af`. No object included. Behavioral checks and limitations in `REVIEW.md`. |
| SHA-1 | [Paul E. Jones's author download](https://www.packetizer.com/security/hash/sha1/); `sha1-c.zip` author-posted SHA-1 `43a368a64feef8339ccf389a5ce9f1b4c5d5637a`; core function bodies matched. `SHA1-FPL.txt` supplies the grant. WSM's wrapper is altered. |
| Apple IEEE AIFF conversion | [Matching SoX source](https://github.com/chirlu/sox/blob/42b3557e13e0fe01a83465b672d89faddbe65f49/src/aiff.c); Apple 1988–1991, Malcolm Slaney and Ken Turkowski. `Apple-IEEE.txt` preserves permission and warranty disclaimer. |
| ASH reference | [NinjaCheetah/rustwii](https://github.com/NinjaCheetah/rustwii/tree/e55edc10fd9cab916fa99964839014e51934988e), formerly rustii. First ASH commit pinned as an attribution reference, not proof of the exact revision consulted. `Rustwii.txt` is its MIT notice. This fork's C++ is a bounds-checked adaptation. |
| Monocypher | `source/third_party/monocypher/LICENCE.md` and headers: BSD-2-Clause or CC0-1.0; BSD notice retained |
| TinyXML / sigslot | Original permissive notices in `source/tinyxml/` and `source/utils/sigslot.h` |
| RSA MD5 | Attribution and redistribution terms retained in `source/utils/md5.cpp` / its header |
| Wii USB / SDK | Tantric/libogc notice and alteration marker in `wsm_libogc_usbmouse.c`; external SDK notice in `libogc.txt`. SDK libraries are not included. |
| Fallback font | Noto-derived atlas; `OFL-Noto.txt` and `OFL-Noto-CJK.txt`. No Nintendo font bundled. |
| Samples/card/icon | Fork-authored synthetic news/weather and generic homebrew assets; no real news article, Nintendo artwork or personal photo supplied |

Runtime Nintendo resources come from the user's own NAND. The legacy header's
non-code BMG transcript comment is excluded from the public snapshot.
Upstream links document provenance, not bundled third-party binaries.
