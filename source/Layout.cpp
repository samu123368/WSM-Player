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
#include "Layout.h"
#include "list.h"
#include "WiiFont.h"
#include "utils/fixedname.h"

namespace
{
	bool BringPaneToFrontInList( PaneList &siblings, const char *name )
	{
		for( u32 i = 0; i < siblings.size(); ++i )
		{
			Pane *pane = siblings[ i ];
			if( pane && !strncmp( pane->getName(), name, 0x10 ) )
			{
				siblings.erase( siblings.begin() + i );
				siblings.push_back( pane );
				return true;
			}
			if( pane && BringPaneToFrontInList( pane->panes, name ) )
				return true;
		}
		return false;
	}

	std::string FixedName( const char *name, size_t capacity )
	{
		size_t length = 0;
		while( length < capacity && name[ length ] )
			++length;
		return std::string( name, length );
	}

	bool IsLanguageGroup( const std::string &name )
	{
		static const char *languages[] = {
			"JPN", "ENG", "GER", "FRA", "SPA", "ITA",
			"NED", "CHN", "KOR", "TWN", "RUS"
		};
		for( u32 i = 0; i < sizeof( languages ) / sizeof( languages[ 0 ] ); ++i )
		{
			if( name == languages[ i ] )
				return true;
		}
		return false;
	}

	const char *LegacyLanguagePane( const std::string &language )
	{
		if( language == "JPN" ) return "font_j";
		if( language == "GER" ) return "font_g";
		if( language == "FRA" ) return "font_f";
		if( language == "SPA" ) return "font_s";
		if( language == "ITA" ) return "font_i";
		if( language == "NED" ) return "font_n";
		return "font_e";
	}

	const char *NintendoChannelLanguagePane( const std::string &language )
	{
		if( language == "JPN" ) return "N_logoJ_00";
		if( language == "GER" ) return "N_logoG_00";
		if( language == "FRA" ) return "N_logoF_00";
		if( language == "SPA" ) return "N_logoS_00";
		if( language == "ITA" ) return "N_logoI_00";
		if( language == "NED" ) return "N_logoN_00";
		return "N_logoE_00";
	}

	const char *NintendoChannelIconLanguagePane( const std::string &language )
	{
		if( language == "JPN" ) return "P_logoJ_00";
		if( language == "GER" ) return "P_logoG_00";
		if( language == "FRA" ) return "P_logoF_00";
		if( language == "SPA" ) return "P_logoSp_00";
		if( language == "ITA" ) return "P_logoI_00";
		if( language == "NED" ) return "P_logoN_00";
		return "P_logoE_00";
	}

	const char *LegacyInternetLanguageSuffix( const std::string &language )
	{
		if( language == "JPN" ) return "J";
		if( language == "GER" ) return "G";
		if( language == "FRA" ) return "F";
		if( language == "SPA" ) return "Sp";
		if( language == "ITA" ) return "I";
		if( language == "NED" ) return "N";
		return "E";
	}

	void CollectLanguageGroups(
		const std::map<std::string, Layout::Group> &groups,
		std::list<std::pair<std::string, const Layout::Group *> > &found )
	{
		std::map<std::string, Layout::Group>::const_iterator it = groups.begin();
		for( ; it != groups.end(); ++it )
		{
			if( IsLanguageGroup( it->first ) )
				found.push_back( std::make_pair( it->first, &it->second ) );
			CollectLanguageGroups( it->second.groups, found );
		}
	}
}

bool Layout::BringPaneToFront( const char *name )
{
	return name && BringPaneToFrontInList( panes, name );
}

Layout::Layout()
	: header(NULL), frameCurrent( 0.0f ), startFrameCount( 0.0f ),
	  loopEndFrame( 0.0f ), hasLoopAnimation( false )
{
}

Layout::~Layout()
{
	for(u32 i = 0; i < panes.size(); ++i)
		delete panes[i];

	for(u32 i = 0; i < resources.materials.size(); ++i)
		delete resources.materials[i];

	for(u32 i = 0; i < resources.textures.size(); ++i)
		delete resources.textures[i];

	for(u32 i = 0; i < resources.fonts.size(); ++i)
	{
		if( !resources.fonts[i]->isSystemFont )
		{
			delete resources.fonts[i];
		}
	}

}

