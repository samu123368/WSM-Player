#pragma once
#include <stdint.h>

struct ScreenshotHold
{
    uint64_t start = 0;
    bool held = false, fired = false;
    bool Update(bool chord, uint64_t now)
    {
        if(!chord) { held = fired = false; return false; }
        if(!held) { start = now; held = true; }
        if(fired || now - start < 5000) return false;
        fired = true;
        return true;
    }
};
