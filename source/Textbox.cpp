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
#include "Textbox.h"
#include "Layout.h"
#include "video.h"
#include "SystemMenu/uifontfallback.h"

Textbox *Textbox::CreateLabel(const char16 *value, u16 fontIndex,
	float width, float height, float fontSize, GXColor color)
{
	Textbox *box = new Textbox;
	box->ownedLayout.resize(sizeof(Pane::Header)+sizeof(Textbox::Header)+2,0);
	Pane::Header *pane=reinterpret_cast<Pane::Header*>(&box->ownedLayout[0]);
	pane->magic=MAGIC;pane->size_section=box->ownedLayout.size();
	pane->flags=3;pane->origin=4;pane->alpha=255;
	memcpy(pane->name,"WSM_LocalTitle",14);
	pane->scale.x=pane->scale.y=1.f;pane->width=width;pane->height=height;
	box->Load(pane);
	box->header->material_index=0xffff;box->header->font_index=fontIndex;
	box->header->color[0]=box->header->color[1]=color;
	box->CenterText();box->SetFontSize(fontSize,fontSize);
	box->SetUniformTextFit(true);box->SetText(value);
	return box;
}

void Textbox::Load(Pane::Header *file)
{
	Pane::Load(file);
	header = (Textbox::Header *) (file + 1);
	text = (const char16 *) (header + 1);
}

float Textbox::MeasureTextWidth(const Resources &resources)
{
	if(!text || header->font_index>=resources.fonts.size()) return 0.f;
	WiiFont *font=resources.fonts[header->font_index];
	if(!font || !font->IsLoaded()) return 0.f;
	SetTextWidth(font,resources.fitTextboxToPane || uniformTextFit);
	return frameWidth;
}

bool Textbox::CopyMaterialFrom( const Textbox &source )
{
	if( !header || !source.header )
		return false;
	header->material_index = source.header->material_index;
	return true;
}

void Textbox::SetTextWidth(WiiFont *font, bool fitToPane)
{
	ClearLinewidths();
	frameWidth = 0.f;
	frameHeight = fitToPane ? header->font_height : header->font_width;
	float currentLine = 0.f;
	float leadingWidth = 0.f;
	float trailingWidth = 0.f;
	bool lineHasGlyph = false;
	bool lineHasVisibleGlyph = false;
	if(!font->CharacterWidth() || !font->CharacterHeight())
		return;
	const float scale = fitToPane
		? header->font_width / (float)font->CharacterWidth()
		: header->font_width / (float)font->CharacterHeight();

	for(const char16 *txtString = text; *txtString != 0; txtString++)
	{
		// Internet title mode: authored/localized hyphen compounds must not
		// acquire a whitespace glyph after '-'. Keep actual line breaks intact.
		if( accurateGlyphAdvance && (*txtString == ' ' || *txtString == '\t') )
		{
			const char16 *previous = txtString;
			while( previous > text && (previous[-1] == ' ' || previous[-1] == '\t') ) --previous;
			if( previous > text && previous[-1] == '-' ) continue;
		}
		if(*txtString == '\n')
		{
			if(fitToPane)
			{
				if(lineHasGlyph)
				{
					currentLine -= header->space_char;
					if( trailingWidth > 0.f ) trailingWidth -= header->space_char;
					if( !lineHasVisibleGlyph && leadingWidth > 0.f )
						leadingWidth -= header->space_char;
				}
			}
			else
			{
				currentLine *= scale;
				leadingWidth *= scale;
				trailingWidth *= scale;
			}
			lineWidths.push_back(currentLine);
			lineLeadingWidths.push_back(leadingWidth);
			lineTrailingWidths.push_back(trailingWidth);
			frameWidth = MAX(frameWidth, currentLine);
			frameHeight += (fitToPane ? header->font_height
				: header->font_width) + header->space_line;
			currentLine = 0.f;
			leadingWidth = 0.f;
			trailingWidth = 0.f;
			lineHasGlyph = false;
			lineHasVisibleGlyph = false;
			continue;
		}

		if( font->isSystemFont && !font->HasGlyph(*txtString)
			&& UiFontFallback::HasGlyph(*txtString) )
		{
			currentLine += UiFontFallback::Advance(*txtString, font->CharacterHeight())
				* (fitToPane ? scale : 1.0f) + (fitToPane ? header->space_char : 0.0f);
			lineHasGlyph = lineHasVisibleGlyph = true;
			trailingWidth = 0.0f;
			continue;
		}
		const WiiFont::CharInfo *charInfo = font->GetCharInfo(*txtString);
		if(!charInfo)
			continue;

		float advance = 0.f;
		if(charInfo->unk && !accurateGlyphAdvance)
			advance += (fitToPane ? scale : 1.0f)
				* (float)charInfo->advanceKerning;
		advance += (fitToPane ? scale : 1.0f)
			* (float)charInfo->advanceGlyphX;
		if(fitToPane) advance += header->space_char;
		currentLine += advance;

		const bool whitespace = *txtString == ' ' || *txtString == '\t';
		if( whitespace )
		{
			if( !lineHasVisibleGlyph ) leadingWidth += advance;
			trailingWidth += advance;
		}
		else
		{
			lineHasVisibleGlyph = true;
			trailingWidth = 0.f;
		}
		lineHasGlyph = true;
	}

	if(fitToPane)
	{
		if(lineHasGlyph)
		{
			currentLine -= header->space_char;
			if( trailingWidth > 0.f ) trailingWidth -= header->space_char;
			if( !lineHasVisibleGlyph && leadingWidth > 0.f )
				leadingWidth -= header->space_char;
		}
	}
	else
	{
		currentLine *= scale;
		leadingWidth *= scale;
		trailingWidth *= scale;
	}
	lineWidths.push_back(currentLine);
	lineLeadingWidths.push_back(leadingWidth);
	lineTrailingWidths.push_back(trailingWidth);
	frameWidth = MAX(frameWidth, currentLine);
}