bool Layout::Load(const u8 *brlyt)
{
	if(!brlyt)
		return false;

	const BRLYT_Header *brlytFile = (const BRLYT_Header *) brlyt;

	if(brlytFile->magic != BRLYT_MAGIC || brlytFile->version != BRLYT_VERSION)
		return false;

	Group* last_group = NULL;
	std::stack<std::map<std::string, Group>*> group_stack;
	group_stack.push(&groups);

	Pane* last_pane = NULL;
	std::stack<std::vector<Pane*>*> pane_stack;
	pane_stack.push(&panes);

	const u8 *position = brlyt + brlytFile->header_len;

	for(u32 i = 0; i < brlytFile->section_count; ++i)
	{
		section_t *section = (section_t *) position;
		position += section->size;

		if(section->magic == Layout::MAGIC)
		{
			header = (Layout::Header *) (section + 1);
		}
		else if (section->magic == TextureList::MAGIC)
		{
			const LytItemList *txl1 = (const LytItemList *) (section+1);
			const char *nameoffset = ((const char *)(txl1+1));
			const LytStringTable *stringTable = (const LytStringTable *) (((const u8 *)(txl1+1))+txl1->offset_to_first);

			for(u32 i = 0; i < txl1->num_items; ++i)
			{
				Texture *texture = new Texture;
				texture->setName(nameoffset+stringTable[i].offset_filename);
				resources.textures.push_back(texture);
			}
		}
		else if (section->magic == MaterialList::MAGIC)
		{
			const LytItemList *mat1 = (const LytItemList *) (section+1);
			const u32 *mat_offsets = (const u32 *) (((const u8 *)(mat1+1))+mat1->offset_to_first);

			for(u32 i = 0; i < mat1->num_items; ++i)
			{
				Material *material = new Material;
				material->Load((Material::Header *) (((const u8 *) section)+mat_offsets[i]));
				resources.materials.push_back(material);
			}
		}
		else if (section->magic == FontList::MAGIC)
		{
			// load font list
			const LytItemList *fnl1 = (const LytItemList *) (section+1);
			const char *nameoffset = ((const char *)(fnl1+1));
			const LytStringTable *stringTable = (const LytStringTable *) (((const u8 *)(fnl1+1))+fnl1->offset_to_first);

			for(u32 i = 0; i < fnl1->num_items; i++)
			{
				const char *name = nameoffset+stringTable[i].offset_filename;

				WiiFont *font = WiiFont::GetSystemFont( name );
				if( !font )
				{
					font = new WiiFont;
					font->SetName( name );
				}
				resources.fonts.push_back( font );
			}
		}
		else if (section->magic == Pane::MAGIC)
		{
			Pane* pane = new Pane;
			pane->Load((Pane::Header *) section);
			pane_stack.top()->push_back(last_pane = pane);
		}
		else if (section->magic == Bounding::MAGIC)
		{
			Bounding* pane = new Bounding;
			pane->Load((Pane::Header *) section);
			pane_stack.top()->push_back(last_pane = pane);
		}
		else if (section->magic == Picture::MAGIC)
		{
			Picture* pane = new Picture;
			pane->Load((Pane::Header *) section);
			pane_stack.top()->push_back(last_pane = pane);
		}
		else if (section->magic == Window::MAGIC)
		{
			Window* pane = new Window;
			pane->Load((Pane::Header *) section);
			pane_stack.top()->push_back(last_pane = pane);
		}
		else if (section->magic == Textbox::MAGIC)
		{
			Textbox* pane = new Textbox;
			pane->Load((Pane::Header *) section);
			pane_stack.top()->push_back(last_pane = pane);
		}
		else if (section->magic == Layout::MAGIC_PANE_PUSH)
		{
			if (last_pane)
				pane_stack.push(&last_pane->panes);
		}
		else if (section->magic == Layout::MAGIC_PANE_POP)
		{
			if (pane_stack.size() > 1)
				pane_stack.pop();
		}
		else if (section->magic == Group::MAGIC)
		{
			const char *grp = (const char *) (section + 1);
			std::string group_name = FixedName( grp, Layout::Group::NAME_LENGTH );
			Group& group_ref = (*group_stack.top())[group_name];
			grp += Layout::Group::NAME_LENGTH;

			u16 sub_count = *(u16 *) grp;
			grp += 4; // 2 bytes reserved

			while (sub_count--)
			{
				std::string pane_name = FixedName( grp, Layout::Group::NAME_LENGTH );
				group_ref.paneNames.push_back( pane_name );

				Pane *thePane = FindPane( pane_name );
				if( thePane )
				{
					group_ref.panes.push_back( thePane );
				}
				else
				{
					gprintf( "didn\'t find pane: \"%s\" for group\n", pane_name.c_str() );
				}

				grp += Layout::Group::NAME_LENGTH;
			}

			last_group = &group_ref;
		}
		else if (section->magic == Layout::MAGIC_GROUP_PUSH)
		{
			if (last_group)
				group_stack.push(&last_group->groups);
		}
		else if (section->magic == Layout::MAGIC_GROUP_POP)
		{
			if (group_stack.size() > 1)
				group_stack.pop();
		}
		else {
			gprintf("Uknown layout section: %08X\n", section->magic);
		}
	}
	return true;
}

