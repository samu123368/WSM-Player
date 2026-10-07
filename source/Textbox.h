/*
Copyright (c) 2010 - Wii Banner Player Project
Copyright (c) 2012 - Dimok and giantpune

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#ifndef WII_BNR_TEXTBOX_H_
#define WII_BNR_TEXTBOX_H_

#include "Pane.h"
#include "utils/char16.h"

class WiiFont;

class Textbox : public Pane
{
public:
	typedef Pane Base;

	Textbox() : header(NULL), text(NULL), frameWidth(0.f), frameHeight(0.f), uniformTextFit(false), accurateGlyphAdvance(false), textYOffset(0.f)
	{ }

	static const u32 MAGIC = MAKE_FOURCC('t', 'x', 't', '1');

	void Load(Pane::Header *file);
	static Textbox *CreateLabel(const char16 *text, u16 fontIndex,
		float width, float height, float fontSize, GXColor color);
	bool CopyMaterialFrom( const Textbox &source );
	float MeasureTextWidth(const Resources &resources);

	u8 GetAlignHor() const { return (header->text_origin % 3); }
	u8 GetAlignVer() const { return (header->text_origin / 3); }
	u8 GetLineAlignHor() const
	{
		// BRLYT stores line alignment as 1=left, 2=center, 3=right.  Zero
		// means unspecified, in which case the string origin is the fallback.
		return (header->line_alignment >= 1 && header->line_alignment <= 3)
			? header->line_alignment - 1 : GetAlignHor();
	}
	int GetMaterialIndex(){ return header ? header->material_index : -1; }

	void SetText( const char16 *newText );
	const char16 *GetText() const { return text; }
	const short *GetInspectionText() const override { return text; }
	GXColor GetTextColor() const { return header ? header->color[0] : (GXColor){51,51,51,255}; }
	void SetTextColor(GXColor color) { if( header ) header->color[0] = header->color[1] = color; }
	void CenterText() { if( header ) { header->text_origin = 4; header->line_alignment = 2; } }
	void SetCharacterSpacing( float spacing )
		{ if( header ) { header->space_char = spacing; ClearLinewidths(); } }
	// Opt-in for translated compact labels; other retail layouts keep their
	// authored fitting behavior. Shrink both glyph axes, never squash letters.
	void SetUniformTextFit( bool enabled ) { if( uniformTextFit != enabled ) ClearLinewidths(); uniformTextFit = enabled; }
	// CWDH bearing places the ink within a cell; it is not an extra pen advance.
	// Opt in for Internet titles without disturbing previously tuned layouts.
	void SetAccurateGlyphAdvance( bool enabled ) { if( accurateGlyphAdvance != enabled ) ClearLinewidths(); accurateGlyphAdvance = enabled; }
	void SetTextYOffset( float offset ) { textYOffset = offset; }
	void ClearLinewidths()
	{
		lineWidths.clear();
		lineLeadingWidths.clear();
		lineTrailingWidths.clear();
	}
	void SetFontSize( float width, float height )
	{
		if( header )
		{
			header->font_width = width;
			header->font_height = height;
			ClearLinewidths();
		}
	}

protected:
	void ProcessHermiteKey(const KeyType& type, float value);
	void ProcessStepKey(const KeyType& type, StepKeyHandler::KeyData data);

private:
	void Draw(const Resources& resources, u8 render_alpha, const float ws_scale, Mtx &view) const;
	void SetupGX(const Resources& resources) const;
	void SetTextWidth(WiiFont *font, bool fitToPane);
	float WhitespaceCenterCorrection( u32 line, float horizontalFit ) const;

	struct Header
	{
		u16			text_buf_bytes;
		u16			text_str_bytes;
		u16			material_index;
		u16			font_index;
		u8			text_origin;
		u8			line_alignment;
		u8			pad2[2];
		u32			text_str_offset;
		GXColor		color[2];
		float		font_width;
		float		font_height;
		float		space_char;
		float		space_line;
	} __attribute__((packed));

	Textbox::Header *header;
	std::vector<u8> ownedLayout;
	const char16 *text;
	float frameWidth;
	float frameHeight;
	bool uniformTextFit;
	bool accurateGlyphAdvance;
	float textYOffset;
	std::vector<float> lineWidths;
	std::vector<float> lineLeadingWidths;
	std::vector<float> lineTrailingWidths;
};

#endif
