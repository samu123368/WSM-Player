#include "appsettingsscreen.h"
#include "playerupdate.h"
#include "wsmversion.h"
#include "agreementviewer.h"
#include "SystemMenuResources.h"
#include "channelimageoverride.h"
#include "wiihtmlcontent.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <malloc.h>
#include <ogc/conf.h>
#include <ogc/ios.h>
#include <network.h>

#include "Inputs.h"
#include "SystemFont.h"
#include "Texture.h"
#include "WiiFont.h"
#include "bannerlist.h"
#include "channelpreviewconfig.h"
#include "localization.h"
#include "uifontfallback.h"
#include "menuaudio.h"
#include "miiprofiles.h"
#include "settings.h"
#include "utils/char16.h"
#include "utils/U8Archive.h"
#include "utils/TextureConverter.h"
#include "utils/gecko.h"
#include "utils/sc.h"
#include "video.h"


namespace
{
	const int TransitionFrames = 10;
	// Keep serialized Wii language IDs stable; Japanese is last in the picker.
	const int SelectableLanguages[] = { -1, 1, 2, 3, 4, 5, 6, 0 };
	const int SelectableLanguageCount = sizeof(SelectableLanguages) / sizeof(SelectableLanguages[0]);

	bool ParseForecastDifference( const std::string &text, int &value )
	{
		if( text.empty() || text.size() > 3 ) return false;
		size_t i = text[0] == '-' || text[0] == '+' ? 1 : 0;
		if( i == text.size() ) return false;
		int parsed = 0;
		for( ; i < text.size(); ++i )
		{
			if( text[i] < '0' || text[i] > '9' ) return false;
			parsed = parsed * 10 + text[i] - '0';
		}
		if( parsed > 99 ) return false;
		value = text[0] == '-' ? -parsed : parsed;
		return true;
	}

	struct Rect
	{
		float x;
		float y;
		float w;
		float h;

		bool Contains( float px, float py ) const
		{
			return px >= x && px <= x + w && py >= y && py <= y + h;
		}
	};

	float SX( const Vec2f &screen ) { return screen.x / 640.0f; }
	float SY( const Vec2f &screen ) { return screen.y / 480.0f; }
	float Scale( const Vec2f &screen ) { return std::min( SX( screen ), SY( screen ) ); }

	Rect MainRowRect( int row, const Vec2f &screen, bool scrollable = true )
	{
		// Five comfortably sized rows, rather than six flattened dialog pills.
		const float y = scrollable ? 76.0f + row * 62.0f : 74.0f + row * 54.0f;
		Rect result = { 52.0f * SX( screen ), y * SY( screen ),
			492.0f * SX( screen ), (scrollable ? 56.0f : 48.0f) * SY( screen ) };
		return result;
	}

	Rect SystemSettingsHitRect( float x, float y, float w, float h,
		const Vec2f &screen )
	{
		Rect result = { (x+16.0f)*SX(screen), (y+12.0f)*SY(screen),
			w*SX(screen), h*SY(screen) };
		return result;
	}

	std::string SystemSettingsRelativePath( const std::string &path,
		const std::string &root )
	{
		std::string relative = path;
		const std::string prefix = root + "/";
		if( !prefix.empty() && relative.find( prefix ) == 0 )
			relative.erase( 0, prefix.size() );
		if( relative.empty() ) relative = "index01.html";
		std::transform( relative.begin(), relative.end(), relative.begin(),
			[]( unsigned char value ) { return (char)std::tolower( value ); } );
		return relative;
	}

	bool IsSystemSettingsRootPage( const std::string &path,
		const std::string &root )
	{
		const std::string relative = SystemSettingsRelativePath( path, root );
		return relative == "index01.html" || relative == "index02.html"
			|| relative == "index03.html";
	}

	bool IsWiiJavascriptLink( const std::string &link )
	{
		return link.empty() || link == "#"
			|| link.find( "javascript:" ) == 0
			|| link.find( "javaScript:" ) == 0;
	}

	std::string ReadOnlyWiiSettingsRoute( const std::string &path,
		const std::string &root, const std::string &id,
		const std::string &authoredLink )
	{
		const std::string relative = SystemSettingsRelativePath( path, root );
		// This launches an external channel on a real menu, not another HTML
		// document. Route the visible control before filtering its native href.
		if( relative == "internet/internet_index.html" && id == "List03" )
			return "wsm:agreement";
		if( relative == "internet/eula_index.html" )
		{
			if( id == "UnderL" ) return "wsm:agreement";
			if( id == "UnderR" ) return "Internet_index.html";
			return std::string();
		}
		if( !IsWiiJavascriptLink( authoredLink ) ) return authoredLink;

		// Main settings pages. These controls normally query privileged Wii
		// state before choosing an HTML document; the read-only browser opens the
		// ordinary installed-system branch directly.
		if( relative == "index02.html" )
		{
			if( id == "List01" ) return "Parental_Control/Parental_Control_index.html";
			if( id == "List03" ) return "Internet/Internet_index.html";
			if( id == "List04" ) return "WiiConnect24/Wiiconnect24_index.html";
		}
		if( relative == "index03.html" )
		{
			if( id == "List02" ) return "Country/EU_Country_select01.html";
			if( id == "List03" ) return "Update/Update_index.html";
			if( id == "List04" ) return "Format/Format_index01.html";
		}

		// Internet connection slots all enter the same authentic connection-type
		// page in this non-mutating viewer. Existing/configured profiles remain
		// available from that page's own authored links.
		if( relative == "internet/connect_set_top.html"
			&& id.size() == 6 && id.find( "List" ) == 0 )
			return "Connect_select.html";
		if( relative == "internet/common0202.html" && id == "UnderL" )
			return "Common0201.html";
		if( relative == "internet/wi_fi_security_keycode.html" && id == "UnderR" )
			return "Wi_Fi_select_security.html";
		if( relative == "internet/hand_wi_fi_security_keycode.html" && id == "UnderR" )
			return "Hand_Wi_Fi_select_security.html";
		if( relative == "internet/hand_proxy_keycode.html" && id == "UnderR" )
			return "Hand_Proxy_select.html";
		if( relative == "internet/hand_basic_keycode.html" && id == "UnderR" )
			return "Hand_Proxy_keycode.html";

		// Parental Controls contains several asynchronous Wii keyboard/dialog
		// hand-offs. Follow the successful/default branch so every real page in
		// the flow remains inspectable without changing NAND settings.
		if( relative == "parental_control/parental_control_description02.html"
			&& id == "UnderR" ) return "Input_Secret_number01.html";
		if( relative == "parental_control/input_secret_number01.html"
			&& id == "UnderR" ) return "Input_Secret_number02.html";
		if( relative == "parental_control/input_secret_number02.html"
			&& id == "UnderR" ) return "Input_Secret_keyword01.html";
		if( relative == "parental_control/input_secret_keyword02.html"
			&& id == "UnderR" ) return "Middle_index.html";
		if( relative == "parental_control/middle_index.html" )
		{
			if( id == "List01" ) return "Re_Setting_index.html";
			if( id == "List02" ) return "Restrictions.html";
			if( id == "UnderL" || id == "UnderR" ) return "../index02.html";
		}
		if( relative == "parental_control/re_setting_index.html" )
		{
			if( id == "List03" )
				return "PEGI_General/PEGI_General01.html";
			if( id == "UnderR" ) return "Middle_index.html";
		}
		if( relative == "parental_control/re_input_secret_number01.html"
			&& id == "UnderR" ) return "Re_Input_Secret_number02.html";
		if( relative == "parental_control/re_input_secret_number02.html"
			&& id == "UnderR" ) return "Re_Input_Secret_keyword01.html";
		if( relative == "parental_control/re_input_secret_keyword02.html"
			&& id == "UnderR" ) return "Re_Setting_index.html";
		if( relative == "parental_control/restrictions.html"
			&& ( id == "UnderL" || id == "UnderR" ) )
			return "Middle_index.html";
		if( relative == "parental_control/parental_control_index.html"
			&& id == "UnderL" ) return "../index02.html";
		if( relative == "parental_control/re_parental_control_index.html"
			&& id == "UnderL" ) return "../index02.html";

		// Frameset-backed lists are captured as complete 640x480 pages. Their
		// bottom buttons live in a sibling frame, so provide the same destinations
		// while the main list document is active.
		if( relative.find( "country/eu_country_select" ) == 0
			&& ( id == "UnderL" || id == "UnderR" ) )
			return "../index03.html";
		if( relative.find( "parental_control/pegi_" ) == 0
			|| relative.find( "parental_control/usk/usk" ) == 0
			|| relative.find( "parental_control/oflc_" ) == 0 )
		{
			if( id == "UnderL" || id == "UnderR" )
				return "../Re_Setting_index.html";
		}

		if( relative == "wiiconnect24/onoff_set.html"
			&& ( id == "UnderL" || id == "UnderR" ) )
			return "Wiiconnect24_index.html";
		if( relative == "update/update_index.html" && id == "UnderL" )
			return "Update_connectTest.html";
		if( relative == "update/update_eula.html" )
			return id == "UnderL" ? "Update_index.html" : "Update_common.html";
		if( relative == "format/format_index04.html" && id == "UnderR" )
			return "../Setup/startup_index1.html";
		if( relative == "setup/nickname_set.html" && id == "UnderR" )
			return "../Country/EU_Country_select01.html";

		return authoredLink;
	}

	void ApplyReadOnlyWiiSettingsRoutes( const std::string &path,
		const std::string &root, std::vector<std::string> &links )
	{
		for( size_t i = 0; i < links.size(); ++i )
		{
			char id[ 16 ];
			snprintf( id, sizeof( id ), "List%02d", (int)i + 1 );
			links[ i ] = ReadOnlyWiiSettingsRoute( path, root, id, links[ i ] );
		}
	}

	Rect ListRowRect( int row, int count, const Vec2f &screen )
	{
		const float y = count <= 4 ? 88.0f + row * 68.0f : 76.0f + row * 62.0f;
		const float h = 56.0f;
		Rect result = { 52.0f * SX( screen ), y * SY( screen ),
			492.0f * SX( screen ), h * SY( screen ) };
		return result;
	}

	const int MaxVisibleListRows = 5;

	int ListWindowStart( int first, int total )
	{
		if( total <= MaxVisibleListRows ) return 0;
		return std::max( 0, std::min( first,
			total - MaxVisibleListRows ) );
	}

	Rect SelectorRect( int row, const Vec2f &screen )
	{
		Rect result = { 352.0f * SX( screen ), ( 82.0f + row * 62.0f ) * SY( screen ),
			188.0f * SX( screen ), 44.0f * SY( screen ) };
		return result;
	}

	Rect SliderRect( const Vec2f &screen )
	{
		Rect result = { 352.0f * SX( screen ), 243.0f * SY( screen ),
			162.0f * SX( screen ), 34.0f * SY( screen ) };
		return result;
	}

	Rect AudioSliderRect( const Vec2f &screen )
	{
		Rect result = { 352.0f * SX( screen ), 135.0f * SY( screen ),
			142.0f * SX( screen ), 34.0f * SY( screen ) };
		return result;
	}

	Rect AudioSelectorRect( const Vec2f &screen )
	{
		Rect result = SelectorRect(0,screen);
		const Rect row = MainRowRect(0,screen,false);
		result.y = row.y + (row.h - result.h) * .5f;
		return result;
	}

	Rect CloseRect( const Vec2f &screen )
	{
		// Matches the authored setupBtn Back hitbox so that the existing pointer
		// button and this replacement screen agree on the clickable region.
		Rect result = { 46.0f * SX( screen ), 399.0f * SY( screen ),
			174.0f * SX( screen ), 55.0f * SY( screen ) };
		return result;
	}

	Rect ScrollUpRect( const Vec2f &screen )
	{
		Rect result = { 552.0f * SX( screen ), 72.0f * SY( screen ),
			42.0f * SX( screen ), 32.0f * SY( screen ) };
		return result;
	}

	Rect ThemeApplyRect( const Vec2f &screen )
	{
		Rect result = { 376.0f * SX( screen ), 399.0f * SY( screen ),
			218.0f * SX( screen ), 55.0f * SY( screen ) };
		return result;
	}

	Rect ScrollDownRect( const Vec2f &screen )
	{
		Rect result = { 552.0f * SX( screen ), 336.0f * SY( screen ),
			42.0f * SX( screen ), 32.0f * SY( screen ) };
		return result;
	}

	Rect ScrollTrackRect( const Vec2f &screen )
	{
		Rect result = { 554.0f * SX( screen ), 108.0f * SY( screen ),
			38.0f * SX( screen ), 224.0f * SY( screen ) };
		return result;
	}

	Rect ScrollThumbRect( const Vec2f &screen, int rows, int first )
	{
		const Rect track = ScrollTrackRect( screen );
		const int visible = std::min( MaxVisibleListRows, rows );
		const float height = 32.0f * SY( screen );
		const float progress = rows > visible
			? ListWindowStart( first, rows ) / (float)( rows - visible ) : 0.0f;
		Rect thumb = { track.x,
			track.y + progress * ( track.h - height ),
			track.w, height };
		return thumb;
	}

	int ScrollFirstFromPointer( float pointerY, float grabOffset,
		const Vec2f &screen, int rows )
	{
		if( rows <= MaxVisibleListRows ) return 0;
		const Rect track = ScrollTrackRect( screen );
		const Rect thumb = ScrollThumbRect( screen, rows, 0 );
		const float ratio = std::max( 0.0f, std::min( 1.0f,
			( pointerY - grabOffset - track.y ) / ( track.h - thumb.h ) ) );
		return ListWindowStart( (int)floorf(
			ratio * ( rows - MaxVisibleListRows ) + 0.5f ), rows );
	}

	Rect WsmSettingsRect( const Vec2f &screen )
	{
		Rect result = { 418.0f * SX( screen ), 14.0f * SY( screen ),
			190.0f * SX( screen ), 42.0f * SY( screen ) };
		return result;
	}

	Rect DropdownOptionRect( int row, int option, const Vec2f &screen )
	{
		Rect control = SelectorRect( row, screen );
		Rect result = { control.x, control.y + control.h * ( option + 1 ),
			control.w, control.h };
		return result;
	}

	Rect KeyboardKeyRect( int row, int column, const Vec2f &screen )
	{
		if( row < 4 )
		{
			Rect result = { ( 34.0f + column * 57.0f ) * SX( screen ),
				( 147.0f + row * 47.0f ) * SY( screen ), 52.0f * SX( screen ),
				40.0f * SY( screen ) };
			return result;
		}
		const float xs[] = { 34.0f, 132.0f, 310.0f, 368.0f, 466.0f };
		const float ws[] = { 92.0f, 172.0f, 52.0f, 92.0f, 140.0f };
		Rect result = { xs[ column ] * SX( screen ), 340.0f * SY( screen ),
			ws[ column ] * SX( screen ), 42.0f * SY( screen ) };
		return result;
	}

	Rect KeyboardPreviewRect( const Vec2f &screen )
	{
		Rect result = { 38.0f * SX( screen ), 70.0f * SY( screen ),
			564.0f * SX( screen ), 66.0f * SY( screen ) };
		return result;
	}

	GXColor WithAlpha( GXColor color, u8 alpha )
	{
		color.a = (u8)( ( (u16)color.a * alpha ) / 255 );
		return color;
	}

	GXColor SettingsButtonInk(u8 alpha)
	{
		ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
		return skin ? skin->Ink(alpha) : WithAlpha((GXColor){45,69,82,255},alpha);
	}