bool Layout::LoadTextures( const U8Archive &archive )
{
	bool success = true;

	for(u32 i = 0; i < resources.textures.size(); ++i)
	{
		const u8 *file = archive.GetFile( "/arc/timg/" + resources.textures[i]->getName() );
		if (file)
			resources.textures[i]->Load(file);
		else
			success = false;
	}

	return success;
}

bool Layout::LoadFonts( const U8Archive &archive )
{
	bool success = true;

	for(u32 i = 0; i < resources.fonts.size(); ++i)
	{
		if( resources.fonts[ i ]->IsLoaded() )
		{
			continue;
		}
		u32 fd = archive.FileDescriptor( "/arc/font/" + resources.fonts[i]->getName() );
		if( !fd )
		{
			gprintf( "error loading font: \"%s\"\n", resources.fonts[i]->getName().c_str() );
			continue;
		}
		const u8 *file = archive.GetFileFromFd( fd );
		if( file )
		{
			resources.fonts[i]->Load(file);
		}
		else
			success = false;
	}

	return success;
}

void Layout::RenderWithCurrentMtx( Mtx &modelview, bool widescreen ) const
{
	for(u32 i = 0; i < panes.size(); ++i)
		panes[i]->Render(resources, 0xff, modelview, widescreen);
}

void Layout::RenderPane( const char *name, Mtx &modelview, u8 alpha ) const
{
	Pane *pane = const_cast<Layout *>(this)->FindPane(name);
	if( pane ) pane->Render(resources, alpha, modelview, false, true);
}

void Layout::Render(Mtx &modelview, const Vec2f &ScreenProps, bool widescreen, u8 render_alpha) const
{
	if(!header)
		return;

	Mtx mv;
	// we draw inverse
	guMtxScaleApply(modelview, mv, 1.0f, -1.0f, 1.0f);

	// centered draw
	if(header->centered)
		guMtxTransApply(mv, mv, ScreenProps.x * 0.5f, ScreenProps.y * 0.5f, 0.f);


	//guMtxTransApply(mv, mv, 0.f, 20.f, 0.f);

	// render all panes
	for(u32 i = 0; i < panes.size(); ++i)
		panes[i]->Render(resources, render_alpha, mv, widescreen);
}

void Layout::ConfigureAnimation( FrameNumber startFrames, FrameNumber loopFrames )
{
	startFrameCount = std::max( 0.0f, startFrames );
	loopEndFrame = startFrameCount + std::max( 0.0f, loopFrames );
	hasLoopAnimation = loopFrames > 0.0f;
	SetFrame( 0.0f );
}

void Layout::SetFrame( FrameNumber frame )
{
	frameCurrent = frame;
	const u8 keySet = hasLoopAnimation && frameCurrent >= startFrameCount ? 1 : 0;
	FrameNumber localFrame = keySet ? frameCurrent - startFrameCount : frameCurrent;
	resources.currentPaletteSet = keySet;

	for( u32 i = 0; i < panes.size(); ++i )
		panes[ i ]->SetFrame( localFrame, keySet );
	for( u32 i = 0; i < resources.materials.size(); ++i )
		resources.materials[ i ]->SetFrame( localFrame, keySet );
}

void Layout::AdvanceFrame()
{
	if( hasLoopAnimation )
	{
		frameCurrent += 1.0f;
		if( frameCurrent >= loopEndFrame )
			frameCurrent = startFrameCount;
	}
	else if( frameCurrent + 1.0f < startFrameCount )
	{
		frameCurrent += 1.0f;
	}
	SetFrame( frameCurrent );
}

