/*
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
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <asndlib.h>
#include <cmath>

#include "gecko.h"
#include "channelpreviewconfig.h"
#include "homebrew/contentlauncher.h"
#include "menuhandler.h"
#include "menuaudio.h"
#include "object.h"
#include "sc.h"
#include "settings.h"
#include "replacementactivity.h"
#include "SoundOperations/audio.h"
#include "SoundOperations/SoundHandler.hpp"
#include "Texture.h"
#include "video.h"
#include "recovery.h"
#include "appsettingsscreen.h"
#include "playerupdate.h"
#include "SystemFont.h"
#include "localization.h"


namespace
{
const int HomeCursorHideFrames = 4;
const int WiiOptionsFadeFrames = 18;

void SetBannerRenderClip(const guVector &topLeft, const guVector &bottomRight,
	const Mtx44 &projection)
{
	// Pixel boundaries must round inward; round-to-nearest can expose a row
	// below the frame when the banner scales during opening/closing.
	const float left = ceilf(std::max(0.0f, std::min(topLeft.x, bottomRight.x)));
	const float top = ceilf(std::max(0.0f, std::min(topLeft.y, bottomRight.y)));
	const float right = floorf(std::min((float)screenwidth, std::max(topLeft.x, bottomRight.x)));
	const float bottom = floorf(std::min((float)screenheight, std::max(topLeft.y, bottomRight.y)));
	Pane::SetRenderClip((u32)std::min(left, (float)screenwidth),
		(u32)std::min(top, (float)screenheight),
		(u32)std::max(0.0f, right - left), (u32)std::max(0.0f, bottom - top), projection);
}

const char *OverlayGlyph( char c )
{
    if( c >= 'a' && c <= 'z' ) c = (char)( c - 'a' + 'A' );
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
    case '/': return "..#" "..#" ".#." "#.." "#..";
    case '.': return "..." "..." "..." "..." ".#.";
    case ':': return "..." ".#." "..." ".#." "...";
    case '+': return "..." ".#." "###" ".#." "...";
	case '<': return "..#" ".#." "#.." ".#." "..#";
	case '>': return "#.." ".#." "..#" ".#." "#..";
    case '?': return "##." "..#" ".#." "..." ".#.";
    case '!': return ".#." ".#." ".#." "..." ".#.";
    case ' ': return "..." "..." "..." "..." "...";
    default:  return "##." "..#" ".#." "..." ".#.";
    }
}

void PrepareOverlayFont()
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

void OverlayPixel( float x, float y, float size, const GXColor &color )
{
    GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
    GX_Position3f32( x, y, 0.0f ); GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x + size, y, 0.0f ); GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x + size, y + size, 0.0f ); GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x, y + size, 0.0f ); GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_End();
}

void DrawOverlayText( float x, float y, const std::string &text, float scale,
                      const GXColor &color, size_t maxCharacters )
{
    if( SystemFont::wbf1 && SystemFont::wbf1->IsLoaded() )
    {
        AppSettingsScreen::DrawInterfaceText(x, y,
            maxCharacters ? maxCharacters * 4.0f * scale : screenwidth - x - 8,
            text.c_str(), scale * 8.0f, scale * 5.0f, color);
        return;
    }
    PrepareOverlayFont();
    const size_t count = maxCharacters && text.size() > maxCharacters ? maxCharacters : text.size();
    for( size_t i = 0; i < count; ++i )
    {
        const char *glyph = OverlayGlyph( text[ i ] );
        for( int row = 0; row < 5; ++row )
            for( int column = 0; column < 3; ++column )
                if( glyph[ row * 3 + column ] == '#' )
                    OverlayPixel( x + ( i * 4 + column ) * scale,
                                  y + row * scale, scale, color );
    }
}

void DrawHomeSnapshot( Texture *texture, const Vec2f &screen,
	float x = 0, float y = 0, float width = 0, float height = 0 );

void RenderHomeUnavailable( const Vec2f &screen, int &frames )
{
	if( frames <= 0 ) return;
	Texture *icon = SystemMenuResources::Instance()->HomeUnavailableIcon();
	// The framebuffer is anamorphic on a 16:9 TV: compensate horizontally.
	const float aspectScale = _CONF_GetAspectRatio() > 0 ? 0.75f : 1.0f;
	if( icon ) DrawHomeSnapshot( icon, screen, 24 * screen.x / 640,
		24 * screen.y / 480, 56 * screen.x / 640 * aspectScale, 56 * screen.y / 480 );
	--frames;
}

void BuildHomeMenuProjection( Mtx44 projection )
{
	// HOME and ButtonCoords are authored directly in the full framebuffer
	// coordinate space. The general System Menu projection trims 10 horizontal
	// and 4 vertical pixels, which subtly enlarges and offsets every HOME pane.
	guOrtho( projection, 0.0f, (f32)screenheight, 0.0f,
		(f32)screenwidth, -1000.0f, 1000.0f );
}

void DrawHomeSnapshot( Texture *texture, const Vec2f &screen,
	float x, float y, float width, float height )
{
	if( !texture || !texture->IsLoaded() )
		return;
	if( width <= 0 ) width = screen.x;
	if( height <= 0 ) height = screen.y;

	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	GX_SetScissor( 0, 0, (u32)screen.x, (u32)screen.y );
	LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
	GX_SetCullMode( GX_CULL_NONE );
	GX_SetZMode( GX_DISABLE, GX_ALWAYS, GX_FALSE );
	GX_SetColorUpdate( GX_TRUE );
	GX_SetAlphaUpdate( GX_TRUE );
	GX_SetNumChans( 1 );
	GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
		GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
	GX_SetNumTexGens( 1 );
	GX_SetTexCoordGen( GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_TEX0,
		GX_IDENTITY );
	GX_SetNumTevStages( 1 );
	GX_SetNumIndStages( 0 );
	GX_SetTevDirect( GX_TEVSTAGE0 );
	GX_SetTevOp( GX_TEVSTAGE0, GX_MODULATE );
	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0,
		GX_COLOR0A0 );
	GX_SetAlphaCompare( GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0 );
	GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_SET );
	GX_ClearVtxDesc();
	GX_InvVtxCache();
	GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );

	u8 tlut = 0;
	texture->Apply( tlut, GX_TEXMAP0, GX_CLAMP, GX_CLAMP );
	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	// Preserve the captured framebuffer exactly; HOME's own bars are aligned
	// to the screen edges in HomeMenu without stretching the paused image.
	GX_Position3f32( x, y, 0.0f );
	GX_Color4u8( 255, 255, 255, 255 ); GX_TexCoord2f32( 0.0f, 0.0f );
	GX_Position3f32( x + width, y, 0.0f );
	GX_Color4u8( 255, 255, 255, 255 ); GX_TexCoord2f32( 1.0f, 0.0f );
	GX_Position3f32( x + width, y + height, 0.0f );
	GX_Color4u8( 255, 255, 255, 255 ); GX_TexCoord2f32( 1.0f, 1.0f );
	GX_Position3f32( x, y + height, 0.0f );
	GX_Color4u8( 255, 255, 255, 255 ); GX_TexCoord2f32( 0.0f, 1.0f );
	GX_End();
}

}

MenuHandler *MenuHandler::instance = NULL;

MenuHandler::MenuHandler()
	: resources( SystemMenuResources::Instance() ),
	  diHandler( DiHandler::Instance() ),
	  state( St_HealthScreen ),
	  selectedBanner( NULL ),
	  bannerSound( NULL ),
	  fakeOverlayKind( FakeOverlayNone ),
	  discChannelBanner( NULL ),
	  discChannelBannerData( NULL ),
	  discChannelBannerDataLen( 0 ),
	  selectedIdx( 0 ),
	  bannerReturnIdx( -1 ),
	  bannerReturnPage( 0 ),
	  discReturnIdx( -2 ),
	  discReturnPage( 0 ),
	  discEntryDirection( 0 ),
	  discBannerState( DBSt_None ),
	  buttonPanel( resources->CreateButtonPanel() ),
	  dcIcon( resources->CreateDiscChannelIcon() ),
	  dcBanner( resources->CreateDiscChannel() ),
	  gcBanner( resources->CreateGCBanner() ),
	  homeMenu( NULL ),
	  cursors( resources->Cursors() ),
	  bg( resources->GreyBG() ),
	  grid( resources->CreateChannelGrid() ),
	  suspendedGridSources( 0 ),
	  suspendedGridPage( 0 ),
	  gridFadeInPending( false ),
	  gridFadeInFrames( 30 ),
	  wiiOptionsFadePending( false ),
	  setupSelect( NULL ),
	  setupBtn( NULL ),
	  wiiSaveGrid( NULL ),
	  channelEdit( NULL ),
	  bigBannerLayout( NULL ),
	  bigBannerObj( NULL ),
	  nativeChannelView( NULL ),
	  messageBoard( NULL ),
	  enterSystemSettingsDirect( false ),
	  launchErrorMessage(),
	  launchErrorFrames( 0 ),
	  launchFadeFrame( 0 ),
	  bannerSwitchDirection( 0 ),
	  homeMenuActive( false ),
	  freeCameraSettingSeen( false ),
	  freeCameraController( -1 ),
      AnimStep( 0 ),
      AnimationRunning( false )
{
	fatarErrorStr[ 0 ] = 0;
	activeThemePath = Settings::resourcePath;
	LWP_MutexInit( &drawMutex, false );


}

MenuHandler::~MenuHandler()
{
	delete bg;
	delete buttonPanel;
	delete channelEdit;
	delete cursors;
	delete dcIcon;
	delete dcBanner;
	delete gcBanner;
	delete grid;
	delete homeMenu;
	delete nativeChannelView;
	delete messageBoard;
	delete setupBtn;
	delete setupSelect;
	delete wiiSaveGrid;
}

void MenuHandler::BackmenuFinished()
{
	gridFadeInPending = true;
	gridFadeInFrames = 30;
	state = St_ChannelGrid;
}

void PrintScreenshot();

void MenuHandler::StartBannerSound()
{
	MenuAudio::Instance()->PauseMusic();
	if( selectedBanner && selectedBanner->LoadSound() )
	{
		bannerSound = new GuiSound( selectedBanner->getSound(), selectedBanner->getSoundSize(), Settings::musicVolume );
		Settings::RegisterMusicSound( bannerSound );
		bannerSound->Play();
	}
}

void MenuHandler::StopBannerSound()
{
	if( bannerSound )
	{
		Settings::UnregisterMusicSound( bannerSound );
		bannerSound->Stop();
		DELETE( bannerSound );
	}
	if( selectedBanner )
	{
		selectedBanner->UnloadSound();
	}
}

bool MenuHandler::IsChannelUsable( int index ) const
{
	if( index == -1 )
	{
		return dcBanner != NULL;
	}
	if( index < 0 || index >= (int)bannerList.size() )
	{
		return false;
	}
	const BannerListEntry *entry = bannerList[ index ];
	if( !entry || entry->emptySlot || entry->knownBad )
	{
		return false;
	}
	if( entry->banner && entry->banner->DidLoadFail() )
	{
		return false;
	}
	return true;
}

void MenuHandler::MarkChannelBad( int index, const char *reason )
{
	if( index < 0 || index >= (int)bannerList.size() )
	{
		return;
	}
	BannerListEntry *entry = bannerList[ index ];
	if( !entry )
	{
		return;
	}
	entry->knownBad = true;
	gprintf( "channel marked bad %08x%08x \"%s\": %s\n",
		(u32)( entry->tid >> 32 ), (u32)( entry->tid & 0xffffffff ),
		entry->filepath.c_str(), reason ? reason : "unknown" );
	// Keep the grid usable and leave a human-readable diagnostic on SD. A bad
	// banner is skipped for this run; it is never deleted from NAND or storage.
	const std::string logPath = Settings::applicationPath
		+ "wsm-bad-channels.log";
	FILE *badLog = fopen( logPath.c_str(), "ab" );
	if( badLog )
	{
		fprintf( badLog, "%lu\t%08x%08x\t%s\t%s\n",
			(unsigned long)time( NULL ), (u32)( entry->tid >> 32 ),
			(u32)( entry->tid & 0xffffffff ), entry->filepath.c_str(),
			reason ? reason : "unknown" );
		fclose( badLog );
	}
	if( entry->banner )
	{
		BannerAsync::RemoveBanner( entry->banner );
		entry->banner = NULL;
		entry->IsBound = false;
	}
}

bool MenuHandler::EnsureBannerForIndex( int index )
{
	if( index < 0 || index >= (int)bannerList.size() )
	{
		return false;
	}
	BannerListEntry *entry = bannerList[ index ];
	if( !entry || entry->emptySlot || entry->knownBad )
	{
		return false;
	}
	if( entry->banner )
	{
		if( entry->tid )
			entry->banner->SetTitleId( entry->tid );
		return !entry->banner->DidLoadFail();
	}
	if( !entry->hbXml )
	{
		entry->banner = new BannerAsync( entry->filepath, entry->tid );
	}
	else
	{
		entry->banner = new BannerAsyncHB( entry->filepath, entry->hbXml,
			entry->tid );
	}
	if( entry->banner && entry->tid )
	{
		entry->banner->SetTitleId( entry->tid );
	}
	return entry->banner != NULL;
}


void MenuHandler::PrepareFakeChannelOverlay( int index )
{
	fakeOverlayKind = FakeOverlayNone;
	fakeOverlayTitle.clear();
	fakeOverlayLine1.clear();
	fakeOverlayLine2.clear();
	fakeOverlayLine3.clear();

	if( index < 0 || index >= (int)bannerList.size() || !bannerList[ index ] )
		return;

	const u32 low = (u32)( bannerList[ index ]->tid & 0xffffffffULL );
	const u32 prefix = low & 0xffffff00;
	char line[ 128 ];

	if( prefix == 0x48414600 ) // HAF? Forecast Channel, all regions
	{
		static const char *conditions[] = {
			"Light spaghetti showers", "Localized Mii storm",
			"One cloud refusing to move", "Sideways umbrellas",
			"Suspiciously rectangular hail", "Unexpected indoor fog"
		};
		const int temp = 14 + rand() % 18;
		const int high = temp + 2 + rand() % 5;
		const int lowTemp = temp - 2 - rand() % 5;
		fakeOverlayKind = FakeOverlayForecast;
		fakeOverlayTitle = "OFFLINE FORECAST";
		snprintf( line, sizeof( line ), "Amogus City: %d C - %s", temp,
			conditions[ rand() % ( sizeof( conditions ) / sizeof( conditions[ 0 ] ) ) ] );
		fakeOverlayLine1 = line;
		snprintf( line, sizeof( line ), "High %d C   Low %d C   Humidity %d%%", high, lowTemp, 35 + rand() % 55 );
		fakeOverlayLine2 = line;
		snprintf( line, sizeof( line ), "Tomorrow %d C   Wind %d km/h", temp + ( rand() % 7 ) - 3, 4 + rand() % 28 );
		fakeOverlayLine3 = line;
	}
	else if( prefix == 0x48414700 ) // HAG? News Channel, all regions
	{
		static const char *headlines[] = {
			"Bus arrives exactly on time; witnesses confused",
			"Local Mii waits patiently for loading screen",
			"Scientists confirm Minus button goes backwards",
			"Disc Channel asks everyone to stop staring",
			"Kitchen table officially declared a railway loop",
			"Forecast predicts suspiciously specific cloud",
			"Town replaces traffic lights with Wii pointers",
			"Settings button reportedly begins doing something"
		};
		fakeOverlayKind = FakeOverlayNews;
		fakeOverlayTitle = "OFFLINE NEWS";
		fakeOverlayLine1 = headlines[ rand() % 8 ];
		fakeOverlayLine2 = headlines[ rand() % 8 ];
		fakeOverlayLine3 = headlines[ rand() % 8 ];
	}
}

void MenuHandler::RenderFakeChannelOverlay( const Vec2f &ScreenProps ) const
{
	if( fakeOverlayKind == FakeOverlayNone )
		return;

	const GXColor panel = { 12, 18, 28, 232 };
	const GXColor title = { 88, 218, 255, 255 };
	const GXColor text = { 245, 248, 252, 255 };
	const float x = 56.0f;
	const float y = 88.0f;
	const float w = ScreenProps.x - 112.0f;
	const float h = 116.0f;

	GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	DrawSquare( x, y, w, h, panel );
	DrawOverlayText( x + 16.0f, y + 12.0f, fakeOverlayTitle, 2.0f, title, 54 );
	DrawOverlayText( x + 16.0f, y + 43.0f, fakeOverlayLine1, 1.5f, text, 76 );
	DrawOverlayText( x + 16.0f, y + 68.0f, fakeOverlayLine2, 1.5f, text, 76 );
	DrawOverlayText( x + 16.0f, y + 93.0f, fakeOverlayLine3, 1.5f, text, 76 );
}

bool MenuHandler::PrepareBannerSelection( int index )
{
	selectedBanner = NULL;
	bigBannerLayout = NULL;
	bigBannerObj = NULL;

	if( !IsChannelUsable( index ) )
	{
		return false;
	}

	if( index < 0 )
	{
		selectedIdx = -1;
		switch( discBannerState )
		{
		case DBSt_None:
			dcBanner->Reset();
			return true;
		case DBSt_Spinup:
			dcBanner->Reset();
			dcBanner->StartReadingDisc();
			return true;
		case DBSt_GCBanner:
			return true;
		case DBSt_Unknown:
			dcBanner->JumpToUnknownDiscAnim();
			return true;
		case DBSt_WiiBanner:
			if( !discChannelBanner )
			{
				discBannerState = DBSt_None;
				dcBanner->Reset();
				return true;
			}
			selectedBanner = discChannelBanner;
			break;
		}
	}
	else
	{
		if( !EnsureBannerForIndex( index ) )
		{
			MarkChannelBad( index, "could not create banner loader" );
			return false;
		}

		BannerAsync *asyncBanner = bannerList[ index ]->banner;
		for( int retry = 0; retry < 2000 && asyncBanner
			&& !asyncBanner->IsLoadComplete(); ++retry )
		{
			usleep( 1000 );
		}

		if( !asyncBanner || asyncBanner->DidLoadFail() )
		{
			MarkChannelBad( index, "banner/icon load failed" );
			return false;
		}
		if( !asyncBanner->IsLoadComplete() )
		{
			gprintf( "channel loader timed out, skipping for now: %i \"%s\"\n",
				index, bannerList[ index ]->filepath.c_str() );
			return false;
		}
		selectedBanner = asyncBanner;
	}

	if( !selectedBanner )
	{
		if( index >= 0 )
		{
			MarkChannelBad( index, "null banner" );
		}
		return false;
	}

	if( !( bigBannerObj = selectedBanner->LoadBanner() )
			|| !( bigBannerLayout = selectedBanner->getBanner() ) )
	{
		if( index >= 0 )
		{
			MarkChannelBad( index, "banner layout load failed" );
		}
		selectedBanner = NULL;
		bigBannerObj = NULL;
		bigBannerLayout = NULL;
		return false;
	}

	selectedIdx = index;
	bigBannerObj->Start();
	return true;
}

bool MenuHandler::MoveBannerSelection( int direction )
{
	if( direction != -1 && direction != 1 )
	{
		return false;
	}

	Banner *oldBanner = selectedBanner;
	Layout *oldLayout = bigBannerLayout;
	Object *oldObj = bigBannerObj;
	const int oldIdx = selectedIdx;
	const int total = (int)bannerList.size() + 1; // include Disc Channel at -1
	int nextIdx = selectedIdx;
	int circularCursor = selectedIdx;
	const bool hasExactDiscReturn = selectedIdx == -1
		&& discReturnIdx >= 0 && discReturnIdx < (int)bannerList.size()
		&& direction == -discEntryDirection;
	const int exactDiscReturnPage = discReturnPage;

	for( int checked = 0; checked < total; ++checked )
	{
		// Reversing the arrow used to enter Disc means "go back", so restore the
		// precise list entry rather than recalculating it from a sentinel index.
		// This matters when the origin is the final item on a partly filled page.
		if( checked == 0 && hasExactDiscReturn )
		{
			nextIdx = discReturnIdx;
			circularCursor = nextIdx;
		}
		else
		{
			circularCursor += direction;
			nextIdx = circularCursor;
		}
		if( nextIdx >= (int)bannerList.size() )
		{
			nextIdx = -1;
			circularCursor = nextIdx;
		}
		else if( nextIdx < -1 )
		{
			nextIdx = ((int)bannerList.size()) - 1;
			circularCursor = nextIdx;
		}

		if( nextIdx == oldIdx || !IsChannelUsable( nextIdx ) )
		{
			// Reversing away from Disc is an exact operation. Do not silently
			// replace a temporarily unavailable final channel with another banner.
			if( checked == 0 && hasExactDiscReturn )
				return false;
			continue;
		}

		if( PrepareBannerSelection( nextIdx ) )
		{
			if( nextIdx == -1 && oldIdx >= 0 )
			{
				discReturnIdx = oldIdx;
				discReturnPage = bannerReturnPage;
				discEntryDirection = direction;
				bannerReturnIdx = -1;
				bannerReturnPage = 0;
				grid->PinBannerLoader( oldIdx );
			}
			else if( oldIdx == -1 )
			{
				bannerReturnIdx = nextIdx;
				bannerReturnPage = hasExactDiscReturn
					? exactDiscReturnPage : ( nextIdx + 1 ) / 12;
				discReturnIdx = -2;
				discReturnPage = 0;
				discEntryDirection = 0;
				grid->ClearPinnedBannerLoader();
				grid->PinBannerLoader( nextIdx );
			}
			else
			{
				bannerReturnIdx = nextIdx;
				bannerReturnPage = ( nextIdx + 1 ) / 12;
				grid->PinBannerLoader( nextIdx );
			}
			if( oldBanner && oldBanner != selectedBanner )
			{
				oldBanner->UnloadBanner();
			}
			grid->SetPage( bannerReturnPage );
			grid->GetIconPaneCoords( selectedIdx, &AnimPosX1, &AnimPosY1, &AnimPosX2, &AnimPosY2 );
			return true;
		}

		selectedBanner = oldBanner;
		bigBannerLayout = oldLayout;
		bigBannerObj = oldObj;
		selectedIdx = oldIdx;
		if( checked == 0 && hasExactDiscReturn )
		{
			gprintf( "exact Disc return banner %i was not ready; staying on Disc\n",
				discReturnIdx );
			return false;
		}
	}

	gprintf( "no usable channel found while moving from %i direction %i\n", oldIdx, direction );
	return false;
}

void MenuHandler::ChannelIconClicked( u8 row, u8 col, int index )
{
	// The background grid is still rendered during the zoom. Never accept a
	// second icon (including the same icon) until the grid owns input again.
	if( state != St_ChannelGrid || homeMenuActive || freeCamera.IsActive() )
		return;
	bannerSwitchDirection = 0;
	if( index >= (int)bannerList.size() )
	{
		gprintf( "index >= bannerlist.size()\n" );
		return;
	}

	// TODO: clean this shit up
	// reset animation step for zooming
	AnimStep = 0;
	if( PrepareBannerSelection( index ) )
	{
		// Save the page that was really clicked.  This is deliberately not
		// derived from selectedIdx: the last page may be only partly populated,
		// and Disc occupies the synthetic index immediately before channel 0.
		bannerReturnIdx = index;
		bannerReturnPage = grid->CurrentPage();
		discReturnIdx = -2;
		discReturnPage = 0;
		discEntryDirection = 0;
		grid->PinBannerLoader( index );
		grid->SetPage( bannerReturnPage );
		grid->GetIconPaneCoords( selectedIdx, &AnimPosX1, &AnimPosY1, &AnimPosX2, &AnimPosY2 );
		state = St_BigBannerFadeIn;
	}
}

void MenuHandler::BannerFrameLeftButtonClicked()
{
	if( state != St_BigBanner || bannerSwitchDirection )
	{
		gprintf( "state != St_BigBanner\n" );
		return;
	}
	state = St_BigBannerFadeOut;
    AnimStep = 0;
}

void MenuHandler::BannerFrameRightButtonClicked()
{
	if( state != St_BigBanner || bannerSwitchDirection )
	{
		gprintf( "state != St_BigBanner\n" );
		return;
	}

	if( !Settings::launchOnStart )
	{
		// Preview-only is deliberately the safe default.
		delete nativeChannelView;
		nativeChannelView = NULL;
		state = St_BigBannerFadeOut;
		AnimStep = 0;
		return;
	}

	if( selectedIdx < 0 )
	{
		ShowLaunchError( "DISC LAUNCH IS NOT AVAILABLE YET", 0 );
		return;
	}
	if( selectedIdx >= (int)bannerList.size() || !bannerList[ selectedIdx ] )
	{
		ShowLaunchError( "SELECTED CHANNEL IS NO LONGER AVAILABLE", 0 );
		return;
	}

	BannerListEntry *entry = bannerList[ selectedIdx ];
	if( !entry->tid && !entry->hbXml )
	{
		ShowLaunchError( "DUMPED OR SD CHANNEL CANNOT BE LAUNCHED", 0 );
		return;
	}

	// Match the Wii Menu hand-off: the pointer disappears immediately and the
	// complete banner fades to black before any IOS/title-launch work begins.
	launchFadeFrame = 0;
	launchErrorFrames = 0;
	state = St_LaunchFade;
	CInputs::Instance()->ClearButtonsDown();
}

void MenuHandler::PerformPendingLaunch()
{
	if( selectedIdx < 0 || selectedIdx >= (int)bannerList.size()
			|| !bannerList[ selectedIdx ] )
	{
		state = St_BigBanner;
		ShowLaunchError( "SELECTED CHANNEL IS NO LONGER AVAILABLE", 0 );
		return;
	}

	BannerListEntry *entry = bannerList[ selectedIdx ];
	if( !entry->tid && !entry->hbXml )
	{
		state = St_BigBanner;
		ShowLaunchError( "DUMPED OR SD CHANNEL CANNOT BE LAUNCHED", 0 );
		return;
	}
	ReplacementActivity::BeginLaunch( BannerOrderKey( entry ) );

	StopBannerSound();
	Recovery::Pause();
	MenuAudio::Instance()->Reset();
	SoundHandler::DestroyInstance();
	const bool bannerWorkerStopped = BannerAsync::QuiesceForLaunch();
	const bool discWorkerWasAwake = Settings::mountDVD;
	if( discWorkerWasAwake )
		diHandler->Sleep();
	ShutdownAudio();

	s32 result = ContentLauncher::InvalidArgument;
	const char *kind = "CONTENT";
	if( entry->tid )
	{
		kind = "CHANNEL";
		result = ContentLauncher::LaunchInstalledTitle( entry->tid );
	}
	else if( entry->hbXml )
	{
		kind = "HOMEBREW";
		result = ContentLauncher::LaunchHomebrewDirectory( entry->filepath.c_str(),
			entry->hbXml->GetArguments() );
	}

	// A successful launch never comes back here.  Rebuild audio and retain the
	// banner when validation, I/O, or the platform launcher reports an error.
	if( bannerWorkerStopped )
		BannerAsync::RestartAfterLaunchFailure();
	if( discWorkerWasAwake )
		diHandler->Wake();
	InitAudio();
	Recovery::Resume();
	StartBannerSound();
	state = St_BigBanner;
	if( result == 0 )
		result = ContentLauncher::BooterReturned;
	ReplacementActivity::CancelLaunch();
	ShowLaunchError( kind, result );
}

void MenuHandler::RestartUpdatedPlayer()
{
	// Settings objects have been destroyed before entering this state. The
	// updater has joined its worker, so neither UI nor network owns an SD file.
	FadeDisplayedMenuToBlack();
	Recovery::Pause();
	const bool stopped=BannerAsync::QuiesceForLaunch();
	if(!stopped) {
		Recovery::Resume();
		PlayerUpdate::RestartFailed();
		state=St_SettingsSelect;
		return;
	}
	StopBannerSound();
	MenuAudio::Instance()->Reset();
	SoundHandler::DestroyInstance();
	const bool discAwake=Settings::mountDVD;
	if(discAwake) diHandler->Sleep();
	ShutdownAudio();
	// This launcher validates/loads the new DOL before any irreversible shutdown
	// and supplies argv[0], preserving the installation directory across reload.
	ContentLauncher::LaunchHomebrewDirectory(Settings::applicationPath.c_str());
	// Only validation/I/O failure returns. Keep the still-running app usable.
	BannerAsync::RestartAfterLaunchFailure();
	if(discAwake) diHandler->Wake();
	InitAudio();
	Recovery::Resume();
	PlayerUpdate::RestartFailed();
	state=St_SettingsSelect;
}

void MenuHandler::ShowLaunchError( const char *message, s32 error )
{
	char line[ 96 ];
	if( error )
		snprintf( line, sizeof( line ), "%s LAUNCH FAILED - ERROR %ld",
			message ? message : "CONTENT", (long)error );
	else
		snprintf( line, sizeof( line ), "%s", message ? message : "LAUNCH FAILED" );
	launchErrorMessage = line;
	launchErrorFrames = 300;
	gprintf( "%s\n", launchErrorMessage.c_str() );
}

void MenuHandler::RenderLaunchError( const Vec2f &screen )
{
	if( launchErrorFrames <= 0 || launchErrorMessage.empty() )
		return;
	GX_SetScissor( 0, 0, (u32)screen.x, (u32)screen.y );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	const GXColor panel = { 24, 28, 34, 238 };
	const GXColor border = { 91, 207, 244, 255 };
	DrawSquare( 42.0f, 24.0f, screen.x - 84.0f, 42.0f, panel );
	DrawSquare( 42.0f, 24.0f, screen.x - 84.0f, 3.0f, border );
	DrawOverlayText( 56.0f, 39.0f, launchErrorMessage, 1.5f, border, 78 );
	--launchErrorFrames;
}

void MenuHandler::LeftArrowBtnClicked()
{
	if( freeCamera.IsActive() ) return;
	switch( state )
	{
	default:
		break;
	case St_ChannelGrid:
	{
		grid->RequestShiftRight();
	}
	break;
	case St_BigBanner:
	{
		if( !bannerSwitchDirection )
		{
			bannerSwitchDirection = -1;
			StopBannerSound();
		}
	}
	break;
	}
}

void MenuHandler::RightArrowBtnClicked()
{
	if( freeCamera.IsActive() ) return;
	switch( state )
	{
	default:
		break;
	case St_ChannelGrid:
	{
		grid->RequestShiftLeft();
	}
	break;
	case St_BigBanner:
	{
		if( !bannerSwitchDirection )
		{
			bannerSwitchDirection = 1;
			StopBannerSound();
		}
	}
	break;
	}
}

static u8 GridEntryFadeAlpha( int frame, int frames )
{
	const float t = (float)frame / (float)frames;
	// Ease both ends, with exact black/clear endpoint frames.
	return (u8)(255.0f * (1.0f - t * t * (3.0f - 2.0f * t)) + 0.5f);
}

void MenuHandler::FadeDisplayedMenuToBlack()
{
	// Freeze the complete outgoing screen while fading. Do not re-run its
	// button callbacks or authored animations after they requested the exit.
	u32 length = 0;
	u16 width = 0, height = 0;
	u8 *pixels = CreateTextureFromDisplayBuffer( length, width, height );
	Texture *snapshot = NULL;
	if( pixels )
	{
		snapshot = new Texture;
		snapshot->LoadFromRawData( pixels, width, height, GX_TF_RGB565 );
	}
	Mtx44 savedProjection;
	memcpy( savedProjection, MainProjection, sizeof(Mtx44) );
	// A display-buffer snapshot is already screen-space; do not apply the
	// menu's cropped native-layout projection to it a second time.
	BuildHomeMenuProjection( MainProjection );
	const Vec2f screen = { (f32)screenwidth, (f32)screenheight };
	for( int frame = 0; frame <= WiiOptionsFadeFrames; ++frame )
	{
		CInputs::Instance()->Update();
		CInputs::Instance()->ClearButtonsDown();
		Recovery::SetCheckpoint( 0x150, "fading Wii Options transition" );
		LWP_MutexLock( drawMutex );
		Pane::ResetRenderClip();
		GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		DrawHomeSnapshot( snapshot, screen );
		DrawSquare( 0, 0, screen.x, screen.y,
			(GXColor){0,0,0,(u8)(255 - GridEntryFadeAlpha(frame, WiiOptionsFadeFrames))} );
		LWP_MutexUnlock( drawMutex );
		Menu_Render();
	}
	// Menu_Render fences every submitted snapshot frame before texture release.
	delete snapshot;
	free( pixels );
	memcpy( MainProjection, savedProjection, sizeof(Mtx44) );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	CInputs::Instance()->ClearButtonsDown();
}

bool MenuHandler::UpdateFreeCamera( const Vec2f &screen, bool allowInput )
{
	const bool stable = ( state == St_ChannelGrid && grid->IsIdleForInspection() )
		|| ( state == St_BigBanner && !bannerSwitchDirection )
		|| state == St_NativeChannel
		|| ( state == St_MessageBoard && messageBoard && !messageBoard->IsTransitioning() )
		|| ( (state == St_SettingsSelect || state == St_WiiSaves || state == St_ChannEdit)
			&& setupSelect && !setupSelect->IsTransitioning()
			&& !setupSelect->IsSystemSettingsTransition() && !setupSelect->IsApplyingTheme() )
		|| homeMenuActive;
	if( !stable )
	{
		freeCamera.SetActive( false );
		freeCameraTools.Close();
		freeCameraController = -1;
		return false;
	}
	if( !allowInput ) return freeCamera.IsActive();
	bool consumed = freeCamera.IsActive();
	freeCameraTools.SavePointers();
	if( freeCameraSettingSeen != Settings::freeCameraEnabled )
	{
		freeCameraSettingSeen = Settings::freeCameraEnabled;
		freeCamera.SetActive( Settings::freeCameraEnabled );
		freeCameraTools.Close();
		freeCameraController = -1;
		consumed = true;
	}
	for( int i = 0; i < 4; ++i )
	{
		// The Settings switch arms inspection everywhere. This shortcut resumes
		// it on the current page after B has returned control to that page.
		if( Settings::freeCameraEnabled &&
			((Pad(i).pOne() && Pad(i).hTwo()) || (Pad(i).pTwo() && Pad(i).hOne())) )
		{
			freeCamera.SetActive( !freeCamera.IsActive() );
			freeCameraTools.Close();
			freeCameraController = -1;
			consumed = true;
			break;
		}
	}
	const bool toolsWereOpen = freeCameraTools.IsOpen();
	bool toolbarClicked=false;
	if( freeCamera.IsActive() && toolsWereOpen ) freeCameraTools.Update( screen );
	else if( freeCamera.IsActive() )
		for( int i = 0; i < 4; ++i )
		{
			const auto &ir=Pad(i).GetData().ir;
			if(Pad(i).pA() && ir.valid && ir.y>=12 && ir.y<40) {
				if(ir.x>=screen.x-144 && ir.x<screen.x-16) {freeCamera.Toggle3D();toolbarClicked=true;break;}
				if(freeCamera.Is3D() && ir.x>=screen.x-280 && ir.x<screen.x-152) {freeCamera.ToggleLayers();toolbarClicked=true;break;}
			}
			const bool clickElements=Pad(i).pA() && ir.valid && ir.x>=8 && ir.x<228
				&& ir.y>=screen.y-38 && ir.y<screen.y-8;
			if( (Pad(i).pTwo() && !Pad(i).hOne()) || clickElements ) {
				freeCamera.DragOrbit(false,false,0,0);freeCameraTools.Open();break;
			}
			if( Pad(i).pOne() && !Pad(i).hTwo() ) RenderInspection::ToggleMasks();
		}
	if(toolbarClicked)freeCamera.DragOrbit(false,false,0,0);
	if(freeCameraTools.TakeInteractionRequest()) {freeCamera.SetActive(false);freeCameraController=-1;consumed=true;}
	if(freeCamera.Is3D() && !toolsWereOpen && !freeCameraTools.IsOpen())
		for(int i=0;i<4;++i)if(Pad(i).pB()) {freeCamera.SetActive(false);freeCameraController=-1;break;}
	if(freeCamera.Is3D()) {
		// A mouse/Classic controller cannot claim flight. Keep one physical
		// Remote + Nunchuk owner; detach pauses movement without resetting view.
		if(freeCameraController<0 || !Pad(freeCameraController).IsWiiRemoteConnected()
			|| Pad(freeCameraController).GetData().exp.type!=WPAD_EXP_NUNCHUK)
			freeCameraController=-1;
		for(int i=0;i<4 && freeCameraController<0;++i)
			if(Pad(i).IsWiiRemoteConnected() && Pad(i).GetData().exp.type==WPAD_EXP_NUNCHUK)
				freeCameraController=i;
	}
	for( int i = 0; i < 4; ++i )
	{
		// Camera controls can come from any logical player, including a remote
		// assigned behind a mouse. The Settings toggle owns permission to enter.
		if( !freeCamera.Is3D() && !toolbarClicked && !toolsWereOpen && !freeCameraTools.IsOpen() && freeCamera.IsActive() && (Pad(i).pA() || Pad(i).pB()
			|| Pad(i).hUp() || Pad(i).hDown() || Pad(i).hLeft() || Pad(i).hRight()
			|| Pad(i).hPlus() || Pad(i).hMinus() || Pad(i).pUp() || Pad(i).pDown()
			|| Pad(i).pLeft() || Pad(i).pRight() || Pad(i).pPlus() || Pad(i).pMinus()) )
		{
			freeCameraController = i;
			break;
		}
	}
	if( !toolbarClicked && !toolsWereOpen && !freeCameraTools.IsOpen() && freeCamera.IsActive() && freeCameraController >= 0 )
	{
		Controller &controller = Pad( freeCameraController );
		if( controller.pB() )
		{
			freeCamera.SetActive( false );
			freeCameraController = -1;
		}
		else if( controller.pA() ) freeCamera.ResetView();
		else if(freeCamera.Is3D())
		{
			const auto &data=controller.GetData();
			float mag=data.exp.nunchuk.js.mag,angle=data.exp.nunchuk.js.ang;
			if(!std::isfinite(mag) || !std::isfinite(angle)){mag=0;angle=0;}
			mag=std::max(0.f,std::min(1.f,mag));
			mag=mag<=.18f ? 0.f : (mag-.18f)/.82f;
			const float rad=.01745329252f;
			// Pointing (including the synthesized Nunchuk pointer) never turns
			// the camera. The physical owner's D-pad controls its view instead.
			// Nunchuk C/Z share bit positions with Classic Left/Up, so do not
			// use Controller's combined expansion-button helpers here.
			const float lookX=(float)(((data.btns_h & WPAD_BUTTON_RIGHT)!=0)
				- ((data.btns_h & WPAD_BUTTON_LEFT)!=0));
			const float lookY=(float)(((data.btns_h & WPAD_BUTTON_UP)!=0)
				- ((data.btns_h & WPAD_BUTTON_DOWN)!=0));
			const int rise=((data.btns_h & WPAD_NUNCHUK_BUTTON_C)!=0)-((data.btns_h & WPAD_NUNCHUK_BUTTON_Z)!=0);
			const bool pal50=_CONF_GetVideo()==CONF_VIDEO_PAL && _CONF_GetEuRGB60()==0;
			freeCamera.Fly(mag*std::sin(angle*rad),mag*std::cos(angle*rad),(float)rise,
				lookX,lookY,controller.pPlus()-controller.pMinus(),pal50 ? 1.f/50 : 1.f/60);
		}
		else
		{
			const int horizontal = (controller.hRight() || controller.pRight())
				- (controller.hLeft() || controller.pLeft());
			const int vertical = (controller.hDown() || controller.pDown())
				- (controller.hUp() || controller.pUp());
			const int zoomDirection = (controller.hPlus() || controller.pPlus())
				- (controller.hMinus() || controller.pMinus());
			const bool pal50 = _CONF_GetVideo() == CONF_VIDEO_PAL && _CONF_GetEuRGB60() == 0;
			freeCamera.Move( horizontal, vertical, zoomDirection,
				pal50 ? 1.0f/50.0f : 1.0f/60.0f, screen.x, screen.y );
		}
	}
	if( consumed )
	{
		// Even the exit/reset edge belongs to the camera, not a Start button or
		// icon underneath it. HOME was handled before this function is called.
		CInputs::Instance()->ClearButtonsDown();
		for( int i = 0; i < 4; ++i ) Pad(i).SuppressUiInput();
		u16 unusedKey;
		while( CInputs::Instance()->PopUsbKey( unusedKey ) ) {}
	}
	return consumed;
}

void MenuHandler::RenderFreeCameraHud( const Vec2f &screen ) const
{
	Mtx44 projection;
	BuildHomeMenuProjection( projection );
	Pane::ResetRenderClip();
	GX_LoadProjectionMtx( projection, GX_ORTHOGRAPHIC );
	if(!freeCameraTools.IsPicking())
	{
	char title[96];
	snprintf( title, sizeof(title), "%s  %.2fx",Localization::GetUtf8("Free camera"),freeCamera.Is3D() ? freeCamera.FlySpeed() : freeCamera.Zoom());
	DrawSquare( 8, 8, screen.x - 16, freeCamera.Is3D() ? 86 : 62, (GXColor){0,0,0,200} );
	DrawOverlayText( 18, 17, title, 1.5f, (GXColor){255,255,255,255}, 0 );
	AppSettingsScreen::DrawInterfaceText(18,42,screen.x-36,Localization::GetUtf8(freeCamera.Is3D()
		? (freeCameraController<0 ? "Connect a Nunchuk to fly" : "Stick: Move   D-pad: Look   C/Z: Up/Down")
		: "D-pad: Pan   +/-: Zoom   A: Reset   B: Exit"),17,12,(GXColor){150,220,255,255});
	if(freeCamera.Is3D())AppSettingsScreen::DrawInterfaceText(18,65,screen.x-36,
		Localization::GetUtf8("+/-: Speed   A: Reset   B: Exit"),17,12,(GXColor){150,220,255,255});
	for(int action=0;action<2;++action)if(action || freeCamera.Is3D()) {
		const float x=screen.x-(action ? 144 : 280);
		DrawSquare(x,12,128,28,freeCameraTools.PointerOver(x,12,128,28)
			? (GXColor){30,98,130,255} : (GXColor){30,75,100,245});
		AppSettingsScreen::DrawInterfaceText(x+8,19,112,Localization::GetUtf8(action
			? (freeCamera.Is3D() ? "3D: On" : "3D: Off")
			: (freeCamera.LayersEnabled() ? "Layers: On" : "Layers: Off")),15,11,(GXColor){245,248,252,255});
	}
	DrawSquare(8,screen.y-38,220,30,freeCameraTools.PointerOver(8,screen.y-38,220,30)
		? (GXColor){30,98,130,255} : (GXColor){8,15,24,220});
	DrawOverlayText(18,screen.y-29,Localization::GetUtf8("Elements"),1.2f,(GXColor){150,220,255,255},0);
	DrawOverlayText(236,screen.y-29,Localization::GetUtf8("2: Elements   1: Masks"),1.2f,(GXColor){150,220,255,255},0);
	}
	freeCameraTools.Render( screen );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
}

void MenuHandler::DoGrid()
{
	const u32 sources = (Settings::useNandBanners ? 1 : 0)
		| (Settings::useDumpedBanners ? 2 : 0)
		| (Settings::useHomebrewForBanners ? 4 : 0);
	const bool resumeGrid = !suspendedGridBanners.empty()
		&& sources == suspendedGridSources && !SystemMenuBannerRefreshPending();
	if( resumeGrid ) bannerList.swap( suspendedGridBanners );
	else
	{
		ReleaseSuspendedGrid();
		BuildBannerList();
	}
	if( bannerList.empty() )
	{
		// An empty NAND/SD/homebrew library is a valid state.  Keep the shell on
		// its Disc Channel plus eleven empty slots and discard any selection left
		// behind by a previous, now-disabled source.
		selectedIdx = -1;
		bannerReturnIdx = -1;
		bannerReturnPage = 0;
		selectedBanner = NULL;
		bigBannerObj = NULL;
		bigBannerLayout = NULL;
		discReturnIdx = -2;
		discReturnPage = 0;
		discEntryDirection = 0;
		gprintf( "No channel banners found; keeping empty grid open\n" );
	}
	MenuAudio::Instance()->StartMusic();

	// get resources
	LWP_MutexLock( drawMutex );
	BannerFrame *frame = resources->CreateBannerFrame();

	// check for errors
	if( !bg || !grid || !frame || !dcIcon || !dcBanner || !gcBanner )
	{
		LWP_MutexUnlock( drawMutex );
		FatalError( "Error loading resources for the grid view\n" );
		return;
	}

	if( discChannelBanner )
	{
		if( discBannerState == DBSt_GCBanner )
		{
			grid->DiscInserted( DiHandler::T_GC );
		}
		else if( discBannerState == DBSt_WiiBanner )
		{
			grid->DiscInserted( DiHandler::T_Wii );
		}
		Object *o = discChannelBanner->LoadIcon();
		Layout *l = discChannelBanner->getIcon();
		grid->SetDiscChannelIcon( l, o );
	}

	// connect arrow buttons to the grid
	//buttons->LeftArrow()->Clicked.connect( grid, &ChannelGrid::RequestShiftRight );
	//buttons->RightArrow()->Clicked.connect( grid, &ChannelGrid::RequestShiftLeft );
	//buttonPanel->SettingsBtnClicked.connect( this, &MenuHandler::SettingsBtnClicked );

	grid->FirstPage.connect( buttonPanel, &ButtonPanel::HideLeftArrow );
	grid->LastPage.connect( buttonPanel, &ButtonPanel::HideRightArrow );

	// listen for clicked buttons in the large frame
	frame->RightBtnClicked.connect( this, &MenuHandler::BannerFrameRightButtonClicked );
	frame->LeftBtnClicked.connect( this, &MenuHandler::BannerFrameLeftButtonClicked );

	if( resumeGrid ) grid->SetPage( suspendedGridPage );
	else grid->ChannelListChanged();

	Vec2f ScreenProps;
	ScreenProps.x = screenwidth;
	ScreenProps.y = screenheight;
	bool widescreen = ( _CONF_GetAspectRatio() == 1);
	int homeMenuDelayFrames = -1;
	int homeUnavailableFrames = 0;

	LWP_MutexUnlock( drawMutex );

	// Publish the entire sliding window together, including edge columns.
	// Input and recovery keep running. Bad archives settle as placeholders.
	for( int warmFrame = 0; state == St_ChannelGrid
		&& !grid->VisiblePageIconsReady() && warmFrame < 900; ++warmFrame )
	{
		CInputs::Instance()->Update();
		Recovery::SetCheckpoint( 0x120, "preparing first channel page" );
		LWP_MutexLock( drawMutex );
		GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
		GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y, (GXColor){0,0,0,255} );
		LWP_MutexUnlock( drawMutex );
		Menu_Render();
	}
	if( gridFadeInPending )
	{
		gridFadeInPending = false;
		// Health has faded fully to black; only reveal the menu after the first
		// icon window is ready. Keep clicks/hover disabled and the native clock
		// label held during the fade so neither is consumed behind black.
		const int fadeFrames = gridFadeInFrames;
		for( int fadeFrame = 0; state == St_ChannelGrid && fadeFrame <= fadeFrames; ++fadeFrame )
		{
			CInputs::Instance()->Update();
			Recovery::SetCheckpoint( 0x120, "fading in prepared channel menu" );
			LWP_MutexLock( drawMutex );
			Pane::ResetRenderClip();
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			bg->Render( GXmodelView2D, ScreenProps, widescreen );
			buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Footer );
			grid->Render( GXmodelView2D, ScreenProps, widescreen, false );
			buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
			Pane::ResetRenderClip();
			DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y,
				(GXColor){0,0,0,GridEntryFadeAlpha(fadeFrame, fadeFrames)} );
			LWP_MutexUnlock( drawMutex );
			Menu_Render();
		}
	}
	CInputs::Instance()->ClearButtonsDown();

	while( state == St_ChannelGrid
		   || state == St_BigBannerFadeIn
		   || state == St_BigBanner
		   || state == St_BigBannerFadeOut
		   || state == St_NativeChannel
		   || state == St_LaunchFade
		   || state == St_MessageBoard )
	{
		Recovery::SetCheckpoint( 0x110, "reading menu controllers" );
		// Keep already-connected mouse reports live in every view, but defer USB
		// enumeration, descriptors and diagnostic SD writes to the stable grid.
		// Those synchronous IOS/filesystem calls used to collide with a banner
		// opener and were reported by the watchdog as WSM-H110.
		CInputs::Instance()->AllowUsbStartup( state == St_ChannelGrid );
		if( state == St_ChannelGrid )
			MenuAudio::Instance()->StartMusic();

		//! Update all inputs
		CInputs::Instance()->Update();
		Recovery::SetCheckpoint( 0x121, "processing channel-menu actions" );

		int primaryController = -1;
		int homeController = -1;
		for( int i = 0; i < 4; ++i )
		{
			if( primaryController < 0 && ( Pad( i ).pA() || Pad( i ).pB()
				|| Pad( i ).pLeft() || Pad( i ).pRight()
				|| Pad( i ).pPlus() || Pad( i ).pMinus() ) )
				primaryController = i;
			if( homeController < 0 && Pad( i ).pHome() )
				homeController = i;
		}

		if( homeMenuDelayFrames < 0 && state != St_LaunchFade
			&& homeController >= 0 )
		{
			if( Settings::directHomeExit )
			{
				state = St_Exit;
				break;
			}
			// The displayed XFB still contains the pointer from the preceding
			// frame.  Draw a few normal frames without any cursor first, then
			// capture that clean XFB for HOME's frozen background.
			homeMenuDelayFrames = HomeCursorHideFrames;
			CInputs::Instance()->ClearButtonsDown();
		}
		else if( homeController >= 0 && state == St_LaunchFade )
		{
			homeUnavailableFrames = 90;
			Pad( homeController ).Take();
		}
		if( homeMenuDelayFrames >= 0 )
		{
			// HOME owns the next action.  Suppress button edges during the short
			// cursor-removal lead-in so nothing underneath can be activated.
			primaryController = -1;
			CInputs::Instance()->ClearButtonsDown();
		}
		const bool cameraOwnedInput = UpdateFreeCamera( ScreenProps, homeMenuDelayFrames < 0 );
		if( cameraOwnedInput ) primaryController = -1;
		if( state == St_BigBanner && primaryController >= 0
			&& selectedIdx >= 0 && selectedIdx < (int)bannerList.size()
			&& bannerList[ selectedIdx ] )
		{
			const std::string activityKey = BannerOrderKey(
				bannerList[ selectedIdx ] );
			if( Pad( primaryController ).pOne() )
			{
				const bool favorite = ReplacementActivity::ToggleFavorite(
					activityKey );
				ShowLaunchError( favorite ? "ADDED TO FAVORITES"
					: "REMOVED FROM FAVORITES", 0 );
				CInputs::Instance()->ClearButtonsDown();
			}
			else if( Pad( primaryController ).pTwo() )
			{
				ReplacementActivity::Record record;
				char status[ 96 ];
				if( ReplacementActivity::GetRecord( activityKey, record ) )
					snprintf( status, sizeof( status ),
						"PLAYED %lu TIMES - %lu MINUTES",
						(unsigned long)record.launches,
						(unsigned long)( record.seconds / 60 ) );
				else snprintf( status, sizeof( status ), "NOT PLAYED YET" );
				ShowLaunchError( status, 0 );
				CInputs::Instance()->ClearButtonsDown();
			}
		}

		if(!cameraOwnedInput && Pad(0).hPlus() && Pad(0).pMinus())
			PrintScreenshot();

		if( state == St_NativeChannel && primaryController >= 0
			&& Pad( primaryController ).pB() )
		{
			delete nativeChannelView;
			nativeChannelView = NULL;
			state = St_BigBanner;
			CInputs::Instance()->ClearButtonsDown();
			StartBannerSound();
		}
		else if( state == St_NativeChannel && nativeChannelView )
		{
			if( primaryController >= 0 && ( Pad( primaryController ).pLeft()
				|| Pad( primaryController ).pMinus() ) )
			{
				nativeChannelView->PreviousPage();
				CInputs::Instance()->ClearButtonsDown();
			}
			else if( primaryController >= 0 && ( Pad( primaryController ).pRight()
				|| Pad( primaryController ).pPlus() ) )
			{
				nativeChannelView->NextPage();
				CInputs::Instance()->ClearButtonsDown();
			}
			else if( primaryController >= 0 && Pad( primaryController ).pA() )
			{
				nativeChannelView->Regenerate();
				CInputs::Instance()->ClearButtonsDown();
			}
		}

		Recovery::SetCheckpoint( 0x122, "channel-menu actions complete" );
		bool launchNow = false;
		Recovery::SetCheckpoint( 0x120, "rendering channel menu" );
		LWP_MutexLock( drawMutex );
		const bool cameraFrame = freeCamera.IsActive();
		const bool cameraBanner = cameraFrame && state == St_BigBanner;
		Mtx44 cameraSavedMain, cameraSavedBanner;
		if( cameraFrame )
		{
			const RenderInspection::Context context = state == St_BigBanner ? RenderInspection::Banner
				: state == St_MessageBoard ? RenderInspection::Board
				: state == St_NativeChannel ? RenderInspection::Native : RenderInspection::Grid;
			RenderInspection::ConfigureSpectator(freeCamera.Is3D(),freeCamera.Yaw(),freeCamera.Pitch(),freeCamera.LayersEnabled(),freeCamera.FlyX(),freeCamera.FlyY(),freeCamera.FlyZ());
			RenderInspection::BeginFrame( context, state == St_ChannelGrid
				? (unsigned)grid->CurrentPage() : (unsigned)(selectedIdx+2) );
			memcpy( cameraSavedMain, MainProjection, sizeof(Mtx44) );
			freeCamera.ApplyProjection( MainProjection );
			if( cameraBanner )
			{
				memcpy( cameraSavedBanner, BannerProjection, sizeof(Mtx44) );
				freeCamera.ApplyProjection( BannerProjection );
			}
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		}
		switch( state )
		{
		default:
			break;
		case St_NativeChannel:
			GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			if( nativeChannelView ) nativeChannelView->Render( GXmodelView2D, ScreenProps, widescreen );
			break;
		case St_ChannelGrid:
			if( !cameraOwnedInput ) buttonPanel->UpdateInput();
			bg->Render( GXmodelView2D, ScreenProps, widescreen );
			buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Footer );
			grid->Render( GXmodelView2D, ScreenProps, widescreen,
				!cameraOwnedInput && state == St_ChannelGrid && homeMenuDelayFrames < 0 );
			buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
			break;
		case St_MessageBoard:
			if( !messageBoard )
			{
				messageBoard = resources->CreateMessageBoard();
				if( !messageBoard )
				{
					LWP_MutexUnlock( drawMutex );
					FatalError( "Error loading the Wii Message Board resources" );
					continue;
				}
				// Capture a clean complete menu, not just board.ash and not the
				// pointer from the frame that received the click.
				bg->Render( GXmodelView2D, ScreenProps, widescreen );
				buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Footer );
				grid->Render( GXmodelView2D, ScreenProps, widescreen, false );
				buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
				Menu_Render();
				messageBoard->Open( bg );
			}
			messageBoard->Update( ScreenProps );
			GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			messageBoard->Render( GXmodelView2D, ScreenProps, widescreen );
			if( messageBoard->IsClosed() )
			{
				const bool openSettings = messageBoard->RequestedSettings();
				enterSystemSettingsDirect = openSettings;
				delete messageBoard;
				messageBoard = NULL;
				resources->DestroyMessageBoard();
				state = openSettings ? St_SettingsSelect : St_ChannelGrid;
				CInputs::Instance()->ClearButtonsDown();
			}
			break;
		case St_BigBannerFadeIn:// TODO: calculate some cool scale matrix up in here
		{
			if( selectedIdx == -1 )
			{
				/*switch( discBannerState )
				{
				case DBSt_None:
					dcBanner->Reset();
					break;
				case DBSt_Spinup:
					dcBanner->Reset();
					dcBanner->StartReadingDisc();
					break;
				case DBSt_GCBanner:
					gprintf( "need the gamecube banner.  exiting...\n" );
					exit( 0 );
					break;
				case DBSt_WiiBanner:// dont do anything
					break;
				case DBSt_Unknown:
					dcBanner->JumpToUnknownDiscAnim();
					break;
				}*/
			}
			else
			{
				if( !bigBannerLayout || !bigBannerObj )
				{
					gprintf( "tried to show a banner with none loaded\n" );
					LWP_MutexUnlock( drawMutex );
					state = St_ChannelGrid;
					continue;
				}
			}

			// Reset at the frame boundary, never inside a button's Finished slot.
			if( AnimStep == 0 ) frame->ResetButtons();
            AnimateZoom(ScreenProps, true);

            // TODO cleanup and optimize the shit
            if(AnimationRunning)
            {
                // load projection for all the usual gui
                GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);

                bg->Render( GXmodelView2D, ScreenProps, widescreen );
                buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Footer );
                grid->Render( GXmodelView2D, ScreenProps, widescreen, false );
                buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
                // Draw black bg shit
                DrawSquare(0, 0, ScreenProps.x, ScreenProps.y, (GXColor) {0,0,0,BGAlpha});

                // load projection for the banner and frame
                GX_LoadProjectionMtx(BannerProjection, GX_ORTHOGRAPHIC);

                // cut the unneeded crap
                Mtx mv1, mv2, mv3;
                guMtxIdentity (mv2);
                guMtxIdentity (mv3);
                guMtxScaleApply(GXmodelView2D,mv1, 1.f, -1.f, 1.f);
                guMtxTransApply(mv1,mv1, 0.5f * ScreenProps.x, 0.5f * ScreenProps.y, 0.f);
                guMtxTransApply(mv2,mv2, -0.5f * fBannerWidth, 0.5f * fBannerHeight, 0.f);
                guMtxTransApply(mv3,mv3, 0.5f * fBannerWidth, -0.5f * fBannerHeight, 0.f);
                guMtxConcat (mv1, mv2, mv2);
                guMtxConcat (mv1, mv3, mv3);

                f32 viewportv[6];
                f32 projectionv[7];

                GX_GetViewportv(viewportv);
                GX_GetProjectionv(BannerProjection, projectionv, GX_ORTHOGRAPHIC);

                guVector vecTL;
                guVector vecBR;
                GX_Project(0.0f, 0.0f, 0.0f, mv2, projectionv, viewportv, &vecTL.x, &vecTL.y, &vecTL.z);
                GX_Project(0.0f, 0.0f, 0.0f, mv3, projectionv, viewportv, &vecBR.x, &vecBR.y, &vecBR.z);

                SetBannerRenderClip(vecTL, vecBR, BannerProjection);

                if(BannerAlpha != 0)
                {
                    if( selectedIdx == -1 )
                    {
                        switch( discBannerState )
                        {
                        case DBSt_Unknown:
                        case DBSt_None:
                        case DBSt_Spinup:
                            //TODO: add banner alpha
                            dcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
                            break;
                        case DBSt_GCBanner:
                            //TODO: add banner alpha
                            gcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
                            break;
                        case DBSt_WiiBanner:
							if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
							bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha );
                            selectedBanner->AdvanceBanner();
                            break;
                        }
                    }
                    else
                    {
						if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
						bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha );
                        selectedBanner->AdvanceBanner();
                    }
                    frame->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha, false );
                }
                // revert scissor and projection
                Pane::ResetRenderClip();
                GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);
                break;
            }
            else
            {
                state = St_BigBanner;

                // setup the button panel to go along with the big banner frame
                buttonPanel->ShowArrowsOnly( true );
                buttonPanel->HideLeftArrow( false );
                buttonPanel->HideRightArrow( false );

                grid->FirstPage.disconnect( buttonPanel );
                grid->LastPage.disconnect( buttonPanel );

                // start playing the sound
                StartBannerSound();

                // no break here to render this frame directly with big banner stuff
                // and not skip it and have a black blink
            }
		}
		case St_LaunchFade:
			// BannerFrame keeps click feedback for one second, then clears it
			// at a safe render boundary. The launch delay/fade is unchanged.
		case St_BigBanner:
		{
			// Swap at the next frame boundary, without moving the banner or chrome.
			if( bannerSwitchDirection )
			{
				// The outgoing layout may free textures; finish the previous GX frame.
				GX_DrawDone();
				MoveBannerSelection( bannerSwitchDirection );
				bannerSwitchDirection = 0;
				frame->ResetButtons();
				StartBannerSound();
			}
			GX_LoadProjectionMtx(BannerProjection, GX_ORTHOGRAPHIC);
			if( state == St_BigBanner && !bannerSwitchDirection && !cameraOwnedInput )
			{
				// respond to user input
				for( int i = 0; i < 4; i++ )
				{
					if( Pad( i ).pMinus() )
					{
						LeftArrowBtnClicked();
					}
					else if( Pad( i ).pPlus() )
					{
						RightArrowBtnClicked();
					}
					else if( Pad( i ).pB() )
					{
						BannerFrameLeftButtonClicked();
					}
				}
			}

            // cut the unneeded crap
            Mtx mv1, mv2, mv3;
            guMtxIdentity (mv2);
            guMtxIdentity (mv3);
            guMtxScaleApply(GXmodelView2D,mv1, 1.f, -1.f, 1.f);
            guMtxTransApply(mv1,mv1, 0.5f * ScreenProps.x, 0.5f * ScreenProps.y, 0.f);
            guMtxTransApply(mv2,mv2, -0.5f * fBannerWidth, 0.5f * fBannerHeight, 0.f);
            guMtxTransApply(mv3,mv3, 0.5f * fBannerWidth, -0.5f * fBannerHeight, 0.f);
            guMtxConcat (mv1, mv2, mv2);
            guMtxConcat (mv1, mv3, mv3);

            f32 viewportv[6];
            f32 projectionv[7];

            GX_GetViewportv(viewportv);
            GX_GetProjectionv(BannerProjection, projectionv, GX_ORTHOGRAPHIC);

            guVector vecTL;
            guVector vecBR;
            GX_Project(0.0f, 0.0f, 0.0f, mv2, projectionv, viewportv, &vecTL.x, &vecTL.y, &vecTL.z);
            GX_Project(0.0f, 0.0f, 0.0f, mv3, projectionv, viewportv, &vecBR.x, &vecBR.y, &vecBR.z);

            SetBannerRenderClip(vecTL, vecBR, BannerProjection);

			if( selectedIdx == -1 )
			{
				switch( discBannerState )
				{
				case DBSt_Unknown:
				case DBSt_None:
				case DBSt_Spinup:
					dcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
					break;
				case DBSt_GCBanner:
					gcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
					break;
				case DBSt_WiiBanner:
					if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
					bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen );
					selectedBanner->AdvanceBanner();
					break;
				}
			}
			else
			{
				if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
				bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen );
				selectedBanner->AdvanceBanner();
			}

			// Forecast/News full applications are not part of banner.bin.  Draw
			// generated session data in the actual System Menu banner path so it
			// is guaranteed visible regardless of the retail pane names.
			GX_LoadProjectionMtx(BannerProjection, GX_ORTHOGRAPHIC);

			// Restore screen-space clipping and matrix after arbitrary channel code.
			// Base0/Base1 extend 12 pixels below the authored 456-pixel frame.
			// Keep the frame clipped too, so its striped backing cannot leak out.
			SetBannerRenderClip(vecTL, vecBR, BannerProjection);
			LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
			if( !bannerSwitchDirection && !cameraOwnedInput ) buttonPanel->UpdateInput();
			frame->Render( GXmodelView2D, ScreenProps, widescreen, 255,
				state == St_BigBanner && !bannerSwitchDirection && !cameraOwnedInput );
			Pane::ResetRenderClip();
			buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
		}
		break;
		case St_BigBannerFadeOut:// TODO: render with a scale matrix and zoom back to the grid
		{
			if( AnimStep == 0 ) frame->ResetButtons();
            AnimateZoom(ScreenProps, false);

            // TODO cleanup and optimize the shit
            if(AnimationRunning)
            {
                // load projection for all the usual gui
                GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);

                bg->Render( GXmodelView2D, ScreenProps, widescreen );
                buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Footer );
                grid->Render( GXmodelView2D, ScreenProps, widescreen, false );
                buttonPanel->Render( GXmodelView2D, ScreenProps, widescreen, ButtonPanel::Arrows );
                // Draw black bg shit
                DrawSquare(0, 0, ScreenProps.x, ScreenProps.y, (GXColor) {0,0,0,BGAlpha});

                // load projection for the banner and frame
                GX_LoadProjectionMtx(BannerProjection, GX_ORTHOGRAPHIC);

                // cut the unneeded crap
                Mtx mv1, mv2, mv3;
                guMtxIdentity (mv2);
                guMtxIdentity (mv3);
                guMtxScaleApply(GXmodelView2D,mv1, 1.f, -1.f, 1.f);
                guMtxTransApply(mv1,mv1, 0.5f * ScreenProps.x, 0.5f * ScreenProps.y, 0.f);
                guMtxTransApply(mv2,mv2, -0.5f * fBannerWidth, 0.5f * fBannerHeight, 0.f);
                guMtxTransApply(mv3,mv3, 0.5f * fBannerWidth, -0.5f * fBannerHeight, 0.f);
                guMtxConcat (mv1, mv2, mv2);
                guMtxConcat (mv1, mv3, mv3);

                f32 viewportv[6];
                f32 projectionv[7];

                GX_GetViewportv(viewportv);
                GX_GetProjectionv(BannerProjection, projectionv, GX_ORTHOGRAPHIC);

                guVector vecTL;
                guVector vecBR;
                GX_Project(0.0f, 0.0f, 0.0f, mv2, projectionv, viewportv, &vecTL.x, &vecTL.y, &vecTL.z);
                GX_Project(0.0f, 0.0f, 0.0f, mv3, projectionv, viewportv, &vecBR.x, &vecBR.y, &vecBR.z);

                SetBannerRenderClip(vecTL, vecBR, BannerProjection);

                if(BannerAlpha != 0)
                {
                    if( selectedIdx == -1 )
                    {
                        switch( discBannerState )
                        {
                        case DBSt_Unknown:
                        case DBSt_None:
                        case DBSt_Spinup:
                            //TODO: add banner alpha
                            dcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
                            break;
                        case DBSt_GCBanner:
                            //TODO: add banner alpha
                            gcBanner->Render( GXmodelView2D, ScreenProps, widescreen );
                            break;
                        case DBSt_WiiBanner:
							if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
							bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha );
                            selectedBanner->AdvanceBanner();
                            break;
                        }
                    }
                    else
                    {
						if( selectedBanner ) selectedBanner->RefreshGeneratedChannelText();
						bigBannerLayout->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha );
                        selectedBanner->AdvanceBanner();
                    }
                    frame->Render( GXmodelView2D, ScreenProps, widescreen, BannerAlpha, false );
                }
                // revert scissor and projection
                Pane::ResetRenderClip();
                GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);
            }
            else
            {
                state = St_ChannelGrid;

                // setup the button panel again for the grid mode
                buttonPanel->ShowArrowsOnly( false );

                grid->FirstPage.connect( buttonPanel, &ButtonPanel::HideLeftArrow );
                grid->LastPage.connect( buttonPanel, &ButtonPanel::HideRightArrow );

				Recovery::SetCheckpoint( 0x131, "closing channel banner" );
				int returnPage = bannerReturnPage;
				if( returnPage < 0 || returnPage >= grid->PageCount() )
					returnPage = ( selectedIdx + 1 ) / 12;
				StopBannerSound();
				if( selectedBanner )
				{
					// Keep the selected loader alive until its large layout is gone.
					// Unpinning first allowed the async cleanup thread to delete it
					// while UnloadBanner() still used it on a cold boot.
					selectedBanner->UnloadBanner();
					bigBannerLayout = NULL;
					bigBannerObj = NULL;
					selectedBanner = NULL;
				}
				grid->ClearPinnedBannerLoader();
				grid->SetPage( returnPage );
				gprintf( "Banner return: selected %i, origin %i, page %i\n",
					selectedIdx, bannerReturnIdx, returnPage );
                // do a new cycle to render this frame and not leave it blank blink
                LWP_MutexUnlock( drawMutex );
                continue;
            }
		}
        break;
		}

		if( cameraFrame )
		{
			RenderInspection::EndFrame();
			freeCameraTools.CapturePreviews();
			Pane::ResetRenderClip();
			memcpy( MainProjection, cameraSavedMain, sizeof(Mtx44) );
			if( cameraBanner ) memcpy( BannerProjection, cameraSavedBanner, sizeof(Mtx44) );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			RenderFreeCameraHud( ScreenProps );
			freeCameraTools.RestorePointers();
		}

		if( state == St_LaunchFade )
		{
			GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			// Hold the selected banner for two seconds, then perform the normal
			// half-second Wii Menu fade. The old build incorrectly stretched the
			// fade itself across the full two seconds.
			const int waitFrames = ( _CONF_GetVideo() == CONF_VIDEO_PAL
				&& _CONF_GetEuRGB60() < 1 ) ? 100 : 120;
			const int fadeFrames = ( _CONF_GetVideo() == CONF_VIDEO_PAL
				&& _CONF_GetEuRGB60() < 1 ) ? 25 : 30;
			if( launchFadeFrame >= waitFrames )
			{
				const int fadePosition = launchFadeFrame - waitFrames;
				const u8 alpha = (u8)MIN( 255,
					( fadePosition * 255 ) / fadeFrames );
				DrawSquare( 0.0f, 0.0f, ScreenProps.x, ScreenProps.y,
					(GXColor){ 0, 0, 0, alpha } );
			}
			if( ++launchFadeFrame >= waitFrames + fadeFrames )
				launchNow = true;
		}
		else
		{
			RenderLaunchError( ScreenProps );
		}
		RenderHomeUnavailable( ScreenProps, homeUnavailableFrames );
		//dcIcon->Render( GXmodelView2D, ScreenProps, widescreen );
		if( state != St_LaunchFade && homeMenuDelayFrames < 0
			&& !(messageBoard && messageBoard->IsTransitioning())
			&& !enterSystemSettingsDirect && !wiiOptionsFadePending )
			cursors->Render( GXmodelView2D, ScreenProps, widescreen );


		LWP_MutexUnlock( drawMutex );
		Menu_Render();
		if( homeMenuDelayFrames >= 0 && --homeMenuDelayFrames <= 0 )
		{
			homeMenuDelayFrames = -1;
			DoHomeMenu();
		}
		if( launchNow )
			PerformPendingLaunch();
	}

	freeCamera.SetActive( false );
	freeCameraController = -1;
	if( state == St_SettingsSelect && wiiOptionsFadePending )
		FadeDisplayedMenuToBlack();

	grid->FirstPage.disconnect( buttonPanel );
	grid->LastPage.disconnect( buttonPanel );

	delete frame;

	// done playing banner sound
	StopBannerSound();

	// free the buffer containing the decompressed ash data for the big frame
	resources->DestroyBannerFrame();

	// make sure the grid doesnt still contain pointers to banners, since we might be going to the settings to delete or add a channel
	const bool suspendGrid = state == St_SettingsSelect;
	suspendedGridPage = grid->CurrentPage();
	suspendedGridSources = sources;
	grid->UnbindAllChannels( !suspendGrid );
	selectedBanner = NULL;

	if( suspendGrid ) suspendedGridBanners.swap( bannerList );
	else FreeBannerList();
}

