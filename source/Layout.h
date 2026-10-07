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
#ifndef LAYOUT_H_
#define LAYOUT_H_

#include <gctypes.h>
#include <map>
#include <string>
#include <list>
#include <stack>
#include "animation.h"
#include "Texture.h"
#include "Material.h"
#include "Pane.h"
#include "Picture.h"
#include "Textbox.h"
#include "U8Archive.h"
#include "Window.h"
#include "WiiFont.h"
#include "Textbox.h"

typedef std::vector< std::string > PaletteList;

struct Resources
{
	MaterialList materials;
	TextureList textures;
	FontList fonts;
	PaletteList palettes[ 2 ];
	u8 currentPaletteSet;
	bool fitTextboxToPane;

	Resources() : currentPaletteSet( 0 ), fitTextboxToPane( false ) {}
};

class Animation;
class Layout
{
public:
	Layout();
	virtual ~Layout();

	static const u32 BRLYT_MAGIC = MAKE_FOURCC('R', 'L', 'Y', 'T');
	static const u32 BRLYT_VERSION = 0xFEFF0008;
	static const u32 MAGIC = MAKE_FOURCC('l', 'y', 't', '1');
	static const u32 MAGIC_PANE_PUSH = MAKE_FOURCC('p', 'a', 's', '1');
	static const u32 MAGIC_PANE_POP = MAKE_FOURCC('p', 'a', 'e', '1');
	static const u32 MAGIC_GROUP_PUSH = MAKE_FOURCC('g', 'r', 's', '1');
	static const u32 MAGIC_GROUP_POP = MAKE_FOURCC('g', 'r', 'e', '1');

	bool Load(const u8 *brlyt);
	bool LoadTextures(const U8Archive &banner_file);
	bool LoadFonts(const U8Archive &banner_file);
	Textbox *AddLocalizedLabel(Pane *parent, const char16 *text, float width,
		float height, float fontSize, GXColor color);

	void Render(Mtx &modelview, const Vec2f &ScreenProps, bool widescreen, u8 render_alpha = 0xFF) const;

	// just draw with whatever matrix is passed to the layout
	//! used for layouts that are attached to a pane which is being drawn from another layout
	void RenderWithCurrentMtx( Mtx &modelview, bool widescreen ) const;
	// Draw an isolated native widget with its own resource table. No input,
	// animation callbacks or parent layout transforms are involved.
	void RenderPane( const char *name, Mtx &modelview, u8 alpha = 255 ) const;

	FrameNumber GetFrame() const { return frameCurrent; }
	void SetFrame( FrameNumber frame );
	void AdvanceFrame();
	void ConfigureAnimation( FrameNumber startFrames, FrameNumber loopFrames );

	float GetWidth() const { return header->width; }
	void SetWidth(float _width) { header->width = _width; }

	float GetHeight() const { return header->height; }
	void SetHeight(float _height) { header->height = _height; }

	bool isCentered() const { return header->centered; }

	void SetLanguage(  std::string language );// purposefully not using a reference
	// Icon layouts are only 170x96.  Mark them so an authored textbox which is
	// wider than its pane can be fitted without changing banner text placement.
	void SetFitTextboxToPane( bool enabled )
		{ resources.fitTextboxToPane = enabled; }

	Pane* FindPane(const std::string& name);
	Pane* FindPane(const char* name);
	const PaneList &Panes() const { return panes; }
	// Move a pane to the end of its immediate sibling list so it is drawn on
	// top without rebuilding the BRLYT hierarchy.
	bool BringPaneToFront( const char *name );

	// assumes textboxes and panes don't exist with the same name
	Textbox *FindTextbox( const char *name );
	float MeasureTextbox(const char *name)
		{ Textbox *box=FindTextbox(name);return box?box->MeasureTextWidth(resources):0.f; }
	inline Textbox *FindTextbox( const std::string &name )
	{
		return FindTextbox( name.c_str() );
	}

	Material* FindMaterial(const std::string& name);
	Material* FindMaterial(const char* name);
	Texture *FindTexture(const std::string &name);
	Texture *FindTexture(const char* name);
	void AddPalette( const std::string &name, u8 keySet );

	void LoadBrlanTpls( Animation* anim, const U8Archive &archive );

	const MaterialList &Materials() const{ return resources.materials; }
	const TextureList &Textures() const{ return resources.textures; }
	struct Group
	{
		static const u32 MAGIC = MAKE_FOURCC('g', 'r', 'p', '1');
		static const u32 NAME_LENGTH = 0x10;

		std::map<std::string, Group> groups;
		PaneList panes;
		std::list<std::string> paneNames;
	};
	const Group *FindGroup( const std::string &name ) const;

private:
	struct BRLYT_Header
	{
	  u32 magic;
	  u32 version;
	  u32 filesize;
	  u16 header_len;
	  u16 section_count;
	} __attribute__((packed));

	struct Header
	{
	  u8 centered;
	  u8 pad[3];
	  float width;
	  float height;
	} __attribute__((packed));

	struct LytItemList
	{
	  u16 num_items;
	  u16 offset_to_first;
	} __attribute__((packed));

	struct LytStringTable
	{
	  u32 offset_filename;
	  u32 pad;
	} __attribute__((packed));

	const Group *FindGroup( const std::string &name, const Group *parent ) const;

	Layout::Header *header;

	Resources resources;
	PaneList panes;
	FrameNumber frameCurrent;
	FrameNumber startFrameCount;
	FrameNumber loopEndFrame;
	bool hasLoopAnimation;

	std::map<std::string, Group> groups;
};

#endif