void Textbox::SetText( const char16 *newText )
{
	text = newText;
	ClearLinewidths();
}

float Textbox::WhitespaceCenterCorrection( u32 line, float horizontalFit ) const
{
	if( line >= lineLeadingWidths.size() || line >= lineTrailingWidths.size() )
		return 0.f;
	return 0.5f * ( lineTrailingWidths[ line ] - lineLeadingWidths[ line ] )
		* horizontalFit;
}

void Textbox::SetupGX(const Resources& resources) const
{
	GX_ClearVtxDesc();
	GX_InvVtxCache();

	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	// Textboxes are frequently drawn after banner materials with custom alpha,
	// indirect-texture and swap-table state.  Reset every state this path owns
	// before binding a font sheet so one channel cannot contaminate the next.
	GX_SetNumChans( 1 );
	GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
		GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
	GX_SetNumTexGens( 1 );
	GX_SetTexCoordGen( GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY );
	GX_SetNumTevStages( 1 );
	GX_SetNumIndStages( 0 );
	GX_SetTevOp( GX_TEVSTAGE0, GX_MODULATE );
	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0 );
	GX_SetTevSwapMode( GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0 );
	GX_SetTevKColorSel( GX_TEVSTAGE0, GX_TEV_KCSEL_1_4 );
	GX_SetTevKAlphaSel( GX_TEVSTAGE0, GX_TEV_KASEL_1 );
	GX_SetTevDirect( GX_TEVSTAGE0 );
	GX_SetTevSwapModeTable( GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA );
	GX_SetTevSwapModeTable( GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA );
	GX_SetTevSwapModeTable( GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA );
	GX_SetTevSwapModeTable( GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA );
	GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
	GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET );

	if(header->material_index < resources.materials.size())
	{
		const Material::Header *matHead = resources.materials[header->material_index]->GetHeader();
		if( !matHead )
			return;

		GX_SetFog(0, 0.0f, 0.0f, 0.0f, 0.0f, (GXColor){0xff, 0xff, 0xff, 0xff});
		GX_SetTevSwapModeTable(0, 0, 1, 2, 3);
		GX_SetZTexture(0, 0x11, 0);
		GX_SetNumChans(1 );
		GX_SetChanCtrl(4, 0, 0, 1, 0, 0, 2);
		GX_SetChanCtrl(5, 0, 0, 0, 0, 0, 2);
		GX_SetNumTexGens(1);
		GX_SetTexCoordGen2(0, 1, 4, 0x3c, 0, 0x7D);
		GX_SetNumIndStages(0);
		GX_SetBlendMode(1, 4, 5, 0xf);
		GX_SetNumTevStages(2);
		GX_SetTevDirect(0);
		GX_SetTevDirect(1);
		GX_SetTevSwapMode(0, 0, 0);
		GX_SetTevSwapMode(1, 0, 0);
		GX_SetTevOrder(0, 0, 0, 0xff);

		for( int i = 0; i < 2; i++ )
		{
			GX_SetTevColor(i + 1, (GXColor){ LIMIT(matHead->color_regs[i].r, 0, 0xFF),
											 LIMIT(matHead->color_regs[i].g, 0, 0xFF),
											 LIMIT(matHead->color_regs[i].b, 0, 0xFF),
											 LIMIT(matHead->color_regs[i].a, 0, 0xFF) });
		}

		GX_SetTevColorIn(0, 2, 4, 8, 0xf);
		GX_SetTevAlphaIn(0, 1, 2, 4, 7);
		GX_SetTevColorOp(0, 0, 0, 0, 1, 0);
		GX_SetTevAlphaOp(0, 0, 0, 0, 1, 0);
		GX_SetTevOrder(1, 0xff, 0xff, 4);
		GX_SetTevColorIn(1, 0xf, 0, 0xa, 0xf);
		GX_SetTevAlphaIn(1, 7, 0,  5, 7);
		GX_SetTevColorOp(1, 0, 0, 0, 1, 0);
		GX_SetTevAlphaOp(1, 0, 0, 0, 1, 0);
	}
}