void MenuHandler::ReleaseSuspendedGrid()
{
	if( suspendedGridBanners.empty() ) return;
	// Data Management owns a different temporary list; do not overwrite it.
	List< BannerListEntry * > settingsBanners;
	settingsBanners.swap( bannerList );
	bannerList.swap( suspendedGridBanners );
	grid->UnbindAllChannels();
	FreeBannerList();
	bannerList.swap( settingsBanners );
}

void MenuHandler::SettingsBtnClicked()
{
	if( state != St_ChannelGrid || freeCamera.IsActive() || homeMenuActive ) return;
	wiiOptionsFadePending = true;
	state = St_SettingsSelect;
}

void MenuHandler::MessageBoardBtnClicked()
{
	if( state == St_ChannelGrid && !freeCamera.IsActive() && !homeMenuActive ) state = St_MessageBoard;
}








void MenuHandler::DoHealthScreen()
{
	Recovery::SetCheckpoint( 0x12F, "loading Health and Safety screen" );
	HealthScreen *health = resources->CreateHealthScreen();
	if( !health )
	{
		// Do not substitute approximation art for Nintendo's warning screen.
		FatalError( "Error loading the Wii Health and Safety resources" );
		return;
	}

	health->Done.connect( this, &MenuHandler::BackmenuFinished );

	Vec2f ScreenProps;
	ScreenProps.x = screenwidth;
	ScreenProps.y = screenheight;
	bool widescreen = ( _CONF_GetAspectRatio() == 1);

	// USB used to be blocked for this entire screen.  That creates a deadlock
	// when a mouse/keyboard receiver is the only controller: the user cannot
	// dismiss Health and Safety, and the driver never gets a chance to attach.
	// CInputs still gives WPAD its three-second exclusive settling window before
	// opening IOS58, while its first-report release guard prevents a button held
	// during power-on from becoming an A click here.
	CInputs::Instance()->AllowUsbStartup();
	while( state == St_HealthScreen )
	{
		//! Update all inputs
		CInputs::Instance()->Update();
		for( int i = 0; i < 4; ++i )
		{
			if( !Pad( i ).pHome() ) continue;
			if( Settings::directHomeExit ) state = St_Exit;
			else DoHomeMenu();
			CInputs::Instance()->ClearButtonsDown();
			break;
		}
		if( state != St_HealthScreen ) break;

		if(Pad(0).hPlus() && Pad(0).pMinus())
			PrintScreenshot();

		Recovery::SetCheckpoint( 0x130, "rendering Health and Safety screen" );
		health->Render( GXmodelView2D, ScreenProps, widescreen );
		Menu_Render();
	}
	// Keep enumeration allowed as the channel grid takes over.
	CInputs::Instance()->AllowUsbStartup();
	if( state == St_ChannelGrid )
	{
		// Present the true black endpoint before synchronous archive/list work.
		// Otherwise the last nearly-faded warning frame stays visible while the
		// grid is prepared, then disappears abruptly at the first menu frame.
		Pane::ResetRenderClip();
		GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y, (GXColor){0,0,0,255} );
		Menu_Render();
	}

	resources->DestroyHealthScreen();
}