	void PrepareFlatGX()
	{
		LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
		GX_SetCullMode( GX_CULL_NONE );
		GX_SetZMode( GX_DISABLE, GX_ALWAYS, GX_FALSE );
		GX_SetColorUpdate( GX_TRUE );
		GX_SetAlphaUpdate( GX_TRUE );
		GX_SetNumChans( 1 );
		GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
			GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
		GX_SetNumTexGens( 0 );
		GX_SetNumTevStages( 1 );
		GX_SetNumIndStages( 0 );
		GX_SetTevDirect( GX_TEVSTAGE0 );
		GX_SetTevSwapMode( GX_TEVSTAGE0, 0, 0 );
		GX_SetTevSwapModeTable( 0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA );
		GX_SetTevOp( GX_TEVSTAGE0, GX_PASSCLR );
		GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0 );
		GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
		GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET );
		GX_ClearVtxDesc();
		GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
		GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
	}

	void PrepareTextGX()
	{
		LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
		GX_SetCullMode( GX_CULL_NONE );
		GX_SetZMode( GX_DISABLE, GX_ALWAYS, GX_FALSE );
		GX_SetColorUpdate( GX_TRUE );
		GX_SetAlphaUpdate( GX_TRUE );
		GX_SetNumChans( 1 );
		GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
			GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
		GX_SetNumTexGens( 1 );
		GX_SetTexCoordGen( GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_TEX0, GX_IDENTITY );
		GX_SetNumTevStages( 1 );
		GX_SetNumIndStages( 0 );
		GX_SetTevDirect( GX_TEVSTAGE0 );
		GX_SetTevSwapMode( GX_TEVSTAGE0, 0, 0 );
		GX_SetTevSwapModeTable( 0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA );
		GX_SetTevOp( GX_TEVSTAGE0, GX_MODULATE );
		GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0 );
		GX_SetAlphaCompare( GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0 );
		GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET );
		GX_ClearVtxDesc();
		GX_InvVtxCache();
		GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
		GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
		GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );
	}

	void DrawRect( float x, float y, float w, float h, GXColor color )
	{
		PrepareFlatGX();
		GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
		GX_Position3f32( x, y, 0.0f );
		GX_Color4u8( color.r, color.g, color.b, color.a );
		GX_Position3f32( x + w, y, 0.0f );
		GX_Color4u8( color.r, color.g, color.b, color.a );
		GX_Position3f32( x + w, y + h, 0.0f );
		GX_Color4u8( color.r, color.g, color.b, color.a );
		GX_Position3f32( x, y + h, 0.0f );
		GX_Color4u8( color.r, color.g, color.b, color.a );
		GX_End();
	}

	void DrawTextureRect( const Texture *texture, const Rect &rect, u8 alpha,
		float u2 = 1.0f, float v2 = 1.0f )
	{
		if( !texture || !texture->IsLoaded() || rect.w <= 0.0f || rect.h <= 0.0f )
			return;
		PrepareTextGX();
		u8 tlut = 0;
		texture->Apply( tlut, GX_TEXMAP0,
			u2 > 1.0f ? GX_REPEAT : GX_CLAMP,
			v2 > 1.0f ? GX_REPEAT : GX_CLAMP );
		GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
		GX_Position3f32( rect.x, rect.y, 0.0f );
		GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 0.0f, 0.0f );
		GX_Position3f32( rect.x + rect.w, rect.y, 0.0f );
		GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( u2, 0.0f );
		GX_Position3f32( rect.x + rect.w, rect.y + rect.h, 0.0f );
		GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( u2, v2 );
		GX_Position3f32( rect.x, rect.y + rect.h, 0.0f );
		GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 0.0f, v2 );
		GX_End();
	}

	void DrawRoundedRect( const Rect &rect, float radius, GXColor color )
	{
		const float r = std::min( radius, std::min( rect.w, rect.h ) * 0.5f );
		DrawRect( rect.x + r, rect.y, rect.w - r * 2.0f, rect.h, color );
		DrawRect( rect.x, rect.y + r, rect.w, rect.h - r * 2.0f, color );
		DrawRect( rect.x + r * 0.35f, rect.y + r * 0.35f,
			rect.w - r * 0.7f, rect.h - r * 0.7f, color );
	}

	void DrawOutlinedRoundedRect( const Rect &rect, float radius,
		GXColor border, GXColor fill, float thickness )
	{
		ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
		if( skin && (rect.h > 110 || rect.h > rect.w * 2)
			&& skin->Panel(rect.x,rect.y,rect.w,rect.h,fill.a) ) return;
		if( skin && skin->Button(rect.x, rect.y, rect.w, rect.h, fill.a,
			fill.r < 245 && fill.g > fill.r) ) return;
		DrawRoundedRect( rect, radius, border );
		Rect inner = { rect.x + thickness, rect.y + thickness,
			rect.w - thickness * 2.0f, rect.h - thickness * 2.0f };
		DrawRoundedRect( inner, std::max( 1.0f, radius - thickness ), fill );
	}

	WiiFont *SettingsFont()
	{
		return SystemFont::wbf1 && SystemFont::wbf1->IsLoaded()
			? SystemFont::wbf1 : NULL;
	}

	float TextWidth( const char16 *text, float pixelHeight )
	{
		WiiFont *font = SettingsFont();
		if( !font || !text || !font->CharacterHeight() )
			return 0.0f;
		const float fontScale = pixelHeight / font->CharacterHeight();
		float width = 0.0f;
		for( ; *text; ++text )
		{
			if( !font->HasGlyph(*text) && UiFontFallback::HasGlyph(*text) )
			{
				width += UiFontFallback::Advance(*text, pixelHeight);
				continue;
			}
			const WiiFont::CharInfo *info = font->GetCharInfo( *text );
			if( !info )
				continue;
			if( info->unk )
				width += fontScale * info->advanceKerning;
			width += fontScale * info->advanceGlyphX;
		}
		return width;
	}

	float FitTextHeight( const char16 *text, float maxWidth,
		float preferredHeight, float minimumHeight )
	{
		float height = preferredHeight;
		while( height > minimumHeight && TextWidth( text, height ) > maxWidth )
			height -= 1.0f;
		return std::max( minimumHeight, height );
	}

	void DrawText( float x, float y, const char16 *text, float pixelHeight, GXColor color )
	{
		// Callers choose ink for their surface. Never reinterpret an already
		// resolved white button label as title/body ink (or vice versa).
		WiiFont *font = SettingsFont();
		if( !font || !text || !font->CharacterHeight() )
			return;
		PrepareTextGX();
		const float fontScale = pixelHeight / font->CharacterHeight();
		const float charWidth = fontScale * font->CharacterWidth();
		u32 lastSheet = 0xffffffff;
		for( ; *text; ++text )
		{
			if( !font->HasGlyph(*text) && UiFontFallback::HasGlyph(*text) )
			{
				UiFontFallback::Draw(*text, x, y, pixelHeight, color);
				x += UiFontFallback::Advance(*text, pixelHeight);
				lastSheet = 0xffffffff;
				continue;
			}
			const WiiFont::CharInfo *info = font->GetCharInfo( *text );
			if( !info )
				continue;
			if( info->sheetIdx != lastSheet )
			{
				if( !font->Apply( info->sheetIdx ) )
					continue;
				lastSheet = info->sheetIdx;
			}
			if( info->unk )
				x += fontScale * info->advanceKerning;
			GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
			GX_Position3f32( x, y, 0.0f );
			GX_Color4u8( color.r, color.g, color.b, color.a );
			// This overlay uses screen coordinates (Y grows downward).  BRLYT
			// Textbox uses layout coordinates (Y grows upward), so copying its
			// t2-at-y/t1-at-y+height order flips glyphs on real hardware.
			GX_TexCoord2f32( info->s1, info->t1 );
			GX_Position3f32( x + charWidth, y, 0.0f );
			GX_Color4u8( color.r, color.g, color.b, color.a );
			GX_TexCoord2f32( info->s2, info->t1 );
			GX_Position3f32( x + charWidth, y + pixelHeight, 0.0f );
			GX_Color4u8( color.r, color.g, color.b, color.a );
			GX_TexCoord2f32( info->s2, info->t2 );
			GX_Position3f32( x, y + pixelHeight, 0.0f );
			GX_Color4u8( color.r, color.g, color.b, color.a );
			GX_TexCoord2f32( info->s1, info->t2 );
			GX_End();
			x += fontScale * info->advanceGlyphX;
		}
	}

	void DrawCentered( const Rect &rect, const char16 *text,
		float preferredHeight, float minimumHeight, GXColor color )
	{
		const float height = FitTextHeight( text, rect.w - 18.0f,
			preferredHeight, minimumHeight );
		const float width = TextWidth( text, height );
		DrawText( rect.x + std::max( 0.0f, ( rect.w - width ) * 0.5f ),
			rect.y + std::max( 0.0f, ( rect.h - height ) * 0.5f ), text, height, color );
	}

	void DrawFit( float x, float y, float maxWidth, const char16 *text,
		float preferredHeight, float minimumHeight, GXColor color )
	{
		const float height = FitTextHeight(text, maxWidth, preferredHeight, minimumHeight);
		char16 fitted[512];
		strlcpy16(fitted, text, 512);
		// Long translations must not spill into the adjacent value or scrollbar.
		if( TextWidth(fitted,height) > maxWidth )
		{
			size_t length = 0; while(fitted[length]) ++length;
			while(length > 1 && TextWidth(fitted,height) > maxWidth)
			{
				--length; fitted[length-1] = 0x2026; fitted[length] = 0;
			}
		}
		DrawText( x, y, fitted, height, color );
	}

	void Utf8ToChar16( const std::string &source, char16 *destination,
		size_t capacity )
	{
		if( !destination || !capacity )
			return;
		size_t written = 0;
		const unsigned char *input = (const unsigned char *)source.c_str();
		while( *input && written + 1 < capacity )
		{
			u32 codepoint = '?';
			if( input[ 0 ] < 0x80 )
				codepoint = *input++;
			else if( ( input[ 0 ] & 0xe0 ) == 0xc0 && input[ 1 ] )
			{
				codepoint = ( ( input[ 0 ] & 0x1f ) << 6 ) | ( input[ 1 ] & 0x3f );
				input += 2;
			}
			else if( ( input[ 0 ] & 0xf0 ) == 0xe0 && input[ 1 ] && input[ 2 ] )
			{
				codepoint = ( ( input[ 0 ] & 0x0f ) << 12 )
					| ( ( input[ 1 ] & 0x3f ) << 6 ) | ( input[ 2 ] & 0x3f );
				input += 3;
			}
			else
				++input;
			destination[ written++ ] = codepoint <= 0xffff ? (char16)codepoint : '?';
		}
		destination[ written ] = 0;
	}

	void DrawUtf8Fit( float x, float y, float maxWidth, const std::string &text,
		float preferredHeight, float minimumHeight, GXColor color )
	{
		char16 decoded[ 320 ];
		Utf8ToChar16( text, decoded, sizeof( decoded ) / sizeof( decoded[ 0 ] ) );
		DrawFit( x, y, maxWidth, decoded, preferredHeight, minimumHeight, color );
	}

	void DrawUtf8Centered( const Rect &rect, const std::string &text,
		float preferredHeight, float minimumHeight, GXColor color )
	{
		char16 decoded[ 320 ];
		Utf8ToChar16( text, decoded, sizeof( decoded ) / sizeof( decoded[ 0 ] ) );
		DrawCentered( rect, decoded, preferredHeight, minimumHeight, color );
	}
	void DrawHtmlContent(const Rect &rect, const std::string &text,
		float fontSize, bool centered, GXColor color)
	{
		if( rect.w <= 0 || rect.h <= 0 ) return;
		std::vector<char16> decoded(2048,0);
		Utf8ToChar16(text,decoded.data(),decoded.size());
		std::vector<std::basic_string<char16>> lines;
		std::basic_string<char16> line;
		for( size_t i = 0; decoded[i]; ++i )
		{
			const char16 ch = decoded[i];
			if( ch == '\n' ) { lines.push_back(line); line.clear(); continue; }
			line += ch;
			if( line.size() > 1 && TextWidth(line.c_str(),fontSize) > rect.w - 8 )
			{
				const size_t space = line.find_last_of(' ');
				if( space != std::basic_string<char16>::npos && space > 0 )
				{ lines.push_back(line.substr(0,space)); line.erase(0,space+1); }
				else { line.pop_back(); lines.push_back(line); line.assign(1,ch); }
			}
		}
		if( !line.empty() ) lines.push_back(line);
		const float leading = fontSize * 1.2f;
		const size_t count = std::min(lines.size(),(size_t)std::max(1,(int)(rect.h / leading)));
		float y = rect.y + std::max(0.0f,(rect.h - count * leading) * .5f);
		for( size_t i = 0; i < count; ++i, y += leading )
			DrawText(rect.x + (centered ? (rect.w - TextWidth(lines[i].c_str(),fontSize)) * .5f : 4),
				y,lines[i].c_str(),fontSize,color);
	}

	std::string DecodeHtmlText( const std::string &source )
	{
		std::string text;
		text.reserve( source.size() );
		bool inTag = false;
		for( size_t i = 0; i < source.size(); ++i )
		{
			if( source[ i ] == '<' )
			{
				if( source.compare(i,3,"<br") == 0 || source.compare(i,3,"<BR") == 0 )
					text += '\n';
				inTag = true; continue;
			}
			if( source[ i ] == '>' ) { inTag = false; continue; }
			if( inTag ) continue;
			if( source[ i ] == '&' )
			{
				const size_t end = source.find( ';', i + 1 );
				if( end != std::string::npos && end - i < 12 )
				{
					const std::string entity = source.substr( i + 1, end - i - 1 );
					if( entity == "amp" ) text += '&';
					else if( entity == "lt" ) text += '<';
					else if( entity == "gt" ) text += '>';
					else if( entity == "quot" ) text += '"';
					else if( entity == "apos" ) text += '\'';
					else if( entity == "nbsp" ) text += ' ';
					else text.append( source, i, end - i + 1 );
					i = end;
					continue;
				}
			}
			const unsigned char ch = (unsigned char)source[ i ];
			if( std::isspace( ch ) )
			{
				if( !text.empty() && text[ text.size() - 1 ] != ' ' ) text += ' ';
			}
			else text += source[ i ];
		}
		while( !text.empty() && text[ 0 ] == ' ' ) text.erase( text.begin() );
		while( !text.empty() && text[ text.size() - 1 ] == ' ' )
			text.erase( text.size() - 1 );
		return text;
	}

	std::string HtmlTextById( const std::string &html, const char *id )
	{
		return DecodeHtmlText(WiiHtmlElementMarkup(html,id));
	}

	std::string HtmlLinkById( const std::string &html, const char *id )
	{
		const std::string marker = std::string( "id=\"" ) + id + "\"";
		const size_t idAt = html.find( marker );
		if( idAt == std::string::npos ) return std::string();
		const size_t tagStart = html.rfind( '<', idAt );
		const size_t tagEnd = html.find( '>', idAt );
		if( tagStart == std::string::npos || tagEnd == std::string::npos )
			return std::string();
		// Nintendo places the id on a wrapping DIV and href on its child A.
		// Search to the end of that element, not only the opening DIV tag.
		const size_t close = html.find( "</div>", tagEnd + 1 );
		const size_t searchEnd = close == std::string::npos ? tagEnd : close;
		const size_t href = html.find( "href=\"", tagStart );
		if( href == std::string::npos || href > searchEnd ) return std::string();
		const size_t value = href + 6;
		const size_t end = html.find( '"', value );
		return end == std::string::npos || end > searchEnd ? std::string()
			: html.substr( value, end - value );
	}

	void HtmlListLabels( const std::string &html,
		std::vector<std::string> &labels )
	{
		labels.clear();
		for( const WiiHtmlCell &cell : WiiHtmlTableCells(html) )
		{
			// Nintendo also uses class="List" on explanatory paragraphs. They
			// must never become short clickable rows or render a second time.
			if( !cell.listButton || labels.size() >= 8 ) continue;
			const std::string value = DecodeHtmlText(cell.markup);
			if( !value.empty() ) labels.push_back(value);
		}
	}

	void HtmlFallbackRows( const std::string &html,
		std::vector<std::string> &labels, std::vector<std::string> &links )
	{
		// Some Wii Settings documents are not List01/List02 menus.  Preserve
		// their ordinary HTML anchors so the read-only navigator can still walk
		// the actual NAND document tree instead of showing an invented blank page.
		size_t at = 0;
		while( labels.size() < 8 )
		{
			const size_t open = html.find( "<a", at );
			if( open == std::string::npos ) break;
			const size_t tagEnd = html.find( '>', open );
			const size_t close = tagEnd == std::string::npos ? std::string::npos
				: html.find( "</a>", tagEnd + 1 );
			if( close == std::string::npos ) break;
			const std::string tag = html.substr( open, tagEnd - open + 1 );
			const std::string text = DecodeHtmlText(
				html.substr( tagEnd + 1, close - tagEnd - 1 ) );
			const size_t hrefAt = tag.find( "href=\"" );
			std::string link;
			if( hrefAt != std::string::npos )
			{
				const size_t value = hrefAt + 6;
				const size_t end = tag.find( '"', value );
				if( end != std::string::npos ) link = tag.substr( value, end - value );
			}
			const bool chrome = tag.find( "id=\"UnderL\"" ) != std::string::npos
				|| tag.find( "id=\"Lbtn\"" ) != std::string::npos
				|| tag.find( "id=\"Rbtn\"" ) != std::string::npos;
			if( !chrome && !text.empty() && text != "1" && text != "2"
				&& text != "3" && link.find( "javascript:" ) != 0
				&& std::find( labels.begin(), labels.end(), text ) == labels.end() )
			{
				labels.push_back( text );
				links.push_back( link );
			}
			at = close + 4;
		}

		// Informational HTML pages contain paragraphs but no links.  Render their
		// real text as non-activating rows rather than fabricating controls.
		static const char *tags[] = { "<h1", "<h2", "<p" };
		if( labels.empty() ) for( u32 kind = 0;
			kind < sizeof( tags ) / sizeof( tags[ 0 ] ); ++kind )
		{
			at = 0;
			while( labels.size() < 8 )
			{
				const size_t open = html.find( tags[ kind ], at );
				if( open == std::string::npos ) break;
				const size_t start = html.find( '>', open );
				const size_t close = start == std::string::npos
					? std::string::npos : html.find( "</", start + 1 );
				if( close == std::string::npos ) break;
				const std::string text = DecodeHtmlText(
					html.substr( start + 1, close - start - 1 ) );
				if( text.size() > 1
					&& std::find( labels.begin(), labels.end(), text ) == labels.end() )
				{
					labels.push_back( text );
					links.push_back( std::string() );
				}
				at = close + 2;
			}
		}
	}

	std::string HtmlTitle( const std::string &html )
	{
		std::string lower = html;
		for( size_t i = 0; i < lower.size(); ++i )
			lower[ i ] = (char)std::tolower( (unsigned char)lower[ i ] );
		const size_t startTag = lower.find( "<title" );
		const size_t start = startTag == std::string::npos ? std::string::npos
			: lower.find( '>', startTag );
		const size_t end = start == std::string::npos ? std::string::npos
			: lower.find( "</title>", start );
		return end == std::string::npos ? std::string()
			: DecodeHtmlText( html.substr( start + 1, end - start - 1 ) );
	}

	std::string ResolveHtmlLink( const std::string &current,
		const std::string &link )
	{
		if( link.empty() || link == "#" || link.find( "javascript:" ) == 0
			|| link.find( "javaScript:" ) == 0 ) return std::string();
		std::string clean = link;
		const size_t suffix = clean.find_first_of( "?#" );
		if( suffix != std::string::npos ) clean.erase( suffix );
		if( clean.empty() ) return std::string();
		std::string combined;
		if( clean[ 0 ] == '/' ) combined = clean;
		else
		{
			const size_t slash = current.find_last_of( '/' );
			combined = ( slash == std::string::npos ? "/"
				: current.substr( 0, slash + 1 ) ) + clean;
		}
		std::vector<std::string> parts;
		size_t start = 0;
		while( start < combined.size() )
		{
			while( start < combined.size() && combined[ start ] == '/' ) ++start;
			const size_t end = combined.find( '/', start );
			const std::string part = combined.substr( start,
				end == std::string::npos ? std::string::npos : end - start );
			if( part == ".." ) { if( !parts.empty() ) parts.pop_back(); }
			else if( !part.empty() && part != "." ) parts.push_back( part );
			if( end == std::string::npos ) break;
			start = end + 1;
		}
		std::string result;
		for( size_t i = 0; i < parts.size(); ++i ) result += "/" + parts[ i ];
		return result;
	}

	void AppendSystemStylesheet( U8Archive &archive, const std::string &path,
		std::string &document, std::vector<std::string> &visited, int depth )
	{
		if( path.empty() || depth > 6
			|| std::find( visited.begin(), visited.end(), path ) != visited.end() )
			return;
		visited.push_back( path );
		u32 size = 0;
		u8 *data = archive.GetFileAllocated( path.c_str(), &size );
		if( !data || !size ) { free( data ); return; }
		const std::string css( (const char *)data, size );
		free( data );

		// CSS @imports are relative to the stylesheet, not the HTML document.
		// Load them first so page-specific rules which follow retain precedence.
		size_t at = 0;
		while( true )
		{
			at = css.find( "@import", at );
			if( at == std::string::npos ) break;
			const size_t url = css.find( "url(", at );
			if( url == std::string::npos ) break;
			size_t start = url + 4;
			while( start < css.size() && ( css[ start ] == ' '
				|| css[ start ] == '\'' || css[ start ] == '"' ) ) ++start;
			size_t end = css.find( ')', start );
			if( end == std::string::npos ) break;
			while( end > start && ( css[ end - 1 ] == ' '
				|| css[ end - 1 ] == '\'' || css[ end - 1 ] == '"' ) ) --end;
			AppendSystemStylesheet( archive, ResolveHtmlLink( path,
				css.substr( start, end - start ) ), document, visited, depth + 1 );
			at = end + 1;
		}
		document += "\n";
		// Merged CSS still resolves artwork relative to its own document, not
		// the HTML page. Otherwise deep pages silently lose their images.
		std::string rebased;
		size_t copied = 0;
		while( true )
		{
			const size_t url = css.find("url(",copied);
			if( url == std::string::npos ) break;
			const size_t close = css.find(')',url + 4);
			if( close == std::string::npos ) break;
			size_t start = url + 4, end = close;
			while( start < end && (css[start] == ' ' || css[start] == '\'' || css[start] == '"') ) ++start;
			while( end > start && (css[end-1] == ' ' || css[end-1] == '\'' || css[end-1] == '"') ) --end;
			rebased.append(css,copied,url-copied);
			rebased += "url(\"" + ResolveHtmlLink(path,css.substr(start,end-start)) + "\")";
			copied = close + 1;
		}
		rebased.append(css,copied,std::string::npos);
		document += rebased;
	}

	std::string CssRule( const std::string &html, const std::string &selector )
	{
		size_t marker = html.rfind(selector);
		while( marker != std::string::npos )
		{
			const size_t next = marker + selector.size();
			if( next < html.size() && (html[next] == '{' || std::isspace((unsigned char)html[next])) ) break;
			marker = marker ? html.rfind(selector,marker-1) : std::string::npos;
		}
		if( marker == std::string::npos ) return std::string();
		const size_t open = html.find( '{', marker + selector.size() );
		const size_t close = open == std::string::npos ? std::string::npos
			: html.find( '}', open + 1 );
		return close == std::string::npos ? std::string()
			: html.substr( open + 1, close - open - 1 );
	}

	float CssPixel( const std::string &rule, const char *property,
		float fallback )
	{
		const size_t at = rule.rfind( property );
		if( at == std::string::npos ) return fallback;
		const size_t colon = rule.find( ':', at + strlen( property ) );
		if( colon == std::string::npos ) return fallback;
		return (float)atof( rule.c_str() + colon + 1 );
	}

	std::string CssImage( const std::string &rule )
	{
		const size_t url = rule.find( "url(" );
		if( url == std::string::npos ) return std::string();
		size_t start = url + 4;
		while( start < rule.size()
			&& ( rule[ start ] == ' ' || rule[ start ] == '\''
				|| rule[ start ] == '"' ) ) ++start;
		size_t end = rule.find( ')', start );
		if( end == std::string::npos ) return std::string();
		while( end > start && ( rule[ end - 1 ] == ' '
			|| rule[ end - 1 ] == '\'' || rule[ end - 1 ] == '"' ) ) --end;
		return rule.substr( start, end - start );
	}

	std::string HtmlElementFragment( const std::string &html, const char *id )
	{
		return WiiHtmlElementMarkup(html,id);
	}

	std::string HtmlAttribute( const std::string &fragment, const char *name )
	{
		const std::string marker = std::string( name ) + "=\"";
		const size_t at = fragment.find( marker );
		if( at == std::string::npos ) return std::string();
		const size_t start = at + marker.size();
		const size_t end = fragment.find( '"', start );
		return end == std::string::npos ? std::string()
			: fragment.substr( start, end - start );
	}

	std::string HtmlHoverImage( const std::string &fragment )
	{
		const size_t swap = fragment.find( "MM_swapImage(" );
		if( swap == std::string::npos ) return std::string();
		const size_t marker = fragment.find( "','','", swap );
		if( marker == std::string::npos ) return std::string();
		const size_t start = marker + 6;
		const size_t end = fragment.find( '\'', start );
		return end == std::string::npos ? std::string()
			: fragment.substr( start, end - start );
	}

	void HtmlClassTexts( const std::string &html, const char *className,
		std::vector<std::string> &texts )
	{
		texts.clear();
		const std::string marker = std::string( "class=\"" ) + className;
		size_t at = 0;
		while( texts.size() < 8 )
		{
			at = html.find( marker, at );
			if( at == std::string::npos ) break;
			const size_t start = html.find( '>', at + marker.size() );
			const size_t end = start == std::string::npos ? std::string::npos
				: html.find( "</", start + 1 );
			if( end == std::string::npos ) break;
			const std::string text = DecodeHtmlText(
				html.substr( start + 1, end - start - 1 ) );
			if( !text.empty() ) texts.push_back( text );
			at = end + 2;
		}
	}

	const char *BaseName( const std::string &path )
	{
		const size_t slash = path.find_last_of( "/\\" );
		return slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1;
	}

	bool HasImageExtension( const char *name )
	{
		if( !name )
			return false;
		const char *dot = strrchr( name, '.' );
		return dot && ( !strcasecmp( dot, ".png" ) || !strcasecmp( dot, ".jpg" )
			|| !strcasecmp( dot, ".jpeg" ) );
	}

	bool HasThemeExtension( const char *name )
	{
		if( !name ) return false;
		const char *dot = strrchr( name, '.' );
		return dot && ( !strcasecmp( dot, ".app" )
			|| !strcasecmp( dot, ".csm" ) || !strcasecmp( dot, ".mym" ) );
	}

	void AddThemeDirectory( const std::string &folder,
		std::vector<std::string> &paths )
	{
		DIR *dir = opendir( folder.c_str() );
		if( !dir ) return;
		struct dirent *entry;
		while( paths.size() < 64 && ( entry = readdir( dir ) ) != NULL )
		{
			if( entry->d_name[ 0 ] == '.' || !HasThemeExtension( entry->d_name ) )
				continue;
			std::string path = folder;
			if( !path.empty() && path[ path.size() - 1 ] != '/' ) path += '/';
			path += entry->d_name;
			struct stat info;
			if( stat( path.c_str(), &info ) || ( info.st_mode & S_IFDIR ) ) continue;
			if( std::find( paths.begin(), paths.end(), path ) == paths.end() )
				paths.push_back( path );
		}
		closedir( dir );
	}

	bool IsCompiledTheme( const std::string &path )
	{
		if( path.empty() ) return true;
		U8FileArchive archive( path.c_str() );
		return archive.FileDescriptor( "/layout/common/chanSel.ash" )
			&& archive.FileDescriptor( "/layout/common/board.ash" )
			&& archive.FileDescriptor( "/layout/common/chanTtl.ash" )
			&& archive.FileDescriptor( "/layout/common/cmnBtn.ash" )
			&& archive.FileDescriptor( "/layout/common/cursor.ash" );
	}

	bool AddImageDirectory( const std::string &folder, int depth, int &directories,
		int &entries, std::vector<std::string> &paths )
	{
		if( depth < 0 || directories >= 96 || entries >= 2048
			|| paths.size() >= 65 )
			return false;
		DIR *dir = opendir( folder.c_str() );
		if( !dir )
			return false;
		++directories;
		struct dirent *entry;
		while( entries < 2048 && ( entry = readdir( dir ) ) != NULL
			&& paths.size() < 65 )
		{
			if( entry->d_name[ 0 ] == '.' )
				continue;
			++entries;
			std::string path = folder;
			if( !path.empty() && path[ path.size() - 1 ] != '/' )
				path += '/';
			path += entry->d_name;
			if( path.size() >= ChannelPreview::MaxPathBytes )
				continue;
			struct stat info;
			if( stat( path.c_str(), &info ) != 0 )
				continue;
			if( info.st_mode & S_IFDIR )
			{
				if( depth > 0 )
					AddImageDirectory( path, depth - 1, directories, entries,
						paths );
			}
			else if( HasImageExtension( entry->d_name )
				&& std::find( paths.begin(), paths.end(), path ) == paths.end() )
			{
				paths.push_back( path );
			}
		}
		closedir( dir );
		return true;
	}

	const char *WeatherName( u16 code )
	{
		switch( code )
		{
		case ChannelPreview::ForecastWeatherCloudy: return "Cloudy";
		case ChannelPreview::ForecastWeatherMostlyCloudy: return "Mostly cloudy";
		case ChannelPreview::ForecastWeatherFog: return "Fog";
		case ChannelPreview::ForecastWeatherShowers: return "Showers";
		case ChannelPreview::ForecastWeatherRain: return "Rain";
		case ChannelPreview::ForecastWeatherMostlyCloudyShowers:
			return "Mostly cloudy with showers";
		case ChannelPreview::ForecastWeatherThunder: return "Thunder";
		case ChannelPreview::ForecastWeatherPartlySunnyThunder:
			return "Partly sunny with thunderstorms";
		case ChannelPreview::ForecastWeatherMostlyCloudyThunder:
			return "Mostly cloudy with thunderstorms";
		case ChannelPreview::ForecastWeatherSnow: return "Snow";
		case ChannelPreview::ForecastWeatherPartlySunnyFlurries:
			return "Partly sunny with flurries";
		case ChannelPreview::ForecastWeatherRainAndSnow: return "Rain and snow";
		case ChannelPreview::ForecastWeatherSleet: return "Sleet";
		case 0x0592: return "Snow"; // Compatibility with an older saved value.
		case ChannelPreview::ForecastWeatherPartlyCloudy: return "Partly cloudy";
		case ChannelPreview::ForecastWeatherPartlySunny: return "Partly sunny";
		case ChannelPreview::ForecastWeatherPartlySunnyRain:
			return "Partly sunny with rain";
		default: return "Sunny";
		}
	}

	int WeatherIndex( u16 code )
	{
		// Migrate the older 0x0592 saved value to the corrected Snow choice.
		if( code == 0x0592 )
			code = ChannelPreview::ForecastWeatherSnow;
		for( int i = 0; i < ChannelPreview::ForecastWeatherChoiceCount; ++i )
			if( ChannelPreview::ForecastWeatherChoices[ i ] == code )
				return i;
		return 0;
	}

	void ConvertForecastTemperature( std::string &text,
		ChannelPreview::Unit from, ChannelPreview::Unit to )
	{
		if( from == to ) return;
		int value = 0;
		char trailing = 0;
		// Convert only a plain numeric setting. Deliberately preserve custom
		// strings such as "VERY HOT" or values with their own suffix.
		if( sscanf( text.c_str(), " %d %c", &value, &trailing ) != 1 )
			return;
		const float converted = to == ChannelPreview::Fahrenheit
			? value * 9.0f / 5.0f + 32.0f
			: ( value - 32.0f ) * 5.0f / 9.0f;
		char buffer[ ChannelPreview::MaxTemperatureBytes ];
		snprintf( buffer, sizeof( buffer ), "%d", (int)roundf( converted ) );
		text = buffer;
	}

	const char *ForecastTimeName( ChannelPreview::ForecastTimeMode mode )
	{
		switch( mode )
		{
		case ChannelPreview::ForecastTimeNight: return "Night";
		case ChannelPreview::ForecastTimeAutomatic: return "Automatic";
		default: return "Day";
		}
	}

}

AppSettingsScreen::AppSettingsScreen()
	: systemSettingsExitPending( false ), phase( Closed ), dropdown( DropdownNone ), page( PageMain ),
	  returnPage( PageMain ), editTarget( EditNone ), transitionFrame( 0 ),
	  selectedRow( 0 ), firstVisibleRow( 0 ), hoveredRow( -1 ), dropdownFocus( 0 ),
	  draggingChannel( -1 ), draggingScrollbar( -1 ), scrollbarGrabOffset( 0.0f ),
	  closeHovered( false ),
	  navigationMode( false ),
	  pointerWasValid( false ), lastPointerX( 0.0f ), lastPointerY( 0.0f ),
	  settingsDirty( false ), saveFailed( false ),
	  channelRefreshRequested( false ),
	  themeApplyFrames( -1 ), themeRestartError( 0 ),
	  openedFromSystemSettings( false ), systemSettingsAvailable( false ),
	  systemSettingsPage( 0 ), systemSettingsColumns( 1 ),
	  systemSettingsArchiveData( NULL ), systemSettingsArchiveSize( 0 ),
	  systemSettingsPageFrames( 0 ),
	  fakeUpdatePercent( -1 ), fakeOobeActive( false ),
	  newsArticleIndex( 0 ),
	  keyboardRow( 0 ), keyboardColumn( 0 ), keyboardHover( -1 ),
	  keyboardShift( false ), keyboardAddingNewsArticle( false ),
	  keyboardPreviousNewsIndex( 0 ), imageIndex( 0 ), imageHover( -1 ),
	  imageTarget( ImagePhoto ), forecastImageRejected( false ), imageScanHadRoot( false )
{
	systemSettingsTitleRect = (HtmlRect){ 32.0f, 27.0f, 384.0f, 36.0f };
	systemSettingsBackSprite.rect = (HtmlRect){ 28.0f, 371.0f, 272.0f, 72.0f };
	systemSettingsConfirmSprite.rect = (HtmlRect){ 308.0f, 371.0f, 272.0f, 72.0f };
	systemSettingsMiddleSprite.rect = (HtmlRect){ 215.0f, 371.0f, 176.0f, 72.0f };
	systemSettingsLeftSprite.rect = (HtmlRect){ 22.0f, 180.0f, 72.0f, 72.0f };
	systemSettingsRightSprite.rect = (HtmlRect){ 514.0f, 180.0f, 72.0f, 72.0f };
	systemSettingsUpSprite.rect = (HtmlRect){ 514.0f, 94.0f, 72.0f, 72.0f };
	systemSettingsDownSprite.rect = (HtmlRect){ 514.0f, 266.0f, 72.0f, 72.0f };
}

AppSettingsScreen::~AppSettingsScreen()
{
	delete systemUpdateDialog;
	delete agreementViewer;
	ClearSystemHtmlTextures();
}

void AppSettingsScreen::ResetSystemSettingsPage()
{
	const std::string relative = SystemSettingsRelativePath(
		systemSettingsPath, systemSettingsRoot);
	systemSettingsPageFrames = 0;
	fakeUpdatePercent = relative == "update/update_common.html" ? 0 : -1;
	if( relative == "setup/startup_index1.html" ) fakeOobeActive = true;
}

void AppSettingsScreen::Open()
{
	systemSettingsExitPending = false;
	dropdown = DropdownNone;
	page = PageMain;
	openedFromSystemSettings = false;
	returnPage = PageMain;
	editTarget = EditNone;
	transitionFrame = 0;
	selectedRow = 0;
	firstVisibleRow = 0;
	hoveredRow = -1;
	dropdownFocus = 0;
	draggingChannel = -1;
	draggingScrollbar = -1;
	closeHovered = false;
	navigationMode = false;
	pointerWasValid = false;
	pointerHoverActive = false;
	hoveredScrollButton = -1;
	keyboardBuffer.clear();
	imagePaths.clear();
	themePaths.clear();
	themeCompiled.clear();
	channelRefreshRequested = false;
	Commit();
	phase = Opening;
}

void AppSettingsScreen::LoadSystemSettingsArchive( const u8 *data, u32 size )
{
	ClearSystemHtmlTextures();
	systemSettingsAvailable = false;
	systemSettingsRoot.clear();
	systemSettingsPath.clear();
	systemSettingsTitle.clear();
	systemSettingsLabels.clear();
	systemSettingsLinks.clear();
	systemSettingsHistory.clear();
	systemSettingsBackLink.clear();
	systemSettingsBackLabel.clear();
	systemSettingsConfirmLink.clear();
	systemSettingsMiddleLink.clear();
	systemSettingsLeftLink.clear();
	systemSettingsRightLink.clear();
	systemSettingsUpLink.clear();
	systemSettingsDownLink.clear();
	systemSettingsArchiveData = data;
	systemSettingsArchiveSize = size;
	if( !data || !size ) return;

	U8Archive archive( data, size );
	const char *roots[] = {
		// Nintendo's later archives use FIX/<region>/<language>. Older Menu
		// revisions use the duplicated region directory. Probe both layouts.
		"/FIX/EU/ENG", "/FIX/EU/GER", "/FIX/EU/FRA", "/FIX/EU/SPA",
		"/FIX/EU/ITA", "/FIX/EU/DUT", "/FIX/US/ENG", "/FIX/US/FRA",
		"/FIX/US/SPA", "/FIX/JP/JPN", "/FIX/KR/KOR",
		"/EU/EU/ENG", "/EU/EU/GER", "/EU/EU/FRA", "/EU/EU/SPA",
		"/EU/EU/ITA", "/EU/EU/DUT", "/US/US/ENG", "/JP/JP/JPN",
		"/KR/KR/KOR"
	};
	const char *language = CONF_GetLanguageString();
	if( !strcmp( language, "NED" ) ) language = "DUT";
	int preferred = -1;
	for( u32 i = 0; i < sizeof( roots ) / sizeof( roots[ 0 ] ); ++i )
		if( strstr( roots[ i ], language ) ) preferred = (int)i;
	for( int pass = 0; pass < 2 && !systemSettingsAvailable; ++pass )
	{
		for( u32 i = 0; i < sizeof( roots ) / sizeof( roots[ 0 ] ); ++i )
		{
			if( ( pass == 0 ) != ( (int)i == preferred ) ) continue;
			char candidate[ 96 ];
			snprintf( candidate, sizeof( candidate ), "%s/index01.html", roots[ i ] );
			u32 htmlSize = 0;
			u8 *html = archive.GetFileAllocated( candidate, &htmlSize );
			if( !html ) continue;
			free( html );
			systemSettingsRoot = roots[ i ];
			systemSettingsAvailable = true;
			break;
		}
	}
	if( !systemSettingsAvailable ) return;

	systemSettingsAvailable = LoadSystemHtmlPage(
		systemSettingsRoot + "/index01.html", false );
	gprintf( "Wii Settings HTML root: %s\n", systemSettingsRoot.c_str() );
}