Textbox *Layout::AddLocalizedLabel(Pane *parent, const char16 *text, float width,
	float height, float fontSize, GXColor color)
{
	if(!parent || !text || parent->FindPane("WSM_LocalTitle")) return NULL;
	WiiFont *font=WiiFont::GetSystemFont("wbf1.brfna");
	if(!font || !font->IsLoaded()) return NULL;
	size_t index=0;
	while(index<resources.fonts.size() && resources.fonts[index]!=font) ++index;
	if(index==resources.fonts.size()) resources.fonts.push_back(font);
	Textbox *label=Textbox::CreateLabel(text,index,width,height,fontSize,color);
	parent->panes.push_back(label);
	return label;
}

void Layout::SetLanguage(  std::string language )
{
	if( !IsLanguageGroup( language ) )
		language = "ENG";

	// Early Nintendo banners such as the Wii Shop Channel do not contain
	// ENG/GER/etc. groups. Instead, each localized title is parented beneath a
	// hidden font_e/font_g/... pane and the BRLAN only animates the children.
	// Select exactly one of those parents before applying normal group rules.
	static const char *legacyPanes[] = {
		"font_j", "font_e", "font_g", "font_f", "font_s", "font_i", "font_n"
	};
	bool haveLegacyPanes = false;
	for( u32 i = 0; i < sizeof( legacyPanes ) / sizeof( legacyPanes[ 0 ] ); ++i )
	{
		Pane *pane = FindPane( legacyPanes[ i ] );
		if( !pane )
			continue;
		haveLegacyPanes = true;
		pane->SetHide( true );
	}
	if( haveLegacyPanes )
	{
		Pane *pane = FindPane( LegacyLanguagePane( language ) );
		if( !pane )
			pane = FindPane( "font_e" );
		if( pane )
		{
			pane->SetHide( false );
			pane->SetVisible( true );
		}
	}

	// Wii Shop's icon has no ENG/GER/etc. groups. Its two title slots contain
	// seven hidden localized children, while Rso0 animates only their parents.
	// Select one child in each slot so the icon cannot become a titleless bag or
	// expose every language at once when later recommendation states are used.
	static const char *shopTitlePanes[ 2 ][ 7 ] = {
		{
			"P_title_J_00", "P_title_E_00", "P_title_F_00",
			"P_title_G_00", "P_title_I_00", "P_title_S_00",
			"P_title_N_00"
		},
		{
			"P_title_J_01", "P_title_E_01", "P_title_F_01",
			"P_title_G_01", "P_title_I_01", "P_title_S_01",
			"P_title_N_01"
		}
	};
	if( FindPane( "N_LogoTitles" ) && FindPane( "P_title_E_00" ) )
	{
		u32 selected = 1; // English is the fallback for unsupported languages.
		if( language == "JPN" ) selected = 0;
		else if( language == "FRA" ) selected = 2;
		else if( language == "GER" ) selected = 3;
		else if( language == "ITA" ) selected = 4;
		else if( language == "SPA" ) selected = 5;
		else if( language == "NED" ) selected = 6;

		for( u32 slot = 0; slot < 2; ++slot )
		{
			for( u32 lang = 0; lang < 7; ++lang )
			{
				Pane *pane = FindPane( shopTitlePanes[ slot ][ lang ] );
				if( pane )
					pane->SetHide( true );
			}
			Pane *pane = FindPane( shopTitlePanes[ slot ][ selected ] );
			if( pane )
			{
				pane->SetHide( false );
				pane->SetVisible( true );
			}
		}
	}

	// The PAL Nintendo Channel predates language groups in banner.brlyt.  It
	// ships seven visible N_logo?_00 trees and expects the menu to select one.
	// Hide the complete alternate trees rather than trying to suppress their
	// many animated child pictures individually.
	static const char *nintendoChannelPanes[] = {
		"N_logoJ_00", "N_logoE_00", "N_logoG_00", "N_logoF_00",
		"N_logoS_00", "N_logoI_00", "N_logoN_00"
	};
	bool haveNintendoChannelPanes = false;
	for( u32 i = 0; i < sizeof( nintendoChannelPanes ) / sizeof( nintendoChannelPanes[ 0 ] ); ++i )
	{
		Pane *pane = FindPane( nintendoChannelPanes[ i ] );
		if( !pane )
			continue;
		haveNintendoChannelPanes = true;
		pane->SetHide( true );
	}
	if( haveNintendoChannelPanes )
	{
		Pane *pane = FindPane( NintendoChannelLanguagePane( language ) );
		if( !pane )
			pane = FindPane( "N_logoE_00" );
		if( pane )
		{
			pane->SetHide( false );
			pane->SetVisible( true );
		}
	}

	// The downloaded-story Nintendo Channel icon uses a second set of seven
	// localized logo pictures during its opening.  They are members of Rso0,
	// not the ENG/GER/etc. groups used by the icon's idle state, so ordinary
	// group selection leaves every translation stacked on top of the others.
	// Keep the authored Rso15 opening animation but expose exactly one logo.
	static const char *nintendoChannelIconPanes[] = {
		"P_logoJ_00", "P_logoE_00", "P_logoG_00", "P_logoF_00",
		"P_logoSp_00", "P_logoI_00", "P_logoN_00"
	};
	bool haveNintendoChannelIconPanes = false;
	for( u32 i = 0; i < sizeof( nintendoChannelIconPanes ) / sizeof( nintendoChannelIconPanes[ 0 ] ); ++i )
	{
		Pane *pane = FindPane( nintendoChannelIconPanes[ i ] );
		if( !pane )
			continue;
		haveNintendoChannelIconPanes = true;
		pane->SetHide( true );
	}
	if( haveNintendoChannelIconPanes )
	{
		Pane *pane = FindPane( NintendoChannelIconLanguagePane( language ) );
		if( !pane )
			pane = FindPane( "P_logoE_00" );
		if( pane )
		{
			pane->SetHide( false );
			pane->SetVisible( true );
		}
	}

	// Internet Channel v768 stores all seven translations as hidden panes
	// beneath its Rso0/Rso1 groups rather than in ENG/GER/etc. groups.  The
	// System Menu explicitly selects these panes; without doing the same here
	// the opening runs but leaves the title and grey message window empty.
	if( FindPane( "N_messWindow_00" )
		&& FindPane( "T_titleE_00" )
		&& FindPane( "N_messageE_00" ) )
	{
		static const char *suffixes[] = { "J", "E", "F", "G", "I", "Sp", "N" };
		static const char *formats[] = {
			"T_title%s_00", "T_telop%s_00", "N_message%s_00",
			"T_message%s_00", "T_message%s_01",
			"P_logo%s_00", "P_logo%s_01"
		};
		char paneName[ 32 ];

		for( u32 suffix = 0; suffix < sizeof( suffixes ) / sizeof( suffixes[ 0 ] ); ++suffix )
		{
			for( u32 format = 0; format < sizeof( formats ) / sizeof( formats[ 0 ] ); ++format )
			{
				snprintf( paneName, sizeof( paneName ), formats[ format ], suffixes[ suffix ] );
				Pane *pane = FindPane( paneName );
				if( pane )
					pane->SetHide( true );
			}
		}

		const char *selected = LegacyInternetLanguageSuffix( language );
		static const char *selectedFormats[] = {
			"T_title%s_00", "T_telop%s_00", "N_message%s_00",
			"T_message%s_00", "P_logo%s_00", "P_logo%s_01"
		};
		for( u32 format = 0;
			format < sizeof( selectedFormats ) / sizeof( selectedFormats[ 0 ] );
			++format )
		{
			snprintf( paneName, sizeof( paneName ), selectedFormats[ format ], selected );
			Pane *pane = FindPane( paneName );
			if( pane )
			{
				pane->SetHide( false );
				pane->SetVisible( true );
			}
		}
	}

	const Group *root = FindGroup( "RootGroup" );
	if( !root )
		return;

	std::list<std::pair<std::string, const Group *> > languageGroups;
	CollectLanguageGroups( root->groups, languageGroups );
	if( languageGroups.empty() )
		return;

	bool haveRequested = false;
	bool haveEnglish = false;
	std::list<std::pair<std::string, const Group *> >::const_iterator it;
	for( it = languageGroups.begin(); it != languageGroups.end(); ++it )
	{
		haveRequested |= it->first == language;
		haveEnglish |= it->first == "ENG";
	}
	if( !haveRequested )
	{
		// A region-only archive still needs exactly one group selected. Leaving
		// all of them visible stacks labels; editable text is translated later.
		language = haveEnglish ? "ENG" : languageGroups.front().first;
	}

	// Resolve group members at language-selection time. Some valid BRLYTs put
	// group sections before all panes have been parsed, so cached pointers alone
	// are not reliable. First hide every localized pane, then unhide only the
	// selected language; this also handles panes listed in multiple groups.
	for( it = languageGroups.begin(); it != languageGroups.end(); ++it )
	{
		std::list<std::string>::const_iterator paneName = it->second->paneNames.begin();
		for( ; paneName != it->second->paneNames.end(); ++paneName )
		{
			Pane *pane = FindPane( *paneName );
			if( pane )
				pane->SetHide( true );
		}
	}

	for( it = languageGroups.begin(); it != languageGroups.end(); ++it )
	{
		if( it->first != language )
			continue;
		std::list<std::string>::const_iterator paneName = it->second->paneNames.begin();
		for( ; paneName != it->second->paneNames.end(); ++paneName )
		{
			Pane *pane = FindPane( *paneName );
			if( pane )
			{
				pane->SetHide( false );
				pane->SetVisible( true );
			}
		}
	}
}