void MenuHandler::ShowWiiSaveMenu( bool show )
{
	if( show )
	{
		BuildSaveList( BannerBin::Nand );
		state = St_WiiSaves;
	}
	else
	{
		FreeSaveList();
		state = St_SettingsSelect;
	}
}

void MenuHandler::DoSettingsSelect()
{
	SettingsBG *bg = resources->CreateSettingsBG();
	setupBtn = resources->CreateSettingsBtn();
	setupSelect = resources->CreateSettingsSelect();
	wiiSaveGrid = resources->CreateSaveGrid();
	channelEdit = resources->CreateChannelEdit();
	DialogWindow *yesNoDlg = resources->CreateDialog( DialogWindow::B_2 );


	if( !bg || !setupBtn || !setupSelect || !wiiSaveGrid || !yesNoDlg || !channelEdit )
	{
		FatalError( "Error creating settings menus" );
		return;
	}

	wiiSaveGrid->SetDialog( yesNoDlg );
	wiiSaveGrid->SetHidden( true );
	channelEdit->SetDialog( yesNoDlg );
	channelEdit->SetHidden( true );

	// connect back button to the select resource
	setupBtn->BackClicked.connect( setupSelect, &SettingsSelect::BackBtnClicked );
	setupBtn->BackClicked.connect( wiiSaveGrid, &SaveGrid::BackButtonClicked );
	setupBtn->BackClicked.connect( channelEdit, &ChannelEdit::BackButtonClicked );

	setupSelect->HideWii.connect( setupBtn, &SettingsBtn::HideWiiLogo );

	setupSelect->AppendWiiSaveData.connect( this, &MenuHandler::EnterLeaveWiiSaveData );
	setupSelect->AppendChannelManager.connect( this, &MenuHandler::EnterLeaveChannelManager );
	setupSelect->ExitSettings.connect( this, &MenuHandler::LeaveSettings );
	wiiSaveGrid->DisableBackButton.connect( setupBtn, &SettingsBtn::DisableBackBtn );
	channelEdit->DisableBackButton.connect( setupBtn, &SettingsBtn::DisableBackBtn );

	wiiSaveGrid->Done.connect( setupSelect, &SettingsSelect::WiiSaveDone );
	channelEdit->Done.connect( setupSelect, &SettingsSelect::ChannelEditDone );
	if( enterSystemSettingsDirect )
	{
		setupSelect->OpenSystemSettingsDirect( true );
		enterSystemSettingsDirect = false;
	}

	Vec2f ScreenProps;
	ScreenProps.x = screenwidth;
	ScreenProps.y = screenheight;
	bool widescreen = ( _CONF_GetAspectRatio() == 1);
	int homeMenuDelayFrames = -1;
	int homeUnavailableFrames = 0;

	int optionsEntryFade = wiiOptionsFadePending ? 0 : -1;
	wiiOptionsFadePending = false;
	CInputs::Instance()->ClearButtonsDown();

	while( state == St_SettingsSelect || state == St_WiiSaves || state == St_ChannEdit )
	{
		if(setupSelect->IsUsbDevicesActive())CInputs::Instance()->AllowUsbStartup();
		//! Update all inputs
		CInputs::Instance()->Update();
		if( optionsEntryFade >= 0 )
		{
			CInputs::Instance()->ClearButtonsDown();
			for( int i = 0; i < 4; ++i ) Pad(i).Take();
		}
		Recovery::SetCheckpoint( 0x150, "processing Wii settings actions" );

		if(Pad(0).hPlus() && Pad(0).pMinus())
			PrintScreenshot();

		int homeController = -1;
		int backController = -1;
		for( int i = 0; i < 4; ++i )
		{
			if( homeController < 0 && Pad( i ).pHome() )
				homeController = i;
			if( backController < 0 && Pad( i ).pB() )
				backController = i;
		}

		// Wii Settings and WSM Settings both support the HOME overlay. Only the
		// simulated first-boot/OOBE sequence intentionally disables HOME, matching
		// the crossed-out HOME state shown by the real console setup flow.
		const bool homeBlocked = setupSelect && ( setupSelect->IsFakeOobeActive()
			|| setupSelect->IsApplyingTheme() || PlayerUpdate::Busy() );
		if( homeMenuDelayFrames < 0 && homeController >= 0 && !homeBlocked )
		{
			if( Settings::directHomeExit )
			{
				state = St_Exit;
				break;
			}
			homeMenuDelayFrames = HomeCursorHideFrames;
			CInputs::Instance()->ClearButtonsDown();
		}
		else if( homeController >= 0 && homeBlocked )
		{
			homeUnavailableFrames = 90;
			Pad( homeController ).Take();
		}
		if( homeMenuDelayFrames >= 0 )
		{
			backController = -1;
			CInputs::Instance()->ClearButtonsDown();
		}
		const bool cameraOwnedInput = UpdateFreeCamera( ScreenProps,
			homeMenuDelayFrames < 0 && optionsEntryFade < 0 );
		if( cameraOwnedInput ) backController = -1;
		if( homeMenuDelayFrames < 0 && backController >= 0 )
		{
			if( state == St_WiiSaves )
			{
				wiiSaveGrid->BackButtonClicked();
				Pad( backController ).Take();
			}
			else if( state == St_ChannEdit )
			{
				channelEdit->BackButtonClicked();
				Pad( backController ).Take();
			}
		}
		// HOME and banner rendering can leave the 608-wide banner projection or
		// a clipped scissor active.  The retail data layouts are screen-space
		// resources; restore their complete drawing state every frame so the
		// header and modal grids cannot inherit a distorted projection.
		Recovery::SetCheckpoint( 0x151, "rendering Wii settings" );
		const bool cameraFrame = freeCamera.IsActive();
		Mtx44 cameraSavedMain;
		if( cameraFrame )
		{
			RenderInspection::ConfigureSpectator(freeCamera.Is3D(),freeCamera.Yaw(),freeCamera.Pitch(),freeCamera.LayersEnabled(),freeCamera.FlyX(),freeCamera.FlyY(),freeCamera.FlyZ());
			RenderInspection::BeginFrame( RenderInspection::Settings, (unsigned)state );
			memcpy( cameraSavedMain, MainProjection, sizeof(Mtx44) );
			freeCamera.ApplyProjection( MainProjection );
		}
		GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
		// Data-management layouts are translucent and assume the retail settings
		// backdrop is already present.  Rendering it only on the selector page left
		// save/channel management over stale or black framebuffer contents.
		bg->Render( GXmodelView2D, ScreenProps, widescreen );
		switch( state )
		{
		case St_SettingsSelect:
			setupBtn->Render( GXmodelView2D, ScreenProps, widescreen );
			setupSelect->Render( GXmodelView2D, ScreenProps, widescreen, !cameraOwnedInput );
			break;
		case St_WiiSaves:
			setupBtn->Render( GXmodelView2D, ScreenProps, widescreen );
			setupSelect->Render( GXmodelView2D, ScreenProps, widescreen, !cameraOwnedInput );
			wiiSaveGrid->Render( GXmodelView2D, ScreenProps, widescreen );
			break;
		case St_ChannEdit:
			setupBtn->Render( GXmodelView2D, ScreenProps, widescreen );
			setupSelect->Render( GXmodelView2D, ScreenProps, widescreen, !cameraOwnedInput );
			channelEdit->Render( GXmodelView2D, ScreenProps, widescreen );
			break;
		default:
			//gprintf( "don\'t do that\n" );
			//exit( 0 );
			break;
		}

		if( cameraFrame )
		{
			RenderInspection::EndFrame();
			freeCameraTools.CapturePreviews();
			Pane::ResetRenderClip();
			memcpy( MainProjection, cameraSavedMain, sizeof(Mtx44) );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			RenderFreeCameraHud( ScreenProps );
			freeCameraTools.RestorePointers();
		}
		if( optionsEntryFade < 0 && state != St_ChannelGrid
			&& homeMenuDelayFrames < 0 && !setupSelect->IsApplyingTheme()
			&& !setupSelect->IsSystemSettingsTransition() )
			cursors->Render( GXmodelView2D, ScreenProps, widescreen );
		RenderHomeUnavailable( ScreenProps, homeUnavailableFrames );
		if( optionsEntryFade >= 0 )
		{
			Pane::ResetRenderClip();
			DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y,
				(GXColor){0,0,0,GridEntryFadeAlpha(optionsEntryFade, WiiOptionsFadeFrames)} );
			if( ++optionsEntryFade > WiiOptionsFadeFrames ) optionsEntryFade = -1;
		}
		Menu_Render();
		// Leave this loop first so its settings objects are destroyed before
		// replacing their backing archives. The displayed status frame stays up.
		if( setupSelect->TakeThemeRestartRequest() )
		{
			state = St_ReloadTheme;
			break;
		}
		if(PlayerUpdate::TakeRestartRequest()) {
			state=St_ReloadPlayer;
			break;
		}
		if( homeMenuDelayFrames >= 0 && --homeMenuDelayFrames <= 0 )
		{
			homeMenuDelayFrames = -1;
			DoHomeMenu();
			// The frozen settings page resumes exactly where it was. Replaying an
			// authored *_Back transition here made every button fly in a second time.
			CInputs::Instance()->ClearButtonsDown();
		}
	}

	if( state == St_ChannelGrid ) FadeDisplayedMenuToBlack();

	DELETE( setupSelect );
	DELETE( setupBtn );
	DELETE( wiiSaveGrid );
	DELETE( channelEdit );
	delete bg;
	delete yesNoDlg;

	resources->DestroyChannelEdit();
	resources->DestroySaveGrid();
	resources->DestroySettingsBtn();
	resources->DestroySettingsSelect();
	resources->DestroySettingsBG();

	CInputs::Instance()->ClearButtonsDown();
}

