#ifndef WSM_UI_FONT_FALLBACK_H
#define WSM_UI_FONT_FALLBACK_H
#include <gccore.h>
namespace UiFontFallback {
bool HasGlyph(u16 code);
float Advance(u16 code, float height);
void Draw(u16 code, float x, float y, float height, GXColor color,
          bool layoutCoordinates = false, float horizontalScale = 1.0f);
}
#endif