Pane* Layout::FindPane(const std::string& find_name)
{
	for(u32 i = 0; i < panes.size(); ++i)
	{
		Pane* found = panes[i]->FindPane(find_name);
		if(found)
			return found;
	}

	return NULL;
}

Pane* Layout::FindPane(const char* find_name)
{
	for(u32 i = 0; i < panes.size(); ++i)
	{
		Pane* found = panes[i]->FindPane(find_name);
		if(found)
			return found;
	}

	return NULL;
}

Textbox* Layout::FindTextbox( const char* name )
{
	Pane *pane = FindPane( name );
	if( !pane )
	{
		return NULL;
	}
	return static_cast< Textbox * >( pane );
}

Material* Layout::FindMaterial(const std::string& find_name)
{
	for(u32 i = 0; i < resources.materials.size(); ++i)
	{
		if( FixedNameEquals( resources.materials[i]->getName(), 20, find_name ) )
			return resources.materials[i];
	}

	return NULL;
}

Material* Layout::FindMaterial(const char* find_name)
{
	for(u32 i = 0; i < resources.materials.size(); ++i)
	{
		if( FixedNameEquals( resources.materials[i]->getName(), 20, find_name ) )
			return resources.materials[i];
	}

	return NULL;
}

Texture* Layout::FindTexture(const std::string& find_name)
{
	for(u32 i = 0; i < resources.textures.size(); ++i)
	{
		if (find_name == resources.textures[i]->getName())
			return resources.textures[i];
	}

	return NULL;
}