void MenuHandler::DoHomeMenu()
{
	homeMenuActive = true;
	// HOME is a separate inspection surface, not a second application of the
	// underlying page's camera. Restore that page's exact view on return.
	const MenuFreeCamera suspendedCamera = freeCamera;
	freeCamera.SetActive( false );
	freeCameraTools.Close();
	freeCameraController = -1;
	Recovery::SetCheckpoint( 0x13F, "creating HOME Menu overlay" );
	// AnimateZoom rewrites MainProjection while a channel banner enters or
	// leaves. Save that exact suspended projection so the banner can continue
	// afterwards, but give HOME its own canonical 640x480 menu projection.
	Mtx44 suspendedProjection;
	memcpy( suspendedProjection, MainProjection, sizeof( Mtx44 ) );
	BuildHomeMenuProjection( MainProjection );
	GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);
	GX_SetViewport( 0.0f, 0.0f, (f32)screenwidth, (f32)screenheight,
		0.0f, 1.0f );
	GX_SetScissor( 0, 0, (u32)screenwidth, (u32)screenheight );

	homeMenu = resources->CreateHomeMenu();
	if( !homeMenu )
	{
		memcpy( MainProjection, suspendedProjection, sizeof( Mtx44 ) );
		GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
		FatalError( "Error creating home menu\n" );
		return;
	}
	u32 texBufLen = 0;
	u16 texWidth = 0;
	u16 texHeight = 0;
	u8* texBuf = CreateTextureFromDisplayBuffer( texBufLen, texWidth, texHeight );
	Texture *bgTex = NULL;
	if( texBuf )
	{
		bgTex = new Texture;
		bgTex->LoadFromRawData( texBuf, texWidth, texHeight, GX_TF_RGB565 );
		homeMenu->SetBGTexture( bgTex );
	}
	homeMenu->Done.connect( this, &MenuHandler::HomeMenuCancel );
	homeMenu->ExitToWiiMenu.connect( this, &MenuHandler::HomeMenuWantsToExit );

	Vec2f ScreenProps;
	ScreenProps.x = screenwidth;
	ScreenProps.y = screenheight;
	bool widescreen = ( _CONF_GetAspectRatio() == 1);

	CInputs::Instance()->ClearButtonsDown();
	// The HOME screen is a true pause overlay.  Its background is a frozen copy
	// of the last displayed frame, and ASND's global pause preserves every
	// playing voice at its exact position until HOME closes.
	ASND_Pause( 1 );

	while( homeMenuActive )
	{
		//! Update all inputs
		CInputs::Instance()->Update();
		bool closeCameraHome = false;
		if( freeCamera.IsActive() )
			for( int i = 0; i < 4; ++i )
				if( Pad(i).pHome() ) closeCameraHome = true;
		if( closeCameraHome ) { HomeMenuCancel(); break; }
		UpdateFreeCamera( ScreenProps, true );

		if(Pad(0).hPlus() && Pad(0).pMinus())
			PrintScreenshot();

		Recovery::SetCheckpoint( 0x140, "rendering HOME Menu overlay" );
		const bool cameraFrame = freeCamera.IsActive();
		Mtx44 cameraSavedMain;
		if( cameraFrame )
		{
			RenderInspection::ConfigureSpectator(freeCamera.Is3D(),freeCamera.Yaw(),freeCamera.Pitch(),freeCamera.LayersEnabled(),freeCamera.FlyX(),freeCamera.FlyY(),freeCamera.FlyZ());
			RenderInspection::BeginFrame( RenderInspection::Home, 0 );
			memcpy( cameraSavedMain, MainProjection, sizeof(Mtx44) );
			freeCamera.ApplyProjection( MainProjection );
		}
		GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);
		GX_SetScissor( 0, 0, ScreenProps.x, ScreenProps.y );
		DrawHomeSnapshot( bgTex, ScreenProps );
		// A uniform subtle shade replaces the retail striped HOME background.
		// The captured screen remains clearly visible but reads as paused.
		DrawSquare( -4.0f, -4.0f, ScreenProps.x + 8.0f,
			ScreenProps.y + 8.0f,
			(GXColor){ 0, 0, 0, 72 } );
		homeMenu->Render( GXmodelView2D, ScreenProps, widescreen );
		if( cameraFrame )
		{
			RenderInspection::EndFrame();
			freeCameraTools.CapturePreviews();
			Pane::ResetRenderClip();
			memcpy( MainProjection, cameraSavedMain, sizeof(Mtx44) );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			RenderFreeCameraHud( ScreenProps );
			freeCameraTools.RestorePointers();
		}
		cursors->Render( GXmodelView2D, ScreenProps, widescreen );
		Menu_Render();
	}
	if( state != St_Exit )
		ASND_Pause( 0 );
	if( state != St_Exit )
	{
		delete homeMenu;
		homeMenu = NULL;
		resources->DestroyHomeMenu();
	}
	// On Back to Wii-Menu, leave the complete HOME tree alive until libogc performs
	// its atomic exit.  Destroying it between the click callback and the loader
	// handoff was another real-Wii DSI window.

	delete bgTex;
	free( texBuf );
	freeCamera = suspendedCamera;
	freeCamera.DragOrbit(false,false,0,0);
	freeCameraTools.Close();
	if( !Settings::freeCameraEnabled ) freeCamera.SetActive( false );
	freeCameraController = -1;
	memcpy( MainProjection, suspendedProjection, sizeof( Mtx44 ) );
	GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
	CInputs::Instance()->ClearButtonsDown();
}

