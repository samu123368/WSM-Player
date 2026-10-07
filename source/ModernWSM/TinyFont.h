#ifndef MODERN_WSM_TINY_FONT_H
#define MODERN_WSM_TINY_FONT_H

#include <gccore.h>
#include <string>

namespace ModernWSM
{

class TinyFont
{
public:
	static void Draw( float x, float y, const std::string &text, float scale,
					  const GXColor &color, size_t maxCharacters = 0 );
	static float Width( const std::string &text, float scale );

private:
	static const char *Glyph( char c );
	static void Prepare();
	static void Pixel( float x, float y, float size, const GXColor &color );
};

}

#endif