Texture* Layout::FindTexture(const char* find_name)
{
	for(u32 i = 0; i < resources.textures.size(); ++i)
	{
		if( !strcmp( find_name, resources.textures[i]->getName().c_str() ) )
			return resources.textures[i];
	}

	return NULL;
}

void Layout::AddPalette( const std::string &name, u8 keySet )
{
	if( keySet > 1 )
		return;
	resources.palettes[ keySet ].push_back( name );
	if( FindTexture( name ) )
		return;

	Texture *texture = new Texture;
	texture->setName( name );
	resources.textures.push_back( texture );
}

void Layout::LoadBrlanTpls( Animation* anim, const U8Archive &archive )
{
	if( !anim )
	{
		return;
	}
	// load textures for palette animations
	u32 size = anim->paletteNames.size();
	for( u32 i = 0; i < size; i++ )
	{
		const std::string &pName = anim->paletteNames[ i ];
		if( FindTexture( pName ) )
		{
			continue;
		}
		u32 filesize = 0;
		const u8 *file = archive.GetFile( "/arc/timg/" + pName, &filesize );
		if( !file )
		{
			gprintf( "failed to load texture from brlan: %s\n", pName.c_str() );
			continue;
		}
		Texture *texture = new Texture;
		texture->setName( pName );
		texture->Load( file );
		resources.textures.push_back( texture );
	}
}

const Layout::Group *Layout::FindGroup( const std::string &name ) const
{
	std::map<std::string, Group>::const_iterator it = groups.begin(), itE = groups.end();
	while( it != itE )
	{
		if( it->first == name )
		{
			return &it->second;
		}
		const Group *ret = FindGroup( name, &(it->second) );
		if( ret )
		{
			return ret;
		}
		++it;
	}
	gprintf( "Layout::FindGroup( \"%s\" ): not found\n", name.c_str() );
	return NULL;
}

const Layout::Group *Layout::FindGroup( const std::string &name, const Layout::Group *parent ) const
{
	std::map<std::string, Group>::const_iterator it = parent->groups.begin(), itE = parent->groups.end();
	while( it != itE )
	{
		if( it->first == name )
		{
			return &it->second;
		}
		const Group *ret = FindGroup( name, &(it->second) );
		if( ret )
		{
			return ret;
		}
		++it;
	}
	return NULL;
}