void AppSettingsScreen::ClearSystemHtmlTextures()
{
	// Previous page commands can still reference these allocations on GX.
	if( !systemSettingsTextures.empty() ) GX_DrawDone();
	for( size_t i = 0; i < systemSettingsTextures.size(); ++i )
	{
		delete systemSettingsTextures[ i ].texture;
		free( systemSettingsTextures[ i ].pixels );
	}
	systemSettingsTextures.clear();
}

bool AppSettingsScreen::LoadSystemHtmlTexture( const std::string &path )
{
	if( path.empty() ) return false;
	for( size_t i = 0; i < systemSettingsTextures.size(); ++i )
		if( systemSettingsTextures[ i ].path == path ) return true;
	if( !systemSettingsArchiveData || !systemSettingsArchiveSize ) return false;
	U8Archive archive( systemSettingsArchiveData, systemSettingsArchiveSize );
	u32 size = 0;
	u8 *encoded = archive.GetFileAllocated( path.c_str(), &size );
	if( !encoded || size < 4 ) { free( encoded ); return false; }
	gdImagePtr image = NULL;
	if( size >= 8 && !memcmp( encoded, "\x89PNG\r\n\x1a\n", 8 ) )
		image = gdImageCreateFromPngPtr( size, encoded );
	else if( size >= 6 && ( !memcmp( encoded, "GIF87a", 6 )
		|| !memcmp( encoded, "GIF89a", 6 ) ) )
		image = gdImageCreateFromGifPtr( size, encoded );
	else if( encoded[ 0 ] == 0xff && encoded[ 1 ] == 0xd8 )
		image = gdImageCreateFromJpegPtr( size, encoded );
	free( encoded );
	if( !image ) return false;
	int width = 0;
	int height = 0;
	u8 *pixels = GDImageToRGBA8( &image, &width, &height );
	gdImageDestroy( image );
	if( !pixels || width <= 0 || height <= 0 || width > 1024 || height > 1024 )
	{
		free( pixels );
		return false;
	}
	Texture *texture = new Texture;
	if( !texture ) { free( pixels ); return false; }
	texture->LoadFromRawData( pixels, width, height, GX_TF_RGBA8 );
	if( !texture->IsLoaded() )
	{
		delete texture;
		free( pixels );
		return false;
	}
	HtmlTexture resource = { path, texture, pixels, width, height };
	systemSettingsTextures.push_back( resource );
	return true;
}

void AppSettingsScreen::ParseSystemHtmlArtwork( const std::string &html )
{
	ClearSystemHtmlTextures();
	systemSettingsBackgroundImage.clear();
	systemSettingsDecorations.clear();
	systemSettingsRowSprites.clear();
	systemSettingsControls.clear();
	systemSettingsContent.clear();
	systemSettingsConfirmLabel.clear();

	auto sprite = [&]( const char *id, float fallbackX, float fallbackY,
		float fallbackW, float fallbackH ) -> HtmlSprite
	{
		HtmlSprite result;
		result.id = id;
		const std::string rule = CssRule( html, std::string( "#" ) + id );
		const std::string fragment = HtmlElementFragment( html, id );
		result.rect.x = CssPixel( rule, "left", fallbackX );
		result.rect.y = CssPixel( rule, "top", fallbackY );
		result.rect.w = CssPixel( rule, "width", fallbackW );
		result.rect.h = CssPixel( rule, "height", fallbackH );
		std::string image = HtmlAttribute( fragment, "src" );
		if( image.empty() ) image = CssImage( rule );
		std::string hover = HtmlHoverImage( fragment );
		result.image = ResolveHtmlLink( systemSettingsPath, image );
		result.hoverImage = ResolveHtmlLink( systemSettingsPath, hover );
		if( !result.image.empty() ) LoadSystemHtmlTexture( result.image );
		if( !result.hoverImage.empty() ) LoadSystemHtmlTexture( result.hoverImage );
		return result;
	};

	const std::string bodyImage = CssImage( CssRule( html, "body" ) );
	systemSettingsBackgroundImage = ResolveHtmlLink( systemSettingsPath, bodyImage );
	LoadSystemHtmlTexture( systemSettingsBackgroundImage );

	// These are the authored visual layers used by Nintendo's pages. Their
	// dimensions, positions and image paths come from this page's CSS/HTML.
	const char *decorationIds[] = { "BnrGray", "BnrWhite", "BnrIcon",
		"Page01Icon", "Page02Icon", "Page03Icon", "ListIcon01",
		"ListIcon02", "ListIcon03", "ListIcon04" };
	for( u32 i = 0; i < sizeof( decorationIds ) / sizeof( decorationIds[ 0 ] ); ++i )
	{
		HtmlSprite item = sprite( decorationIds[ i ], 0, 0, 0, 0 );
		if( !item.image.empty() && item.rect.w > 0 && item.rect.h > 0 )
			systemSettingsDecorations.push_back( item );
	}
	// Deeper pages have their own illustrations (TV, Sensor Bar, Internet,
	// etc.). Keep those applied-archive assets as well as the common chrome.
	size_t divAt = 0;
	while( systemSettingsDecorations.size() < 96 )
	{
		const size_t open = html.find("<div",divAt);
		if( open == std::string::npos ) break;
		const size_t end = html.find('>',open);
		if( end == std::string::npos ) break;
		divAt = end + 1;
		const std::string id = HtmlAttribute(html.substr(open,end-open+1),"id");
		if( id.empty() || id.find("List") == 0 || id.find("Under") == 0
			|| id.find("myUnder") == 0 || id == "BnrName" ) continue;
		bool known = false;
		for( const HtmlSprite &item : systemSettingsDecorations ) if( item.id == id ) known = true;
		if( known ) continue;
		const std::string rule = CssRule(html,"#"+id);
		if( rule.find("visibility: hidden") != std::string::npos || rule.find("display:none") != std::string::npos ) continue;
		HtmlSprite item = sprite(id.c_str(),0,0,0,0);
		if( !item.image.empty() && item.rect.w > 0 && item.rect.h > 0 ) systemSettingsDecorations.push_back(item);
	}

	// Some native controls (TV previews and connection cards) have labels in
	// DIVs, not button-backed table cells. Discover their authored anchors too.
	for( size_t i = 0; i < 8; ++i )
	{
		char id[ 16 ];
		snprintf( id, sizeof( id ), "List%02d", (int)i + 1 );
		const std::string fragment = HtmlElementFragment(html,id);
		if( i >= systemSettingsLabels.size() && fragment.empty() ) break;
		if( i >= systemSettingsLabels.size() ) systemSettingsLabels.resize(i+1);
		if( i >= systemSettingsLinks.size() )
			systemSettingsLinks.push_back(ReadOnlyWiiSettingsRoute(systemSettingsPath,
				systemSettingsRoot,id,HtmlLinkById(html,id)));
		const float y = systemSettingsLabels.size() <= 3
			? 110.0f + i * 96.0f : 78.0f + i * 72.0f;
		systemSettingsRowSprites.push_back( sprite( id, 104.0f, y,
			400.0f, systemSettingsLabels.size() <= 3 ? 70.0f : 60.0f ) );
	}

	// Nintendo's deeper pages use many named anchors beyond List01/UnderL:
	// connection slots, rating choices, setup choices and other submenus.  Keep
	// their authored CSS rectangles and links so every screenshot control which
	// leads to another HTML document is actually clickable.
	size_t anchorAt = 0;
	while( true )
	{
		const size_t open = html.find( "<a", anchorAt );
		if( open == std::string::npos ) break;
		const size_t close = html.find( '>', open );
		if( close == std::string::npos ) break;
		const std::string tag = html.substr( open, close - open + 1 );
		const std::string id = HtmlAttribute( tag, "id" );
		const std::string authoredLink = HtmlAttribute( tag, "href" );
		const std::string link = ReadOnlyWiiSettingsRoute( systemSettingsPath,
			systemSettingsRoot, id, authoredLink );
		const bool known = id == "UnderL" || id == "UnderR"
			|| id == "UnderM" || id == "Lbtn" || id == "Rbtn"
			|| id == "ListUP" || id == "ListDW"
			|| ( id.size() == 6 && id.find( "List" ) == 0
				&& std::isdigit( (unsigned char)id[ 4 ] )
				&& std::isdigit( (unsigned char)id[ 5 ] ) );
		if( !id.empty() && !known )
		{
			HtmlControl control;
			control.sprite = sprite( id.c_str(), 0, 0, 0, 0 );
			control.link = link;
			control.label = HtmlTextById(html,id.c_str());
			if( control.sprite.rect.w > 0 && control.sprite.rect.h > 0 )
				systemSettingsControls.push_back( control );
		}
		anchorAt = close + 1;
	}

	systemSettingsBackSprite = sprite( "UnderL", 28, 371, 272, 72 );
	systemSettingsConfirmSprite = sprite( "UnderR", 308, 371, 272, 72 );
	systemSettingsMiddleSprite = sprite( "UnderM", 215, 371, 176, 72 );
	systemSettingsLeftSprite = sprite( "Lbtn", 22, 180, 72, 72 );
	systemSettingsRightSprite = sprite( "Rbtn", 514, 180, 72, 72 );
	systemSettingsUpSprite = sprite( "ListUP", 514, 94, 72, 72 );
	systemSettingsDownSprite = sprite( "ListDW", 514, 266, 72, 72 );
	for( const WiiHtmlCell &cell : WiiHtmlTableCells(html) )
		if( cell.cssClass.find("Under") != std::string::npos )
		{
			const std::string label = DecodeHtmlText(cell.markup);
			const float center = cell.x + cell.w * .5f;
			const float back = systemSettingsBackSprite.rect.x + systemSettingsBackSprite.rect.w * .5f;
			const float confirm = systemSettingsConfirmSprite.rect.x + systemSettingsConfirmSprite.rect.w * .5f;
			if( std::fabs(center-back) < std::fabs(center-confirm) ) systemSettingsBackLabel = label;
			else systemSettingsConfirmLabel = label;
		}
	// JavaScript-era pages put footer words in their own overlay DIVs.
	const std::string leftLabel = HtmlTextById(html,"myUnderLBtn");
	const std::string rightLabel = HtmlTextById(html,"myUnderRBtn");
	if( !leftLabel.empty() ) systemSettingsBackLabel = leftLabel;
	if( !rightLabel.empty() ) systemSettingsConfirmLabel = rightLabel;
	// Match table labels to their authored button positions, not source order.
	const std::vector<WiiHtmlCell> tableCells = WiiHtmlTableCells(html);
	std::vector<bool> used(tableCells.size(),false);
	for( size_t row = 0; row < systemSettingsRowSprites.size(); ++row )
	{
		const HtmlRect &rect = systemSettingsRowSprites[row].rect;
		const size_t chosen = WiiHtmlButtonCell(tableCells,rect.x,rect.y,rect.w,rect.h,used);
		if( chosen < tableCells.size() )
		{ used[chosen] = true; systemSettingsLabels[row] = DecodeHtmlText(tableCells[chosen].markup); }
		// TV-preview anchors contain transparent hover maps. Their visible
		// frame is the centered table-cell background, not that anchor's PNG.
		for( const WiiHtmlCell &cell : tableCells )
		{
			if( cell.background.empty() ) continue;
			const float dx = cell.x + cell.w*.5f - rect.x - rect.w*.5f;
			const float dy = cell.y + cell.h*.5f - rect.y - rect.h*.5f;
			if( std::fabs(dx) <= 1 && std::fabs(dy) <= 1 )
			{
				systemSettingsRowSprites[row].backgroundImage = ResolveHtmlLink(systemSettingsPath,cell.background);
				LoadSystemHtmlTexture(systemSettingsRowSprites[row].backgroundImage);
				break;
			}
		}
	}

	const HtmlSprite *banner = NULL;
	for( size_t i = 0; i < systemSettingsDecorations.size(); ++i )
		if( systemSettingsDecorations[ i ].id == "BnrWhite" )
			banner = &systemSettingsDecorations[ i ];
	const std::string titleRule = CssRule( html, "#BnrName" );
	systemSettingsTitleRect.x = ( banner ? banner->rect.x : 24.0f )
		+ CssPixel( titleRule, "left", 8.0f );
	systemSettingsTitleRect.y = ( banner ? banner->rect.y : 27.0f )
		+ CssPixel( titleRule, "top", 0.0f );
	systemSettingsTitleRect.w = CssPixel( titleRule, "width", 384.0f );
	systemSettingsTitleRect.h = CssPixel( titleRule, "height", 36.0f );

	// The old atlas hid these authored table contents, including paragraphs
	// and JavaScript-populated date/time fields. Render them as content, not
	// invented clickable list rows. Settings remain a read-only preview.
	const time_t now = time(NULL);
	const tm date = *localtime(&now);
	for( const WiiHtmlCell &cell : WiiHtmlTableCells(html) )
	{
		if( cell.listButton
			|| cell.cssClass.find("Under") != std::string::npos
			|| cell.cssClass.find("Title_name") != std::string::npos
			|| cell.cssClass.find("page_index") != std::string::npos
			|| cell.markup.find("BnrName") != std::string::npos ) continue;
		std::string text = DecodeHtmlText(cell.markup);
		if( cell.markup.find("id=\"myMac\"") != std::string::npos )
		{
			u8 mac[6] = {};
			char value[32];
			if( net_get_mac_address(mac) >= 0 )
				snprintf(value,sizeof(value),"%02X-%02X-%02X-%02X-%02X-%02X",
					mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
			else snprintf(value,sizeof(value),"--:--:--:--:--:--");
			text += value;
		}
		if( cell.markup.find("id=\"myLanMac\"") != std::string::npos )
		{
			// Read Nintendo's translated caption from the native script; no JS
			// execution, network connection, NAND write or invented MAC address.
			U8Archive archive(systemSettingsArchiveData,systemSettingsArchiveSize);
			const std::string language = systemSettingsRoot.substr(systemSettingsRoot.find_last_of('/')+1);
			const std::string path = ResolveHtmlLink(systemSettingsPath,"../../../js/EU/"+language+"/iplMessage.js");
			u32 bytes = 0; u8 *data = archive.GetFileAllocated(path,&bytes);
			if( data )
			{
				const std::string script = WiiHtmlUtf8((const char *)data,bytes);
				const size_t function = script.find("function _msgMacAddress(");
				const size_t assignment = script.find("string2 =",function);
				const size_t begin = script.find('"',assignment);
				const size_t end = script.find('"',begin == std::string::npos ? begin : begin+1);
				if( begin != std::string::npos && end != std::string::npos )
					text = script.substr(begin+1,end-begin-1) + "\n--:--:--:--:--:--";
				free(data);
			}
		}
		if( cell.markup.find("id=\"box\"") != std::string::npos )
		{
			s8 offset = 0; CONF_GetDisplayOffsetH(&offset);
			char value[16]; snprintf(value,sizeof(value),offset ? "%+d" : "%d",(int)-offset/2);
			text = value;
		}
		const bool nicknameField = cell.markup.find("<input") != std::string::npos
			&& cell.markup.find("Nickname") != std::string::npos;
		if(nicknameField)
		{
			u8 nickname[22] = {};
			const s32 length = CONF_Get("IPL.NIK", nickname, sizeof(nickname));
			text = length > 0 ? WiiNicknameUtf8(nickname, length) : "";
		}
		const char *fields[] = {"draw_date","draw_month","draw_year","draw_hour","draw_minute"};
		const int values[] = {date.tm_mday,date.tm_mon+1,date.tm_year+1900,date.tm_hour,date.tm_min};
		for( int field = 0; field < 5; ++field )
			if( cell.markup.find(fields[field]) != std::string::npos )
			{
				char value[24]; snprintf(value,sizeof(value),field == 2 ? "%d" : "%02d",values[field]);
				text = value;
			}
		if( (!nicknameField && text.empty()) || cell.w <= 0 || cell.h <= 0 || cell.y >= 371 ) continue;
		std::string rule;
		std::string classes = cell.cssClass;
		const size_t paragraph = cell.markup.find("<p");
		if( paragraph != std::string::npos )
		{
			const size_t end = cell.markup.find('>',paragraph);
			if( end != std::string::npos ) classes += " " + HtmlAttribute(cell.markup.substr(paragraph,end-paragraph+1),"class");
		}
		const size_t span = cell.markup.find("<span");
		if( span != std::string::npos )
		{
			const size_t end = cell.markup.find('>',span);
			if( end != std::string::npos ) classes += " " + HtmlAttribute(cell.markup.substr(span,end-span+1),"class");
		}
		size_t at = 0;
		while( at < classes.size() )
		{
			const size_t end = classes.find(' ',at);
			if( end != at ) rule += CssRule(html,"." + classes.substr(at,end-at));
			if( end == std::string::npos ) break;
			at = end + 1;
		}
		HtmlContentText content;
		content.rect = (HtmlRect){cell.x,cell.y,cell.w,std::min(cell.h,371 - cell.y)};
		content.inputField = nicknameField;
		if(nicknameField)
		{
			rule += CssRule(html,".Nickname");
			const float width = std::min(440.0f, cell.w - 24);
			content.rect = (HtmlRect){cell.x + (cell.w-width)*0.5f,
				cell.y + (cell.h-60)*0.5f,width,60};
		}
		content.text = text;
		content.fontSize = std::max(12.0f,std::min(60.0f,CssPixel(rule,"font-size",24)));
		content.centered = nicknameField || cell.centered;
		content.bright = rule.find("#FFFFFF") != std::string::npos || rule.find("#ffffff") != std::string::npos;
		systemSettingsContent.push_back(content);
	}

	// Preserve authored standalone text (Update prompt, connection captions,
	// etc.). A DIV must not borrow a later sibling's IMG or table geometry.
	std::string lowerHtml = html;
	for( char &ch : lowerHtml ) ch = (char)std::tolower((unsigned char)ch);
	divAt = 0;
	while( systemSettingsContent.size() < 256 )
	{
		const size_t open = lowerHtml.find("<div",divAt);
		if( open == std::string::npos ) break;
		const size_t end = html.find('>',open);
		if( end == std::string::npos ) break;
		divAt = end+1;
		const std::string tag = html.substr(open,end-open+1);
		const std::string id = HtmlAttribute(tag,"id");
		if( id.empty() || id.find("Bnr") == 0 || id.find("List") == 0
			|| id.find("Under") == 0 || id.find("myUnder") == 0 ) continue;
		const std::string fragment = HtmlElementFragment(html,id.c_str());
		const std::string text = DecodeHtmlText(fragment);
		if( text.empty() ) continue;
		bool inTable = false;
		for( const WiiHtmlCell &cell : tableCells )
			if( cell.markup.find(fragment) != std::string::npos ) inTable = true;
		if( inTable ) continue;
		const std::string rule = CssRule(html,"#"+id);
		if( rule.find("visibility: hidden") != std::string::npos || rule.find("display:none") != std::string::npos ) continue;
		HtmlContentText content;
		content.rect = (HtmlRect){CssPixel(rule,"left",0),CssPixel(rule,"top",0),
			CssPixel(rule,"width",0),CssPixel(rule,"height",0)};
		if( content.rect.w <= 0 || content.rect.h <= 0 ) continue;
		content.text = text; content.fontSize = CssPixel(rule,"font-size",24);
		content.centered = true; content.bright = rule.find("#ffffff") != std::string::npos
			|| rule.find("#FFFFFF") != std::string::npos;
		content.inputField = false;
		systemSettingsContent.push_back(content);
	}
}

bool AppSettingsScreen::LoadSystemHtmlPage( const std::string &nextPath,
	bool pushHistory )
{
	if( !systemSettingsArchiveData || !systemSettingsArchiveSize
		|| nextPath.empty() ) return false;
	U8Archive archive( systemSettingsArchiveData, systemSettingsArchiveSize );
	u32 htmlSize = 0;
	u8 *htmlData = archive.GetFileAllocated( nextPath.c_str(), &htmlSize );
	if( !htmlData || !htmlSize ) { free( htmlData ); return false; }
	const std::string html = WiiHtmlUtf8((const char *)htmlData,htmlSize);
	free( htmlData );
	if( systemUpdateDialog )
	{
		GX_DrawDone();
		delete systemUpdateDialog;
		systemUpdateDialog = NULL;
	}

	std::string nextTitle = HtmlTextById( html, "BnrName" );
	if( nextTitle.empty() )
	{
		const std::string documentTitle = HtmlTitle( html );
		nextTitle = documentTitle.find( '_' ) == std::string::npos
			? documentTitle : systemSettingsTitle;
	}
	std::vector<std::string> nextLabels;
	HtmlListLabels( html, nextLabels );
	std::vector<std::string> nextLinks;
	for( size_t i = 0; i < nextLabels.size(); ++i )
	{
		char id[ 16 ];
		snprintf( id, sizeof( id ), "List%02d", (int)i + 1 );
		nextLinks.push_back( HtmlLinkById( html, id ) );
	}
	if( nextLabels.empty() && html.find("<table") == std::string::npos )
		HtmlFallbackRows( html, nextLabels, nextLinks );
	ApplyReadOnlyWiiSettingsRoutes( nextPath, systemSettingsRoot, nextLinks );
	if( pushHistory && !systemSettingsPath.empty() )
		systemSettingsHistory.push_back( systemSettingsPath );
	systemSettingsPath = nextPath;
	systemSettingsTitle = nextTitle;
	systemSettingsLabels.swap( nextLabels );
	systemSettingsLinks.swap( nextLinks );
	systemSettingsBackLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "UnderL", HtmlLinkById( html, "UnderL" ) );
	systemSettingsBackLabel = HtmlTextById( html, "UnderL" );
	systemSettingsConfirmLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "UnderR", HtmlLinkById( html, "UnderR" ) );
	systemSettingsMiddleLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "UnderM", HtmlLinkById( html, "UnderM" ) );
	systemSettingsLeftLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "Lbtn", HtmlLinkById( html, "Lbtn" ) );
	systemSettingsRightLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "Rbtn", HtmlLinkById( html, "Rbtn" ) );
	systemSettingsUpLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "ListUP", HtmlLinkById( html, "ListUP" ) );
	systemSettingsDownLink = ReadOnlyWiiSettingsRoute( nextPath,
		systemSettingsRoot, "ListDW", HtmlLinkById( html, "ListDW" ) );
	systemSettingsColumns = html.find( "Btn_List_M03" ) != std::string::npos
		? 2 : 1;
	const size_t index = systemSettingsPath.find( "/index0" );
	if( index != std::string::npos && index + 7 < systemSettingsPath.size()
		&& systemSettingsPath[ index + 7 ] >= '1'
		&& systemSettingsPath[ index + 7 ] <= '3' )
		systemSettingsPage = systemSettingsPath[ index + 7 ] - '1';
	// Positions and dimensions live in Nintendo's linked CSS, not in the HTML
	// tags.  Merge those real rules before parsing artwork/hitboxes; otherwise
	// every deeper menu silently uses the generic fallback coordinates.
	std::string styledHtml = html;
	std::vector<std::string> stylesheets;
	size_t linkAt = 0;
	while( true )
	{
		const size_t open = html.find( "<link", linkAt );
		if( open == std::string::npos ) break;
		const size_t close = html.find( '>', open );
		if( close == std::string::npos ) break;
		const std::string tag = html.substr( open, close - open + 1 );
		const std::string href = HtmlAttribute( tag, "href" );
		if( href.size() >= 4
			&& !strcasecmp( href.c_str() + href.size() - 4, ".css" ) )
			AppendSystemStylesheet( archive, ResolveHtmlLink(
				systemSettingsPath, href ), styledHtml, stylesheets, 0 );
		linkAt = close + 1;
	}
	ParseSystemHtmlArtwork( styledHtml );
	const std::string relative = SystemSettingsRelativePath(
		systemSettingsPath, systemSettingsRoot );
	if( relative == "setup/language_index.html" && !fakeOobeActive )
	{
		// The same file doubles as OOBE. In Settings, Confirm/Back must not
		// unexpectedly continue through the initial Sensor Bar setup flow.
		systemSettingsBackLink = "../index03.html";
		systemSettingsConfirmLink = "../index03.html";
		systemSettingsBackLabel = Localization::GetUtf8("Back");
	}
	const bool countryList = relative.find( "country/eu_country_select" ) == 0;
	const bool ratingList = ( relative.find( "parental_control/pegi_" ) == 0
		|| relative.find( "parental_control/usk/usk" ) == 0
		|| relative.find( "parental_control/oflc_" ) == 0 )
		&& relative.find( "re_rating_select" ) == std::string::npos
		&& relative.find( "_flame_" ) == std::string::npos;
	if( countryList || ratingList )
	{
		// Country/rating HTML is authored for a middle frame. Captured pages include
		// Nintendo's 64 px title frame above it, so move the interactive row and
		// arrow hitboxes by the same amount.
		for( size_t i = 0; i < systemSettingsRowSprites.size(); ++i )
			systemSettingsRowSprites[ i ].rect.y += 64.0f;
		for( HtmlContentText &content : systemSettingsContent ) content.rect.y += 64.0f;
		systemSettingsUpSprite.rect.y += 64.0f;
		systemSettingsDownSprite.rect.y += 64.0f;
	}
	if( fakeOobeActive && countryList )
	{
		systemSettingsBackLink = "../Setup/Nickname_set.html";
		systemSettingsConfirmLink =
			"../Parental_Control/Parental_Control_index.html";
	}
	if( fakeOobeActive
		&& relative == "parental_control/parental_control_index.html" )
		systemSettingsBackLink = "../Setup/ScreenSave.html";
	if( fakeOobeActive
		&& relative == "parental_control/middle_index.html" )
		systemSettingsConfirmLink = "../Setup/ScreenSave.html";
	ResetSystemSettingsPage();
	if( relative == "update/update_common.html" ) SetSystemUpdateDialog(false);
	// Retain the current page's applied-theme sprites for live rendering.
	gprintf( "Wii Settings HTML page: %s\n", systemSettingsPath.c_str() );
	return true;
}

void AppSettingsScreen::NavigateSystemSettings( const std::string &link,
	bool pushHistory )
{
	if(link == "wsm:agreement") { OpenAgreement(); return; }
	const std::string destination = ResolveHtmlLink( systemSettingsPath, link );
	if( !destination.empty() && destination != systemSettingsPath )
		LoadSystemHtmlPage( destination, pushHistory );
}

void AppSettingsScreen::UpdateSystemSettingsSimulation()
{
	if( phase != Active || page != PageSystemSettings ) return;
	++systemSettingsPageFrames;
	const std::string relative = SystemSettingsRelativePath(
		systemSettingsPath, systemSettingsRoot );
	if( relative == "format/format_index03.html"
		&& systemSettingsPageFrames >= 120 )
	{
		LoadSystemHtmlPage( systemSettingsRoot
			+ "/Format/Format_index04.html", true );
		return;
	}
	if( relative == "setup/startup_index2.html"
		&& systemSettingsPageFrames >= 180 )
	{
		LoadSystemHtmlPage( systemSettingsRoot
			+ "/Setup/Language_index.html", true );
		return;
	}
	if( relative == "update/update_connecttest.html"
		&& systemSettingsPageFrames >= 90 )
	{
		LoadSystemHtmlPage( systemSettingsRoot
			+ "/Update/Update_common.html", true );
		return;
	}
	if( relative == "update/update_common.html" )
	{
		const int previous = fakeUpdatePercent;
		fakeUpdatePercent = std::min(100, systemSettingsPageFrames * 100 / 300);
		if( previous < 100 && fakeUpdatePercent == 100 ) SetSystemUpdateDialog(true);
		if( systemUpdateDialog && fakeUpdatePercent < 100 )
			systemUpdateDialog->SetProgress((u8)fakeUpdatePercent);
	}
}