void Textbox::Draw(const Resources& resources, u8 parent_alpha, const float ws_scale, Mtx &modelview) const
{
	if(!text)
	{
		//gprintf( "!text\n" );
		return;
	}

	if(header->font_index >= resources.fonts.size())
	{
		gprintf( "header->font_index >= resources.fonts.size()\n" );
		return;
	}

	WiiFont *font = resources.fonts[header->font_index];
	if(!font->IsLoaded())
	{
		gprintf( "font->IsLoaded(): %s\n", font->getName().c_str() );
		return;
	}

	// Ugly...but doing it by going through all panes is more ugly
	// TODO: move it to somewhere else
	if(lineWidths.empty())
		((Textbox *) this)->SetTextWidth(font, resources.fitTextboxToPane || uniformTextFit);

	if(lineWidths.empty())
		return;

	SetupGX(resources);

	LoadMenuPositionMatrix(modelview, GX_PNMTX0);

	// Setup text color
	GXColor color0 = { header->color[0].r,
					   header->color[0].g,
					   header->color[0].b,
					   MultiplyAlpha(header->color[0].a, parent_alpha) };

	GXColor color1 = { header->color[1].r,
					   header->color[1].g,
					   header->color[1].b,
					   MultiplyAlpha(header->color[1].a, parent_alpha) };

	u32 lastSheetIdx = 0xffff;
	if(!font->CharacterWidth() || !font->CharacterHeight())
		return;
	const bool fitToPane = resources.fitTextboxToPane || uniformTextFit;
	const float paneWidth = GetWidth() * ws_scale;
	float horizontalFit = 1.0f;
	if(fitToPane && frameWidth > 0.0f)
	{
		// Leave a four-percent inset on each side.  DiskCheck's icon authors a
		// label wider than its pane; without this inset the outer K is clipped.
		const float safeWidth = MAX(1.0f, paneWidth * 0.92f);
		if(frameWidth > safeWidth) horizontalFit = safeWidth / frameWidth;
	}

	float xPos;
	if(fitToPane)
	{
		const float visibleFrameWidth = frameWidth * horizontalFit;
		const u8 stringAlign = horizontalFit < 1.0f ? 1 : GetAlignHor();
		const u8 lineAlign = horizontalFit < 1.0f ? 1 : GetLineAlignHor();
		const float textFrameLeft = -0.5f * GetOriginX() * paneWidth
			+ 0.5f * stringAlign * (paneWidth - visibleFrameWidth);
		xPos = textFrameLeft + 0.5f * lineAlign
			* (visibleFrameWidth - lineWidths[0] * horizontalFit);
		if( horizontalFit < 1.0f )
			xPos += WhitespaceCenterCorrection( 0, horizontalFit );
	}
	else
	{
		// Preserve the original WSM/banner-player placement for banner and
		// System Menu layouts.  Only compact icon layouts use BRLYT line alignment.
		const float textWidth = (GetAlignHor() == 1) ? lineWidths[0] : frameWidth;
		xPos = -0.5f * ( GetOriginX() * paneWidth
			+ GetAlignHor() * (-paneWidth + textWidth) );
	}

	const float verticalFit = fitToPane && uniformTextFit ? horizontalFit : 1.0f;
	float yPos = -0.5f * ( GetAlignVer() * -frameHeight * verticalFit +
						    GetHeight() * (GetAlignVer() - (2 - GetOriginY())) )
					     - (fitToPane ? header->font_height * verticalFit : header->font_width);
	yPos -= textYOffset;

	const float advanceScale = fitToPane
		? (header->font_width / (float)font->CharacterWidth()) * horizontalFit
		: header->font_width / (float)font->CharacterHeight();
	const float charWidth = fitToPane ? header->font_width * horizontalFit
		: advanceScale * (float)font->CharacterWidth();
	const float charHeight = fitToPane ? header->font_height * verticalFit : header->font_width;
	const float charSpacing = fitToPane ? header->space_char * horizontalFit : 0.0f;
	int lineNumber = 0;

	for(const char16 *txtString = text; *txtString != 0; txtString++)
	{
		if( accurateGlyphAdvance && (*txtString == ' ' || *txtString == '\t') )
		{
			const char16 *previous = txtString;
			while( previous > text && (previous[-1] == ' ' || previous[-1] == '\t') ) --previous;
			if( previous > text && previous[-1] == '-' ) continue;
		}
		if(*txtString == '\n')
		{
			lineNumber++;
			if(fitToPane)
			{
				const float visibleFrameWidth = frameWidth * horizontalFit;
				const u8 stringAlign = horizontalFit < 1.0f ? 1 : GetAlignHor();
				const u8 lineAlign = horizontalFit < 1.0f ? 1 : GetLineAlignHor();
				const float textFrameLeft = -0.5f * GetOriginX() * paneWidth
					+ 0.5f * stringAlign * (paneWidth - visibleFrameWidth);
				xPos = textFrameLeft + 0.5f * lineAlign
					* (visibleFrameWidth - lineWidths[lineNumber] * horizontalFit);
				if( horizontalFit < 1.0f )
					xPos += WhitespaceCenterCorrection( lineNumber, horizontalFit );
			}
			else
			{
				const float textWidth = (GetAlignHor() == 1)
					? lineWidths[lineNumber] : frameWidth;
				xPos = -0.5f * (GetOriginX() * paneWidth
					+ GetAlignHor() * (-paneWidth + textWidth));
			}
			// go one line down
			yPos -= ((fitToPane ? header->font_height * verticalFit : header->font_width)
				+ header->space_line);
			continue;
		}

		if( font->isSystemFont && !font->HasGlyph(*txtString)
			&& UiFontFallback::HasGlyph(*txtString) )
		{
			const float horizontalScale = fitToPane ? charWidth / charHeight : 1.0f;
			UiFontFallback::Draw(*txtString, xPos, yPos + charHeight, charHeight,
				color0, true, horizontalScale);
			xPos += UiFontFallback::Advance(*txtString, charHeight) * horizontalScale + charSpacing;
			lastSheetIdx = 0xffff;
			continue;
		}
		const WiiFont::CharInfo *charInfo = font->GetCharInfo(*txtString);
		if(!charInfo)
			continue;

		if(charInfo->sheetIdx != lastSheetIdx)
		{
			lastSheetIdx = charInfo->sheetIdx;

			if(!font->Apply(charInfo->sheetIdx))
				continue;
		}

		const float bearing = charInfo->unk ? advanceScale * (float)charInfo->advanceKerning : 0.f;
		if( !accurateGlyphAdvance ) xPos += bearing;
		const float glyphX = xPos + (accurateGlyphAdvance ? bearing : 0.f);

		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(glyphX, yPos, 0);
		GX_Color4u8(color1.r, color1.g, color1.b, color1.a);
		GX_TexCoord2f32(charInfo->s1, charInfo->t2);

		GX_Position3f32(glyphX + charWidth, yPos, 0);
		GX_Color4u8(color1.r, color1.g, color1.b, color1.a);
		GX_TexCoord2f32(charInfo->s2, charInfo->t2);

		GX_Position3f32(glyphX + charWidth, yPos + charHeight, 0);
		GX_Color4u8(color0.r, color0.g, color0.b, color0.a);
		GX_TexCoord2f32(charInfo->s2, charInfo->t1);

		GX_Position3f32(glyphX, yPos + charHeight, 0);
		GX_Color4u8(color0.r, color0.g, color0.b, color0.a);
		GX_TexCoord2f32(charInfo->s1, charInfo->t1);

		xPos += advanceScale * (float)charInfo->advanceGlyphX + charSpacing;
	}
}

void Textbox::ProcessHermiteKey(const KeyType& type, float value)
{
	if (type.type == ANIMATION_TYPE_VERTEX_COLOR)	// vertex color
	{
		if(type.target < 4)
		{
			(&header->color[0].r)[type.target] = FLOAT_2_U8(value);
			return;
		}
		else if(type.target >= 8 && type.target < 12)
		{
			(&header->color[1].r)[type.target - 8] = FLOAT_2_U8(value);
			return;
		}
	}
	Base::ProcessHermiteKey(type, value);
}

void Textbox::ProcessStepKey(const KeyType& type, StepKeyHandler::KeyData data)
{
	Base::ProcessStepKey(type, data);
}
