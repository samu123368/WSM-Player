#pragma once
#include <cmath>
#include <cstdint>

// GX_GetTexObjData returns a physical address, not a dereferenceable CPU
// pointer. Map valid MEM1/MEM2 ranges to the cached PPC alias before decoding.
inline uint32_t ThemeTextureCpuAddress(uint32_t physical, uint32_t bytes)
{
	if( !physical || !bytes ) return 0;
	const uint32_t end = physical < 0x01800000 ? 0x01800000
		: physical >= 0x10000000 && physical < 0x14000000 ? 0x14000000 : 0;
	if( !end || bytes > end - physical ) return 0;
	return physical | 0x80000000;
}

inline int ThemeInkLuma(int r, int g, int b)
{
	return (r * 299 + g * 587 + b * 114 + 500) / 1000;
}

// Material::ApplyTevStages' one-texture default is (1-T)*REG0 + T*REG1.
// A white IA8 mask can therefore draw black; the mask is NOT the backdrop.
inline int ThemeRampChannel(int texel, int low, int high)
{
	const int value = (low * (255 - texel) + high * texel + 127) / 255;
	return value < 0 ? 0 : value > 255 ? 255 : value;
}

// Preserve authored ink where readable. Texture-only themes often leave the
// stock gray label material unchanged after replacing a white button with black.
inline bool ThemeInkNeedsContrast(int r, int g, int b, int background)
{
	return background >= 0 && std::abs(ThemeInkLuma(r,g,b) - background) < 100;
}
