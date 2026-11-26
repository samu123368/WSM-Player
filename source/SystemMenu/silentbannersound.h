#pragma once
#include <cstring>
#include "../utils/sha1.h"

// Legacy homebrew-card "silence" contains a loud first DSP block followed
// by near-silence. Match only that exact placeholder, not quiet music or an
// intentional short sound. No audio/artwork is embedded in this fingerprint.
inline bool IsKnownSilentBannerSound(const unsigned char *data, unsigned size)
{
    if(!data || size != 18608 || std::memcmp(data,"BNS ",4)) return false;
    SHA1Context hash;
    SHA1Reset(&hash);
    SHA1Input(&hash,data,size);
    if(!SHA1Result(&hash)) return false;
    const unsigned expected[5]={0x08c3e715,0xe90651ec,0x970b17bf,0x1965d4c8,0xf4550d91};
    for(unsigned i=0;i<5;++i) if(hash.Message_Digest[i]!=expected[i]) return false;
    return true;
}
