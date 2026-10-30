#pragma once

// JPEG viewers assume square pixels; the Wii framebuffer is anamorphic.
// Round height up minimally to make the selected display ratio exact.
inline void ScreenshotDimensions(int height, bool widescreen, int &width, int &outHeight)
{
    const int divisor = widescreen ? 9 : 3;
    outHeight = ((height + divisor - 1) / divisor) * divisor;
    width = outHeight / divisor * (widescreen ? 16 : 4);
}
