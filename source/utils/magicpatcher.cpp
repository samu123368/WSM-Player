// SPDX-License-Identifier: GPL-2.0-only
// Original MagicPatches supplied by the GPL-labelled Wii System Menu Player
// project by Dimok and giantpune, first commit db100b636d5aa6526e896195010dafab5093f149.
// Altered version: readable C++ source reconstruction by WSM Player contributors,
// 2026. This is not the lost original source. Tables, validation order, return
// codes, cached/uncached aliases and memory-protection restoration are preserved.
// See LICENSES/PROVENANCE.md. GPL v2, without warranty; see COPYING.txt.
#include "magicpatcher.h"

namespace WsmMagicPatch
{
namespace
{
const uint32_t Protection = 0xcd8b420a;
struct Patch { uint32_t address, original, changed; };
const Patch FsEarly = {0x93a11304, 0x428bd001, 0x428be001};
const Patch FsLate  = {0x93a112f0, 0x428bd001, 0x428be001};
const Patch DiA = {0x939b66e4, 0x00014000, 0x7ed40000};
const Patch DiB = {0x939b65f4, 0x00014000, 0x7ed40000};
const Patch DiC = {0x939b6640, 0x00014000, 0x7ed40000};
const Patch DiD = {0x939b636c, 0x00014000, 0x7ed40000};
const Patch EsA[17] = {
 {0x93a75624,0xe0002007,0xe0002000}, {0x939f0e74,0xd123684b,0x2803684b},
 {0x939f0eec,0xd121684b,0x2803684b}, {0x939f52e4,0xd0102900,0xe0102900},
 {0x939f5324,0xd4014c43,0xe0014c43}, {0x939f5348,0xd4014c3a,0xe0014c3a},
 {0x939f5450,0xd0032900,0x46c02900}, {0x939f5454,0xdb01290f,0x46c0290f},
 {0x939f5458,0xdd01480b,0xe001480b}, {0x939f54a0,0xd0032900,0x46c02900},
 {0x939f54a4,0xdb01290f,0x46c0290f}, {0x939f54a8,0xdd01480b,0xe001480d},
 {0x939f57bc,0x42a3d12a,0x42a346c0}, {0x939f2cb8,0xd2014e56,0xe0014e56},
 {0x939f8560,0x4299d800,0x4299e000}, {0,0,0}, {0,0,0}
};
const Patch EsB[17] = {
 {0x93a75624,0xe0002007,0xe0002000}, {0x939f0e74,0xd123684b,0x2803684b},
 {0x939f0eec,0xd121684b,0x2803684b}, {0x939f5290,0xd0102900,0xe0102900},
 {0x939f52d0,0xd4014c43,0xe0014c43}, {0x939f52f4,0xd4014c3a,0xe0014c3a},
 {0x939f53fc,0xd0032900,0x46c02900}, {0x939f5400,0xdb01290f,0x46c0290f},
 {0x939f5404,0xdd01480b,0xe001480b}, {0x939f5498,0xd0032900,0x46c02900},
 {0x939f549c,0xdb01290f,0x46c0290f}, {0x939f54a0,0xdd01480d,0xe001480d},
 {0x939f5768,0x42a3d12a,0x42a3d12a}, {0x939f2c74,0xd2014e56,0xe0014e56},
 {0x939f8498,0x4299d800,0x4299e000}, {0x939f650c,0xd0004803,0xe0004803}, {0,0,0}
};
const Patch EsC[17] = {
 {0x93a754f8,0xe0002007,0xe0002000}, {0x93a756a4,0xe0002007,0xe0002000},
 {0x939f0da4,0xd123684b,0x2803684b}, {0x939f0e1c,0xd121684b,0x2803684b},
 {0x939f4d60,0xd0102900,0xe0102900}, {0x939f4da0,0xd4014c43,0xe0014c43},
 {0x939f4dc4,0xd4014c3a,0xe0014c3a}, {0x939f4ecc,0xd0032900,0x46c02900},
 {0x939f4ed0,0xdb01290f,0x46c0290f}, {0x939f4ed4,0xdd01480b,0xe001480b},
 {0x939f4f68,0xd0032900,0x46c02900}, {0x939f4f6c,0xdb01290f,0x46c0290f},
 {0x939f4f70,0xdd01480d,0xe001480d}, {0x939f5238,0x42a3d12a,0x42a346c0},
 {0x939f2800,0xd2014e56,0xe0014e56}, {0x939f7b30,0x4299d800,0x4299e000},
 {0x939f5fd0,0xd0004803,0xe0004803}
};
const Patch EsD[17] = {
 {0x93a752e4,0xe0002007,0xe0002000}, {0x939f0d44,0x2803d123,0x28032803},
 {0x939f0dbc,0x2803d121,0x28032803}, {0x939f4df4,0x429dd003,0x429de003},
 {0x939f4e18,0x07dad401,0x07dae001}, {0x939f4e3c,0x07d8d401,0x07d8e001},
 {0x939f4f38,0xd0032900,0x46c02900}, {0x939f4f3c,0xdb01290f,0x46c0290f},
 {0x939f4f40,0xdd01480b,0xe001480b}, {0x939f4f88,0xd0032900,0x46c02900},
 {0x939f4f8c,0xdb01290f,0x46c0290f}, {0x939f4f90,0xdd01480b,0xe001480b},
 {0x939f52a4,0x42a3d12a,0x42a346c0}, {0x939f2818,0xd2014e56,0xe0014e56},
 {0x939f7ba8,0x4299d800,0x4299e000}, {0,0,0}, {0,0,0}
};
void Write(const Io &io, const Patch &patch, bool enable)
{
    if(!patch.address) return;
    io.write32(io.context, patch.address | 0xc0000000, enable ? patch.changed : patch.original);
    io.flush(io.context, patch.address, 4);
}
bool Matches(const Io &io, const Patch &patch, bool enable)
{
    return !patch.address || io.read32(io.context, patch.address | 0xc0000000) ==
        (enable ? patch.original : patch.changed);
}
}

int Apply(int enableValue, const Io &io)
{
    const bool enable = enableValue != 0;
    const uint32_t protection = io.read32(io.context, Protection);
    if(!protection) return -202;
    io.write16(io.context, Protection, 0);
    io.flush(io.context, Protection & ~0x40000000u, 2);
    int result = -199;
    const Patch *di = 0, *fs = 0, *es = 0;
    switch(io.read32(io.context, 0x939b0040)) {
        case 0x20207c2c: di = &DiD; break;
        case 0x20207db8: di = &DiB; break;
        case 0x20207ea8: di = &DiA; break;
        case 0x20207f40: di = &DiC; break;
    }
    if(di) {
        result = -200;
        switch(io.read32(io.context, 0x93a10044)) {
            case 0x20005d89: fs = &FsLate; break;
            case 0x20006009: fs = &FsEarly; break;
        }
        if(fs) {
            result = -198;
            switch(io.read32(io.context, 0x939f0044)) {
                case 0x2010142d: es = EsD; break;
                case 0x201014d5: es = EsC; break;
                case 0x201015a5: es = EsB; break;
                case 0x201015e9: es = EsA; break;
            }
            if(es) {
                result = -203;
                if(Matches(io, *fs, enable)) {
                    result = -201;
                    unsigned i = 0;
                    for(; i < 17 && Matches(io, es[i], enable); ++i) {}
                    if(i == 17) {
                        result = -202;
                        if(Matches(io, *di, enable)) {
                            // Validate ALL expected words before any IOS write.
                            Write(io, *fs, enable);
                            for(i = 0; i < 17; ++i) Write(io, es[i], enable);
                            Write(io, *di, enable);
                            result = 1;
                        }
                    }
                }
            }
        }
    }
    // Restore the exact incoming protection word on every post-disable exit.
    io.write32(io.context, Protection, protection);
    io.flush(io.context, Protection & ~0x40000000u, 4);
    return result;
}
}

#ifndef WSM_PATCH_HOST_TEST
namespace
{
uint32_t Read32(void *, uint32_t address)
{
    const uint32_t result = *(volatile uint32_t *)(uintptr_t)address;
    asm volatile("sync" ::: "memory");
    return result;
}
void Write32(void *, uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
    asm volatile("eieio" ::: "memory");
}
void Write16(void *, uint32_t address, uint16_t value)
{
    *(volatile uint16_t *)(uintptr_t)address = value;
    asm volatile("eieio" ::: "memory");
}
void Flush(void *, uint32_t address, uint32_t length)
{
    const uint32_t end = (address + length + 31) & ~31u;
    for(uint32_t line = address & ~31u; line < end; line += 32)
        asm volatile("dcbst 0,%0" : : "r"(line) : "memory");
    asm volatile("sync; isync" ::: "memory");
}
}
extern "C" int MagicPatches(int enable)
{
    const WsmMagicPatch::Io io = {0, Read32, Write32, Write16, Flush};
    return WsmMagicPatch::Apply(enable, io);
}
#endif