void MenuHandler::HomeMenuWantsToExit()
{
	homeMenuActive = false;
	state = St_Exit;
}

void MenuHandler::HomeMenuCancel()
{
	homeMenuActive = false;
}

void MenuHandler::EnterLeaveWiiSaveData( bool enter )
{
	if( enter )
	{
		if( state != St_SettingsSelect )
		{
			gprintf( "state != St_SettingsSelect\n" );
			return;
		}
		state = St_WiiSaves;
		BuildSaveList( BannerBin::Nand );
		wiiSaveGrid->SetPage( 0 );
	}
	else
	{
		if( state != St_WiiSaves )
		{
			gprintf( "state != St_WiiSaves\n" );
			return;
		}
		state = St_SettingsSelect;
		FreeSaveList();
		wiiSaveGrid->SetPage( 0 );
	}
	wiiSaveGrid->SetHidden( !enter );
}

void MenuHandler::EnterLeaveChannelManager( bool enter )
{
	if( enter )
	{
		if( state != St_SettingsSelect )
		{
			gprintf( "state != St_SettingsSelect\n" );
			return;
		}
		state = St_ChannEdit;
		// This screen can delete titles and allocates its own icon copies.
		// Drop the suspended cache here, not on an ordinary settings visit.
		ReleaseSuspendedGrid();
		FreeBannerList();
		BuildBannerList( NandUserChannels );
		channelEdit->SetPage( 0 );
	}
	else
	{
		if( state != St_ChannEdit )
		{
			gprintf( "state != St_ChannEdit\n" );
			return;
		}
		state = St_SettingsSelect;
		FreeBannerList();
		channelEdit->SetPage( 0 );
	}
	channelEdit->SetHidden( !enter );
}

