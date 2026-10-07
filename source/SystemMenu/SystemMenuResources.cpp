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
#include <string.h>

#include "ash.h"
#include "gecko.h"
#include "SaveData/databin.h"
#include "sdcontentcrypto.h"
#include "settings.h"
#include "SystemMenu/SystemMenuResources.h"
#include "SystemFont.h"
#include "U8Archive.h"
#include "utils/crc32.h"
#include "utils/fileops.h"
#include "utils/nandtitle.h"
#include "utils/sc.h"

#define INIT_ASH( x )\
	x##AshSize( 0 ), x##Ash( NULL )

namespace
{
	bool HasCoreSystemMenuResources( U8Archive *archive )
	{
		return archive
			&& archive->FileDescriptor( "/layout/common/chanSel.ash" )
			&& archive->FileDescriptor( "/layout/common/board.ash" )
			&& archive->FileDescriptor( "/layout/common/chanTtl.ash" )
			&& archive->FileDescriptor( "/layout/common/cmnBtn.ash" )
			&& archive->FileDescriptor( "/layout/common/cursor.ash" )
			&& archive->FileDescriptor( "/layout/common/diskBann.ash" )
			&& archive->FileDescriptor( "/layout/common/diskThum.ash" );
	}
}

SystemMenuResources *SystemMenuResources::instance = NULL;
SystemMenuResources::SystemMenuResources():
	isInited( false ),
	preserveFonts( false ),
	mainArc( NULL ),
	homeUnavailableData( NULL ),
	INIT_ASH( chanSel ),
	INIT_ASH( board ),
	INIT_ASH( calendar ),
	INIT_ASH( chanTtl ),
	INIT_ASH( cmnBtn ),
	INIT_ASH( health ),
	backMenuData( NULL ),
	INIT_ASH( cursor ),
	INIT_ASH( fatalDlg ),
	INIT_ASH( balloon ),
	INIT_ASH( diskBann ),
	INIT_ASH( dlgWdw ),
	INIT_ASH( setupBg ),
	INIT_ASH( setupBtn ),
	INIT_ASH( setupSel ),
	INIT_ASH( iplSetting ),
	INIT_ASH( memory ),
	INIT_ASH( diskThum ),
	INIT_ASH( GCBann ),
	INIT_ASH( chanEdit ),
	INIT_ASH( homeBtn1 ),
	INIT_ASH( homeBtn1L ),
	bmgData( NULL ),
	bmgDataLen( 0 ),
	fatalErrordialog( NULL )
{
}

SystemMenuResources::~SystemMenuResources()
{
	FreeEverything();
}

