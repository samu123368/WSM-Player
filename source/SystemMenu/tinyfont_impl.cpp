#include "../ModernWSM/TinyFont.h"

#include <cctype>

#include "video.h"

namespace ModernWSM
{

const char *TinyFont::Glyph( char c )
{
	c = (char)toupper( (unsigned char)c );
	switch( c )
	{
	case 'A': return ".#." "#.#" "###" "#.#" "#.#";
	case 'B': return "##." "#.#" "##." "#.#" "##.";
	case 'C': return ".##" "#.." "#.." "#.." ".##";
	case 'D': return "##." "#.#" "#.#" "#.#" "##.";
	case 'E': return "###" "#.." "##." "#.." "###";
	case 'F': return "###" "#.." "##." "#.." "#..";
	case 'G': return ".##" "#.." "#.#" "#.#" ".##";
	case 'H': return "#.#" "#.#" "###" "#.#" "#.#";
	case 'I': return "###" ".#." ".#." ".#." "###";
	case 'J': return "..#" "..#" "..#" "#.#" ".#.";
	case 'K': return "#.#" "#.#" "##." "#.#" "#.#";
	case 'L': return "#.." "#.." "#.." "#.." "###";
	case 'M': return "#.#" "###" "###" "#.#" "#.#";
	case 'N': return "#.#" "###" "###" "###" "#.#";
	case 'O': return ".#." "#.#" "#.#" "#.#" ".#.";
	case 'P': return "##." "#.#" "##." "#.." "#..";
	case 'Q': return ".#." "#.#" "#.#" "###" "..#";
	case 'R': return "##." "#.#" "##." "#.#" "#.#";
	case 'S': return ".##" "#.." ".#." "..#" "##.";
	case 'T': return "###" ".#." ".#." ".#." ".#.";
	case 'U': return "#.#" "#.#" "#.#" "#.#" ".#.";
	case 'V': return "#.#" "#.#" "#.#" "#.#" ".#.";
	case 'W': return "#.#" "#.#" "###" "###" "#.#";
	case 'X': return "#.#" "#.#" ".#." "#.#" "#.#";
	case 'Y': return "#.#" "#.#" ".#." ".#." ".#.";
	case 'Z': return "###" "..#" ".#." "#.." "###";
	case '0': return ".#." "#.#" "###" "#.#" ".#.";
	case '1': return ".#." "##." ".#." ".#." "###";
	case '2': return "##." "..#" ".#." "#.." "###";
	case '3': return "##." "..#" ".#." "..#" "##.";
	case '4': return "#.#" "#.#" "###" "..#" "..#";
	case '5': return "###" "#.." "##." "..#" "##.";
	case '6': return ".##" "#.." "##." "#.#" ".#.";
	case '7': return "###" "..#" ".#." ".#." ".#.";
	case '8': return ".#." "#.#" ".#." "#.#" ".#.";
	case '9': return ".#." "#.#" ".##" "..#" "##.";
	case '-': return "..." "..." "###" "..." "...";
	case '_': return "..." "..." "..." "..." "###";
	case '/': return "..#" "..#" ".#." "#.." "#..";
	case '.': return "..." "..." "..." "..." ".#.";
	case ':': return "..." ".#." "..." ".#." "...";
	case '[': return ".##" ".#." ".#." ".#." ".##";
	case ']': return "##." ".#." ".#." ".#." "##.";
	case '+': return "..." ".#." "###" ".#." "...";
	case '%': return "#.#" "..#" ".#." "#.." "#.#";
	case '?': return "##." "..#" ".#." "..." ".#.";
	case '!': return ".#." ".#." ".#." "..." ".#.";
	case ' ': return "..." "..." "..." "..." "...";
	default:  return "##." "..#" ".#." "..." ".#.";
	}
}

void TinyFont::Prepare()
{
	LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
	GX_SetNumChans( 1 );
	GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
		GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
	GX_SetNumTexGens( 0 );
	GX_SetNumTevStages( 1 );
	GX_SetNumIndStages( 0 );
	GX_SetTevOp( GX_TEVSTAGE0, GX_PASSCLR );
	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0 );
	GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
	GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET );
	GX_ClearVtxDesc();
	GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
}

void TinyFont::Pixel( float x, float y, float size, const GXColor &color )
{
	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	GX_Position3f32( x, y, 0.0f );
	GX_Color4u8( color.r, color.g, color.b, color.a );
	GX_Position3f32( x + size, y, 0.0f );
	GX_Color4u8( color.r, color.g, color.b, color.a );
	GX_Position3f32( x + size, y + size, 0.0f );
	GX_Color4u8( color.r, color.g, color.b, color.a );
	GX_Position3f32( x, y + size, 0.0f );
	GX_Color4u8( color.r, color.g, color.b, color.a );
	GX_End();
}

void TinyFont::Draw( float x, float y, const std::string &text, float scale,
					 const GXColor &color, size_t maxCharacters )
{
	Prepare();
	const size_t count = maxCharacters && text.size() > maxCharacters
		? maxCharacters : text.size();
	for( size_t i = 0; i < count; ++i )
	{
		const char *glyph = Glyph( text[ i ] );
		for( int row = 0; row < 5; ++row )
		{
			for( int column = 0; column < 3; ++column )
			{
				if( glyph[ row * 3 + column ] == '#' )
					Pixel( x + ( i * 4 + column ) * scale, y + row * scale, scale, color );
			}
		}
	}
}

float TinyFont::Width( const std::string &text, float scale )
{
	return text.empty() ? 0.0f : ( text.size() * 4 - 1 ) * scale;
}

}