void MenuHandler::LeaveSettings()
{
	gridFadeInPending = true;
	gridFadeInFrames = WiiOptionsFadeFrames;
	state = St_ChannelGrid;
}

void MenuHandler::DoFatalError()
{
	FatalErrordialog *dlg = resources->FatarError();
	if( dlg )
		dlg->SetMessage( fatarErrorStr );

	Vec2f ScreenProps;
	ScreenProps.x = screenwidth;
	ScreenProps.y = screenheight;
	bool widescreen = ( _CONF_GetAspectRatio() == 1);

	while( state == St_FatalError )
	{
		//! Update all inputs
		CInputs::Instance()->Update();
		Recovery::SetCheckpoint( 0x160, "rendering WSM error dialog" );

		if(Pad(0).hPlus() && Pad(0).pMinus())
			PrintScreenshot();

		for( int i = 0; i < 4; i++ )
		{
			if( Pad( i ).pHome() || Pad( i ).pA() )
			{
				state = St_Exit;
				break;
			}
		}

		if( dlg )
		{
			dlg->Render( GXmodelView2D, ScreenProps, widescreen );
		}
		else
		{
			// Old menu revisions may not contain the exact executable-embedded
			// ASH used by the 2012 player. Keep the error screen operable with a
			// renderer-native fallback instead of dereferencing a null dialog.
			GX_SetScissor( 0, 0, (u32)ScreenProps.x, (u32)ScreenProps.y );
			GX_LoadProjectionMtx( MainProjection, GX_ORTHOGRAPHIC );
			const GXColor black = { 0, 0, 0, 255 };
			const GXColor white = { 255, 255, 255, 255 };
			DrawSquare( 0.0f, 0.0f, ScreenProps.x, ScreenProps.y, black );
			DrawOverlayText( 52.0f, 150.0f, "WSM Player error", 3.0f,
				white, 40 );
			DrawOverlayText( 52.0f, 215.0f, "Press A or HOME to exit", 2.0f,
				white, 40 );
		}
		Menu_Render();
	}

}