void AppSettingsScreen::SetSystemUpdateDialog(bool complete)
{
	// Nintendo's dlgWdw A0 is the real update window: authored progress BRLAN,
	// spinner, panel textures and Wii font. A1 supplies its completion button.
	GX_DrawDone();
	delete systemUpdateDialog;
	systemUpdateDialog = NULL;
	std::string message = Localization::GetUtf8(complete
		? "The system update was completed successfully."
		: "Performing a Wii system update.");
	if( !complete ) message += std::string("\n\n") + Localization::GetUtf8("Please do not turn off the power.");
	char16 decoded[512];
	Utf8ToChar16(message, decoded, 512);
	systemUpdateText.assign(decoded);
	Utf8ToChar16(Localization::GetUtf8("OK"), decoded, 512);
	systemUpdateButtonText.assign(decoded);
	systemUpdateDialog = SystemMenuResources::Instance()->CreateDialog(
		complete ? DialogWindow::A1 : DialogWindow::A0);
	if( systemUpdateDialog )
		systemUpdateDialog->SetText(systemUpdateText.c_str(),
			complete ? systemUpdateButtonText.c_str() : NULL);
}

void AppSettingsScreen::ActivateSystemSettingsRow()
{
	if( selectedRow >= 0 && selectedRow < (int)systemSettingsLinks.size() )
		NavigateSystemSettings( systemSettingsLinks[ selectedRow ], true );
	selectedRow = std::max( 0, std::min( selectedRow, RowsForPage() - 1 ) );
}

void AppSettingsScreen::OpenSystemSettings()
{
	systemSettingsExitPending = false;
	dropdown = DropdownNone;
	page = PageSystemSettings;
	returnPage = PageSystemSettings;
	editTarget = EditNone;
	transitionFrame = 0;
	selectedRow = 0;
	hoveredRow = -1;
	closeHovered = false;
	navigationMode = false;
	pointerWasValid = false;
	pointerHoverActive = false;
	hoveredScrollButton = -1;
	openedFromSystemSettings = true;
	systemSettingsPage = 0;
	systemSettingsPageFrames = 0;
	fakeUpdatePercent = -1;
	fakeOobeActive = false;
	systemSettingsHistory.clear();
	if( systemSettingsAvailable )
		LoadSystemHtmlPage( systemSettingsRoot + "/index01.html", false );
	else
		ResetSystemSettingsPage();
	phase = Opening;
}

int AppSettingsScreen::RowsForPage() const
{
	switch( page )
	{
	case PageSystemSettings:
		return std::max( 1, std::min( 8, (int)systemSettingsLabels.size() ) );
	case PageMain: return 11;
	case PageUpdates: return 3;
	case PageClock: return 1;
	case PageLanguages: return SelectableLanguageCount;
	case PageAgreement: return agreementViewer ? agreementViewer->Count()+MaxVisibleListRows-1 : 1;
	case PageAudio: return 2;
	case PageInput: return 3;
	case PageUsbDevices: return 9 + std::max(1,(int)usbInventory.count);
	case PageThemes: return std::max( 1, (int)themePaths.size() + 1 );
	case PageChannels: return 6;
	case PageNews: return 7;
	case PageForecast: return ChannelPreview::Get().forecast.japaneseIcons ? 15 : 14;
	case PageNintendo: return 6;
	case PagePhoto: return 3;
	case PageEverybodyVotes: return 5;
	case PageTodayTomorrow: return 5;
	case PageMiiContest: return 4;
	case PageWiiFit: return 3;
	case PageMiiChannel: return 3;
	case PageImagePicker: return (int)imagePaths.size();
	default: return 0;
	}
}

void AppSettingsScreen::SetPage( Page next )
{
	dropdown = DropdownNone;
	draggingChannel = -1;
	draggingScrollbar = -1;
	page = next;
	selectedRow = 0;
	firstVisibleRow = 0;
	hoveredRow = -1;
	pointerHoverActive = false;
	hoveredScrollButton = -1;
	navigationMode = true;
}

void AppSettingsScreen::BeginClose()
{
	if(PlayerUpdate::Busy()) { PlayerUpdate::Cancel(); return; }
	if( IsApplyingTheme() ) return;
	if( phase == Closed || phase == Closing )
		return;
	dropdown = DropdownNone;
	draggingChannel = -1;
	draggingScrollbar = -1;
	Commit();
	if( page == PageSystemSettings )
	{
		// The parent owns both fades; retain this page until it has faded to black.
		systemSettingsExitPending = true;
		return;
	}
	phase = Closing;
}

void AppSettingsScreen::HandleSystemSettingsHistoryBack()
{
	if( phase != Active || page != PageSystemSettings ) return;
	if( IsSystemSettingsRootPage( systemSettingsPath, systemSettingsRoot ) )
	{
		BeginClose();
		return;
	}
	if( !systemSettingsHistory.empty() )
	{
		const std::string previous = systemSettingsHistory.back();
		systemSettingsHistory.pop_back();
		LoadSystemHtmlPage( previous, false );
		selectedRow = std::max( 0, std::min( selectedRow, RowsForPage() - 1 ) );
		hoveredRow = -1;
		return;
	}
	LoadSystemHtmlPage( systemSettingsRoot + "/index01.html", false );
	selectedRow = 0;
	hoveredRow = -1;
}

void AppSettingsScreen::HandleBack()
{
	if(PlayerUpdate::Busy()) { PlayerUpdate::Cancel(); return; }
	if( IsApplyingTheme() ) return;
	if( phase != Active ) return;
	if(page == PageAgreement) { SetPage(PageSystemSettings); return; }
	if( dropdown != DropdownNone )
	{
		dropdown = DropdownNone;
		return;
	}
	if( page == PageKeyboard ) { FinishKeyboard( false ); return; }
	if( page == PageImagePicker ) { SetPage( returnPage ); return; }
	if( page == PageUsbDevices ) { SetPage( PageInput ); selectedRow=2; return; }
	if( page == PageSystemSettings )
	{
		const std::string relative = SystemSettingsRelativePath(
			systemSettingsPath, systemSettingsRoot );
		if( relative.find( "country/eu_country_select" ) == 0 )
		{
			systemSettingsHistory.clear();
			LoadSystemHtmlPage( systemSettingsRoot + "/index03.html", false );
			selectedRow = std::max( 0,
				std::min( 1, RowsForPage() - 1 ) );
			hoveredRow = -1;
			return;
		}
		// The NAND HTML declares the real parent of most submenus. Some selection
		// pages (notably Language) implement Back with Wii-only JavaScript, so an
		// unresolved UnderL falls back to history. Only the three top-level pages
		// treat their JavaScript-only Back button as Close.
		if( !systemSettingsBackLink.empty() )
		{
			const std::string destination = ResolveHtmlLink(
				systemSettingsPath, systemSettingsBackLink );
			if( destination.empty() )
			{
				if( IsSystemSettingsRootPage( systemSettingsPath,
					systemSettingsRoot ) )
				{
					BeginClose();
					return;
				}
				if( !systemSettingsHistory.empty() )
				{
					const std::string previous = systemSettingsHistory.back();
					systemSettingsHistory.pop_back();
					LoadSystemHtmlPage( previous, false );
					selectedRow = std::max( 0,
						std::min( selectedRow, RowsForPage() - 1 ) );
					hoveredRow = -1;
					return;
				}
				BeginClose();
				return;
			}
			while( !systemSettingsHistory.empty()
				&& systemSettingsHistory.back() != destination )
				systemSettingsHistory.pop_back();
			if( !systemSettingsHistory.empty() )
				systemSettingsHistory.pop_back();
			LoadSystemHtmlPage( destination, false );
			selectedRow = std::max( 0,
				std::min( selectedRow, RowsForPage() - 1 ) );
			hoveredRow = -1;
		}
		else if( !systemSettingsHistory.empty() )
		{
			const std::string previous = systemSettingsHistory.back();
			systemSettingsHistory.pop_back();
			LoadSystemHtmlPage( previous, false );
			selectedRow = std::max( 0,
				std::min( selectedRow, RowsForPage() - 1 ) );
			hoveredRow = -1;
		}
		else BeginClose();
		return;
	}
	if( page == PageMain ) BeginClose();
	else if( page == PageChannels || page == PageAudio || page == PageClock || page == PageInput || page == PageLanguages || page == PageUpdates )
		SetPage( PageMain );
	else if( page == PageThemes )
	{
		SetPage( PageMain );
		selectedRow = 6;
	}
	else SetPage( PageChannels );
}

bool AppSettingsScreen::LauncherButtonContains( float x, float y,
	const Vec2f &screen ) const
{
	return WsmSettingsRect( screen ).Contains( x, y );
}

void AppSettingsScreen::RenderLauncherButton( const Vec2f &screen,
	bool hovered ) const
{
	const float scale = Scale( screen );
	Rect rect = WsmSettingsRect( screen );
	const GXColor blue = (GXColor){ 42, 174, 221, 255 };
	const GXColor white = (GXColor){ 255, 255, 255, 245 };
	const GXColor focus = (GXColor){ 225, 247, 255, 255 };
	DrawOutlinedRoundedRect( rect, 13.0f * scale, blue,
		hovered ? focus : white, 2.0f * scale );
	char16 text[ 64 ];
	Utf8ToChar16( Localization::GetUtf8( "WSM Player Settings" ), text, 64 );
	DrawCentered( rect, text, 17.0f * scale, 12.0f * scale,
		SettingsButtonInk(255) );
}

bool AppSettingsScreen::IsOpen() const
{
	return phase != Closed;
}

bool AppSettingsScreen::IsClosed() const
{
	return phase == Closed;
}

bool AppSettingsScreen::TakeChannelRefreshRequest()
{
	const bool requested = channelRefreshRequested;
	channelRefreshRequested = false;
	return requested;
}

void AppSettingsScreen::UpdateTransition()
{
	if( phase == Opening )
	{
		if( ++transitionFrame >= TransitionFrames )
		{
			transitionFrame = TransitionFrames;
			phase = Active;
		}
	}
	else if( phase == Closing )
	{
		if( --transitionFrame <= 0 )
		{
			transitionFrame = 0;
			phase = Closed;
		}
	}
}

void AppSettingsScreen::Commit()
{
	if( settingsDirty )
	{
		// Existing News/Forecast BRLYTs are long-lived grid objects. Publish the
		// settings revision before saving so their native panes update immediately.
		ChannelPreview::MarkChanged();
		if( Settings::Save() )
		{
			settingsDirty = false;
			saveFailed = false;
		}
		else
		{
			saveFailed = true;
		}
	}
}

void AppSettingsScreen::ApplyThemeAndRestart()
{
	if( page != PageThemes || IsApplyingTheme() ) return;
	// Save must succeed before any shutdown work. Retain the page on failure.
	settingsDirty = true;
	Commit();
	if( saveFailed ) return;
	themeRestartError = 0;
	themeApplyFrames = 0;
	CInputs::Instance()->ClearButtonsDown();
}

bool AppSettingsScreen::TakeThemeRestartRequest()
{
	if( themeApplyFrames != 60 ) return false;
	++themeApplyFrames;
	return true;
}

void AppSettingsScreen::ThemeRestartFailed( s32 error )
{
	themeApplyFrames = -1;
	themeRestartError = error;
	CInputs::Instance()->ClearButtonsDown();
}

void AppSettingsScreen::SelectDropdownOption( int option )
{
	option = option ? 1 : 0;
	if( dropdown == DropdownHomeAction )
	{
		// Option order is Back to Wii-Menu, HOME Menu.
		Settings::directHomeExit = option == 0;
	}
	else if( dropdown == DropdownHomebrew )
	{
		// Yes hides apps; No means "do not hide" and keeps them visible.
		Settings::useHomebrewForBanners = option == 1;
	}
	else if( dropdown == DropdownLaunch )
	{
		// No/Yes in that order.  The safe preview-only behavior remains default.
		Settings::launchOnStart = option == 1;
	}
	settingsDirty = true;
	dropdown = DropdownNone;
	Commit();
}

void AppSettingsScreen::AdjustMusic( int delta, bool saveNow )
{
	Settings::musicVolume = std::max( 0, std::min( 100, Settings::musicVolume + delta ) );
	Settings::ApplyMusicVolume();
	settingsDirty = true;
	if( saveNow )
		Commit();
}

void AppSettingsScreen::SetMusicFromPointer( float pointerX, const Vec2f &screen )
{
	Rect slider = SliderRect( screen );
	const float trackLeft = slider.x;
	const float trackWidth = slider.w;
	int value = (int)( ( pointerX - trackLeft ) * 100.0f / trackWidth + 0.5f );
	value = std::max( 0, std::min( 100, value ) );
	if( value != Settings::musicVolume )
	{
		Settings::musicVolume = value;
		Settings::ApplyMusicVolume();
		settingsDirty = true;
	}
}

void AppSettingsScreen::OpenKeyboard( EditTarget target, const std::string &value )
{
	returnPage = page;
	editTarget = target;
	keyboardBuffer = value;
	keyboardRow = 0;
	keyboardColumn = 0;
	keyboardHover = -1;
	keyboardShift = false;
	keyboardAddingNewsArticle = false;
	SetPage( PageKeyboard );
}

void AppSettingsScreen::FinishKeyboard( bool accept )
{
	if( accept )
	{
		ChannelPreview::Config &config = ChannelPreview::Get();
		switch( editTarget )
		{
		case EditUpdateSource:
			Settings::updateManifestUrl=keyboardBuffer.empty() ? WSM_UPDATE_MANIFEST_URL : keyboardBuffer;
			PlayerUpdate::Reset();
			break;
		case EditNewsArticle:
			if( keyboardAddingNewsArticle
				&& config.news.count < ChannelPreview::MaxNewsArticles )
				++config.news.count;
			config.news.articles[ newsArticleIndex ] = keyboardBuffer;
			config.news.enabled = true;
			break;
		case EditForecastCity:
			config.forecast.city = keyboardBuffer;
			config.forecast.enabled = true;
			break;
		case EditForecastTemperature:
			config.forecast.temperature = keyboardBuffer;
			config.forecast.enabled = true;
			break;
		case EditForecastUnit:
			config.forecast.customUnit = keyboardBuffer;
			config.forecast.enabled = true;
			break;
		case EditForecastDifference:
			if( !ParseForecastDifference( keyboardBuffer,
				config.forecast.temperatureDifference ) ) return;
			config.forecast.enabled = true;
			break;
		case EditForecastCondition:
			config.forecast.condition = keyboardBuffer;
			if( config.forecast.automaticIcon )
				config.forecast.weatherCode =
					ChannelPreview::ForecastWeatherCodeFromCondition( keyboardBuffer );
			config.forecast.enabled = true;
			break;
		case EditForecastFooter:
			config.forecast.footer = keyboardBuffer;
			config.forecast.enabled = true;
			break;
		case EditForecastAttribution:
			config.forecast.attribution = keyboardBuffer;
			config.forecast.enabled = true;
			break;
		case EditNintendoText:
			config.nintendo.text = keyboardBuffer;
			config.nintendo.enabled = true;
			break;
		case EditNintendoText2:
		case EditNintendoText3:
			config.nintendo.extraText[editTarget == EditNintendoText2 ? 0 : 1] = keyboardBuffer;
			config.nintendo.enabled = true;
			break;
		case EditEverybodyVotesFirstBlue:
			config.everybodyVotes.firstBlue = keyboardBuffer;
			if( config.everybodyVotes.style
				!= ChannelPreview::EverybodyVotesOgSequence )
				config.everybodyVotes.style = ChannelPreview::EverybodyVotesCustom;
			break;
		case EditEverybodyVotesGreenQuestion:
			config.everybodyVotes.greenQuestion = keyboardBuffer;
			if( config.everybodyVotes.style
				!= ChannelPreview::EverybodyVotesOgSequence )
				config.everybodyVotes.style = ChannelPreview::EverybodyVotesCustom;
			break;
		case EditEverybodyVotesFinalBlue:
			config.everybodyVotes.finalBlue = keyboardBuffer;
			if( config.everybodyVotes.style
				!= ChannelPreview::EverybodyVotesOgSequence )
				config.everybodyVotes.style = ChannelPreview::EverybodyVotesCustom;
			break;
		case EditTodayAffinity:
			config.todayTomorrow.affinity = keyboardBuffer;
			config.todayTomorrow.enabled = true;
			break;
		case EditTodayCleaning:
			config.todayTomorrow.cleaning = keyboardBuffer;
			config.todayTomorrow.enabled = true;
			break;
		case EditTodayPlay:
			config.todayTomorrow.play = keyboardBuffer;
			config.todayTomorrow.enabled = true;
			break;
		case EditTodayMeal:
			config.todayTomorrow.meal = keyboardBuffer;
			config.todayTomorrow.enabled = true;
			break;
		case EditMiiContestComment:
			config.miiContest.comment = keyboardBuffer;
			config.miiContest.enabled = true;
			break;
		case EditWiiFitProfile:
			config.wiiFit.profile = keyboardBuffer;
			config.wiiFit.enabled = true;
			break;
		case EditWiiFitStatus:
			config.wiiFit.status = keyboardBuffer;
			config.wiiFit.enabled = true;
			break;
		default:
			break;
		}
		settingsDirty = true;
		Commit();
	}
	else if( keyboardAddingNewsArticle )
	{
		ChannelPreview::Get().news.articles[ newsArticleIndex ].clear();
		newsArticleIndex = keyboardPreviousNewsIndex;
	}
	Page destination = returnPage;
	editTarget = EditNone;
	keyboardBuffer.clear();
	SetPage( destination );
}

size_t AppSettingsScreen::KeyboardCapacity() const
{
	size_t capacity = ChannelPreview::MaxNintendoTextBytes;
	switch( editTarget )
	{
	case EditUpdateSource: capacity=513; break;
	case EditNewsArticle: capacity = ChannelPreview::MaxNewsArticleBytes; break;
	case EditForecastCity: capacity = ChannelPreview::MaxCityBytes; break;
	case EditForecastTemperature: capacity = ChannelPreview::MaxTemperatureBytes; break;
	case EditForecastUnit: capacity = 17; break;
	case EditForecastDifference: capacity = 4; break;
	case EditForecastCondition:
	case EditForecastFooter:
	case EditForecastAttribution: capacity = ChannelPreview::MaxConditionBytes; break;
	case EditEverybodyVotesFirstBlue:
	case EditEverybodyVotesGreenQuestion:
	case EditEverybodyVotesFinalBlue:
		capacity = ChannelPreview::MaxEverybodyVotesTextBytes;
		break;
	case EditTodayAffinity:
	case EditTodayCleaning:
	case EditTodayPlay:
	case EditTodayMeal:
		capacity = ChannelPreview::MaxTodayTomorrowTextBytes;
		break;
	case EditMiiContestComment:
		capacity = ChannelPreview::MaxMiiContestTextBytes;
		break;
	case EditWiiFitProfile:
	case EditWiiFitStatus:
		capacity = ChannelPreview::MaxWiiFitTextBytes;
		break;
	default: break;
	}
	return capacity;
}

void AppSettingsScreen::DeleteKeyboardCharacter()
{
	if( keyboardBuffer.empty() )
		return;
	size_t start = keyboardBuffer.size() - 1;
	while( start > 0
		&& ( (unsigned char)keyboardBuffer[ start ] & 0xc0 ) == 0x80 )
		--start;
	keyboardBuffer.erase( start );
}

void AppSettingsScreen::ActivateKeyboardKey( int key )
{
	if( key < 0 )
		return;
	const size_t capacity = KeyboardCapacity();
	if( key < 40 )
	{
		static const char lower[ 4 ][ 11 ] = {
			"1234567890", "qwertyuiop", "asdfghjkl?", "zxcvbnm.,!"
		};
		static const char upper[ 4 ][ 11 ] = {
			"1234567890", "QWERTYUIOP", "ASDFGHJKL?", "ZXCVBNM:-/"
		};
		const int row = key / 10;
		const int column = key % 10;
		const char character = keyboardShift ? upper[ row ][ column ]
			: lower[ row ][ column ];
		if( keyboardBuffer.size() + 1 < capacity )
			keyboardBuffer += character;
		return;
	}

	switch( key - 40 )
	{
	case 0:
		keyboardShift = !keyboardShift;
		break;
	case 1:
		if( keyboardBuffer.size() + 1 < capacity )
			keyboardBuffer += ' ';
		break;
	case 2:
		if( keyboardBuffer.size() + 1 < capacity )
			keyboardBuffer += '\'';
		break;
	case 3:
		DeleteKeyboardCharacter();
		break;
	case 4:
		FinishKeyboard( true );
		break;
	}
}

void AppSettingsScreen::ScanImages()
{
	imagePaths.clear();
	// The first row is an explicit reset choice and is always usable, even when
	// no SD card is mounted.
	imagePaths.push_back( std::string() );
	int directories = 0;
	int entries = 0;
	imageScanHadRoot = false;
	imageScanHadRoot |= AddImageDirectory( "sd:/wsm-player/photos", 3,
		directories, entries, imagePaths );
	imageScanHadRoot |= AddImageDirectory( "sd:/photos", 3, directories,
		entries, imagePaths );
	imageScanHadRoot |= AddImageDirectory( "sd:/DCIM", 3, directories,
		entries, imagePaths );
	imageScanHadRoot |= AddImageDirectory( "sd:/", 1, directories, entries,
		imagePaths );
	if( imagePaths.size() > 1 )
		std::sort( imagePaths.begin() + 1, imagePaths.end() );
}

void AppSettingsScreen::OpenImagePicker( ImageTarget target )
{
	forecastImageRejected = false;
	returnPage = page;
	imageTarget = target;
	ScanImages();
	const ChannelPreview::Config &config = ChannelPreview::Get();
	const std::string *current = &config.photo.imagePath;
	switch( imageTarget )
	{
	case ImageNintendo: current = &config.nintendo.imagePath; break;
	case ImageForecast: current = &config.forecast.imagePath; break;
	case ImageMiiContest: current = &config.miiContest.imagePath; break;
	case ImageWiiFit: current = &config.wiiFit.imagePath; break;
	case ImageMiiChannel: current = &config.miiChannel.imagePath; break;
	default: break;
	}
	imageIndex = 0;
	for( size_t i = 1; i < imagePaths.size(); ++i )
		if( imagePaths[ i ] == *current )
			imageIndex = (int)i;
	imageHover = -1;
	SetPage( PageImagePicker );
	firstVisibleRow = ListWindowStart( imageIndex, (int)imagePaths.size() );
}

void AppSettingsScreen::SelectImage()
{
	if( imageIndex < 0 || imageIndex >= (int)imagePaths.size() )
		return;
	ChannelPreview::Config &config = ChannelPreview::Get();
	const std::string &selected = imagePaths[ imageIndex ];
	if( imageTarget == ImageForecast && !selected.empty()
		&& !ChannelImageOverride::ValidateForecastImage( selected ) )
	{
		forecastImageRejected = true;
		return;
	}
	switch( imageTarget )
	{
	case ImageForecast:
		config.forecast.imagePath = selected;
		if( !selected.empty() ) config.forecast.enabled = true;
		break;
	case ImageNintendo:
		config.nintendo.imagePath = selected;
		if( !selected.empty() ) config.nintendo.enabled = true;
		break;
	case ImageMiiContest:
		config.miiContest.imagePath = selected;
		if( !selected.empty() ) config.miiContest.enabled = true;
		break;
	case ImageWiiFit:
		config.wiiFit.imagePath = selected;
		if( !selected.empty() ) config.wiiFit.enabled = true;
		break;
	case ImageMiiChannel:
		config.miiChannel.imagePath = selected;
		if( !selected.empty() ) config.miiChannel.enabled = true;
		break;
	default:
		config.photo.imagePath = selected;
		if( !selected.empty() ) config.photo.enabled = true;
		break;
	}
	settingsDirty = true;
	Commit();
	SetPage( returnPage );
}

void AppSettingsScreen::ScanThemes()
{
	themePaths.clear();
	themeCompiled.clear();
	mkdir( "sd:/themes", 0777 );
	mkdir( ( Settings::applicationPath + "themes" ).c_str(), 0777 );
	AddThemeDirectory( "sd:/themes", themePaths );
	AddThemeDirectory( Settings::applicationPath + "themes", themePaths );
	std::sort( themePaths.begin(), themePaths.end() );
	for( size_t i = 0; i < themePaths.size(); ++i )
		themeCompiled.push_back( IsCompiledTheme( themePaths[ i ] ) );
}

void AppSettingsScreen::SelectTheme()
{
	if( selectedRow < 0 || selectedRow > (int)themePaths.size() ) return;
	// An unreadable CSM or uncompiled MYM must not replace a working theme.
	if( selectedRow > 0 && ( selectedRow - 1 >= (int)themeCompiled.size()
		|| !themeCompiled[ selectedRow - 1 ] ) ) return;
	Settings::customThemePath = selectedRow == 0
		? std::string() : themePaths[ selectedRow - 1 ];
	// CSM and APP packages contain the compiled System Menu resource archive.
	// Raw MYM files are kept as the user's selection, but are not sent to the
	// boot loader unless they also contain a valid compiled U8 payload.
	const bool compiled = selectedRow == 0
		|| ( selectedRow - 1 < (int)themeCompiled.size()
			&& themeCompiled[ selectedRow - 1 ] );
	Settings::resourcePath = compiled ? Settings::customThemePath : std::string();
	settingsDirty = true;
	Commit();
}