bool SystemMenuResources::Init()
{
	if( isInited )
	{
		return true;
	}
	u8* resourceArc = NULL;
	u32 resourceLen = 0;

	// System Menu
	if( !preserveFonts && !SystemFont::Init() )
	{
		return false;
	}

	// get tmd
	tmd *p_tmd = NandTitles.GetTMD( 0x100000002ull );
	if( !p_tmd )
	{
		gprintf( "can\'t get system menu TMD\n" );
		return false;
	}
	tmd_content *contents = TMD_CONTENTS( p_tmd );

	// determine which file to use for resources
	char path[ ISFS_MAXPATH ]__attribute__((aligned( 32 )));
	if( Settings::resourcePath.size() )
	{
		// user-specified path
		U8FileArchive *candidate = new U8FileArchive(
			Settings::resourcePath.c_str() );
		if( HasCoreSystemMenuResources( candidate ) )
			mainArc = candidate;
		else
		{
			gprintf( "Theme rejected (not a compiled System Menu archive): %s\n",
				Settings::resourcePath.c_str() );
			delete candidate;
		}
	}
	if( !mainArc )
	{
		// Index 1 is standard on later menus, but early/regional revisions can
		// assign the resource archive a different content index. Probe index 1
		// first, then every other TMD content for the actual layout signature.
		for( int pass = 0; pass < 2 && !mainArc; ++pass )
		{
			for( u16 i = 0; i < p_tmd->num_contents; ++i )
			{
				const bool preferred = contents[ i ].index == 1;
				if( ( pass == 0 ) != preferred )
					continue;
				sprintf( path, "/title/00000001/00000002/content/%08x.app",
					contents[ i ].cid );
				U8NandArchive *candidate = new U8NandArchive( path );
				if( HasCoreSystemMenuResources( candidate ) )
				{
					mainArc = candidate;
					gprintf( "System Menu resources: content index %u, cid %08x\n",
						contents[ i ].index, contents[ i ].cid );
					break;
				}
				delete candidate;
			}
		}
		if( !mainArc )
		{
			gprintf( "SM main resource archive not found in any content\n" );
			return false;
		}
	}


	// setup bmg file
	if( !SetupBmg( mainArc ) )
	{
		FreeEverything();
		return false;
	}

	// read some files
#define READ_ASH( x )																				\
	do																								\
	{																								\
		if( !( x##Ash = mainArc->GetFileAllocated( "/layout/common/" #x ".ash", & x##AshSize ) ) )	\
		{																							\
			gprintf( "Error while loading " #x ".ash\n" );											\
			FreeEverything();																		\
			return false;																			\
		}																							\
	}																								\
	while( 0 )

	// read ash files
	//READ_ASH( balloon );
	READ_ASH( dlgWdw );	// read dialog window data and keep it laying around in memory for whenever we
						// need a dialog

	// Fatal-dialog and certificate blobs are executable-embedded conveniences,
	// not prerequisites for browsing banners. Old System Menus move/change
	// these signatures, so use them when found and otherwise keep running.
	resourceArc = LoadSystemMenuExecutable( &resourceLen );
	u32 *exeStart = NULL;
	u32 *exeEnd = NULL;
	if( resourceArc && resourceLen >= 0x5a20 )
	{
		exeStart = (u32*)resourceArc;
		exeEnd = (u32*)( resourceArc + resourceLen - 0x5a20 );
		while( exeStart < exeEnd )
		{
			if( exeStart[ 0 ] == 0x41534830 )// found an ASH file
			{
				u32 ashSize = 0x5a20;
				u8* ashStuff = DecompressAsh( (const u8*)exeStart, ashSize );
				if( ashStuff )
				{
					fatalErrordialog = new FatalErrordialog;
					if( fatalErrordialog->Load( ashStuff, ashSize ) )
					{
						fatalDlgAsh = ashStuff;
						fatalDlgAshSize = ashSize;
						break;
					}
					DELETE( fatalErrordialog );
					free( ashStuff );
				}
			}
			exeStart++;
		}
	}
	if( fatalErrordialog )
		fatalErrordialog->SetMessage( NULL );
	else
	{
		gprintf( "System Menu fatal dialog signature not found; using WSM fallback\n" );
	}

	// get some stuff for the save banners & SD channels
	bool foundCerts[ 4 ] = { false, false, false, false };
	if( resourceArc && resourceLen >= 0x400 )
	{
		exeStart = (u32*)( resourceArc + 0x10 );
		exeEnd = (u32*)( resourceArc + resourceLen - 0x400 );
		while( exeStart < exeEnd )
		{
			if( exeStart[ 0 ] == 0x74780414 && Crc32( ((const u8*)exeStart) - 0x10, 0x400 ) == 0x0b65bd72 )
			{
				memcpy( &ca_dpki, ((const u8*)exeStart) - 0x10, 0x400 );
				foundCerts[ 0 ] = true;
			}
			else if( exeStart[ 0 ] == 0x16ff4f7a && Crc32( ((const u8*)exeStart) - 0x10, 0x400 ) == 0xca213498 )
			{
				memcpy( &ca_ppki, ((const u8*)exeStart) - 0x10, 0x400 );
				foundCerts[ 1 ] = true;
			}
			else if( exeStart[ 0 ] == 0xf2a869f7 && Crc32( ((const u8*)exeStart) - 0x10, 0x240 ) == 0xb15e343c )
			{
				memcpy( &ms_dpki, ((const u8*)exeStart) - 0x10, 0x240 );
				foundCerts[ 2 ] = true;
			}
			else if( exeStart[ 0 ] == 0x45381a4f && Crc32( ((const u8*)exeStart) - 0x10, 0x240 ) == 0x4715ed4c )
			{
				memcpy( &ms_ppki, ((const u8*)exeStart) - 0x10, 0x240 );
				foundCerts[ 3 ] = true;
			}
			exeStart++;
		}
	}
	if( !foundCerts[ 0 ]
			|| !foundCerts[ 1 ]
			|| !foundCerts[ 2 ]
			|| !foundCerts[ 3 ] )
	{
		gprintf( "System Menu certificate signatures differ; using bundled certificates\n" );
	}


	// done with the executable
	FREE( resourceArc );

	isInited = true;
	return true;
}

HomeMenu *SystemMenuResources::CreateHomeMenu()
{
	char path[ 65 ];
	snprintf( path, sizeof( path ), "/layout/%s/homeBtn1.ash", CONF_GetLanguageString() );
	if( !homeBtn1Ash )
		homeBtn1Ash = mainArc->GetFileAllocated( "/layout/common/homeBtn1.ash", &homeBtn1AshSize );
	if( !homeBtn1LAsh )
		homeBtn1LAsh = mainArc->GetFileAllocated( path, &homeBtn1LAshSize );
	if( !homeBtn1LAsh && strcasecmp( CONF_GetLanguageString(), "ENG" ) )
	{
		gprintf( "Home menu language resource failed (%s); using English\n", path );
		homeBtn1LAsh = mainArc->GetFileAllocated( "/layout/ENG/homeBtn1.ash", &homeBtn1LAshSize );
	}
	if( !homeBtn1Ash || !homeBtn1LAsh )
	{
		gprintf( "Error while loading homeBtn1.ash\n" );
		return NULL;
	}
	HomeMenu *ret = new HomeMenu;
	if( !ret->Load( homeBtn1Ash, homeBtn1AshSize, homeBtn1LAsh, homeBtn1LAshSize ) )
	{
		gprintf( "error loading homeBtn1Ash\n" );
		delete ret;
		FREE( homeBtn1Ash );
		FREE( homeBtn1LAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroyHomeMenu()
{
	FREE( homeBtn1Ash );
	FREE( homeBtn1LAsh );
}

ChannelEdit *SystemMenuResources::CreateChannelEdit()
{
	if( !chanEditAsh && !( chanEditAsh = mainArc->GetFileAllocated( "/layout/common/chanEdit.ash", &chanEditAshSize ) ) )
	{
		gprintf( "Error while loading chanEdit.ash\n" );
		return NULL;
	}
	ChannelEdit *ret = new ChannelEdit;
	if( !ret->Load( chanEditAsh, chanEditAshSize ) )
	{
		gprintf( "error loading chanEditAsh\n" );
		delete ret;
		FREE( chanEditAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroyChannelEdit()
{
	FREE( chanEditAsh );
}

WiiCursors *SystemMenuResources::Cursors()
{
	if( !cursorAsh && !( cursorAsh = mainArc->GetFileAllocated( "/layout/common/cursor.ash", &cursorAshSize ) ) )
	{
		gprintf( "Error while loading cursor.ash\n" );
		return NULL;
	}
	WiiCursors *ret = new WiiCursors;
	if( !ret->Load( cursorAsh, cursorAshSize ) )
	{
		gprintf( "error loading cursorAsh\n" );
		delete ret;
		FREE( cursorAsh );
		return NULL;
	}
	return ret;
}

GCBanner *SystemMenuResources::CreateGCBanner()
{
	if( !GCBannAsh && !( GCBannAsh = mainArc->GetFileAllocated( "/layout/common/GCBann.ash", &GCBannAshSize ) ) )
	{
		gprintf( "Error while loading GCBann.ash\n" );
		return NULL;
	}
	GCBanner *ret = new GCBanner;
	if( !ret->Load( GCBannAsh, GCBannAshSize ) )
	{
		gprintf( "error loading GCBannAsh\n" );
		delete ret;
		FREE( GCBannAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroyGCBanner()
{
	FREE( GCBannAsh );
}

DiscChannel *SystemMenuResources::CreateDiscChannel()
{
	if( !diskBannAsh && !( diskBannAsh = mainArc->GetFileAllocated( "/layout/common/diskBann.ash", &diskBannAshSize ) ) )
	{
		gprintf( "Error while loading diskBann.ash\n" );
		return NULL;
	}
	DiscChannel *ret = new DiscChannel;
	if( !ret->Load( diskBannAsh, diskBannAshSize ) )
	{
		gprintf( "error loading diskBannAsh\n" );
		delete ret;
		FREE( diskBannAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroyDiscChannel()
{
	FREE( diskBannAsh );
}

DiscChannelIcon *SystemMenuResources::CreateDiscChannelIcon()
{
	if( !diskThumAsh && !( diskThumAsh = mainArc->GetFileAllocated( "/layout/common/diskThum.ash", &diskThumAshSize ) ) )
	{
		gprintf( "Error while loading diskThum.ash\n" );
		return NULL;
	}
	DiscChannelIcon *ret = new DiscChannelIcon;
	if( !ret->Load( diskThumAsh, diskThumAshSize ) )
	{
		gprintf( "error loading diskThumAsh\n" );
		delete ret;
		FREE( diskThumAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroyDiscChannelIcon()
{
	FREE( diskThumAsh );
}

SaveGrid *SystemMenuResources::CreateSaveGrid()
{
	if( !memoryAsh && !( memoryAsh = mainArc->GetFileAllocated( "/layout/common/memory.ash", &memoryAshSize ) ) )
	{
		gprintf( "Error while loading memory.ash\n" );
		return NULL;
	}
	SaveGrid *ret = new SaveGrid;
	if( !ret->Load( memoryAsh, memoryAshSize ) )
	{
		gprintf( "error loading memoryAsh\n" );
		delete ret;
		FREE( memoryAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroySaveGrid()
{
	FREE( memoryAsh );
}

SettingsSelect *SystemMenuResources::CreateSettingsSelect()
{
	if( !setupSelAsh && !( setupSelAsh = mainArc->GetFileAllocated( "/layout/common/setupSel.ash", &setupSelAshSize ) ) )
	{
		gprintf( "Error while loading setupSelAsh.ash\n" );
		return NULL;
	}
	if( !iplSettingAsh )
	{
		const char *candidates[] = {
			"/html/EU2/iplsetting.ash", "/html/US2/iplsetting.ash",
			"/html/JP2/iplsetting.ash", "/html/KR2/iplsetting.ash",
			"/html/EU/iplsetting.ash", "/html/US/iplsetting.ash",
			"/html/JP/iplsetting.ash", "/html/KR/iplsetting.ash"
		};
		for( u32 i = 0; i < sizeof( candidates ) / sizeof( candidates[ 0 ] ); ++i )
		{
			iplSettingAsh = mainArc->GetFileAllocated( candidates[ i ],
				&iplSettingAshSize );
			if( iplSettingAsh )
			{
				gprintf( "Wii Settings HTML: %s\n", candidates[ i ] );
				break;
			}
		}
	}
	SettingsSelect *ret = new SettingsSelect;
	if( !ret->Load( setupSelAsh, setupSelAshSize,
		iplSettingAsh, iplSettingAshSize ) )
	{
		gprintf( "error loading settingsBtn\n" );
		delete ret;
		FREE( setupSelAsh );
		return NULL;
	}
	return ret;
}

void SystemMenuResources::DestroySettingsSelect()
{
	FREE( setupSelAsh );
	FREE( iplSettingAsh );
}

SettingsBtn *SystemMenuResources::CreateSettingsBtn()
{
	if( !setupBtnAsh && !( setupBtnAsh = mainArc->GetFileAllocated( "/layout/common/setupBtn.ash", &setupBtnAshSize ) ) )
	{
		gprintf( "Error while loading setupBtn.ash\n" );
		return NULL;
	}
	SettingsBtn *settingsBtn = new SettingsBtn;
	if( !settingsBtn->Load( setupBtnAsh, setupBtnAshSize ) )
	{
		gprintf( "error loading settingsBtn\n" );
		delete settingsBtn;
		FREE( setupBtnAsh );
		return NULL;
	}
	return settingsBtn;
}

void SystemMenuResources::DestroySettingsBtn()
{
	FREE( setupBtnAsh );
}

SettingsBG *SystemMenuResources::CreateSettingsBG()
{
	if( !setupBgAsh && !( setupBgAsh = mainArc->GetFileAllocated( "/layout/common/setupBg.ash", &setupBgAshSize ) ) )
	{
		gprintf( "Error while loading setupBg.ash\n" );
		return NULL;
	}
	SettingsBG *settingsBG = new SettingsBG;
	if( !settingsBG->Load( setupBgAsh, setupBgAshSize ) )
	{
		gprintf( "error loading settingsBG\n" );
		delete settingsBG;
		FREE( setupBgAsh );
		return NULL;
	}
	return settingsBG;
}

void SystemMenuResources::DestroySettingsBG()
{
	FREE( setupBgAsh );
}

DialogWindow *SystemMenuResources::CreateDialog( DialogWindow::Type t )
{
	if( !dlgWdwAsh && mainArc )
		dlgWdwAsh = mainArc->GetFileAllocated("/layout/common/dlgWdw.ash", &dlgWdwAshSize);
	DialogWindow *dialogWindow = new DialogWindow;
	if( !dialogWindow->Load( dlgWdwAsh, dlgWdwAshSize, t ) )
	{
		gprintf( "error loading dialogWindow\n" );
		delete dialogWindow;
		return NULL;
	}
	return dialogWindow;
}

BannerFrame *SystemMenuResources::CreateBannerFrame()
{
	if( !chanTtlAsh && !( chanTtlAsh = mainArc->GetFileAllocated( "/layout/common/chanTtl.ash", &chanTtlAshSize ) ) )
	{
		gprintf( "Error while loading chanTtl.ash\n" );
		return NULL;
	}
	BannerFrame *bannerFrame = new BannerFrame;
	if( !bannerFrame->Load( chanTtlAsh, chanTtlAshSize ) )
	{
		gprintf( "error loading bannerFrame\n" );
		delete bannerFrame;
		FREE( chanTtlAsh );
		return NULL;
	}
	return bannerFrame;
}

void SystemMenuResources::DestroyBannerFrame()
{
	FREE( chanTtlAsh );
}

GreyBackground *SystemMenuResources::GreyBG()
{
	if( !boardAsh && !( boardAsh = mainArc->GetFileAllocated( "/layout/common/board.ash", &boardAshSize ) ) )
	{
		gprintf( "Error while loading board.ash\n" );
		return NULL;
	}
	GreyBackground *greyBg = new GreyBackground;
	if( !greyBg->Load( boardAsh, boardAshSize ) )
	{
		gprintf( "error loading grey background\n" );
		delete greyBg;
		FREE( boardAsh );
		return NULL;
	}

	return greyBg;
}

void SystemMenuResources::DestroyGreyBG()
{
	FREE( boardAsh );
}

MessageBoardScreen *SystemMenuResources::CreateMessageBoard()
{
	MessageBoardScreen *screen = new MessageBoardScreen;
	if( !screen->Load(mainArc) )
	{
		gprintf( "Error loading themed Message Board resources\n" );
		delete screen;
		return NULL;
	}
	return screen;
}

void SystemMenuResources::DestroyMessageBoard()
{
}


ButtonPanel *SystemMenuResources::CreateButtonPanel()
{
	if( !cmnBtnAsh && !( cmnBtnAsh = mainArc->GetFileAllocated( "/layout/common/cmnBtn.ash", &cmnBtnAshSize ) ) )
	{
		gprintf( "Error while loading cmnBtn.ash\n" );
		return NULL;
	}
	ButtonPanel *buttonPanel = new ButtonPanel;
	if( !buttonPanel->Load(  cmnBtnAsh, cmnBtnAshSize ) )
	{
		gprintf( "error loading common buttons\n" );
		delete buttonPanel;
		FREE( cmnBtnAsh );
		return NULL;
	}
	return buttonPanel;
}

void SystemMenuResources::DestroyButtonPanel()
{
	FREE( cmnBtnAsh );
}

ChannelGrid *SystemMenuResources::CreateChannelGrid()
{
	if( !chanSelAsh && !( chanSelAsh = mainArc->GetFileAllocated( "/layout/common/chanSel.ash", &chanSelAshSize ) ) )
	{
		gprintf( "Error while loading chanSel.ash\n" );
		return NULL;
	}
	ChannelGrid *channelGrid = new ChannelGrid;
	if( !channelGrid->Load( chanSelAsh, chanSelAshSize ) )
	{
		gprintf( "error loading channel grid\n" );
		delete channelGrid;
		FREE( chanSelAsh );
		return NULL;
	}
	return channelGrid;
}

void SystemMenuResources::DestroyChannelGrid()
{
	FREE( chanSelAsh );
}

u8* SystemMenuResources::LoadSystemMenuExecutable( u32 *outLen )
{
	// get tmd
	tmd *p_tmd = NandTitles.GetTMD( 0x100000002ull );
	if( !p_tmd )
	{
		gprintf( "can\'t get system menu TMD\n" );
		return NULL;
	}

	// determine resource cid
	u16 idx = 0xffff;
	tmd_content *contents = TMD_CONTENTS( p_tmd );
	for( u16 i = 0; i < p_tmd->num_contents; i++ )
	{
		if( contents[ i ].index == p_tmd->boot_index )
		{
			idx = i;
			break;
		}
	}
	if( idx == 0xffff )
	{
		gprintf( "SM executable not found\n" );
		return NULL;
	}

	u8* exeBuf;
	u32 exeSize;
	int ret;
	// build file path
	char path[ ISFS_MAXPATH ]__attribute__((aligned( 32 )));
	sprintf( path, "/title/00000001/00000002/content/%08x.app", contents[ idx ].cid );
	// The TMD content ID is authoritative. The old implementation overwrote
	// its first hexadecimal digit before trying the real path, which breaks
	// revisions whose boot CID is not in that guessed namespace.
	if( ( ret = NandTitles.LoadFileFromNand( path, &exeBuf, &exeSize ) ) < 0 || !exeBuf )
	{
		// Preserve the historical 1xxxxxxx probe only as a last compatibility
		// fallback for unusual extracted NAND layouts.
		path[ 33 ] = '1';
		if( ( ret = NandTitles.LoadFileFromNand( path, &exeBuf, &exeSize ) ) < 0 || !exeBuf )
		{
			gprintf( "Error reading executable from nand: %i\n", ret );
			return NULL;
		}
	}

	*outLen = exeSize;
	return exeBuf;
}

HealthScreen *SystemMenuResources::CreateHealthScreen()
{
	// health.ash is Nintendo's complete Health and Safety screen. Load it first;
	// the optional back-menu transition embedded in the executable must not make
	// the real warning unavailable on an older System Menu revision.
	if( !( healthAsh = mainArc->GetFileAllocated( "/layout/common/health.ash", &healthAshSize ) ) )
	{
		gprintf( "Error while loading health.ash\n" );
		return NULL;
	}

	u32 exeSize = 0;
	u8* exeBuf = LoadSystemMenuExecutable( &exeSize );
	if( exeBuf )
	{
		const u8* backMenu = FindBackMenu( exeBuf, exeSize );
		if( backMenu )
		{
			backMenuData = (u8*)memalign( 32, 0x2720 );
			if( backMenuData )
				memcpy( backMenuData, backMenu, 0x2720 );
		}
		else
			gprintf( "Back-menu transition not found; health.ash exits directly\n" );
		free( exeBuf );
	}

	// create health screen
	HealthScreen *healthScreen = new HealthScreen;
	if( !healthScreen->Load( healthAsh, healthAshSize, backMenuData,
			backMenuData ? 0x2720 : 0 ) )
	{
		gprintf( "error creating health screen\n" );
		delete healthScreen;
		FREE( backMenuData );
		FREE( healthAsh );
		return NULL;
	}
	return healthScreen;
}

void SystemMenuResources::DestroyHealthScreen()
{
	FREE( healthAsh );
	FREE( backMenuData );
}

bool SystemMenuResources::SetupBmg( U8Archive *arc )
{
	const char *lang = CONF_GetLanguageString();
	char path[ 64 ];

	// try to find language file using a couple different paths
	for( int i = 0; i < 4; i++ )
	{
		switch( i )
		{
		case 0:sprintf( path, "/message/%s/ipl_common.bmg", lang ); break;
		case 1:sprintf( path, "/message/%s/ipl_common_noe.bmg", lang ); break;
		case 2:strcpy( path, "/message/eng/ipl_common.bmg" );break;
		case 3:strcpy( path, "/message/eng/ipl_common_noe.bmg" );break;
		}
		bmgData = arc->GetFileAllocated( path, &bmgDataLen );
		if( bmgData )
		{
			if( !Bmg::Instance()->SetResource( bmgData, bmgDataLen ) )
			{
				gprintf( "failed to parse bmg file\n" );
				return false;
			}
			return true;
		}
	}
	gprintf( "no bmg file found\n" );
	return false;
}

const u8* SystemMenuResources::FindBackMenu( const u8* executable, u32 len )
{
	if( !executable || !len )
	{
		return NULL;
	}
	u32 tag = 0x55aa382d;
	u32 crc32Expected = 0x4452a728;
	u32 bmSize = 0x2720;
	u32 *start = (u32*)executable, *end = (u32*)( (executable + len) - bmSize );

	// search for that bastart
	while( start < end )
	{
		if( start[ 0 ] == tag )
		{
			u32 crc = Crc32( (u8*)start, bmSize );
			if( crc == crc32Expected )
			{
				return (u8*)start;
			}
		}
		start++;
	}
	return NULL;
}

Texture *SystemMenuResources::HomeUnavailableIcon()
{
	if( !homeUnavailableData && mainArc )
	{
		u32 size = 0;
		homeUnavailableData = mainArc->GetFileAllocated( "/homebutton/homeBtnIcon.tpl", &size );
		if( homeUnavailableData && size >= 64 ) homeUnavailableIcon.Load( homeUnavailableData );
	}
	return homeUnavailableIcon.IsLoaded() ? &homeUnavailableIcon : NULL;
}

void SystemMenuResources::FreeEverything()
{
	DELETE(themeWidgets);
	themeWidgetsAttempted = false;
	homeUnavailableIcon.Load( NULL );
	FREE( homeUnavailableData );
	isInited = false;
	DELETE( mainArc );

	DELETE( fatalErrordialog );

	FREE( chanSelAsh );
	FREE( boardAsh );
	FREE( calendarAsh );
	FREE( chanTtlAsh );
	FREE( cmnBtnAsh );
	FREE( healthAsh );
	FREE( cursorAsh );
	FREE( bmgData );
	FREE( backMenuData );
	FREE( fatalDlgAsh );
	FREE( balloonAsh );
	FREE( diskBannAsh );
	FREE( dlgWdwAsh );
	FREE( setupBgAsh );
	FREE( setupBtnAsh );
	FREE( setupSelAsh );
	FREE( iplSettingAsh );
	FREE( memoryAsh );
	FREE( diskThumAsh );
	FREE( GCBannAsh );
	FREE( chanEditAsh );
	FREE( homeBtn1Ash );
	FREE( homeBtn1LAsh );

	if( !preserveFonts ) SystemFont::DeInit();

}

bool SystemMenuResources::ReloadTheme()
{
	// Loaded channel icons still share these fonts. Only the menu's resource
	// archives change; reinitializing global fonts would invalidate their glyphs.
	preserveFonts = true;
	FreeEverything();
	const bool loaded = Init();
	preserveFonts = false;
	return loaded;
}

ThemeUi *SystemMenuResources::ThemeWidgets()
{
	if( !themeWidgetsAttempted && mainArc )
	{
		themeWidgetsAttempted = true;
		themeWidgets = new ThemeUi;
		if( !themeWidgets->Load(*mainArc) ) { delete themeWidgets; themeWidgets = NULL; }
	}
	return themeWidgets;
}