void MenuHandler::FatalError( const char* fmt, ... )
{
	state = St_FatalError;

	char *charStr = NULL;

	va_list argptr;
	va_start( argptr, fmt );
	int ret = vasprintf( &charStr, fmt, argptr );
	va_end( argptr );

	// some error
	if( ret < 1 || !charStr )
	{
		strcpy16( fatarErrorStr, "A Fatal Error occurred while\nsetting the fatal error message.\n" );
		return;
	}
	strlcpy16( fatarErrorStr, charStr, FatarErrorStrLen );
	free( charStr );
}

void MenuHandler::Start()
{
	//state = St_ChannelGrid;// speed up testing.  the health screen already works
	//state = St_SettingsSelect;

	if ( Settings::mountDVD )
	{
		diHandler->Wake();
	}

	// make sure the serources were created right
	if( !cursors )
	{
		FatalError( "Error creating cursors\n" );
	}
	else if( !buttonPanel )
	{
		FatalError( "Error creating button panel\n" );
	}
	else if( !grid )
	{
		FatalError( "Error creating grid:\n%s\n", ChannelGrid::LastLoadError() );
	}
	else if( !dcBanner )
	{
		FatalError( "Error creating dcBanner\n" );
	}
	else if( !dcIcon )
	{
		FatalError( "Error creating dcIcon\n" );
	}
	else
	{
		// everything looks like it was created, connect the resources together

		// connect the DiHandler to this
		diHandler->DiscInserted.connect( this, &MenuHandler::DiscInserted );
		diHandler->DiscEjected.connect( this, &MenuHandler::DiscEjected );
		diHandler->OpeningBnrReady.connect( this, &MenuHandler::GetDiscChannelBannerData );
		diHandler->StartingToReadDisc.connect( this, &MenuHandler::StartingToReadDisc );

		ConnectMenuGraphics();
	}


	while( state != St_Exit )
	{
		switch( state )
		{
		case St_FatalError:
			DoFatalError();
			break;
		case St_HealthScreen:
			DoHealthScreen();
			break;
		case St_ChannelGrid:
			DoGrid();
			break;
		case St_SettingsSelect:
			DoSettingsSelect();
			break;
		case St_ReloadTheme:
			ReloadTheme();
			break;
		case St_ReloadPlayer:
			RestartUpdatedPlayer();
			break;
		default:
			break;
		}
	}
	MenuAudio::Instance()->Reset();
	ReleaseSuspendedGrid();
}

