// SPDX-License-Identifier: GPL-2.0-only
// Source reconstruction for this altered Wii System Menu Player fork (2026).
// The original GPL-labelled project supplied MagicPatches as an object only.
#ifndef WSM_MAGICPATCHER_H
#define WSM_MAGICPATCHER_H
#include <stdint.h>

namespace WsmMagicPatch
{
// Keep the decision/validation logic independent of real MMIO for regression
// testing. No allocation, IOS reload, USB call, NAND write or network access.
struct Io
{
    void *context;
    uint32_t (*read32)(void *, uint32_t);
    void (*write32)(void *, uint32_t, uint32_t);
    void (*write16)(void *, uint32_t, uint16_t);
    void (*flush)(void *, uint32_t, uint32_t);
};
int Apply(int enable, const Io &io);
}
extern "C" int MagicPatches(int enable);
#endif