void AppSettingsScreen::ActivateCurrent()
{
	ChannelPreview::Config &config = ChannelPreview::Get();
	switch( page )
	{
	case PageMain:
		if( selectedRow == 0 )
			Settings::freeCameraEnabled = !Settings::freeCameraEnabled;
		else if( selectedRow >= 1 && selectedRow <= 3 )
		{
			dropdown = selectedRow == 1 ? DropdownHomeAction
				: selectedRow == 2 ? DropdownHomebrew : DropdownLaunch;
			dropdownFocus = selectedRow == 1
				? ( Settings::directHomeExit ? 0 : 1 )
				: selectedRow == 2
					? ( Settings::useHomebrewForBanners ? 1 : 0 )
					: ( Settings::launchOnStart ? 1 : 0 );
		}
		else if( selectedRow == 4 )
			SetPage( PageAudio );
		else if( selectedRow == 5 )
			SetPage( PageChannels );
		else if( selectedRow == 6 )
			SetPage( PageInput );
		else if( selectedRow == 7 )
		{
			ScanThemes();
			SetPage( PageThemes );
			for( size_t i = 0; i < themePaths.size(); ++i )
				if( themePaths[ i ] == Settings::customThemePath )
					selectedRow = (int)i + 1;
		}
		else if( selectedRow == 9 )
			SetPage( PageClock );
		else if( selectedRow == 10 )
			SetPage( PageUpdates );
		else if( selectedRow == 8 )
		{
			SetPage( PageLanguages );
			selectedRow = 0;
			for(int i=0;i<SelectableLanguageCount;++i)
				if(SelectableLanguages[i]==Settings::uiLanguage) selectedRow=i;
			firstVisibleRow = std::max( 0, selectedRow - MaxVisibleListRows + 1 );
		}
		break;
	case PageUpdates:
		if(PlayerUpdate::Busy()) { PlayerUpdate::Cancel(); break; }
		if(selectedRow==0) OpenKeyboard(EditUpdateSource,Settings::updateManifestUrl);
		else if(selectedRow==1) {
			Commit();
			if(!saveFailed) PlayerUpdate::Check(Settings::updateManifestUrl,Settings::applicationPath);
		} else if(selectedRow==2) {
			const PlayerUpdate::Snapshot update=PlayerUpdate::Get();
			if(update.state==PlayerUpdate::Available) PlayerUpdate::Install();
			else if(update.state==PlayerUpdate::Installed) PlayerUpdate::RequestRestart();
		}
		break;
	case PageClock:
		Settings::clock24Hour = !Settings::clock24Hour;
		settingsDirty = true;
		Commit();
		break;
	case PageLanguages:
		if( selectedRow >= 0 && selectedRow < SelectableLanguageCount )
		{
			const int previous = Settings::uiLanguage;
			Settings::uiLanguage = SelectableLanguages[selectedRow];
			settingsDirty = true;
			Commit();
			if( saveFailed ) Settings::uiLanguage = previous;
			else if( previous != Settings::uiLanguage )
			{
				// Rebuild archive-backed menus through the existing in-process
				// graphics reload. USB, WPAD and IOS remain running untouched.
				themeApplyFrames = 60;
			}
		}
		break;
	case PageAudio:
		if( selectedRow == 0 )
		{
			Settings::menuMusicEnabled = !Settings::menuMusicEnabled;
			MenuAudio::Instance()->ApplySettings();
			settingsDirty = true;
			Commit();
		}
		else if( selectedRow == 1 )
			AdjustMusic( 5, true );
		break;
	case PageInput:
		if( selectedRow == 0 )
		{
			Settings::usbInputEnabled = !Settings::usbInputEnabled;
			settingsDirty = true;
			Commit();
		}
		else if( selectedRow == 1 )
		{
			Settings::mouseSpeed += 25;
			if( Settings::mouseSpeed > 400 ) Settings::mouseSpeed = 50;
			settingsDirty = true;
			Commit();
		}
		else if( selectedRow == 2 ) {
			WSM_GetUsbInventory(&usbInventory);
			SetPage(PageUsbDevices);
		}
		break;
	case PageThemes:
		SelectTheme();
		break;
	case PageChannels:
	{
		if( selectedRow == 0 )
		{
			RequestSystemMenuBannerRefresh();
			channelRefreshRequested = true;
			BeginClose();
			break;
		}
		const Page channelPages[] = { PageNews, PageForecast, PageNintendo,
			PagePhoto, PageEverybodyVotes };
		if( selectedRow > 0 && selectedRow <= 5 )
			SetPage( channelPages[ selectedRow - 1 ] );
		break;
	}
	case PageNews:
		if( selectedRow == 0 || selectedRow == 6 )
			AdjustCurrent( 1 );
		else if( selectedRow == 2 )
			OpenKeyboard( EditNewsArticle, config.news.articles[ newsArticleIndex ] );
		else if( selectedRow == 3 && config.news.count < ChannelPreview::MaxNewsArticles )
		{
			keyboardPreviousNewsIndex = newsArticleIndex;
			newsArticleIndex = config.news.count;
			OpenKeyboard( EditNewsArticle, std::string() );
			keyboardAddingNewsArticle = true;
		}
		else if( selectedRow == 4 )
		{
			if( config.news.count > 1 )
			{
				for( int i = newsArticleIndex; i + 1 < config.news.count; ++i )
					config.news.articles[ i ] = config.news.articles[ i + 1 ];
				config.news.articles[ --config.news.count ].clear();
				newsArticleIndex = std::min( newsArticleIndex, config.news.count - 1 );
			}
			else
				config.news.articles[ 0 ].clear();
			config.news.enabled = true;
			settingsDirty = true;
			Commit();
		}
		break;
	case PageForecast:
		if( selectedRow == 0 || selectedRow == 3 || selectedRow == 5
			|| selectedRow == 6 || selectedRow == 7 || selectedRow == 8 || selectedRow == 13 )
			AdjustCurrent( 1 );
		else if( selectedRow == 1 )
			OpenKeyboard( EditForecastCity, config.forecast.city );
		else if( selectedRow == 2 )
			OpenKeyboard( EditForecastTemperature, config.forecast.temperature );
		else if( selectedRow == 4 )
			OpenKeyboard( EditForecastCondition, config.forecast.condition );
		else if( selectedRow == 9 )
			OpenKeyboard( EditForecastFooter, config.forecast.footer );
		else if( selectedRow == 10 )
			OpenKeyboard( EditForecastAttribution, config.forecast.attribution );
		else if( selectedRow == 11 ) OpenImagePicker( ImageForecast );
		else if( selectedRow == 12 )
			OpenKeyboard( EditForecastUnit, config.forecast.customUnit );
		else if( selectedRow == 14 && config.forecast.japaneseIcons )
		{
			char value[16];
			snprintf( value, sizeof(value), "%d", config.forecast.temperatureDifference );
			OpenKeyboard( EditForecastDifference, value );
		}
		break;
	case PageNintendo:
		if( selectedRow == 0 || selectedRow == 3 )
			AdjustCurrent( 1 );
		else if( selectedRow == 1 )
			OpenKeyboard( EditNintendoText, config.nintendo.text );
		else if( selectedRow == 2 )
			OpenImagePicker( ImageNintendo );
		else if( selectedRow == 4 || selectedRow == 5 )
			OpenKeyboard(selectedRow == 4 ? EditNintendoText2 : EditNintendoText3,
				config.nintendo.extraText[selectedRow-4]);
		break;
	case PagePhoto:
		if( selectedRow == 0 || selectedRow == 2 )
			AdjustCurrent( 1 );
		else if( selectedRow == 1 )
			OpenImagePicker( ImagePhoto );
		break;
	case PageEverybodyVotes:
		if( selectedRow == 0 )
			AdjustCurrent( 1 );
		else if( selectedRow == 1 )
			OpenKeyboard( EditEverybodyVotesFirstBlue,
				config.everybodyVotes.firstBlue );
		else if( selectedRow == 2 )
			OpenKeyboard( EditEverybodyVotesGreenQuestion,
				config.everybodyVotes.greenQuestion );
		else if( selectedRow == 3 )
			OpenKeyboard( EditEverybodyVotesFinalBlue,
				config.everybodyVotes.finalBlue );
		else if( selectedRow == 4 )
		{
			ChannelPreview::ResetEverybodyVotesJoke();
			settingsDirty = true;
			Commit();
		}
		break;
	case PageTodayTomorrow:
		if( selectedRow == 0 ) AdjustCurrent( 1 );
		else if( selectedRow == 1 ) OpenKeyboard( EditTodayAffinity,
			config.todayTomorrow.affinity );
		else if( selectedRow == 2 ) OpenKeyboard( EditTodayCleaning,
			config.todayTomorrow.cleaning );
		else if( selectedRow == 3 ) OpenKeyboard( EditTodayPlay,
			config.todayTomorrow.play );
		else if( selectedRow == 4 ) OpenKeyboard( EditTodayMeal,
			config.todayTomorrow.meal );
		break;
	case PageMiiContest:
		if( selectedRow == 0 || selectedRow == 3 ) AdjustCurrent( 1 );
		else if( selectedRow == 1 ) OpenKeyboard( EditMiiContestComment,
			config.miiContest.comment );
		else if( selectedRow == 2 ) OpenImagePicker( ImageMiiContest );
		break;
	case PageWiiFit:
		if( selectedRow == 0 ) AdjustCurrent( 1 );
		else if( selectedRow == 1 ) AdjustCurrent( 1 );
		else if( selectedRow == 2 ) OpenKeyboard( EditWiiFitStatus,
			config.wiiFit.status );
		break;
	case PageMiiChannel:
		if( selectedRow == 0 || selectedRow == 2 ) AdjustCurrent( 1 );
		else if( selectedRow == 1 ) OpenImagePicker( ImageMiiChannel );
		break;
	case PageImagePicker:
		SelectImage();
		break;
	default:
		break;
	}
}

void AppSettingsScreen::AdjustCurrent( int direction )
{
	ChannelPreview::Config &config = ChannelPreview::Get();
	bool changed = false;
	if( page == PageMain )
	{
		if( selectedRow >= 0 && selectedRow <= 3 )
			ActivateCurrent();
		return;
	}
	if( page == PageClock )
	{
		ActivateCurrent();
		return;
	}
	if( page == PageAudio )
	{
		if( selectedRow == 1 )
			AdjustMusic( direction < 0 ? -5 : 5, true );
		else
			ActivateCurrent();
		return;
	}
	if( page == PageThemes )
	{
		SelectTheme();
		return;
	}
	if( page == PageInput )
	{
		if(selectedRow==2){ActivateCurrent();return;}
		if( selectedRow == 0 ) Settings::usbInputEnabled = !Settings::usbInputEnabled;
		else if( selectedRow == 1 )
			Settings::mouseSpeed = std::max( 50, std::min( 400,
				Settings::mouseSpeed + ( direction < 0 ? -25 : 25 ) ) );
		settingsDirty = true;
		Commit();
		return;
	}
	if( page == PageNews )
	{
		if( selectedRow == 0 )
		{
			config.news.enabled = !config.news.enabled;
			changed = true;
		}
		else if( selectedRow == 1 )
		{
			newsArticleIndex = ( newsArticleIndex + ( direction < 0 ? -1 : 1 )
				+ config.news.count ) % config.news.count;
		}
		else if( selectedRow == 6 )
		{
			config.news.japaneseGlobe = !config.news.japaneseGlobe;
			changed = true;
		}
		else if( selectedRow == 5 && config.news.count > 1 )
		{
			const int target = newsArticleIndex + ( direction < 0 ? -1 : 1 );
			if( target >= 0 && target < config.news.count )
			{
				std::swap( config.news.articles[ newsArticleIndex ],
					config.news.articles[ target ] );
				newsArticleIndex = target;
				config.news.enabled = true;
				changed = true;
			}
		}
	}
	else if( page == PageForecast )
	{
		if( selectedRow == 14 && config.forecast.japaneseIcons )
		{
			config.forecast.temperatureDifference = std::max( -99, std::min( 99,
				config.forecast.temperatureDifference + ( direction < 0 ? -1 : 1 ) ) );
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 0 )
		{
			config.forecast.enabled = !config.forecast.enabled;
			changed = true;
		}
		else if( selectedRow == 3 )
		{
			const ChannelPreview::Unit next =
				config.forecast.unit == ChannelPreview::Celsius
				? ChannelPreview::Fahrenheit : ChannelPreview::Celsius;
			ConvertForecastTemperature( config.forecast.temperature,
				config.forecast.unit, next );
			const float factor = next == ChannelPreview::Fahrenheit ? 1.8f : 1.0f / 1.8f;
			config.forecast.temperatureDifference = std::max( -99, std::min( 99,
				(int)roundf( config.forecast.temperatureDifference * factor ) ) );
			config.forecast.unit = next;
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 5 )
		{
			const int count = ChannelPreview::ForecastWeatherChoiceCount;
			int index = WeatherIndex( config.forecast.weatherCode );
			index = ( index + ( direction < 0 ? count - 1 : 1 ) ) % count;
			config.forecast.weatherCode =
				ChannelPreview::ForecastWeatherChoices[ index ];
			// Selecting art must never destroy the user's weather description.
			config.forecast.automaticIcon = false;
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 6 )
		{
			int mode = (int)config.forecast.timeMode;
			mode = ( mode + ( direction < 0 ? 2 : 1 ) ) % 3;
			config.forecast.timeMode =
				(ChannelPreview::ForecastTimeMode)mode;
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 7 )
		{
			config.forecast.japaneseIcons = !config.forecast.japaneseIcons;
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 13 )
		{
			config.forecast.japaneseWeatherIcons = !config.forecast.japaneseWeatherIcons;
			config.forecast.enabled = true;
			changed = true;
		}
		else if( selectedRow == 8 )
		{
			config.forecast.automaticIcon = !config.forecast.automaticIcon;
			if( config.forecast.automaticIcon )
				config.forecast.weatherCode = ChannelPreview::ForecastWeatherCodeFromCondition(
					config.forecast.condition );
			config.forecast.enabled = true;
			changed = true;
		}
	}
	else if( page == PageNintendo )
	{
		if( selectedRow == 0 )
		{
			config.nintendo.enabled = !config.nintendo.enabled;
			changed = true;
		}
		else if( selectedRow == 3 )
		{
			config.nintendo.fitMode = config.nintendo.fitMode == ChannelPreview::Fit
				? ChannelPreview::Fill : ChannelPreview::Fit;
			changed = true;
		}
	}
	else if( page == PagePhoto )
	{
		if( selectedRow == 0 )
		{
			config.photo.enabled = !config.photo.enabled;
			changed = true;
		}
		else if( selectedRow == 2 )
		{
			config.photo.fitMode = config.photo.fitMode == ChannelPreview::Fit
				? ChannelPreview::Fill : ChannelPreview::Fit;
			changed = true;
		}
	}
	else if( page == PageEverybodyVotes )
	{
		if( selectedRow == 0 )
		{
			// Preserve custom text while cycling modes. The Up Dog preset is
			// renderer-owned, so returning to Custom restores the user's last
			// three saved strings instead of destroying them.
			const ChannelPreview::EverybodyVotesStyle modes[] = {
				ChannelPreview::EverybodyVotesOriginal,
				ChannelPreview::EverybodyVotesJoke,
				ChannelPreview::EverybodyVotesCustom,
				ChannelPreview::EverybodyVotesOgSequence
			};
			int mode = 0;
			for( int i = 0; i < 4; ++i )
				if( modes[ i ] == config.everybodyVotes.style )
					mode = i;
			mode = ( mode + ( direction < 0 ? 3 : 1 ) ) % 4;
			config.everybodyVotes.style = modes[ mode ];
			changed = true;
		}
	}
	else if( page == PageTodayTomorrow )
	{
		if( selectedRow == 0 )
		{
			config.todayTomorrow.enabled = !config.todayTomorrow.enabled;
			changed = true;
		}
	}
	else if( page == PageMiiContest )
	{
		if( selectedRow == 0 )
		{
			config.miiContest.enabled = !config.miiContest.enabled;
			changed = true;
		}
		else if( selectedRow == 3 )
		{
			config.miiContest.fitMode = config.miiContest.fitMode
				== ChannelPreview::Fit ? ChannelPreview::Fill : ChannelPreview::Fit;
			changed = true;
		}
	}
	else if( page == PageWiiFit )
	{
		if( selectedRow == 0 )
		{
			config.wiiFit.enabled = !config.wiiFit.enabled;
			changed = true;
		}
		else if( selectedRow == 1 )
		{
			const std::string selected = MiiProfiles::Step(
				config.wiiFit.profile, direction );
			if( selected != config.wiiFit.profile )
			{
				config.wiiFit.profile = selected;
				// Selecting a real NAND Mii replaces the old arbitrary portrait
				// override. Otherwise a legacy image (for example Roblox.jpg)
				// would continue covering the chosen Wii Fit profile artwork.
				config.wiiFit.imagePath.clear();
				config.wiiFit.enabled = true;
				changed = true;
			}
		}
	}
	else if( page == PageMiiChannel )
	{
		if( selectedRow == 0 )
		{
			config.miiChannel.enabled = !config.miiChannel.enabled;
			changed = true;
		}
		else if( selectedRow == 2 )
		{
			config.miiChannel.fitMode = config.miiChannel.fitMode
				== ChannelPreview::Fit ? ChannelPreview::Fill : ChannelPreview::Fit;
			changed = true;
		}
	}
	if( changed )
	{
		settingsDirty = true;
		Commit();
	}
}

