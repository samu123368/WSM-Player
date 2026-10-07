#pragma once
#include <algorithm>

// Resize only the center strip. End caps retain their authored aspect ratio.
struct ThemeButtonGeometry { float scale, middle; };
inline ThemeButtonGeometry ThemeButtonSize(float width, float height)
{
	const float scale = std::min(height / 80.0f, width / 65.0f);
	return {scale, std::max(1.0f,width / scale - 64.0f)};
}
