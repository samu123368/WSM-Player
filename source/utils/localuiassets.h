#pragma once
#include <gccore.h>
#include <vector>

// Read-only resource acquisition. Returned allocations belong to the caller.
// No downloads, NAND writes, SD cache, or firmware installation.
namespace LocalUiAssets {
 u8 *TitleFile(u64 title, const char *member, u32 *size);
 u8 *ForecastArchive(bool icon, u32 *size);
 u8 *AgreementLayout(u32 *size);
 bool AgreementSkin(std::vector<std::vector<u8> > &images);
}