void AppSettingsScreen::UpdateInput( const Vec2f &screen )
{
	if( phase != Active )
		return;
	if(PlayerUpdate::Busy()) {
		// Do not leave this screen or replace IOS while a file/socket is live.
		// Back/B cancels asynchronously; the UI and controller polling stay alive.
		for(int i=0;i<4;++i) {
			Controller &pad=Pad(i); if(pad.IsTaken()) continue;
			const WPADData &data=pad.GetData();
			if(pad.pB() || (pad.pA() && data.ir.valid && CloseRect(screen).Contains(data.ir.x,data.ir.y))) {
				PlayerUpdate::Cancel(); pad.Take();
			}
		}
		return;
	}
	if( page == PageAgreement )
	{
		navigationMode=false;
		const int last=agreementViewer ? std::max(0,agreementViewer->Count()-1) : 0;
		for(int i=0;i<4;++i) {
			Controller &pad=Pad(i); if(pad.IsTaken())continue;
			const WPADData &data=pad.GetData();
			if(pad.pB()){HandleBack();pad.Take();return;}
			if(pad.pUp()||pad.pMinus()||pad.UsbMouseWheel()>0)--firstVisibleRow;
			if(pad.pDown()||pad.pPlus()||pad.UsbMouseWheel()<0)++firstVisibleRow;
			const float x=data.ir.x/SX(screen),y=data.ir.y/SY(screen);
			if(data.ir.valid && pad.pA()) {
				if((!agreementViewer || !agreementViewer->Count()) && CloseRect(screen).Contains(data.ir.x,data.ir.y)) {HandleBack();pad.Take();return;}
				if(y>=381 && y<=461 && x>=65 && x<=293) {HandleBack();pad.Take();return;}
				if(x>=543 && x<=595 && y>=46 && y<=381) {
					if(y<98)--firstVisibleRow;
					else if(y>329)++firstVisibleRow;
					else draggingScrollbar=i;
					pad.Take();
				}
			}
			if(draggingScrollbar==i) {
				if(data.ir.valid&&pad.hA())firstVisibleRow=(int)((y-98)/231*last+0.5f);
				else draggingScrollbar=-1;
			}
		}
		firstVisibleRow=std::max(0,std::min(last,firstVisibleRow));
		return;
	}
	if( page == PageKeyboard )
	{
		u16 key = WiredUsbMouse::KeyNone;
		while( CInputs::Instance()->PopUsbKey( key ) )
		{
			if( key == WiredUsbMouse::KeyEscape )
			{
				FinishKeyboard( false );
				return;
			}
			if( key == WiredUsbMouse::KeyEnter )
			{
				FinishKeyboard( true );
				return;
			}
			if( key == WiredUsbMouse::KeyBackspace
				|| key == WiredUsbMouse::KeyDelete )
				DeleteKeyboardCharacter();
			else if( key >= 0x20 && key < 0x7f
				&& keyboardBuffer.size() + 1 < KeyboardCapacity() )
				keyboardBuffer += (char)key;
		}
	}

	hoveredRow = -1;
	keyboardHover = -1;
	imageHover = -1;
	closeHovered = false;
	pointerHoverActive = false;
	hoveredScrollButton = -1;
	// Prefer the mouse pointer when a Wii Remote and mouse are both connected.
	// The old first-valid-pointer rule made player 1's stationary IR coordinate
	// steal hover state from a mouse assigned to player 2.
	int mousePointer = -1;
	for( int i = 0; i < 4; ++i )
		if( Pad( i ).IsUsbMouseConnected() && Pad( i ).GetData().ir.valid )
		{
			mousePointer = i;
			break;
		}
	for( int pass = 0; pass < 5; ++pass )
	{
		const int i = pass == 0 ? mousePointer : pass - 1;
		if( i < 0 || ( pass > 0 && i == mousePointer ) ) continue;
		const WPADData &wpad = Pad( i ).GetData();
		if( !wpad.ir.valid )
			continue;
		if( pointerWasValid
			&& ( std::fabs( wpad.ir.x - lastPointerX ) > 2.0f
				|| std::fabs( wpad.ir.y - lastPointerY ) > 2.0f ) )
		{
			// A deliberate pointer move hands dropdown ownership back to IR;
			// a stationary valid IR coordinate remains harmless during D-pad use.
			navigationMode = false;
		}
		lastPointerX = wpad.ir.x;
		lastPointerY = wpad.ir.y;
		pointerWasValid = true;
		pointerHoverActive = true;
		if( page != PageSystemSettings && RowsForPage() > MaxVisibleListRows )
		{
			if( ScrollUpRect(screen).Contains(wpad.ir.x,wpad.ir.y) ) hoveredScrollButton = 0;
			else if( ScrollDownRect(screen).Contains(wpad.ir.x,wpad.ir.y) ) hoveredScrollButton = 1;
		}
		Rect htmlBack = SystemSettingsHitRect( systemSettingsBackSprite.rect.x,
			systemSettingsBackSprite.rect.y, systemSettingsBackSprite.rect.w,
			systemSettingsBackSprite.rect.h, screen );
		const bool overBack = page == PageSystemSettings
			? htmlBack.Contains( wpad.ir.x, wpad.ir.y )
			: CloseRect( screen ).Contains( wpad.ir.x, wpad.ir.y );
		if( page != PageKeyboard && page != PageImagePicker && overBack )
			closeHovered = true;
		if( page == PageKeyboard )
		{
			for( int row = 0; row < 5; ++row )
			{
				const int columns = row < 4 ? 10 : 5;
				for( int column = 0; column < columns; ++column )
					if( KeyboardKeyRect( row, column, screen ).Contains(
						wpad.ir.x, wpad.ir.y ) )
						keyboardHover = row < 4 ? row * 10 + column : 40 + column;
			}
		}
		else if( page == PageImagePicker )
		{
			const int start = ListWindowStart( firstVisibleRow, (int)imagePaths.size() );
			const int visible = std::min( MaxVisibleListRows, (int)imagePaths.size() - start );
			for( int row = 0; row < visible; ++row )
				if( ListRowRect( row, visible, screen ).Contains( wpad.ir.x, wpad.ir.y ) )
					imageHover = start + row;
		}
		else
		{
			if( page == PageSystemSettings )
			{
				for( size_t row = 0; row < systemSettingsRowSprites.size(); ++row )
				{
					const HtmlRect &source = systemSettingsRowSprites[ row ].rect;
					Rect hit = SystemSettingsHitRect( source.x, source.y,
						source.w, source.h, screen );
					if( hit.Contains( wpad.ir.x, wpad.ir.y ) ) hoveredRow = row;
				}
			}
			else
			{
				const int rows = RowsForPage();
				const int first = ListWindowStart( firstVisibleRow, rows );
				const int visible = std::max( 1,
					std::min( MaxVisibleListRows, rows ) );
				for( int localRow = 0; localRow < visible; ++localRow )
				{
					const Rect rect = ( page == PageMain || page == PageAudio )
						? MainRowRect( localRow, screen, page == PageMain )
						: ListRowRect( localRow, visible, screen );
					if( rect.Contains( wpad.ir.x, wpad.ir.y ) )
						hoveredRow = first + localRow;
				}
			}
		}
		if( dropdown != DropdownNone && !navigationMode )
		{
			const int row = ( dropdown == DropdownHomeAction ? 1
				: dropdown == DropdownHomebrew ? 2 : 3 )
				- ListWindowStart( firstVisibleRow, RowsForPage() );
			for( int option = 0; option < 2; ++option )
			{
				if( DropdownOptionRect( row, option, screen ).Contains( wpad.ir.x, wpad.ir.y ) )
					dropdownFocus = option;
			}
		}
		break;
	}

	if( draggingChannel >= 0 )
	{
		Controller &pad = Pad( draggingChannel );
		const WPADData &wpad = pad.GetData();
		if( pad.hA() && wpad.ir.valid )
		{
			SetMusicFromPointer( wpad.ir.x, screen );
			pad.Take();
		}
		else
		{
			draggingChannel = -1;
			Commit();
		}
	}

	if( draggingScrollbar >= 0 )
	{
		Controller &pad = Pad( draggingScrollbar );
		const WPADData &wpad = pad.GetData();
		const int rows = RowsForPage();
		if( pad.hA() && wpad.ir.valid && page != PageSystemSettings
			&& rows > MaxVisibleListRows )
		{
			firstVisibleRow = ScrollFirstFromPointer( wpad.ir.y,
				scrollbarGrabOffset, screen, rows );
			selectedRow = std::max( firstVisibleRow,
				std::min( selectedRow, firstVisibleRow + MaxVisibleListRows - 1 ) );
			if( page == PageImagePicker )
				imageIndex = std::max( firstVisibleRow,
					std::min( imageIndex, firstVisibleRow + MaxVisibleListRows - 1 ) );
			hoveredRow = -1;
			navigationMode = false;
			pad.Take();
		}
		else
		{
			draggingScrollbar = -1;
		}
	}

	for( int i = 0; i < 4; ++i )
	{
		Controller &pad = Pad( i );
		const WPADData &wpad = pad.GetData();

		if( page == PageThemes && ( ( pad.pPlus() && !pad.UsbMouseWheel() )
			|| ( pad.pA() && !pad.IsTaken() && wpad.ir.valid
				&& ThemeApplyRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) ) ) )
		{
			ApplyThemeAndRestart();
			pad.Take();
			return;
		}

		// HOME is intentionally swallowed while this screen is active.  A newly
		// selected Back-to-Loader action cannot terminate the app mid-edit.
		if( pad.pHome() )
		{
			dropdown = DropdownNone;
			pad.Take();
			continue;
		}
		if( page == PageKeyboard )
		{
			if( pad.pB() )
			{
				FinishKeyboard( false );
				pad.Take();
				continue;
			}
			// A mouse-wheel pulse is represented as Plus/Minus for the Wii Menu
			// pages, but must never accept the on-screen keyboard accidentally.
			if( pad.pPlus() && pad.UsbMouseWheel() == 0 )
			{
				FinishKeyboard( true );
				pad.Take();
				continue;
			}
			if( pad.pUp() || pad.pDown() )
			{
				navigationMode = true;
				if( pad.pUp() )
				{
					if( keyboardRow == 0 )
					{
						keyboardRow = 4;
						keyboardColumn = std::min( 4, keyboardColumn / 2 );
					}
					else
						--keyboardRow;
				}
				else
				{
					if( keyboardRow == 4 )
					{
						keyboardRow = 0;
						keyboardColumn = std::min( 9, keyboardColumn * 2 );
					}
					else if( keyboardRow == 3 )
					{
						keyboardRow = 4;
						keyboardColumn = std::min( 4, keyboardColumn / 2 );
					}
					else
						++keyboardRow;
				}
				if( keyboardRow == 4 )
					keyboardColumn = std::min( keyboardColumn, 4 );
				pad.Take();
				continue;
			}
			if( pad.pLeft() || pad.pRight() )
			{
				navigationMode = true;
				const int columns = keyboardRow < 4 ? 10 : 5;
				keyboardColumn = ( keyboardColumn
					+ ( pad.pLeft() ? columns - 1 : 1 ) ) % columns;
				pad.Take();
				continue;
			}
			if( pad.pA() )
			{
				int key = navigationMode || !wpad.ir.valid
					? ( keyboardRow < 4 ? keyboardRow * 10 + keyboardColumn
						: 40 + keyboardColumn )
					: -1;
				if( !navigationMode && wpad.ir.valid )
				{
					for( int row = 0; row < 5; ++row )
					{
						const int columns = row < 4 ? 10 : 5;
						for( int column = 0; column < columns; ++column )
							if( KeyboardKeyRect( row, column, screen ).Contains(
								wpad.ir.x, wpad.ir.y ) )
								key = row < 4 ? row * 10 + column : 40 + column;
					}
				}
				if( key >= 0 ) ActivateKeyboardKey( key );
				pad.Take();
				continue;
			}
			continue;
		}

		if( page == PageImagePicker )
		{
			const int count = (int)imagePaths.size();
			if( pad.pA() && !pad.IsTaken() && wpad.ir.valid && count > MaxVisibleListRows )
			{
				if( ScrollUpRect(screen).Contains(wpad.ir.x, wpad.ir.y)
					|| ScrollDownRect(screen).Contains(wpad.ir.x, wpad.ir.y) )
				{
					firstVisibleRow = ListWindowStart( firstVisibleRow
						+ (ScrollUpRect(screen).Contains(wpad.ir.x,wpad.ir.y) ? -1 : 1), count );
					imageIndex = std::max(firstVisibleRow, std::min(imageIndex, firstVisibleRow + 5));
					pad.Take();
					continue;
				}
				if( ScrollTrackRect(screen).Contains(wpad.ir.x,wpad.ir.y) )
				{
					const Rect thumb = ScrollThumbRect(screen,count,firstVisibleRow);
					scrollbarGrabOffset = thumb.Contains(wpad.ir.x,wpad.ir.y)
						? wpad.ir.y - thumb.y : thumb.h * 0.5f;
					firstVisibleRow = ScrollFirstFromPointer(wpad.ir.y,scrollbarGrabOffset,screen,count);
					imageIndex = firstVisibleRow;
					draggingScrollbar = i;
					pad.Take();
					continue;
				}
			}
			if( pad.pB() )
			{
				SetPage( returnPage );
				pad.Take();
				continue;
			}
			if( pad.UsbMouseWheel() )
			{
				const int count = (int)imagePaths.size();
				if( count > 0 )
					imageIndex = ( imageIndex
						+ ( pad.UsbMouseWheel() > 0 ? count - 1 : 1 ) ) % count;
				if( imageIndex < firstVisibleRow ) firstVisibleRow = imageIndex;
				if( imageIndex >= firstVisibleRow + MaxVisibleListRows )
					firstVisibleRow = imageIndex - MaxVisibleListRows + 1;
				navigationMode = false;
				pad.Take();
				continue;
			}
			if( pad.pUp() || pad.pDown() )
			{
				navigationMode = true;
				const int count = (int)imagePaths.size();
				if( count > 0 )
					imageIndex = ( imageIndex
						+ ( pad.pUp() ? count - 1 : 1 ) ) % count;
				if( imageIndex < firstVisibleRow ) firstVisibleRow = imageIndex;
				if( imageIndex >= firstVisibleRow + MaxVisibleListRows )
					firstVisibleRow = imageIndex - MaxVisibleListRows + 1;
				pad.Take();
				continue;
			}
			if( pad.pA() )
			{
				int clickedImage = navigationMode || !wpad.ir.valid
					? imageIndex : -1;
				if( !navigationMode && wpad.ir.valid )
				{
					const int start = ListWindowStart( firstVisibleRow, (int)imagePaths.size() );
					const int visible = std::max( 0,
						std::min( MaxVisibleListRows, (int)imagePaths.size() - start ) );
					for( int row = 0; row < visible; ++row )
						if( ListRowRect( row, visible, screen ).Contains(
							wpad.ir.x, wpad.ir.y ) )
							clickedImage = start + row;
				}
				if( clickedImage >= 0 )
				{
					imageIndex = clickedImage;
					SelectImage();
				}
				pad.Take();
				continue;
			}
			continue;
		}

		if( pad.pB() )
		{
			if( page == PageSystemSettings ) HandleSystemSettingsHistoryBack();
			else HandleBack();
			pad.Take();
			continue;
		}

		if( dropdown != DropdownNone )
		{
			if( pad.UsbMouseWheel() )
			{
				dropdownFocus = pad.UsbMouseWheel() > 0 ? 0 : 1;
				navigationMode = false;
				pad.Take();
				continue;
			}
			if( pad.pUp() || pad.pLeft() )
			{
				navigationMode = true;
				dropdownFocus = 0;
				pad.Take();
				continue;
			}
			if( pad.pDown() || pad.pRight() )
			{
				navigationMode = true;
				dropdownFocus = 1;
				pad.Take();
				continue;
			}
			if( pad.pA() )
			{
				int option = -1;
				const int row = ( dropdown == DropdownHomeAction ? 1
					: dropdown == DropdownHomebrew ? 2 : 3 )
					- ListWindowStart( firstVisibleRow, RowsForPage() );
				if( navigationMode )
				{
					// D-pad navigation owns confirmation even when the Wii Remote
					// continues reporting a stale, valid IR coordinate.
					option = dropdownFocus;
				}
				else if( wpad.ir.valid )
				{
					for( int candidate = 0; candidate < 2; ++candidate )
					{
						if( DropdownOptionRect( row, candidate, screen ).Contains( wpad.ir.x, wpad.ir.y ) )
							option = candidate;
					}
					if( option < 0 )
					{
						// Clicking the current value or anywhere outside cancels.
						dropdown = DropdownNone;
					}
				}
				else
				{
					option = dropdownFocus;
				}
				if( option >= 0 )
					SelectDropdownOption( option );
				pad.Take();
				continue;
			}
		}

		if( page == PageSystemSettings && ( pad.pMinus() || pad.pPlus() ) )
		{
			// Match the Wii Settings' page controls: - walks toward the visible
			// left/up page and + walks toward the visible right/down page.  This
			// covers the three main pages and the multi-page country/rating lists.
			const bool forward = pad.pPlus();
			const std::string &link = forward
				? ( !systemSettingsRightLink.empty()
					? systemSettingsRightLink : systemSettingsDownLink )
				: ( !systemSettingsLeftLink.empty()
					? systemSettingsLeftLink : systemSettingsUpLink );
			if( !link.empty() )
			{
				NavigateSystemSettings( link, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			pad.Take();
			continue;
		}

		if( pad.UsbMouseWheel() )
		{
			if( page == PageSystemSettings
				&& ( !systemSettingsUpLink.empty()
					|| !systemSettingsDownLink.empty() ) )
			{
				const std::string &link = pad.UsbMouseWheel() > 0
					? systemSettingsUpLink : systemSettingsDownLink;
				if( !link.empty() ) NavigateSystemSettings( link, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
				navigationMode = false;
				pad.Take();
				continue;
			}
			const int rows = std::max( 1, RowsForPage() );
			// Viewport position is independent of the selected item. Every wheel
			// notch moves one row, even before a pointer has selected anything.
			firstVisibleRow = ListWindowStart( firstVisibleRow
				+ ( pad.UsbMouseWheel() > 0 ? -1 : 1 ), rows );
			selectedRow = std::max( firstVisibleRow,
				std::min( selectedRow, firstVisibleRow + MaxVisibleListRows - 1 ) );
			hoveredRow = -1;
			// Wheel scrolling moves the viewport without making a row look hovered.
			navigationMode = false;
			pad.Take();
			continue;
		}

		if( pad.pUp() )
		{
			navigationMode = true;
			const int rows = RowsForPage();
			selectedRow = ( selectedRow + rows - 1 ) % rows;
			pad.Take();
			continue;
		}
		if( pad.pDown() )
		{
			navigationMode = true;
			selectedRow = ( selectedRow + 1 ) % RowsForPage();
			pad.Take();
			continue;
		}
		if( pad.pLeft() || pad.pRight() )
		{
			navigationMode = true;
			if( page == PageSystemSettings )
			{
				NavigateSystemSettings( pad.pRight() ? systemSettingsRightLink
					: systemSettingsLeftLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else
				AdjustCurrent( pad.pLeft() ? -1 : 1 );
			pad.Take();
			continue;
		}

		if( page == PageSystemSettings && pad.pA() && !pad.IsTaken() )
		{
			const std::string relative = SystemSettingsRelativePath(
				systemSettingsPath, systemSettingsRoot );
			if( relative == "setup/startup_index1.html" )
			{
				LoadSystemHtmlPage( systemSettingsRoot
					+ "/Setup/startup_index2.html", true );
				pad.Take();
				continue;
			}
			if( relative == "format/format_index04.html" )
			{
				LoadSystemHtmlPage( systemSettingsRoot
					+ "/Setup/startup_index1.html", true );
				pad.Take();
				continue;
			}
			if( relative == "update/update_common.html"
				&& fakeUpdatePercent >= 100 )
			{
				if( fakeOobeActive ) BeginClose();
				else LoadSystemHtmlPage( systemSettingsRoot
					+ "/index03.html", false );
				pad.Take();
				continue;
			}
			if( fakeOobeActive && ( relative == "setup/screensave.html"
				|| relative == "setup/parental_notice.html" ) )
			{
				BeginClose();
				pad.Take();
				continue;
			}
		}

		if( pad.pA() && !pad.IsTaken() )
		{
			// Resolve the row from the controller that actually clicked. Hover is
			// mouse-prioritized for drawing, but a second pointer must never activate
			// the mouse's row, and clicking blank space must do nothing.
			int clickedRow = -1;
			if( !navigationMode && wpad.ir.valid )
			{
				if( page == PageSystemSettings )
				{
					for( size_t row = 0; row < systemSettingsRowSprites.size(); ++row )
					{
						const HtmlRect &source = systemSettingsRowSprites[ row ].rect;
						Rect rect = SystemSettingsHitRect( source.x, source.y,
							source.w, source.h, screen );
						if( rect.Contains( wpad.ir.x, wpad.ir.y ) ) clickedRow = row;
					}
				}
				else
				{
					const int rows = RowsForPage();
					const int first = ListWindowStart( firstVisibleRow, rows );
					const int visible = std::max( 1,
						std::min( MaxVisibleListRows, rows ) );
					for( int localRow = 0; localRow < visible; ++localRow )
					{
						const Rect rect = ( page == PageMain || page == PageAudio )
							? MainRowRect( localRow, screen, page == PageMain )
							: ListRowRect( localRow, visible, screen );
						if( rect.Contains( wpad.ir.x, wpad.ir.y ) )
							clickedRow = first + localRow;
					}
				}
			}
			if( page != PageSystemSettings && !navigationMode && wpad.ir.valid
				&& RowsForPage() > MaxVisibleListRows )
			{
				if( ScrollUpRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) )
				{
					firstVisibleRow = ListWindowStart( firstVisibleRow - 1, RowsForPage() );
					selectedRow = std::max( firstVisibleRow,
						std::min( selectedRow, firstVisibleRow + MaxVisibleListRows - 1 ) );
					hoveredRow = -1;
					pad.Take();
					continue;
				}
				if( ScrollDownRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) )
				{
					firstVisibleRow = ListWindowStart( firstVisibleRow + 1, RowsForPage() );
					selectedRow = std::max( firstVisibleRow,
						std::min( selectedRow, firstVisibleRow + MaxVisibleListRows - 1 ) );
					hoveredRow = -1;
					pad.Take();
					continue;
				}
				if( ScrollTrackRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) )
				{
					const Rect thumb = ScrollThumbRect( screen, RowsForPage(),
						firstVisibleRow );
					// Keep the point where the thumb was grabbed under the pointer.
					// A track click centres the thumb at the clicked location.
					scrollbarGrabOffset = wpad.ir.y >= thumb.y
						&& wpad.ir.y <= thumb.y + thumb.h
						? wpad.ir.y - thumb.y : thumb.h * 0.5f;
					firstVisibleRow = ScrollFirstFromPointer( wpad.ir.y,
						scrollbarGrabOffset, screen, RowsForPage() );
					selectedRow = std::max( firstVisibleRow,
						std::min( selectedRow, firstVisibleRow + MaxVisibleListRows - 1 ) );
					hoveredRow = -1;
					draggingScrollbar = i;
					pad.Take();
					continue;
				}
			}
			const HtmlRect &leftSource = systemSettingsLeftSprite.rect;
			const HtmlRect &rightSource = systemSettingsRightSprite.rect;
			const HtmlRect &backSource = systemSettingsBackSprite.rect;
			const HtmlRect &confirmSource = systemSettingsConfirmSprite.rect;
			const HtmlRect &middleSource = systemSettingsMiddleSprite.rect;
			const HtmlRect &upSource = systemSettingsUpSprite.rect;
			const HtmlRect &downSource = systemSettingsDownSprite.rect;
			Rect htmlLeft = SystemSettingsHitRect( leftSource.x, leftSource.y,
				leftSource.w, leftSource.h, screen );
			Rect htmlRight = SystemSettingsHitRect( rightSource.x, rightSource.y,
				rightSource.w, rightSource.h, screen );
			Rect htmlBackClick = SystemSettingsHitRect( backSource.x, backSource.y,
				backSource.w, backSource.h, screen );
			Rect htmlConfirm = SystemSettingsHitRect( confirmSource.x,
				confirmSource.y, confirmSource.w, confirmSource.h, screen );
			Rect htmlMiddle = SystemSettingsHitRect( middleSource.x, middleSource.y,
				middleSource.w, middleSource.h, screen );
			Rect htmlUp = SystemSettingsHitRect( upSource.x, upSource.y,
				upSource.w, upSource.h, screen );
			Rect htmlDown = SystemSettingsHitRect( downSource.x, downSource.y,
				downSource.w, downSource.h, screen );
			std::string genericLink;
			if( page == PageSystemSettings && !navigationMode && wpad.ir.valid )
				for( size_t control = 0;
					control < systemSettingsControls.size(); ++control )
				{
					const HtmlRect &source =
						systemSettingsControls[ control ].sprite.rect;
					const Rect hit = SystemSettingsHitRect( source.x, source.y,
						source.w, source.h, screen );
					if( hit.Contains( wpad.ir.x, wpad.ir.y ) )
						genericLink = systemSettingsControls[ control ].link;
				}
			if( !genericLink.empty() )
			{
				NavigateSystemSettings( genericLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsLeftLink.empty()
				&& htmlLeft.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsLeftLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsRightLink.empty()
				&& htmlRight.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsRightLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsUpLink.empty()
				&& htmlUp.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsUpLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsDownLink.empty()
				&& htmlDown.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsDownLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsMiddleLink.empty()
				&& htmlMiddle.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsMiddleLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( page == PageSystemSettings && !navigationMode && wpad.ir.valid
				&& !systemSettingsConfirmLink.empty()
				&& htmlConfirm.Contains( wpad.ir.x, wpad.ir.y ) )
			{
				NavigateSystemSettings( systemSettingsConfirmLink, true );
				selectedRow = std::max( 0,
					std::min( selectedRow, RowsForPage() - 1 ) );
			}
			else if( !navigationMode && wpad.ir.valid
				&& ( page == PageSystemSettings
					? htmlBackClick.Contains( wpad.ir.x, wpad.ir.y )
					: CloseRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) ) )
			{
				if(page == PageSystemSettings && systemSettingsBackLink == "wsm:agreement") OpenAgreement();
				else HandleBack();
			}
			else if( !navigationMode && page == PageAudio && wpad.ir.valid
				&& AudioSliderRect( screen ).Contains( wpad.ir.x, wpad.ir.y ) )
			{
				navigationMode = false;
				selectedRow = 1;
				draggingChannel = i;
				SetMusicFromPointer( wpad.ir.x, screen );
			}
			else if( page == PageSystemSettings )
			{
				if( navigationMode || clickedRow >= 0 )
				{
					if( clickedRow >= 0 ) selectedRow = clickedRow;
					ActivateSystemSettingsRow();
				}
			}
			else if( wpad.ir.valid && !navigationMode && clickedRow >= 0 )
			{
				selectedRow = clickedRow;
				ActivateCurrent();
			}
			else if( navigationMode || !wpad.ir.valid )
			{
				ActivateCurrent();
			}
			pad.Take();
		}
	}
}

void AppSettingsScreen::RenderDropdown( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	if( dropdown == DropdownNone )
		return;

	const int row = ( dropdown == DropdownHomeAction ? 1
		: dropdown == DropdownHomebrew ? 2 : 3 )
		- ListWindowStart( firstVisibleRow, RowsForPage() );
	const char16 *options[ 2 ] = {
		Localization::Get( dropdown == DropdownHomeAction
			? Localization::BackToLoader
			: dropdown == DropdownHomebrew ? Localization::Yes : Localization::No ),
		Localization::Get( dropdown == DropdownHomeAction
			? Localization::HomeMenu
			: dropdown == DropdownHomebrew ? Localization::No : Localization::Yes )
	};
	const GXColor border = WithAlpha( (GXColor){ 53, 174, 218, 255 }, alpha );
	const GXColor active = WithAlpha( (GXColor){ 214, 245, 255, 255 }, alpha );
	const GXColor ink = SettingsButtonInk(alpha);

	// Dropdowns are one opaque popup above the rows, not two translucent
	// buttons that expose the next row's label between/behind the options.
	Rect popup = DropdownOptionRect(row,0,screen);
	popup.x += slide; popup.h *= 2;
	ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
	const GXColor themeInk = skin ? skin->Ink(alpha) : ink;
	const bool lightInk = (int)themeInk.r + themeInk.g + themeInk.b > 450;
	DrawRect(popup.x-2*scale,popup.y-2*scale,popup.w+4*scale,popup.h+4*scale,border);
	DrawRect(popup.x,popup.y,popup.w,popup.h,
		WithAlpha(lightInk ? (GXColor){30,30,30,255} : (GXColor){255,255,255,255},alpha));

	for( int option = 0; option < 2; ++option )
	{
		Rect optionRect = DropdownOptionRect( row, option, screen );
		optionRect.x += slide;
		if( option == dropdownFocus )
		{
			DrawRect(optionRect.x,optionRect.y,optionRect.w,optionRect.h,
				WithAlpha(lightInk ? (GXColor){60,95,112,255} : active,alpha));
		}
		DrawCentered( optionRect, options[ option ], 22.0f * scale,
			16.0f * scale, themeInk );
	}
}

namespace
{
	std::string Abbreviate( const std::string &text, size_t maximum )
	{
		if( text.size() <= maximum )
			return text;
		if( maximum < 4 )
			return text.substr( 0, maximum );
		size_t length = maximum - 3;
		while( length > 0
			&& ( (unsigned char)text[ length ] & 0xc0 ) == 0x80 )
			--length;
		return text.substr( 0, length ) + "...";
	}

	void DrawChrome16( const Vec2f &screen, float slide, float scale, u8 alpha,
		const char16 *title )
	{
		const float sx = SX( screen );
		const float sy = SY( screen );
		ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
		if( skin && skin->Chrome(screen, slide, alpha) )
		{
			DrawFit(slide + 52 * sx, 20 * sy, screen.x - 104 * sx,
				title, 28 * scale, 19 * scale, skin->TitleInk(alpha));
			return;
		}
		DrawRect( 0, 0, screen.x, screen.y,
			WithAlpha( (GXColor){ 240, 244, 244, 255 }, alpha ) );
		DrawRect( slide, 0, screen.x, 64.0f * sy,
			WithAlpha( (GXColor){ 252, 253, 253, 255 }, alpha ) );
		DrawRect( slide, 61.0f * sy, screen.x, 3.0f * sy,
			WithAlpha( (GXColor){ 48, 183, 222, 255 }, alpha ) );
		DrawRect( slide, 390.0f * sy, screen.x, 90.0f * sy,
			WithAlpha( (GXColor){ 226, 229, 226, 255 }, alpha ) );
		for( int line = 0; line < 9; ++line )
			DrawRect( slide, ( 393.0f + line * 9.0f ) * sy, screen.x,
				1.0f * sy, WithAlpha( (GXColor){ 199, 204, 201, 150 }, alpha ) );
		DrawFit( slide + 52.0f * sx, 20.0f * sy,
			screen.x - 104.0f * sx, title, 28.0f * scale, 19.0f * scale,
			WithAlpha( (GXColor){ 75, 78, 80, 255 }, alpha ) );
	}

	void DrawChrome( const Vec2f &screen, float slide, float scale, u8 alpha,
		const std::string &title )
	{
		char16 decoded[ 96 ];
		Utf8ToChar16( Localization::GetUtf8( title.c_str() ), decoded,
			sizeof( decoded ) / sizeof( decoded[ 0 ] ) );
		DrawChrome16( screen, slide, scale, alpha, decoded );
	}

	void DrawSettingsScrollbar( const Vec2f &screen, float slide, float scale,
		u8 alpha, int rows, int first, int hovered = -1 )
	{
		if( rows <= MaxVisibleListRows ) return;
		ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
		const GXColor grey = WithAlpha((GXColor){142,146,148,255},alpha);
		Rect up = ScrollUpRect( screen ); up.x += slide;
		Rect down = ScrollDownRect( screen ); down.x += slide;
		Rect track = ScrollTrackRect( screen ); track.x += slide;
		DrawRect(track.x,track.y,track.w,track.h,WithAlpha((GXColor){200,200,200,255},alpha));
		// Exactly the agreement viewer's rectangular buttons and arrow images,
		// not squeezed horizontal Wii dialog buttons or font triangle glyphs.
		for( int i = 0; i < 2; ++i )
		{
			const Rect rect = i == 0 ? up : down;
			DrawRect(rect.x,rect.y,rect.w,rect.h,grey);
			Rect inside = {rect.x+1,rect.y+1,rect.w-2,rect.h-2};
			if( skin && skin->ScrollArtwork(3) ) DrawTextureRect(skin->ScrollArtwork(3),inside,alpha);
			else DrawRect(inside.x,inside.y,inside.w,inside.h,WithAlpha((GXColor){244,244,244,255},alpha));
			if( hovered == i )
				DrawRect(inside.x,inside.y,inside.w,inside.h,(GXColor){42,174,221,(u8)(alpha/3)});
			const Texture *arrow = skin ? skin->ScrollArtwork(i) : NULL;
			if( arrow )
			{
				const float fit = std::min((rect.w-8)/arrow->GetWidth(),(rect.h-8)/arrow->GetHeight());
				const Rect art = {rect.x+(rect.w-arrow->GetWidth()*fit)*.5f,
					rect.y+(rect.h-arrow->GetHeight()*fit)*.5f,arrow->GetWidth()*fit,arrow->GetHeight()*fit};
				DrawTextureRect(arrow,art,alpha);
			}
			else
			{
				const char16 symbol[] = {(char16)(i == 0 ? 0x25b2 : 0x25bc),0};
				DrawCentered(rect,symbol,17*scale,13*scale,grey);
			}
		}
		Rect thumb = ScrollThumbRect( screen, rows, first ); thumb.x += slide;
		if( skin && skin->ScrollArtwork(2) ) DrawTextureRect(skin->ScrollArtwork(2),thumb,alpha);
		else DrawRect(thumb.x,thumb.y,thumb.w,thumb.h,WithAlpha((GXColor){240,240,240,255},alpha));
	}

	void DrawListPanel( const Vec2f &screen, float slide, float scale, u8 alpha,
		int rows, int selected, int hovered, bool showSelected, int firstVisible = 0,
		int scrollbarHovered = -1 )
	{
		const float sx = SX( screen );
		const float sy = SY( screen );
		const GXColor panel = WithAlpha( (GXColor){ 247, 250, 252, 255 }, alpha );
		const GXColor fill = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
		const GXColor selectedFill = WithAlpha( (GXColor){ 229, 248, 255, 255 }, alpha );
		const GXColor border = WithAlpha( (GXColor){ 176, 203, 214, 255 }, alpha );
		const GXColor blue = WithAlpha( (GXColor){ 42, 174, 221, 255 }, alpha );
		Rect outer = { slide + 38.0f * sx, 68.0f * sy, 564.0f * sx,
			324.0f * sy };
		DrawOutlinedRoundedRect( outer, 15.0f * scale, border, panel, 2.0f * scale );
		const int first = ListWindowStart( firstVisible, rows );
		const int visible = std::max( 1, std::min( MaxVisibleListRows, rows ) );
		for( int localRow = 0; localRow < visible; ++localRow )
		{
			const int row = first + localRow;
			Rect rect = ListRowRect( localRow, visible, screen );
			rect.x += slide;
			const bool focus = row == hovered || ( showSelected && row == selected );
			DrawOutlinedRoundedRect( rect, 10.0f * scale,
				focus ? blue : border, focus ? selectedFill : fill, 2.0f * scale );
		}
		DrawSettingsScrollbar( screen, slide, scale, alpha, rows, first, scrollbarHovered );
	}

	void DrawUtf8Row( const Vec2f &screen, float slide, float scale, u8 alpha,
		int row, int rows, const std::string &label, const std::string &value,
		bool translateValue = true )
	{
		if( row < 0 || row >= rows ) return;
		const float sx = SX( screen );
		const float sy = SY( screen );
		Rect rect = ListRowRect( row, rows, screen );
		ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
		const GXColor ink = skin ? skin->Ink(alpha) : WithAlpha( (GXColor){45,69,82,255},alpha );
		const GXColor valueColor = skin ? ink : WithAlpha( (GXColor){38,139,180,255},alpha );
		if( value.empty() )
		{
			DrawUtf8Fit(slide + rect.x + 18 * sx,rect.y + 18 * sy,
				rect.w - 36 * sx,Localization::GetUtf8(label.c_str()),
				22 * scale,14 * scale,ink);
			return;
		}
		DrawUtf8Fit( slide + rect.x + 18.0f * sx, rect.y + 18.0f * sy,
			228.0f * sx, Localization::GetUtf8( label.c_str() ),
			rows > 4 ? 19.0f * scale : 22.0f * scale,
			14.0f * scale, ink );
		DrawUtf8Fit( slide + rect.x + 262.0f * sx, rect.y + 18.0f * sy,
			rect.w - 280.0f * sx, translateValue ? Localization::GetUtf8( value.c_str() ) : value.c_str(),
			rows > 4 ? 18.0f * scale : 21.0f * scale,
			13.0f * scale, valueColor );
	}

	void DrawNavigationRow( const Vec2f &screen, float slide, float scale,
		u8 alpha, int row, int rows, const char *label, const char *action )
	{
		if( row < 0 || row >= rows ) return;
		Rect rect = ListRowRect(row,rows,screen); rect.x += slide;
		const float sx = SX(screen);
		const GXColor ink = SettingsButtonInk(alpha);
		const float actionWidth = action && action[0] != '>' ? 92 * sx : 34 * sx;
		const Rect name = {rect.x + 18 * sx,rect.y,rect.w - actionWidth - 44 * sx,rect.h};
		char16 text[320]; Utf8ToChar16(Localization::GetUtf8(label),text,320);
		const float height = FitTextHeight(text,name.w,22 * scale,16 * scale);
		DrawFit(name.x,name.y + (name.h-height) * .5f,name.w,text,height,16 * scale,ink);
		const Rect end = {rect.x + rect.w - actionWidth - 12 * sx,rect.y,actionWidth,rect.h};
		DrawUtf8Centered(end,Localization::GetUtf8(action),20 * scale,14 * scale,ink);
	}

	void DrawBackControl( const Vec2f &screen, float slide, float scale, u8 alpha,
		bool hovered, bool saveFailed )
	{
		const float sx = SX( screen );
		const float sy = SY( screen );
		const GXColor blue = WithAlpha( (GXColor){ 42, 174, 221, 255 }, alpha );
		const GXColor white = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
		const GXColor focus = WithAlpha( (GXColor){ 229, 248, 255, 255 }, alpha );
		const GXColor ink = SettingsButtonInk(alpha);
		Rect close = CloseRect( screen );
		close.x += slide;
		DrawOutlinedRoundedRect( close, 18.0f * scale, blue,
			hovered ? focus : white, 3.0f * scale );
		DrawCentered( close, Localization::Get( Localization::Back ),
			24.0f * scale, 18.0f * scale, ink );
		if( saveFailed )
			DrawFit( slide + 245.0f * sx, 414.0f * sy, 340.0f * sx,
				Localization::Get( Localization::SettingsSaveFailed ),
				18.0f * scale, 14.0f * scale,
				WithAlpha( (GXColor){ 182, 55, 55, 255 }, alpha ) );
	}
}

void AppSettingsScreen::RenderMain( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const float sx = SX( screen );
	const float sy = SY( screen );
	const GXColor panel = WithAlpha( (GXColor){ 247, 250, 252, 255 }, alpha );
	const GXColor fill = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
	const GXColor focus = WithAlpha( (GXColor){ 229, 248, 255, 255 }, alpha );
	const GXColor border = WithAlpha( (GXColor){ 176, 203, 214, 255 }, alpha );
	const GXColor blue = WithAlpha( (GXColor){ 42, 174, 221, 255 }, alpha );
	const GXColor ink = SettingsButtonInk(alpha);
	const GXColor muted = WithAlpha( (GXColor){ 105, 126, 137, 255 }, alpha );
	const GXColor white = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );

	const int total = RowsForPage();
	const int first = ListWindowStart( firstVisibleRow, total );
	const int visible = std::min( MaxVisibleListRows, total );
	DrawChrome16( screen, slide, scale, alpha,
		Localization::Get( Localization::SettingsTitle ) );
	Rect outer = { slide + 38.0f * sx, 68.0f * sy, 564.0f * sx, 324.0f * sy };
	DrawOutlinedRoundedRect( outer, 15.0f * scale, border, panel, 2.0f * scale );
	for( int localRow = 0; localRow < visible; ++localRow )
	{
		const int row = first + localRow;
		Rect rect = MainRowRect( localRow, screen );
		rect.x += slide;
		const bool active = row == hoveredRow
			|| ( navigationMode && row == selectedRow );
		DrawOutlinedRoundedRect( rect, 10.0f * scale,
			active ? blue : border, active ? focus : fill, 2.0f * scale );
	}
	for( int localRow = 0; localRow < visible; ++localRow )
	{
		const int row = first + localRow;
		Rect rect = MainRowRect( localRow, screen );
			const float textY = rect.y + 18.0f * sy;
		if( row == 1 )
			DrawFit( slide + 76.0f * sx, textY, 250.0f * sx,
				Localization::Get( Localization::HomeButtonAction ),
				21.0f * scale, 15.0f * scale, ink );
		else if( row == 2 )
			DrawFit( slide + 76.0f * sx, textY, 250.0f * sx,
				Localization::Get( Localization::HideHomebrewApps ),
				20.0f * scale, 14.0f * scale, ink );
		else if( row == 3 )
			DrawFit( slide + 76.0f * sx, textY, 250.0f * sx,
				Localization::Get( Localization::LaunchOnStart ),
				20.0f * scale, 14.0f * scale, ink );
		else if( row == 0 )
		{
			DrawUtf8Fit( slide + 76.0f * sx, textY, 250.0f * sx,
				Localization::GetUtf8( "Free camera" ), 20.0f * scale, 14.0f * scale, ink );
			Rect selector = SelectorRect( localRow, screen );
			selector.x += slide;
			const bool selectorActive = row == hoveredRow || (navigationMode && row == selectedRow);
			DrawOutlinedRoundedRect( selector, 10.0f * scale, blue,
				selectorActive ? focus : white, 2.0f * scale );
			DrawCentered( selector, Localization::Get( Settings::freeCameraEnabled
				? Localization::Yes : Localization::No ), 20.0f * scale, 14.0f * scale, ink );
		}
		else
		{
			const char *labels[] = { "Audio Settings", "Channel Settings",
				"Controller & USB", "Themes", "Language", "Clock format", "WSM Player Updates" };
			DrawUtf8Fit( slide + 76.0f * sx, textY, 390.0f * sx,
				Localization::GetUtf8( labels[ row - 4 ] ),
				21.0f * scale, 14.0f * scale, ink );
			DrawUtf8Fit( slide + 504.0f * sx, textY - 1.0f * sy,
				40.0f * sx, ">", 24.0f * scale, 18.0f * scale, blue );
		}
	}

	for( int row = std::max(1,first); row < 4 && row < first + visible; ++row )
	{
		Rect selector = SelectorRect( row - first, screen );
		selector.x += slide;
		const bool selectorActive = row == hoveredRow || (navigationMode && row == selectedRow);
		DrawOutlinedRoundedRect( selector, 10.0f * scale, blue, selectorActive ? focus : white, 2.0f * scale );
		const char16 *value = row == 1
			? Localization::Get( Settings::directHomeExit
				? Localization::BackToLoader : Localization::HomeMenu )
			: row == 2
				? Localization::Get( Settings::useHomebrewForBanners
					? Localization::No : Localization::Yes )
				: Localization::Get( Settings::launchOnStart
					? Localization::Yes : Localization::No );
		Rect valueRect = selector;
		valueRect.w -= 28.0f * sx;
		DrawCentered( valueRect, value, 20.0f * scale, 14.0f * scale, ink );
		const char16 arrow[] = { (char16)0x25bc, 0 };
		DrawText( selector.x + selector.w - 23.0f * sx,
			selector.y + 11.0f * sy, arrow, 18.0f * scale, muted );
	}

	DrawSettingsScrollbar( screen, slide, scale, alpha, total, first, hoveredScrollButton );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
	RenderDropdown( screen, slide, scale, alpha );
}

void AppSettingsScreen::RenderUpdates(const Vec2f &screen,float slide,float scale,u8 alpha) const
{
	const PlayerUpdate::Snapshot update=PlayerUpdate::Get();
	DrawChrome(screen,slide,scale,alpha,"WSM Player Updates");
	DrawListPanel(screen,slide,scale,alpha,3,selectedRow,update.busy ? -1 : hoveredRow,
		!update.busy && navigationMode,0,hoveredScrollButton);
	DrawUtf8Row(screen,slide,scale,alpha,0,3,"Update source",
		Settings::updateManifestUrl==WSM_UPDATE_MANIFEST_URL ? Localization::GetUtf8("Official releases")
		: Abbreviate(Settings::updateManifestUrl,25),false);
	DrawNavigationRow(screen,slide,scale,alpha,1,3,"Check for updates",">");
	DrawNavigationRow(screen,slide,scale,alpha,2,3,
		update.state==PlayerUpdate::Installed ? "Restart WSM Player" : "Download and install",
		(update.state==PlayerUpdate::Available || update.state==PlayerUpdate::Installed) ? ">" : "-");
	const float sx=SX(screen),sy=SY(screen);
	ThemeUi *skin=SystemMenuResources::Instance()->ThemeWidgets();
	// These labels sit on N_Base; white button ink is unreadable on a light
	// dialog panel even when the theme correctly uses white text on dark buttons.
	const GXColor ink=skin ? skin->PanelInk(alpha) : (GXColor){45,69,82,alpha};
	char version[100];
	snprintf(version,sizeof(version),"WSM Player %s (%u)%s%s",WSM_VERSION,WSM_UPDATE_BUILD,
		update.version.empty() ? "" : " -> ",update.version.c_str());
	DrawUtf8Fit(slide+58*sx,287*sy,490*sx,version,16*scale,12*scale,ink);
	DrawUtf8Fit(slide+58*sx,312*sy,490*sx,
		Localization::GetUtf8(PlayerUpdate::StatusText(update.state)),18*scale,12*scale,ink);
	if(update.busy && update.total) {
		const Rect track={slide+58*sx,340*sy,440*sx,8*sy};
		DrawRect(track.x,track.y,track.w,track.h,WithAlpha((GXColor){120,140,150,255},alpha));
		DrawRect(track.x,track.y,track.w*std::min(1.0f,update.received/(float)update.total),track.h,
			WithAlpha((GXColor){42,174,221,255},alpha));
		char progress[24]; snprintf(progress,sizeof(progress),"%u%%",(unsigned)(100ULL*update.received/update.total));
		DrawUtf8Fit(slide+508*sx,335*sy,42*sx,progress,16*scale,12*scale,ink);
	}
	const char *detail=(update.state==PlayerUpdate::Failed || update.state==PlayerUpdate::Installed)
		&& update.error!=PlayerUpdate::NoError ? PlayerUpdate::ErrorText(update.error)
		: "Signed updates only. No recovery archives.";
	std::string detailText=Localization::GetUtf8(detail);
	if(update.error==PlayerUpdate::NetworkError && update.networkStage!=PlayerUpdate::NoNetworkStage) {
		char code[24]; snprintf(code,sizeof(code)," (%d)",update.networkResult);
		detailText=std::string(Localization::GetUtf8(PlayerUpdate::NetworkStageText(update.networkStage)))+code;
	}
	DrawUtf8Fit(slide+58*sx,361*sy,490*sx,detailText,15*scale,11*scale,ink);
	DrawBackControl(screen,slide,scale,alpha,closeHovered,saveFailed);
}

void AppSettingsScreen::RenderAudio( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const float sx = SX( screen );
	const float sy = SY( screen );
	const GXColor panel = WithAlpha( (GXColor){ 247, 250, 252, 255 }, alpha );
	const GXColor fill = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
	const GXColor focus = WithAlpha( (GXColor){ 229, 248, 255, 255 }, alpha );
	const GXColor border = WithAlpha( (GXColor){ 176, 203, 214, 255 }, alpha );
	const GXColor blue = WithAlpha( (GXColor){ 42, 174, 221, 255 }, alpha );
	const GXColor ink = SettingsButtonInk(alpha);
	const GXColor muted = WithAlpha( (GXColor){ 105, 126, 137, 255 }, alpha );
	const GXColor white = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );

	DrawChrome( screen, slide, scale, alpha, "Audio Settings" );
	Rect outer = { slide + 38.0f * sx, 68.0f * sy, 564.0f * sx, 120.0f * sy };
	DrawOutlinedRoundedRect( outer, 15.0f * scale, border, panel, 2.0f * scale );
	for( int row = 0; row < 2; ++row )
	{
		Rect rect = MainRowRect( row, screen, false );
		// The name and its control are separate controls, not an enormous
		// dialog pill underneath another pill/slider.
		rect.w = 276.0f * sx;
		rect.x += slide;
		const bool active = row == hoveredRow
			|| ( navigationMode && row == selectedRow );
		DrawOutlinedRoundedRect( rect, 10.0f * scale,
			active ? blue : border, active ? focus : fill, 2.0f * scale );
	}

	DrawUtf8Fit( slide + 76.0f * sx, 89.0f * sy, 250.0f * sx,
		Localization::GetUtf8( "Menu music" ), 21.0f * scale, 15.0f * scale, ink );
	DrawFit( slide + 76.0f * sx, 143.0f * sy, 250.0f * sx,
		Localization::Get( Localization::Music ), 21.0f * scale,
		15.0f * scale, ink );

	Rect selector = AudioSelectorRect( screen );
	selector.x += slide;
	const bool selectorActive = hoveredRow == 0 || (navigationMode && selectedRow == 0);
	DrawOutlinedRoundedRect( selector, 10.0f * scale, blue, selectorActive ? focus : white, 2.0f * scale );
	char16 onText[64], offText[64];
	Utf8ToChar16( Localization::GetUtf8("On"), onText, 64 );
	Utf8ToChar16( Localization::GetUtf8("Off"), offText, 64 );
	DrawCentered( selector, Settings::menuMusicEnabled ? onText : offText,
		20.0f * scale, 14.0f * scale, ink );
	const char16 arrows[] = { (char16)0x25c0, (char16)0x25b6, 0 };
	DrawText( selector.x + selector.w - 42.0f * sx,
		selector.y + 11.0f * sy, arrows, 17.0f * scale, muted );

	Rect slider = AudioSliderRect( screen );
	slider.x += slide;
	Rect track = { slider.x, slider.y + 12.0f * sy, slider.w, 8.0f * sy };
	DrawRoundedRect( track, 4.0f * scale, border );
	Rect filled = track;
	filled.w = track.w * Settings::musicVolume / 100.0f;
	DrawRoundedRect( filled, 4.0f * scale, blue );
	const float knobX = track.x + track.w * Settings::musicVolume / 100.0f;
	Rect knob = { knobX - 8.0f * sx, slider.y + 4.0f * sy,
		16.0f * sx, 24.0f * sy };
	DrawRoundedRect( knob, 7.0f * scale, blue );
	char16 volume[ 8 ];
	snprintf16( volume, 8, "%d", Settings::musicVolume );
	Rect volumeRect = { slide + 505.0f * sx, 129.0f * sy,
		44.0f * sx, 40.0f * sy };
	DrawCentered( volumeRect, volume, 22.0f * scale, 17.0f * scale, ink );
	DrawSettingsScrollbar( screen, slide, scale, alpha, 2, 0 );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderClock( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	DrawChrome( screen, slide, scale, alpha, "Clock format" );
	DrawListPanel( screen, slide, scale, alpha, 1, selectedRow, hoveredRow, navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 1, "Clock format",
		Settings::clock24Hour ? "24-hour" : "12-hour" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderInput( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const int rows = 3;
	DrawChrome( screen, slide, scale, alpha, "Controller & USB" );
	DrawListPanel( screen, slide, scale, alpha, rows, selectedRow,
		hoveredRow, navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, rows, "USB HID input",
		Settings::usbInputEnabled ? "On" : "Off" );
	char speed[ 24 ];
	snprintf( speed, sizeof( speed ), "%d%%", Settings::mouseSpeed );
	DrawUtf8Row( screen, slide, scale, alpha, 1, rows, "Mouse pointer speed",
		speed );
	DrawNavigationRow(screen,slide,scale,alpha,2,rows,"USB devices",">");
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderUsbDevices( const Vec2f &screen,float slide,
	float scale,u8 alpha ) const
{
	const int total=RowsForPage(),first=ListWindowStart(firstVisibleRow,total);
	const int visible=std::min(MaxVisibleListRows,total);
	DrawChrome(screen,slide,scale,alpha,"USB devices");
	DrawListPanel(screen,slide,scale,alpha,total,selectedRow,hoveredRow,
		navigationMode,first,hoveredScrollButton);
	const char *labels[]={"Runtime","USB HID input","Initialization","Scans",
		"HID list","All USB","HID notifications","Other USB notifications","Mouse reports"};
	for(int row=first;row<first+visible;++row){
		char text[112];const char *value=text;
		if(row<9){
			switch(row){
			case 0: snprintf(text,sizeof(text),"IOS %d / rev %d",IOS_GetVersion(),IOS_GetRevision());break;
			case 1: value=Localization::GetUtf8(Settings::usbInputEnabled ? "On" : "Off");break;
			case 2:
				if(!usbInventory.initAttempted)value=Localization::GetUtf8("Not started");
				else snprintf(text,sizeof(text),"%ld / %s",(long)usbInventory.initResult,
					Localization::GetUtf8(usbInventory.running ? "Running" : "Stopped"));break;
			case 3: snprintf(text,sizeof(text),"%lu",(unsigned long)usbInventory.scans);break;
			case 4: case 5:
				if(!usbInventory.scans)value=Localization::GetUtf8("Not scanned");
				else snprintf(text,sizeof(text),"%ld / %u",(long)(row==4 ? usbInventory.hidResult : usbInventory.allResult),
					row==4 ? usbInventory.hidCount : usbInventory.allCount);break;
			case 6: case 7:{const unsigned i=row-6;
				if(!usbInventory.initAttempted)value=Localization::GetUtf8("Not started");
				else snprintf(text,sizeof(text),"%ld / %lu / %d",(long)usbInventory.watchResult[i],
					(unsigned long)usbInventory.watchCallbacks[i],usbInventory.watchPending[i]);break;}
			default:{WSMMouseStatus status;WSM_MouseStatus(&status);
				snprintf(text,sizeof(text),"%lu / %lu",(unsigned long)status.submitted,(unsigned long)status.completed);break;}
			}
			DrawUtf8Row(screen,slide,scale,alpha,row-first,visible,labels[row],value,false);
		}else if(!usbInventory.count){
			DrawUtf8Row(screen,slide,scale,alpha,row-first,visible,
				usbInventory.scans ? "No USB devices listed" : "Not scanned","",false);
		}else{
			const unsigned index=row-9;const WSMUsbEntry &entry=usbInventory.entries[index];
			Rect rect=ListRowRect(row-first,visible,screen);rect.x+=slide;
			const GXColor ink=SettingsButtonInk(alpha);
			snprintf(text,sizeof(text),"#%u  %04X:%04X  %s",index+1,entry.vid,entry.pid,
				entry.frontend==1 ? "HID" : entry.frontend==2 ? "VEN" : "?");
			DrawUtf8Fit(rect.x+18*SX(screen),rect.y+8*SY(screen),rect.w-36*SX(screen),text,19*scale,14*scale,ink);
			snprintf(text,sizeof(text),"ID %08lX   token %08lX",(unsigned long)(u32)entry.id,(unsigned long)entry.token);
			DrawUtf8Fit(rect.x+18*SX(screen),rect.y+32*SY(screen),rect.w-36*SX(screen),text,16*scale,12*scale,ink);
		}
	}
	DrawBackControl(screen,slide,scale,alpha,closeHovered,false);
	const GXColor ink=SettingsButtonInk(alpha);
	DrawUtf8Fit(slide+245*SX(screen),405*SY(screen),365*SX(screen),
		Localization::GetUtf8("Lists: result / count"),14*scale,11*scale,ink);
	DrawUtf8Fit(slide+245*SX(screen),426*SY(screen),365*SX(screen),
		Localization::GetUtf8("Events: result / changes / pending"),14*scale,11*scale,ink);
}

void AppSettingsScreen::RenderThemes( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const int total = std::max( 1, (int)themePaths.size() + 1 );
	const int first = ListWindowStart( firstVisibleRow, total );
	const int visible = std::min( MaxVisibleListRows, total );
	DrawChrome( screen, slide, scale, alpha, "Themes" );
	DrawListPanel( screen, slide, scale, alpha, total, selectedRow,
		hoveredRow, navigationMode, first, hoveredScrollButton );
	for( int row = first; row < first + visible; ++row )
	{
		const std::string label = row == 0 ? Localization::GetUtf8( "Original Wii Menu" )
			: BaseName( themePaths[ row - 1 ] );
		const std::string path = row == 0 ? std::string() : themePaths[ row - 1 ];
		const bool active = path == Settings::customThemePath;
		const bool compiled = path.empty()
			|| ( row - 1 < (int)themeCompiled.size()
				&& themeCompiled[ row - 1 ] );
		DrawUtf8Row( screen, slide, scale, alpha, row - first, visible,
			label, !compiled ? Localization::GetUtf8( "Not a compiled theme" )
				: active ? Localization::GetUtf8( "Active after restart" ) : "" );
	}
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
	Rect apply = ThemeApplyRect( screen );
	const bool hover = pointerHoverActive && apply.Contains(lastPointerX,lastPointerY);
	apply.x += slide;
	const GXColor blue = WithAlpha( (GXColor){42,174,221,255}, alpha );
	DrawOutlinedRoundedRect( apply, 18.0f * scale, blue,
		WithAlpha( hover ? (GXColor){229,248,255,255} : (GXColor){255,255,255,255}, alpha ),
		3.0f * scale );
	DrawUtf8Fit( apply.x + 22.0f * SX(screen), apply.y + 17.0f * SY(screen),
		apply.w - 44.0f * SX(screen), Localization::GetUtf8("Apply (+)"), 23.0f * scale, 16.0f * scale, SettingsButtonInk(alpha) );
	if( themeRestartError )
	{
		char message[96];
		snprintf( message, sizeof(message), Localization::GetUtf8("Restart failed (%ld). Check boot.dol on SD."),
			(long)themeRestartError );
		DrawUtf8Fit( 46.0f * SX(screen), 377.0f * SY(screen), 548.0f * SX(screen),
			message, 17.0f * scale, 12.0f * scale, (GXColor){182,55,55,255} );
	}
}

void AppSettingsScreen::RenderLanguages( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const int total = SelectableLanguageCount;
	const int first = ListWindowStart( firstVisibleRow, total );
	const int visible = std::min( MaxVisibleListRows, total );
	DrawChrome( screen, slide, scale, alpha, "Language" );
	DrawListPanel( screen, slide, scale, alpha, total, selectedRow,
		hoveredRow, navigationMode, first, hoveredScrollButton );
	for( int row = first; row < first + visible; ++row )
		DrawUtf8Row( screen, slide, scale, alpha, row - first, visible,
			Localization::LanguageName( SelectableLanguages[row] ),
			Settings::uiLanguage == SelectableLanguages[row] ? "Selected" : "" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::OpenAgreement()
{
	agreementAccepted=false;
	// Labels transcribed from the EULA channel's own eula_message.bmg.
	static const char *no[]={u8"同意しません","I DO NOT ACCEPT","ICH AKZEPTIERE NICHT","JE REFUSE","NO ACEPTO","NON ACCETTO","NIET AKKOORD"};
	char16 noLabel[64];
	const int language=std::max(0,std::min(6,Localization::CurrentLanguage()));
	Utf8ToChar16(no[language],noLabel,64);
	if(!agreementViewer)agreementViewer=new AgreementViewer;
	agreementViewer->Open(noLabel);
	SetPage(PageAgreement);
}

void AppSettingsScreen::RenderAgreement(const Vec2f &screen, float slide,
	float scale, u8 alpha)
{
	const float sx=SX(screen), sy=SY(screen);
	if(!agreementViewer||!agreementViewer->Count()) {
		DrawChrome(screen,slide,scale,alpha,"WiiConnect24");
		DrawInterfaceText(slide+45*sx,100*sy,540*sx,"EULA channel resources are not installed.",20*scale,14*scale,(GXColor){255,255,255,alpha});
		DrawBackControl(screen,slide,scale,alpha,closeHovered,saveFailed);
		return;
	}
	Mtx view;guMtxScaleApply(GXmodelView2D,view,sx,sy,1);
	guMtxTransApply(view,view,slide,0,0);
	const Vec2f nativeScreen={640,480};
	agreementViewer->Render(view,nativeScreen,alpha);
	DrawInterfaceText(slide+65*sx,145*sy,510*sx,"The historical agreement was provided online.",20*scale,14*scale,(GXColor){51,51,51,alpha});
	DrawInterfaceText(slide+65*sx,190*sy,510*sx,"Its text is not stored in NAND.",20*scale,14*scale,(GXColor){51,51,51,alpha});
}

void AppSettingsScreen::DrawInterfaceText( float x, float y, float maxWidth,
	const char *text, float height, float minimumHeight, GXColor color )
{
	DrawUtf8Fit( x, y, maxWidth, Localization::GetUtf8(text), height, minimumHeight, color );
}

void AppSettingsScreen::RenderChannels( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const int total = 6;
	const int first = ListWindowStart( firstVisibleRow, total );
	const int visible = std::min( MaxVisibleListRows, total );
	DrawChrome( screen, slide, scale, alpha, "Channel Settings" );
	DrawListPanel( screen, slide, scale, alpha, total, selectedRow, hoveredRow,
		navigationMode, first, hoveredScrollButton );
	const char *labels[] = { "Refresh from Wii Menu", "News Channel", "Forecast Channel",
		"Nintendo Channel", "Photo Channel", "Everybody Votes / Meinungs Kanal" };
	for( int row = 0; row < total; ++row )
		DrawNavigationRow( screen, slide, scale, alpha, row - first, visible,
			labels[ row ], row == 0 ? "Press A" : ">" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderNews( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::NewsConfig &news = ChannelPreview::Get().news;
	const int first = ListWindowStart( firstVisibleRow, RowsForPage() );
	const int visible = std::min( MaxVisibleListRows, RowsForPage() );
	DrawChrome( screen, slide, scale, alpha, "News Channel" );
	DrawListPanel( screen, slide, scale, alpha, RowsForPage(), selectedRow, hoveredRow,
		navigationMode, first, hoveredScrollButton );
	char value[ 48 ];
	snprintf( value, sizeof( value ), "< %d / %d >", newsArticleIndex + 1, news.count );
	DrawUtf8Row( screen, slide, scale, alpha, 0 - first, visible, "Use custom headlines",
		news.enabled ? "On" : "Off (built-in)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1 - first, visible, "Article", value );
	DrawUtf8Row( screen, slide, scale, alpha, 2 - first, visible, "Edit text",
		Abbreviate( news.articles[ newsArticleIndex ], 34 ), false );
	snprintf( value, sizeof( value ), "%d / %d", news.count,
		ChannelPreview::MaxNewsArticles );
	DrawUtf8Row( screen, slide, scale, alpha, 3 - first, visible, "Add article", value );
	DrawUtf8Row( screen, slide, scale, alpha, 4 - first, visible, "Delete article", "Press A" );
	DrawUtf8Row( screen, slide, scale, alpha, 5 - first, visible, "Move article", "Left / Right" );
	DrawUtf8Row( screen, slide, scale, alpha, 6 - first, visible, "Japanese globe",
		news.japaneseGlobe ? "On" : "Off" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderForecast( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::ForecastConfig &forecast = ChannelPreview::Get().forecast;
	const int total = RowsForPage();
	const int first = ListWindowStart( firstVisibleRow, total );
	const int visible = std::min( MaxVisibleListRows, total );
	DrawChrome( screen, slide, scale, alpha, "Forecast Channel" );
	DrawListPanel( screen, slide, scale, alpha, total, selectedRow, hoveredRow,
		navigationMode, first, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0 - first, visible, "Use custom forecast",
		forecast.enabled ? "On" : "Off (built-in)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1 - first, visible, "Banner city",
		Abbreviate( forecast.city, 34 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2 - first, visible, "Temperature",
		Abbreviate( forecast.temperature, 24 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 3 - first, visible, "Unit",
		forecast.unit == ChannelPreview::Fahrenheit ? "Fahrenheit (F)" : "Celsius (C)" );
	DrawUtf8Row( screen, slide, scale, alpha, 4 - first, visible, "Weather name",
		Abbreviate( forecast.condition, 34 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 5 - first, visible, "Weather icon",
		WeatherName( forecast.weatherCode ) );
	DrawUtf8Row( screen, slide, scale, alpha, 6 - first, visible, "Time of day",
		ForecastTimeName( forecast.timeMode ) );
	DrawUtf8Row( screen, slide, scale, alpha, 7 - first, visible, "Japanese UI",
		forecast.japaneseIcons ? "On" : "Off (worldwide UI)" );
	DrawUtf8Row( screen, slide, scale, alpha, 8 - first, visible, "Icon from weather name",
		forecast.automaticIcon ? "Yes" : "No" );
	DrawUtf8Row( screen, slide, scale, alpha, 9 - first, visible, "Forecast footer",
		Abbreviate( forecast.footer, 34 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 10 - first, visible, "Weather attribution",
		Abbreviate( forecast.attribution, 34 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 11 - first, visible, "Icon image",
		forecast.imagePath.empty() ? "Choose from SD..."
			: Abbreviate( BaseName( forecast.imagePath ), 30 ) );
	DrawUtf8Row( screen, slide, scale, alpha, 12 - first, visible, "Degree label",
		forecast.customUnit.empty() ? ( forecast.unit == ChannelPreview::Fahrenheit ? "F" : "C" )
			: forecast.customUnit, false );
	DrawUtf8Row( screen, slide, scale, alpha, 13 - first, visible, "Japanese weather icons",
		forecast.japaneseWeatherIcons ? "On" : "Off" );
	if( forecast.japaneseIcons )
	{
		char difference[32];
		snprintf( difference, sizeof(difference), "%+d %s", forecast.temperatureDifference,
			ChannelPreview::ForecastUnitLabel().c_str() );
		DrawUtf8Row( screen, slide, scale, alpha, 14 - first, visible,
			"Change", difference, false );
	}
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderNintendo( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::NintendoConfig &config = ChannelPreview::Get().nintendo;
	DrawChrome( screen, slide, scale, alpha, "Nintendo Channel" );
	DrawListPanel( screen, slide, scale, alpha, 6, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 6, "Custom icon story",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 6, "Icon message",
		Abbreviate( config.text, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 6, "Icon image",
		config.imagePath.empty() ? "Choose from SD..."
			: Abbreviate( BaseName( config.imagePath ), 30 ) );
	DrawUtf8Row( screen, slide, scale, alpha, 3, 6, "Image sizing",
		config.fitMode == ChannelPreview::Fill ? "Fill" : "Fit" );
	DrawUtf8Row( screen, slide, scale, alpha, 4, 6,
		std::string(Localization::GetUtf8("Icon message"))+" 2", Abbreviate(config.extraText[0],32),false);
	DrawUtf8Row( screen, slide, scale, alpha, 5, 6,
		std::string(Localization::GetUtf8("Icon message"))+" 3", Abbreviate(config.extraText[1],32),false);
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderPhoto( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::PhotoConfig &config = ChannelPreview::Get().photo;
	DrawChrome( screen, slide, scale, alpha, "Photo Channel" );
	DrawListPanel( screen, slide, scale, alpha, 3, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 3, "Show selected photo",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 3, "Banner photo",
		config.imagePath.empty() ? "Choose from SD..."
			: Abbreviate( BaseName( config.imagePath ), 30 ) );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 3, "Image sizing",
		config.fitMode == ChannelPreview::Fill ? "Fill" : "Fit" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderEverybodyVotes( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::EverybodyVotesConfig &config =
		ChannelPreview::Get().everybodyVotes;
	DrawChrome( screen, slide, scale, alpha, "Everybody Votes / Meinungs Kanal" );
	DrawListPanel( screen, slide, scale, alpha, 5, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 5, "Preset / mode",
		config.style == ChannelPreview::EverybodyVotesOriginal
			? "Original"
			: ( config.style == ChannelPreview::EverybodyVotesJoke
				? "Up Dog preset"
				: ( config.style == ChannelPreview::EverybodyVotesOgSequence
					? "OG colours (custom)" : "Custom blue/green/blue" ) ) );
	const bool ogSequence = config.style
		== ChannelPreview::EverybodyVotesOgSequence;
	DrawUtf8Row( screen, slide, scale, alpha, 1, 5, "First blue",
		Abbreviate( config.firstBlue, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 5,
		ogSequence ? "Second purple" : "Green question",
		Abbreviate( config.greenQuestion, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 3, 5,
		ogSequence ? "Third green" : "Final blue",
		Abbreviate( config.finalBlue, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 4, 5,
		"Restore Up Dog preset", "Press A" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderTodayTomorrow( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::TodayTomorrowConfig &config =
		ChannelPreview::Get().todayTomorrow;
	DrawChrome( screen, slide, scale, alpha, "Today & Tomorrow Channel" );
	DrawListPanel( screen, slide, scale, alpha, 5, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 5, "Use custom prompts",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 5, "Compatibility",
		Abbreviate( config.affinity, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 5, "Cleaning",
		Abbreviate( config.cleaning, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 3, 5, "Fun",
		Abbreviate( config.play, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 4, 5, "Meal",
		Abbreviate( config.meal, 32 ), false );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderMiiContest( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::MiiContestConfig &config =
		ChannelPreview::Get().miiContest;
	DrawChrome( screen, slide, scale, alpha, "Mii Contest / Check Mii Out" );
	DrawListPanel( screen, slide, scale, alpha, 4, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 4, "Custom icon entry",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 4, "Comment",
		Abbreviate( config.comment, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 4, "Mii picture",
		config.imagePath.empty() ? "Choose from SD..."
			: Abbreviate( BaseName( config.imagePath ), 30 ) );
	DrawUtf8Row( screen, slide, scale, alpha, 3, 4, "Image sizing",
		config.fitMode == ChannelPreview::Fill ? "Fill" : "Fit" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderWiiFit( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::WiiFitConfig &config = ChannelPreview::Get().wiiFit;
	const std::vector<std::string> &miiNames = MiiProfiles::Names();
	DrawChrome( screen, slide, scale, alpha, "Wii Fit / Wii Fit Plus" );
	DrawListPanel( screen, slide, scale, alpha, 3, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 3,
		"Custom preview data",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 3,
		"Selected Mii + portrait",
		miiNames.empty() ? Localization::GetUtf8("No Miis found on this Wii")
			: Abbreviate( config.profile, 32 ), false );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 3, "Status message",
		Abbreviate( config.status, 32 ), false );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderMiiChannel( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const ChannelPreview::MiiChannelConfig &config =
		ChannelPreview::Get().miiChannel;
	DrawChrome( screen, slide, scale, alpha, "Mii Channel" );
	DrawListPanel( screen, slide, scale, alpha, 3, selectedRow, hoveredRow,
		navigationMode, firstVisibleRow, hoveredScrollButton );
	DrawUtf8Row( screen, slide, scale, alpha, 0, 3, "Custom portrait",
		config.enabled ? "On" : "Off (original)" );
	DrawUtf8Row( screen, slide, scale, alpha, 1, 3, "Portrait image",
		config.imagePath.empty() ? "Choose from SD..."
			: Abbreviate( BaseName( config.imagePath ), 30 ) );
	DrawUtf8Row( screen, slide, scale, alpha, 2, 3, "Image sizing",
		config.fitMode == ChannelPreview::Fill ? "Fill" : "Fit" );
	DrawBackControl( screen, slide, scale, alpha, closeHovered, saveFailed );
}

void AppSettingsScreen::RenderKeyboard( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const float sx = SX( screen );
	const float sy = SY( screen );
	const char *targetName = "Text";
	size_t capacity = ChannelPreview::MaxNintendoTextBytes;
	switch( editTarget )
	{
	case EditNewsArticle: targetName = "News article"; capacity = ChannelPreview::MaxNewsArticleBytes; break;
	case EditForecastCity: targetName = "Forecast city"; capacity = ChannelPreview::MaxCityBytes; break;
	case EditForecastTemperature: targetName = "Temperature"; capacity = ChannelPreview::MaxTemperatureBytes; break;
	case EditForecastUnit: targetName = "Degree label"; capacity = 17; break;
	case EditForecastDifference: targetName = "Change"; capacity = 4; break;
	case EditForecastCondition: targetName = "Weather name"; capacity = ChannelPreview::MaxConditionBytes; break;
	case EditForecastFooter: targetName = "Forecast footer"; capacity = ChannelPreview::MaxConditionBytes; break;
	case EditForecastAttribution: targetName = "Weather attribution"; capacity = ChannelPreview::MaxConditionBytes; break;
	case EditNintendoText: targetName = "Nintendo icon message"; break;
	case EditNintendoText2: case EditNintendoText3: targetName = "Nintendo icon message"; break;
	case EditEverybodyVotesFirstBlue:
		targetName = "First blue bubble";
		capacity = ChannelPreview::MaxEverybodyVotesTextBytes;
		break;
	case EditEverybodyVotesGreenQuestion:
		targetName = ChannelPreview::Get().everybodyVotes.style
			== ChannelPreview::EverybodyVotesOgSequence
			? "Second purple bubble" : "Green question bubble";
		capacity = ChannelPreview::MaxEverybodyVotesTextBytes;
		break;
	case EditEverybodyVotesFinalBlue:
		targetName = ChannelPreview::Get().everybodyVotes.style
			== ChannelPreview::EverybodyVotesOgSequence
			? "Third green bubble" : "Final blue bubble";
		capacity = ChannelPreview::MaxEverybodyVotesTextBytes;
		break;
	case EditTodayAffinity:
		targetName = "Today compatibility prompt";
		capacity = ChannelPreview::MaxTodayTomorrowTextBytes;
		break;
	case EditTodayCleaning:
		targetName = "Today cleaning prompt";
		capacity = ChannelPreview::MaxTodayTomorrowTextBytes;
		break;
	case EditTodayPlay:
		targetName = "Today fun prompt";
		capacity = ChannelPreview::MaxTodayTomorrowTextBytes;
		break;
	case EditTodayMeal:
		targetName = "Today meal prompt";
		capacity = ChannelPreview::MaxTodayTomorrowTextBytes;
		break;
	case EditMiiContestComment:
		targetName = "Mii Contest comment";
		capacity = ChannelPreview::MaxMiiContestTextBytes;
		break;
	case EditWiiFitProfile:
		targetName = "Wii Fit profile";
		capacity = ChannelPreview::MaxWiiFitTextBytes;
		break;
	case EditWiiFitStatus:
		targetName = "Wii Fit status";
		capacity = ChannelPreview::MaxWiiFitTextBytes;
		break;
	default: break;
	}
	DrawChrome( screen, slide, scale, alpha,
		std::string( Localization::GetUtf8( "Edit" ) ) + ": "
		+ Localization::GetUtf8( targetName ) );
	const GXColor border = WithAlpha( (GXColor){ 176, 203, 214, 255 }, alpha );
	const GXColor blue = WithAlpha( (GXColor){ 42, 174, 221, 255 }, alpha );
	const GXColor white = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
	const GXColor focus = WithAlpha( (GXColor){ 229, 248, 255, 255 }, alpha );
	const GXColor ink = SettingsButtonInk(alpha);
	const GXColor muted = WithAlpha( (GXColor){ 105, 126, 137, 255 }, alpha );
	Rect preview = KeyboardPreviewRect( screen );
	preview.x += slide;
	DrawOutlinedRoundedRect( preview, 10.0f * scale, border, white, 2.0f * scale );
	DrawUtf8Fit( preview.x + 12.0f * sx, preview.y + 10.0f * sy,
		preview.w - 24.0f * sx,
		keyboardBuffer.empty() ? std::string( Localization::GetUtf8("<empty>") ) : Abbreviate( keyboardBuffer, 90 ),
		19.0f * scale, 12.0f * scale, ink );
	char count[ 128 ];
	snprintf( count, sizeof( count ), Localization::GetUtf8("%u / %u bytes"), (unsigned)keyboardBuffer.size(),
		(unsigned)( capacity - 1 ) );
	int difference = 0;
	const bool invalidDifference = editTarget == EditForecastDifference
		&& !ParseForecastDifference( keyboardBuffer, difference );
	if( editTarget == EditForecastDifference )
		snprintf( count, sizeof(count), "%s-99 ... +99", invalidDifference ? "! " : "" );
	DrawUtf8Fit( preview.x + 12.0f * sx, preview.y + 39.0f * sy,
		preview.w - 24.0f * sx, count, 15.0f * scale, 12.0f * scale,
		invalidDifference || keyboardBuffer.size() + 1 >= capacity
			? WithAlpha( (GXColor){ 182, 55, 55, 255 }, alpha ) : muted );

	static const char lower[ 4 ][ 11 ] = {
		"1234567890", "qwertyuiop", "asdfghjkl?", "zxcvbnm.,!"
	};
	static const char upper[ 4 ][ 11 ] = {
		"1234567890", "QWERTYUIOP", "ASDFGHJKL?", "ZXCVBNM:-/"
	};
	for( int row = 0; row < 5; ++row )
	{
		const int columns = row < 4 ? 10 : 5;
		for( int column = 0; column < columns; ++column )
		{
			const int key = row < 4 ? row * 10 + column : 40 + column;
			Rect rect = KeyboardKeyRect( row, column, screen );
			rect.x += slide;
			const bool active = key == keyboardHover
				|| ( navigationMode && row == keyboardRow && column == keyboardColumn );
			DrawOutlinedRoundedRect( rect, 7.0f * scale, active ? blue : border,
				active ? focus : white, 2.0f * scale );
			std::string label;
			if( row < 4 )
				label.assign( 1, keyboardShift ? upper[ row ][ column ] : lower[ row ][ column ] );
			else
			{
				const char *actions[] = { "Shift", "Space", "'", "Back", "Done" };
				label = actions[ column ];
			}
			char16 decoded[ 24 ];
			Utf8ToChar16( Localization::GetUtf8( label.c_str() ), decoded, 24 );
			DrawCentered( rect, decoded, row < 4 ? 20.0f * scale : 17.0f * scale,
				12.0f * scale, ink );
		}
	}
	DrawUtf8Fit( slide + 42.0f * sx, 405.0f * sy, 550.0f * sx,
		Localization::GetUtf8( "B: Cancel     +: Done" ),
		17.0f * scale, 13.0f * scale, muted );
}

bool AppSettingsScreen::SystemSettingsHover(const HtmlSprite &sprite, const Vec2f &screen) const
{
	if( !pointerHoverActive || phase != Active ) return false;
	const HtmlRect &source = sprite.rect;
	const Rect hit = SystemSettingsHitRect(source.x,source.y,source.w,source.h,screen);
	return hit.Contains(lastPointerX,lastPointerY);
}

void AppSettingsScreen::RenderSystemSettings( const Vec2f &screen,
	float slide, float scale, u8 alpha ) const
{
	const float sx = SX( screen );
	const float sy = SY( screen );
	const float originX = slide + 16.0f * sx;
	const float originY = 12.0f * sy;
	ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
	const GXColor ink = skin ? skin->Ink(alpha) : WithAlpha( (GXColor){ 51, 51, 51, 255 }, alpha );
	const GXColor white = WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha );
	DrawRect( slide, 0.0f, screen.x, screen.y, white );

	auto textureFor = [&]( const std::string &path ) -> const Texture *
	{
		for( size_t i = 0; i < systemSettingsTextures.size(); ++i )
			if( systemSettingsTextures[ i ].path == path )
				return systemSettingsTextures[ i ].texture;
		return NULL;
	};
	auto screenRect = [&]( const HtmlRect &htmlRect ) -> Rect
	{
		Rect result = { originX + htmlRect.x * sx, originY + htmlRect.y * sy,
			htmlRect.w * sx, htmlRect.h * sy };
		return result;
	};
	auto drawSprite = [&]( const HtmlSprite &item, bool focused )
	{
		const Rect rect = screenRect(item.rect);
		if( skin && item.image.find("/BTN/") != std::string::npos
			&& rect.w >= 120 * sx && rect.h <= 96 * sy
			&& skin->Button(rect.x, rect.y, rect.w, rect.h, alpha, focused) ) return;
		const std::string &path = focused && !item.hoverImage.empty()
			? item.hoverImage : item.image;
		if( !item.backgroundImage.empty() )
			DrawTextureRect(textureFor(item.backgroundImage),rect,alpha);
		DrawTextureRect( textureFor( path ), screenRect( item.rect ), alpha );
	};

	const Texture *background = textureFor( systemSettingsBackgroundImage );
	if( skin && skin->Chrome(screen, slide, alpha) ) {}
	else if( background )
	{
		Rect viewport = { originX, originY, 608.0f * sx, 456.0f * sy };
		const float repeats = std::max( 1.0f,
			608.0f / std::max( 1, (int)background->GetWidth() ) );
		DrawTextureRect( background, viewport, alpha, repeats, 1.0f );
	}
	else
		DrawRect( originX, originY, 608.0f * sx, 456.0f * sy,
			WithAlpha( (GXColor){ 235, 238, 240, 255 }, alpha ) );

	for( size_t i = 0; i < systemSettingsDecorations.size(); ++i )
		if( !skin || systemSettingsDecorations[i].id.find("Bnr") != 0 )
			drawSprite( systemSettingsDecorations[ i ], false );
	for( size_t i = 0; i < systemSettingsControls.size(); ++i )
	{
		const HtmlControl &control = systemSettingsControls[i];
		drawSprite(control.sprite,SystemSettingsHover(control.sprite,screen));
		if( !control.label.empty() ) DrawUtf8Centered(screenRect(control.sprite.rect),
			control.label,24*scale,14*scale,ink);
	}
	for( size_t row = 0; row < systemSettingsRowSprites.size(); ++row )
	{
		const bool focused = (navigationMode && (int)row == selectedRow) || (int)row == hoveredRow;
		drawSprite( systemSettingsRowSprites[ row ], focused );
		if( row < systemSettingsLabels.size() )
		{
			Rect label = screenRect( systemSettingsRowSprites[ row ].rect );
			DrawUtf8Centered( label, systemSettingsLabels[ row ],
				24.0f * scale, 14.0f * scale, ink );
		}
	}
	// Authored text overlays (connection captions, TV captions, etc.) sit
	// ABOVE their image controls. Drawing these first hides them under cards.
	for( const HtmlContentText &content : systemSettingsContent )
	{
		if(content.inputField)
		{
			const Rect field = screenRect(content.rect);
			DrawRect(field.x,field.y,field.w,field.h,WithAlpha((GXColor){128,128,128,255},alpha));
			DrawRect(field.x+scale,field.y+scale,field.w-2*scale,field.h-2*scale,
				WithAlpha((GXColor){255,255,255,255},alpha));
		}
		DrawHtmlContent(screenRect(content.rect),content.text,content.fontSize * scale,
			content.centered,content.inputField ? WithAlpha((GXColor){51,51,51,255},alpha)
				: content.bright ? white : ink);
	}

	auto footer = [&](const HtmlSprite &sprite,const std::string &link,
		const std::string &label,const char *fallback,bool focused)
	{
		if( sprite.image.empty() && link.empty() ) return;
		const Rect rect = screenRect(sprite.rect);
		// Country/rating lists get their footer from a sibling frame; Language
		// needs Back only outside OOBE. A valid hitbox must have a visible button.
		if( !sprite.image.empty() ) drawSprite(sprite,focused);
		else DrawOutlinedRoundedRect(rect,18*scale,WithAlpha((GXColor){42,174,221,255},alpha),
			focused ? WithAlpha((GXColor){229,248,255,255},alpha) : white,2*scale);
		DrawUtf8Centered(rect,label.empty() ? Localization::GetUtf8(fallback) : label,
			24*scale,15*scale,ink);
	};
	footer(systemSettingsBackSprite,systemSettingsBackLink,systemSettingsBackLabel,"Back",closeHovered);
	footer(systemSettingsConfirmSprite,systemSettingsConfirmLink,systemSettingsConfirmLabel,"Confirm",SystemSettingsHover(systemSettingsConfirmSprite,screen));
	if( !systemSettingsMiddleLink.empty() )
		drawSprite( systemSettingsMiddleSprite, SystemSettingsHover(systemSettingsMiddleSprite,screen) );
	if( !systemSettingsLeftLink.empty() ) drawSprite( systemSettingsLeftSprite, SystemSettingsHover(systemSettingsLeftSprite,screen) );
	if( !systemSettingsRightLink.empty() ) drawSprite( systemSettingsRightSprite, SystemSettingsHover(systemSettingsRightSprite,screen) );
	if( !systemSettingsUpLink.empty() ) drawSprite( systemSettingsUpSprite, SystemSettingsHover(systemSettingsUpSprite,screen) );
	if( !systemSettingsDownLink.empty() ) drawSprite( systemSettingsDownSprite, SystemSettingsHover(systemSettingsDownSprite,screen) );

	Rect title = screenRect( systemSettingsTitleRect );
	DrawUtf8Fit( title.x, title.y + 4.0f * sy, title.w,
		systemSettingsTitle.empty() ? "Wii System Settings" : systemSettingsTitle,
		24.0f * scale, 15.0f * scale, skin ? skin->TitleInk(alpha) : ink );
	for( size_t i = 0; i < systemSettingsDecorations.size(); ++i )
		if( systemSettingsDecorations[ i ].id.find( "Page0" ) == 0 )
		{
			const int number = std::max( 1, std::min( 3, 4 - atoi(
				systemSettingsDecorations[ i ].id.c_str() + 4 ) ) );
			char text[ 2 ] = { (char)( '0' + number ), 0 };
			DrawUtf8Centered( screenRect( systemSettingsDecorations[ i ].rect ),
				text, 18.0f * scale, 13.0f * scale,
				WithAlpha( (GXColor){ 255, 255, 255, 255 }, alpha ) );
		}
	if( systemUpdateDialog )
	{
		Mtx view;
		guMtxCopy( GXmodelView2D, view );
		guMtxTransApply( view, view, slide, 0.0f, 0.0f );
		systemUpdateDialog->Render( view, screen, _CONF_GetAspectRatio() > 0 );
	}
}

void AppSettingsScreen::RenderImagePicker( const Vec2f &screen, float slide,
	float scale, u8 alpha ) const
{
	const float sx = SX( screen );
	const float sy = SY( screen );
	const char *title = "Choose Photo Channel image";
	switch( imageTarget )
	{
	case ImageNintendo: title = "Choose Nintendo icon image"; break;
	case ImageMiiContest: title = "Choose Mii Contest image"; break;
	case ImageWiiFit: title = "Choose Wii Fit image"; break;
	case ImageMiiChannel: title = "Choose Mii Channel image"; break;
	case ImageForecast: title = "Weather icon"; break;
	default: break;
	}
	DrawChrome( screen, slide, scale, alpha, title );
	const int start = ListWindowStart( firstVisibleRow, (int)imagePaths.size() );
	const int visible = std::min( MaxVisibleListRows, (int)imagePaths.size() - start );
	DrawListPanel( screen, slide, scale, alpha, (int)imagePaths.size(), imageIndex,
		imageHover, navigationMode, start, hoveredScrollButton );
	for( int row = 0; row < visible; ++row )
	{
		const int index = start + row;
		std::string label = index == 0 ? "<No image / use original>"
			: BaseName( imagePaths[ index ] );
		char number[ 32 ];
		snprintf( number, sizeof( number ), "%d / %d", index,
			(int)imagePaths.size() - 1 );
		DrawUtf8Row( screen, slide, scale, alpha, row, visible,
			Abbreviate( label, 38 ), number );
	}
	const GXColor muted = WithAlpha( (GXColor){ 105, 126, 137, 255 }, alpha );
	std::string status = Localization::GetUtf8( "A: Select     B: Cancel" );
	if( imageTarget == ImageForecast )
		status = std::string( forecastImageRejected ? "! " : "" )
			+ Localization::GetUtf8( "Image sizing" ) + ": 1:1 / 4:3; PNG/JPEG; <= 2 MP";
	if( imagePaths.size() == 1 )
		status = Localization::GetUtf8( imageScanHadRoot
			? "No PNG/JPEG images found     B: Cancel"
			: "SD card unavailable     B: Cancel" );
	DrawUtf8Fit( slide + 42.0f * sx, 405.0f * sy, 550.0f * sx,
		status, 17.0f * scale, 13.0f * scale, muted );
}

void AppSettingsScreen::Render( Mtx &modelview, const Vec2f &screen, bool widescreen, bool allowInput )
{
	(void)modelview;
	(void)widescreen;
	if( phase == Closed )
		return;
	UpdateTransition();
	if( phase == Closed )
		return;
	UpdateSystemSettingsSimulation();
	if(page==PageUsbDevices){
		WSM_GetUsbInventory(&usbInventory);
		selectedRow=std::max(0,std::min(selectedRow,RowsForPage()-1));
	}
	if( allowInput && !IsApplyingTheme() ) UpdateInput( screen );
	if( page != PageSystemSettings && page != PageKeyboard
		&& page != PageImagePicker )
	{
		const int rows = RowsForPage();
		// D-pad focus scrolls only when it crosses a viewport edge. Pointer
		// selection leaves the viewport still, so clicking an upper row cannot
		// unexpectedly scroll the whole list back toward its beginning.
		if( navigationMode )
		{
			if( selectedRow < firstVisibleRow ) firstVisibleRow = selectedRow;
			else if( selectedRow >= firstVisibleRow + MaxVisibleListRows )
				firstVisibleRow = selectedRow - MaxVisibleListRows + 1;
		}
		firstVisibleRow = ListWindowStart( firstVisibleRow, rows );
	}
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	GX_SetScissor( 0, 0, (u32)screen.x, (u32)screen.y );
	const float progress = transitionFrame / (float)TransitionFrames;
	const float eased = progress * progress * ( 3.0f - 2.0f * progress );
	const float slide = ( 1.0f - eased ) * screen.x;
	const float scale = Scale( screen );
	const u8 alpha = (u8)( eased * 255.0f );

	switch( page )
	{
	case PageSystemSettings:
		// The parent owns the fade through black. Never slide the HTML image.
		RenderSystemSettings( screen, 0.0f, scale, 255 );
		break;
	case PageMain: RenderMain( screen, slide, scale, alpha ); break;
	case PageAudio: RenderAudio( screen, slide, scale, alpha ); break;
	case PageUpdates: RenderUpdates(screen,slide,scale,alpha); break;
	case PageClock: RenderClock( screen, slide, scale, alpha ); break;
	case PageInput: RenderInput( screen, slide, scale, alpha ); break;
	case PageUsbDevices: RenderUsbDevices(screen,slide,scale,alpha); break;
	case PageThemes: RenderThemes( screen, slide, scale, alpha ); break;
	case PageLanguages: RenderLanguages( screen, slide, scale, alpha ); break;
	case PageAgreement: RenderAgreement( screen, slide, scale, alpha ); break;
	case PageChannels: RenderChannels( screen, slide, scale, alpha ); break;
	case PageNews: RenderNews( screen, slide, scale, alpha ); break;
	case PageForecast: RenderForecast( screen, slide, scale, alpha ); break;
	case PageNintendo: RenderNintendo( screen, slide, scale, alpha ); break;
	case PagePhoto: RenderPhoto( screen, slide, scale, alpha ); break;
	case PageEverybodyVotes: RenderEverybodyVotes( screen, slide, scale, alpha ); break;
	case PageTodayTomorrow: RenderTodayTomorrow( screen, slide, scale, alpha ); break;
	case PageMiiContest: RenderMiiContest( screen, slide, scale, alpha ); break;
	case PageWiiFit: RenderWiiFit( screen, slide, scale, alpha ); break;
	case PageMiiChannel: RenderMiiChannel( screen, slide, scale, alpha ); break;
	case PageKeyboard: RenderKeyboard( screen, slide, scale, alpha ); break;
	case PageImagePicker: RenderImagePicker( screen, slide, scale, alpha ); break;
	}

	if( IsApplyingTheme() )
	{
		const Rect full = {0,0,screen.x,screen.y};
		DrawRoundedRect( full, 0, (GXColor){0,0,0,210} );
		DrawUtf8Fit( 40.0f * SX(screen), 226.0f * SY(screen), 560.0f * SX(screen),
			Localization::GetUtf8("Applying and restarting WSMPlayer..."), 24.0f * scale, 16.0f * scale,
			(GXColor){255,255,255,255} );
		if( themeApplyFrames < 60 ) ++themeApplyFrames;
	}

	GX_SetScissor( 0, 0, (u32)screen.x, (u32)screen.y );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	PrepareFlatGX();
}