void MenuHandler::ConnectMenuGraphics()
{
	grid->DateChanged.connect( bg, &GreyBackground::SetDate );
	grid->ChannelClicked.connect( this, &MenuHandler::ChannelIconClicked );
	dcBanner->Finished.connect( this, &MenuHandler::DiscChannelAnimFinished );
	grid->SetDiscChannelPointer( dcIcon );
	buttonPanel->LeftArrow()->Clicked.connect( this, &MenuHandler::LeftArrowBtnClicked );
	buttonPanel->RightArrow()->Clicked.connect( this, &MenuHandler::RightArrowBtnClicked );
	buttonPanel->SettingsBtnClicked.connect( this, &MenuHandler::SettingsBtnClicked );
	buttonPanel->MessageBoardClicked.connect( this, &MenuHandler::MessageBoardBtnClicked );
}

void MenuHandler::DestroyMenuGraphics()
{
	// Unbind icon layouts before destroying the native disc icon they reference.
	DELETE( grid );
	DELETE( buttonPanel );
	DELETE( homeMenu );
	DELETE( nativeChannelView );
	DELETE( messageBoard );
	DELETE( cursors );
	DELETE( bg );
	DELETE( dcIcon );
	DELETE( dcBanner );
	DELETE( gcBanner );
	selectedBanner = NULL;
	bigBannerLayout = NULL;
	bigBannerObj = NULL;
}

bool MenuHandler::CreateMenuGraphics()
{
	buttonPanel = resources->CreateButtonPanel();
	dcIcon = resources->CreateDiscChannelIcon();
	dcBanner = resources->CreateDiscChannel();
	gcBanner = resources->CreateGCBanner();
	cursors = resources->Cursors();
	bg = resources->GreyBG();
	grid = resources->CreateChannelGrid();
	return buttonPanel && dcIcon && dcBanner && gcBanner && cursors && bg && grid;
}

void MenuHandler::ReloadTheme()
{
	Recovery::Pause();
	StopBannerSound();
	ASND_Pause( 1 );
	const bool workerStopped = BannerAsync::QuiesceForLaunch();
	// DI may still report an insertion; serialize it rather than suspending a
	// thread that could own this mutex. USB and WPAD stay initialized throughout.
	LWP_MutexLock( drawMutex );
	GX_DrawDone();
	ReleaseSuspendedGrid();
	DestroyMenuGraphics();
	bool loaded = resources->ReloadTheme() && CreateMenuGraphics();
	bool restored = false;
	if( !loaded )
	{
		DestroyMenuGraphics();
		Settings::resourcePath = activeThemePath;
		Settings::customThemePath = activeThemePath;
		Settings::Save();
		loaded = resources->ReloadTheme() && CreateMenuGraphics();
		restored = loaded;
	}
	if( loaded )
	{
		activeThemePath = Settings::resourcePath;
		ConnectMenuGraphics();
		// Inserted-disc layouts survive the grid rebuild. Re-select their
		// language too; ordinary channel loaders are recreated by DoGrid().
		if( discChannelBanner )
		{
			if( discChannelBanner->getIcon() )
				discChannelBanner->getIcon()->SetLanguage( CONF_GetLanguageString() );
			if( discChannelBanner->getBanner() )
				discChannelBanner->getBanner()->SetLanguage( CONF_GetLanguageString() );
			discChannelBanner->RefreshGeneratedChannelText();
		}
		state = St_ChannelGrid;
		if( restored )
		{
			launchErrorMessage = "Theme could not load; previous theme restored";
			launchErrorFrames = 300;
		}
	}
	else FatalError( "Unable to reload menu theme resources" );
	LWP_MutexUnlock( drawMutex );
	if( workerStopped ) BannerAsync::RestartAfterLaunchFailure();
	ASND_Pause( 0 );
	CInputs::Instance()->ClearButtonsDown();
	Recovery::Resume();
}

void MenuHandler::DiscInserted( DiHandler::DiscType dt )
{
	LWP_MutexLock( drawMutex );
	if( grid && dt != DiHandler::T_Wii )
	{
		grid->DiscInserted( dt );
	}
	if( selectedIdx == -1 && ( state == St_BigBannerFadeIn || state == St_BigBanner ) )
	{
		switch( dt )
		{
		case DiHandler::T_GC:
			dcBanner->DiscIsGC();
			break;
		case DiHandler::T_Wii:
			dcBanner->DiscIsWii();
			break;
		case DiHandler::T_Unknown:
			discBannerState = DBSt_Unknown;
			dcBanner->DiscIsUnknown();
			break;
		}
	}
	else
	{
		switch( dt )
		{
		case DiHandler::T_GC:
			discBannerState = DBSt_GCBanner;
			break;
		case DiHandler::T_Wii:
			discBannerState = DBSt_WiiBanner;
			break;
		case DiHandler::T_Unknown:
			discBannerState = DBSt_Unknown;
			break;
		}

	}
	LWP_MutexUnlock( drawMutex );
}

void MenuHandler::DiscEjected()
{
	LWP_MutexLock( drawMutex );

	if( selectedIdx == -1 && state == St_BigBanner )
	{
		StopBannerSound();
	}

	if( grid )
	{
		grid->DiscEjected();
	}
	if( dcBanner )
	{
		if( selectedIdx == -1 && ( state == St_BigBannerFadeIn || state == St_BigBanner ) )// the disc channel was showing the banner before
		{
			selectedBanner = NULL;
			if( discBannerState == DBSt_Spinup )
			{
				dcBanner->EjectDisc();
			}
			else
			{
				dcBanner->Reset();
			}
		}
	}

	DELETE( discChannelBanner );
	FREE( discChannelBannerData );

	discBannerState = DBSt_None;

	LWP_MutexUnlock( drawMutex );
}

void MenuHandler::GetDiscChannelBannerData( u8* data, u32 len, bool &taken )
{
	LWP_MutexLock( drawMutex );

	if( data )
	{
		// create banner instance
		discChannelBannerData = data;
		discChannelBannerDataLen = len;
		discChannelBanner = new Banner( discChannelBannerData, discChannelBannerDataLen );

		// load icon for small disc channel
		Object *o = discChannelBanner->LoadIcon();
		Layout *l = discChannelBanner->getIcon();
		if( grid )
		{
			grid->SetDiscChannelIcon( l, o );
			grid->DiscInserted( DiHandler::T_Wii );
		}

		// if we are currently looking at the disc channel, load the big banner up
		if( selectedIdx == -1 && ( state == St_BigBannerFadeIn || state == St_BigBanner ) )
		{
			selectedBanner = discChannelBanner;
			if( !( bigBannerObj = selectedBanner->LoadBanner() )
				|| !( bigBannerLayout = selectedBanner->getBanner() ) )
			{
				gprintf( "error loading banner for disc channel\n" );
				bigBannerObj = NULL;
				bigBannerLayout = NULL;
				selectedBanner = NULL;
				return;
			}


		}
		if( state == St_BigBannerFadeIn || state == St_BigBanner )
		{
			// tell the disc channel to play the "insert wii disc" animation
			dcBanner->DiscIsWii();
		}
		else
		{
			discBannerState = DBSt_WiiBanner;
		}
		taken = true;
	}
	LWP_MutexUnlock( drawMutex );
}

void MenuHandler::StartingToReadDisc()
{
	LWP_MutexLock( drawMutex );
	discBannerState = DBSt_Spinup;
	if( dcBanner && ( state == St_BigBannerFadeIn || state == St_BigBanner ) )
	{
		dcBanner->StartReadingDisc();
	}
	LWP_MutexUnlock( drawMutex );
}

void MenuHandler::DiscChannelAnimFinished()
{
	if( discBannerState == DBSt_Spinup && discChannelBanner )
	{
		discBannerState = DBSt_WiiBanner;
		if( selectedIdx == -1 && state == St_BigBanner )
		{
			StartBannerSound();
		}
	}
	else
	{
		discBannerState = DBSt_GCBanner;
	}
}

void MenuHandler::AnimateZoom(const Vec2f &ScreenProps, bool AnimZoomIn)
{
    static const bool PAL50 = (CONF_GetVideo() == CONF_VIDEO_PAL) && (CONF_GetEuRGB60() == 0);
    float xDiff = 310.f;
    float yDiff = 236.0f;
    if(PAL50)
        yDiff = 220.0f;

    // animation is on going
	if(AnimStep < MaxAnimSteps)
	{
        AnimationRunning = true;
        AnimStep++;

        // Original banner transition: no added ease-in/ease-out curve.
        if(AnimZoomIn) {
            BGAlpha = std::min(255.f * AnimStep * 2.f / MaxAnimSteps, 255.f);
            if(AnimStep < 0.4f * MaxAnimSteps)
                BannerAlpha = 0;
            else
                BannerAlpha = std::min(255.f * (AnimStep - 0.4f * MaxAnimSteps) / (0.6f * MaxAnimSteps), 255.f);
        }
        else {
            BGAlpha = std::min(255.f * (MaxAnimSteps-AnimStep) * 2.f / MaxAnimSteps, 255.f);
            if((MaxAnimSteps - AnimStep) < 0.4f * MaxAnimSteps)
                BannerAlpha = 0;
            else
                BannerAlpha = std::min(255.f * ((MaxAnimSteps - AnimStep) - 0.4f * MaxAnimSteps) / (0.6f * MaxAnimSteps), 255.f);
        }

        //! This works good for banners
        f32 chopXOffset = (ScreenProps.x * 0.5f - xDiff );
        f32 chopYOffset = (ScreenProps.y * 0.5f - yDiff );
        f32 chopX = (ScreenProps.x * 0.5f + xDiff );
        f32 chopY = (ScreenProps.y * 0.5f + yDiff );


        float curAnimStep = AnimZoomIn ? ((float)(MaxAnimSteps - AnimStep)/(float)MaxAnimSteps) : ((float)AnimStep/(float)MaxAnimSteps);

		float stepx1 = chopXOffset - AnimPosX1;
		float stepy1 = chopYOffset - AnimPosY1;
        float stepx2 = chopX - AnimPosX2;
		float stepy2 = chopY - AnimPosY2;

		float top = AnimPosY1 + stepy1 * curAnimStep;
		float bottom = AnimPosY2 + stepy2 * curAnimStep;
		float left = AnimPosX1 + stepx1 * curAnimStep;
		float right = AnimPosX2 + stepx2 * curAnimStep;

        // set main projection of all GUI stuff
        guOrtho(MainProjection, top, bottom, left, right, -1000, 1000);

        // this just looks better for banner/icon ratio
        f32 ratioX = xDiff * 2.f / 128.f;
        f32 ratioY = yDiff * 2.f / 96.f;

        stepx1 = ((ScreenProps.x * 0.5f - xDiff) - AnimPosX1) * ratioX;
        stepx2 = ((ScreenProps.x * 0.5f + xDiff) - AnimPosX2) * ratioX;
        stepy1 = ((ScreenProps.y * 0.5f - yDiff) - AnimPosY1) * ratioY;
        stepy2 = ((ScreenProps.y * 0.5f + yDiff) - AnimPosY2) * ratioY;

        //! This works good for banners
		top = (ScreenProps.y * 0.5f - yDiff) + stepy1 * curAnimStep;
        bottom = (ScreenProps.y * 0.5f + yDiff) + stepy2 * curAnimStep;
        left = (ScreenProps.x * 0.5f - xDiff) + stepx1 * curAnimStep;
		right = (ScreenProps.x * 0.5f + xDiff) + stepx2 * curAnimStep;

        // set banner projection
		guOrtho(BannerProjection,top, bottom, left, right,-1000,1000);
	}
    else {
        //! This works good for banners
        f32 chopXOffset = (ScreenProps.x * 0.5f - xDiff );
        f32 chopYOffset = (ScreenProps.y * 0.5f - yDiff );
        f32 chopX = (ScreenProps.x * 0.5f + xDiff );
        f32 chopY = (ScreenProps.y * 0.5f + yDiff );

        guOrtho(MainProjection,chopYOffset,chopY,chopXOffset,chopX,-1000,1000);
        GX_LoadProjectionMtx(MainProjection, GX_ORTHOGRAPHIC);
        AnimationRunning = false;
    }
}
