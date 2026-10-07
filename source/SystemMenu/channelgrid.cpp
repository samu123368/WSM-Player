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
#include <stdio.h>
#include <stdarg.h>
#include <ctime>
#include <algorithm>
#include "bannerlist.h"
#include "bmg.h"
#include "buttoncoords.h"
#include "channelgrid.h"
#include "settings.h"
#include "sc.h"
#include "utils/tools.h"
#include "video.h"

// array for which banners go on which icon panes
static const s8 iconBindingIdx[ 42 ] =
{
	-22,					// far left page
	-18,
	-14,

	-13, -12, -11, -10,		// left page
	-9, -8, -7, -6,
	-5, -4, -3, -2,

	-1, 0, 1, 2,			// center page, starting with -1 because the disc channel goes there
	3, 4, 5, 6,
	7, 8, 9, 10,

	11, 12, 13, 14,			// right page
	15, 16, 17, 18,
	19, 20, 21, 22,

	23,						// far right page
	27,
	31
};

namespace
{
	const u16 DragEdgePageFrames = 18;

	void CollectChannelPageChildren( Pane *pages, List< Pane * > &result )
	{
		if( !pages )
			return;
		for( u32 pageIndex = 0; pageIndex < pages->panes.size(); ++pageIndex )
		{
			Pane *page = pages->panes[ pageIndex ];
			if( !strncmp( page->getName(), "BaseMask", 8 ) )
				continue;
			for( u32 iconIndex = 0; iconIndex < page->panes.size(); ++iconIndex )
				result.push_back( page->panes[ iconIndex ] );
		}
	}

	void DrawRoundedScreenRect( f32 x, f32 y, f32 width, f32 height,
		f32 radius, const GXColor &color )
	{
		if( width <= 0.0f || height <= 0.0f )
			return;
		const f32 r = MIN( radius, MIN( width, height ) * 0.5f );
		DrawSquare( x + r, y, width - r * 2.0f, height, color );
		DrawSquare( x, y + r, width, height - r * 2.0f, color );
		DrawSquare( x + r * 0.35f, y + r * 0.35f,
			width - r * 0.70f, height - r * 0.70f, color );
	}

	void DrawRoundedScreenFrame( f32 x, f32 y, f32 width, f32 height,
		f32 radius, f32 thickness, const GXColor &border, const GXColor &fill )
	{
		DrawRoundedScreenRect( x, y, width, height, radius, border );
		DrawRoundedScreenRect( x + thickness, y + thickness,
			width - thickness * 2.0f, height - thickness * 2.0f,
			MAX( 1.0f, radius - thickness ), fill );
	}
}

char ChannelGrid::loadError[ 128 ] = "grid loader did not run";

bool ChannelGrid::LoadFailed( const char *format, ... )
{
	va_list arguments;
	va_start( arguments, format );
	vsnprintf( loadError, sizeof( loadError ), format, arguments );
	va_end( arguments );
	gprintf( "ChannelGrid: %s\n", loadError );
	DeleteEverything();
	return false;
}

ChannelGrid::ChannelGrid()
	: dcIcon( NULL ),
	  lastTime( 0 ),
	  clockIntroFrames( 90 ),
	  channelFrame( NULL ),
	  channelStatic( NULL ),
	  clockLyt( NULL ),
	  gridObj( NULL),
	  clockObj( NULL ),
	  staticObj( NULL ),
	  state( St_Unk ),
	  dragController( -1 ),
	  dragSourceIndex( -1 ),
	  dragHoverSlot( -1 ),
	  dragEdgeDirection( 0 ),
	  dragEdgeFrames( 0 ),
	  pinnedChannelIndex( -1 ),
	  preloadDirection( 1 ),
	  pendingShift( 0 ),
	  pageCnt( 1 ),
	  pageNo( 0 )
{
	for( int i = 0; i < 12; i++ )
	{
		highliteBuffer[ i ] = NULL;
		highliteBrlanBufferClick[ i ] = NULL;
		highliteBrlanBufferMouseOver[ i ] = NULL;
		highliteBrlanBufferMouseOut[ i ] = NULL;
	}

	//gprintf( "loaded %u banners\n", bannerList.size() );
}

ChannelGrid::~ChannelGrid()
{
	DeleteEverything();
}

void ChannelGrid::SetDiscChannelPointer( DiscChannelIcon * dcIconPointer )
{
	dcIcon = dcIconPointer;

	// get layout & animation object for the empty disc channel
	dcLayout = dcIcon->GetNoDiscLayout();
	dcObj = dcIcon->GetNoDiscObject();
}

void ChannelGrid::EnsureBannerLoader( int channelIndex )
{
	if( channelIndex < 0 || channelIndex >= (int)bannerList.size() )
		return;
	BannerListEntry *entry = bannerList[ channelIndex ];
	if( !entry || entry->emptySlot || entry->banner )
		return;
	if( !entry->hbXml )
		entry->banner = new BannerAsync( entry->filepath, entry->tid );
	else
		entry->banner = new BannerAsyncHB( entry->filepath, entry->hbXml,
			entry->tid );
	if( entry->banner && entry->tid )
		entry->banner->SetTitleId( entry->tid );
}

bool ChannelGrid::ChannelNeededForPage( int channelIndex, int centerPage ) const
{
	if( channelIndex < 0 || centerPage < 0 || centerPage >= pageCnt )
		return false;
	// The authored grid has six overshoot slots spread across pages +/-2.
	// Keep those entire pages, not only their three overshoot samples: paging
	// quickly otherwise cancels the remaining jobs just before revealing them.
	return abs((channelIndex + 1) / 12 - centerPage) <= 2;
}

void ChannelGrid::PreloadIconWindow( int centerPage )
{
	if( centerPage < 0 || centerPage >= pageCnt )
		return;

	// Warm all five pages represented by the 42-pane sliding window. Previously
	// only three icons on each far page were loaded, leaving holes at the sides.
	const int priorityPages[ 5 ] = { centerPage, centerPage + preloadDirection,
		centerPage + 2 * preloadDirection, centerPage - preloadDirection,
		centerPage - 2 * preloadDirection };
	for( int priority = 0; priority < 5; ++priority )
	{
		const int page = priorityPages[ priority ];
		if( page < 0 || page >= pageCnt )
			continue;
		for( int slot = 0; slot < 12; ++slot )
			EnsureBannerLoader( page * 12 + slot - 1 );
	}

	// Reprioritize existing jobs too; queue insertion order alone favours old
	// pages when direction changes. Reverse order leaves the current page first.
	for( int priority = 4; priority >= 0; --priority )
		if( priorityPages[priority] >= 0 && priorityPages[priority] < pageCnt )
			PrioritizePage(priorityPages[priority]);
}

bool ChannelGrid::VisiblePageIconsReady() const
{
	return IconSlotsReady( pageNo );
}

bool ChannelGrid::IconSlotsReady( int page ) const
{
	// Include the neighbouring and overshoot columns, not just the twelve
	// settled tiles. Every icon that can enter at an edge must already exist.
	for( int slot = 0; slot < 42; ++slot )
	{
		const int index = page * 12 + iconBindingIdx[slot];
		if( index < 0 || index >= (int)bannerList.size() ) continue;
		const BannerListEntry *entry = bannerList[index];
		if( !entry || entry->emptySlot ) continue;
		// Failed archives are settled too; do not trap the shell on black.
		if( !entry->banner || !entry->banner->IsLoadComplete() ) return false;
	}
	return true;
}

void ChannelGrid::UnbindAllChannels( bool releaseLoaders )
{
	pinnedChannelIndex = -1;
	// Never free layouts still referenced by a pane or by queued GX commands.
	GX_DrawDone();
	foreach( Pane *pane, iconPanes ) pane->UnbindAllLayouts();
	for(int i = 0; i < (int) bannerList.size(); i++)
	{
		if( releaseLoaders )
		{
			BannerAsync::RemoveBanner(bannerList[i]->banner);
			bannerList[i]->banner = NULL;
		}
		bannerList[i]->IsBound = false;
	}

}

void ChannelGrid::PrioritizePage( int page )
{
	for( int slot = 0; slot < 12; ++slot ) EnsureBannerLoader(page * 12 + slot - 1);
	// Moving jobs to the front in reverse preserves the authored slot order.
	for( int slot = 11; slot >= 0; --slot )
	{
		const int index = page * 12 + slot - 1;
		if( index >= 0 && index < (int)bannerList.size() && bannerList[index] )
			BannerAsync::PrioritizeBanner(bannerList[index]->banner);
	}
}

void ChannelGrid::TrimIconCache()
{
	std::vector<std::pair<int, int> > candidates;
	for( int i = 0; i < (int)bannerList.size(); ++i )
	{
		BannerListEntry *entry = bannerList[i];
		if( !entry || !entry->banner || i == pinnedChannelIndex
			|| (dragController >= 0 && i == dragSourceIndex)
			|| ChannelNeededForPage(i, pageNo) ) continue;
		candidates.push_back(std::make_pair(abs((i + 1) / 12 - pageNo), i));
	}
	std::sort(candidates.begin(), candidates.end());
	const u32 budget = 8 * 1024 * 1024;
	u32 retainedBytes = 0;
	int retainedIcons = 0;
	bool fenced = false;
	for( unsigned n = 0; n < candidates.size(); ++n )
	{
		BannerListEntry *entry = bannerList[candidates[n].second];
		const u32 cost = entry->banner->CacheBytes();
		if( retainedIcons < 24 && cost <= budget - retainedBytes )
		{
			retainedBytes += cost;
			++retainedIcons;
			continue;
		}
		// No GPU stall at all on a warm page switch which evicts nothing.
		// A fence is required only before freeing old texture backing stores.
		if( !fenced ) { GX_DrawDone(); fenced = true; }
		BannerAsync::RemoveBanner(entry->banner);
		entry->banner = NULL;
	}
}

void ChannelGrid::BindReadyIcons()
{
	for( int i = 0; i < 42; ++i )
	{
		const int index = pageNo * 12 + iconBindingIdx[i];
		if( index < 0 || index >= (int)bannerList.size() ) continue;
		BannerListEntry *entry = bannerList[index];
		if( !entry || entry->IsBound || !entry->banner || !entry->banner->IsLoadComplete() ) continue;
		// Never replace an empty tile halfway across the screen. A ready icon
		// may bind while still off-screen, or once the page is fully settled.
		if( (state == St_ShiftLeft || state == St_ShiftRight)
			&& iconPanes[i]->IsIconViewportVisible() ) continue;
		Layout *icon = entry->banner->DidLoadFail() ? NULL : entry->banner->getIcon();
		iconPanes[i]->UnbindAllLayouts();
		iconPanes[i]->BindLayout(icon ? icon : channelStatic, true);
		entry->IsBound = true;
		if( i >= 15 && i < 27 && state == St_IdleDown && dragController < 0 )
			channelHighlites[i - 15]->SetEnabled(!entry->banner->DidLoadFail());
	}
}

void ChannelGrid::SetPage( u32 newPage )
{
	if( newPage >= (u32)pageCnt )
	{
		gprintf( "pageN >= gridPageCnt\n" );
		return;
	}
	pageNo = newPage;
	pendingShift = 0;
	foreach( Pane *pane, iconPanes ) pane->UnbindAllLayouts();
	for( int i = 0; i < (int)bannerList.size(); ++i )
		if( bannerList[i] ) bannerList[i]->IsBound = false;
	// Cache trimming happens only after all old references are detached.
	TrimIconCache();
	PreloadIconWindow( pageNo );

	int chIdx = pageNo * 12;
	int sIdx = -15;

	for( int i = 0; i < 42; i++, sIdx++ )
	{
		Pane *pane = iconPanes[ i ];
		pane->UnbindAllLayouts();

		bool enable = false;

		int whichChannelGoesHere = chIdx + iconBindingIdx[ i ];


		// disc channel
		if( whichChannelGoesHere == -1 )
		{
			enable = true;
			pane->BindLayout( dcLayout, true );
			if( dcState != DSt_None && !pageNo )
			{
				highliteLyts[ 0 ]->FindPane( "RootPane" )->BindLayout( dcIcon->GetInsertDiscLayout(), true );
			}
			else
			{
				highliteLyts[ 0 ]->FindPane( "RootPane" )->UnbindAllLayouts();
			}
		}

		else if( whichChannelGoesHere >= 0 && whichChannelGoesHere < (int)bannerList.size())
		{
			EnsureBannerLoader( whichChannelGoesHere );
			BannerAsync *banner = bannerList[ whichChannelGoesHere ]->banner;

			//! If banner was loaded before bind it right away otherwise later when its loaded async
			if( banner && banner->IsLoadComplete() && !banner->DidLoadFail()
				&& banner->getIcon() != NULL )
			{
				enable = true;
				pane->BindLayout( banner->getIcon(), true );
				bannerList[ whichChannelGoesHere ]->IsBound = true;
			}
			else if( banner && banner->IsLoadComplete() && !banner->DidLoadFail() )
			{
				// Some retail/promo channels have unusual or absent icon.bin
				// layouts, but their banner.bin is still valid.  Keep the slot
				// selectable and display the system's normal empty tile.
				enable = true;
				pane->BindLayout( channelStatic, true );
				bannerList[ whichChannelGoesHere ]->IsBound = false;
			}
			else
			{
				bannerList[ whichChannelGoesHere ]->IsBound = false;
			}
		}

		if( !enable )
		{
			pane->BindLayout( channelStatic, true );
		}

		if( sIdx >= 0 && sIdx < 12 )
		{
			channelHighlites[ sIdx ]->SetEnabled( enable );
		}
	}

	if( !pageNo )
	{
		FirstPage( true );
	}
	if( pageNo >= pageCnt - 1 )
	{
		LastPage( true );
	}
	HidePage( 1, !pageNo );
	HidePage( 3, pageNo >= pageCnt - 1 );
	if( dragController >= 0 )
		HideDraggedSourceSlot();
}

bool ChannelGrid::GetIconPaneCoords( int selected, f32 *paneX1, f32 *paneY1, f32 *paneX2, f32 *paneY2 )
{
	selected++;
	int first = pageNo * 12;
	int diff = selected - first;
	if( selected < first || diff > 11 )
	{
		gprintf( "ChannelGrid::GetIconPaneCoords( %i ): out of range\n", selected - 1 );
		return false;
	}
	Pane *pane = iconPanes[ diff + 15 ];

	f32 z;
	f32 AnimPosX1;
	f32 AnimPosY1;
	f32 AnimPosX2;
	f32 AnimPosY2;

	f32 viewport[6];
	f32 projection[7];

	GX_GetViewportv( viewport );
	GX_GetProjectionv( MainProjection, projection, GX_ORTHOGRAPHIC );

	Mtx mv2, mv3;
	guMtxIdentity( mv2 );
	guMtxIdentity( mv3 );
	guMtxTransApply( mv2, mv2, -0.5f * pane->GetOriginX() * pane->GetWidth(),
							 -0.5f * pane->GetOriginY() * pane->GetHeight(), 0.f );
	guMtxTransApply( mv3,mv3, 0.5f * pane->GetOriginX() * pane->GetWidth(),
							 0.5f * pane->GetOriginY() * pane->GetHeight(), 0.f );
	guMtxScaleApply( mv2, mv2, 1.0f, -1.0f, 1.0f );
	guMtxScaleApply( mv3, mv3, 1.0f, -1.0f, 1.0f );
	guMtxConcat( pane->GetView(), mv2, mv2 );
	guMtxConcat( pane->GetView(), mv3, mv3 );

	GX_Project( 0.0f, 0.0f, 0.0f, mv2, projection, viewport, &AnimPosX1, &AnimPosY1, &z );
	GX_Project( 0.0f, 0.0f, 0.0f, mv3, projection, viewport, &AnimPosX2, &AnimPosY2, &z );
	if( paneX1 )
	{
		*paneX1 = AnimPosX1;
	}
	if( paneX2 )
	{
		*paneX2 = AnimPosX2;
	}
	if( paneY1 )
	{
		*paneY1 = AnimPosY1;
	}
	if( paneY2 )
	{
		*paneY2 = AnimPosY2;
	}

	return true;
}

void ChannelGrid::ChannelListChanged()
{
	pinnedChannelIndex = -1;
	dragController = -1;
	dragSourceIndex = -1;
	dragHoverSlot = -1;
	dragEdgeDirection = 0;
	dragEdgeFrames = 0;
	pageNo = 0;
	pageCnt = (RU( bannerList.size() + 1, 12 )) / 12;
	if( pageCnt < 1 )
		pageCnt = 1;

	// Bind the disc channel and all eleven empty placeholders even when NAND,
	// SD and Homebrew discovery found no channels.  The old guard skipped this
	// first binding pass and left a valid empty installation looking broken.
	SetPage( 0 );
}

void ChannelGrid::DeleteEverything()
{
	dragController = -1;
	dragSourceIndex = -1;
	dragHoverSlot = -1;
	dragEdgeDirection = 0;
	dragEdgeFrames = 0;
	DELETE( gridObj );
	DELETE( clockObj );
	DELETE( staticObj );

	DELETE( channelFrame );
	DELETE( channelStatic );
	DELETE( clockLyt );

	foreach( Object *obj, channelHighlites )
	{
		delete obj;
	}
	channelHighlites.clear();

	foreach( Layout * l, highliteLyts )
	{
		delete l;
	}
	highliteLyts.clear();

	std::map< std::string, Animation *>:: iterator it = brlans.begin(), itE = brlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
	brlans.clear();
	iconPanes.clear();

	for( int i = 0; i < 12; i++ )
	{
		FREE( highliteBuffer[ i ] );
		FREE( highliteBrlanBufferClick[ i ] );
		FREE( highliteBrlanBufferMouseOver[ i ] );
		FREE( highliteBrlanBufferMouseOut[ i ] );
	}

	foreach( Animation *anim, highliteAnims )
	{
		delete anim;
	}
	highliteAnims.clear();
}

void ChannelGrid::UpdateClock()
{
	time_t now;
	struct tm * timeinfo;

	// get current time and see if it has changed enough for us to update the clock
	if( time( &now ) == lastTime )
	{
		return;
	}
	timeinfo = localtime( &now );

	// convert timeinfo into texture indexes
	int mins = timeinfo->tm_min;
	int hours = timeinfo->tm_hour;
	//gprintf( "%.02u:%.02u\n", hours, mins );

	const bool pm = ( hours > 11 );
	if( !Settings::clock24Hour )
	{
		hours %= 12;
		if( !hours ) hours = 12;
	}

	int digit0 = mins % 10;
	int digit1 = mins / 10;
	int digit2 = hours % 10;
	int digit3 = hours / 10;

	// The retail 12-hour clock omits the leading zero. The optional 24-hour
	// mode keeps it so midnight is displayed as 00 instead of 0 AM.
	if( !Settings::clock24Hour && !digit3 )
	{
		digit3 = 10;
	}

	// set material texture to use the digits for each number
	SetMaterialIndex( clockLyt, "Clock0", 0, digit0 );
	SetMaterialIndex( clockLyt, "Clock1", 0, digit1 );
	SetMaterialIndex( clockLyt, "Clock2", 0, digit2 );
	SetMaterialIndex( clockLyt, "Clock3", 0, digit3 );
	SetMaterialIndex( clockLyt, "AM_PM_R", 0, pm ? 13 : 12 );// texture 12 is am, 13 is pm
	if( Pane *ampm = clockLyt->FindPane( "AM_PM_R" ) )
		ampm->SetVisible( !Settings::clock24Hour );


	// theres a brlan that blinks the ':' but i haven't quite figured it out. everything
	// i tried makes it blink too fast.  so for now just use this so it blinks in 2 second cycles
	Pane *pane;
	if( (pane = clockLyt->FindPane( "ClockTen" ) ) )
	{
		pane->SetVisible( !pane->GetVisible() );
	}

	if( ( !timeinfo->tm_hour && !timeinfo->tm_min && !timeinfo->tm_sec ) || !lastTime )
	{
		DateChanged( timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon + 1 );
	}

	lastTime = now;
}

bool ChannelGrid::Load( const u8* chanSelAshData, u32 chanSelAshSize )
{
	DeleteEverything();
	strlcpy( loadError, "unknown grid load failure", sizeof( loadError ) );
	loaded = false;
	state = St_Unk;
	//pageNo = 0;
	//lastHighlited = -1;

	U8Archive chanSelArc( chanSelAshData, chanSelAshSize );

	if( !( channelFrame = LoadLayout( chanSelArc, "my_IplTop_a" ) ) )
	{
		return LoadFailed( "layout my_IplTop_a" );
	}

	if( !( channelStatic = LoadLayout( chanSelArc, "my_IplTop_b" ) ) )
	{
		return LoadFailed( "layout my_IplTop_b" );
	}

	if( !( clockLyt = LoadLayout( chanSelArc, "my_Clock_a" ) ) )
	{
		return LoadFailed( "layout my_Clock_a" );
	}


#define LOADANIM( x )										\
	do														\
	{														\
		if( !(anim = LoadAnimation( chanSelArc, x ) ) )		\
		{													\
			return LoadFailed( "animation %s", x );			\
		}													\
		brlans[ x ] = anim;									\
	}														\
	while( 0 )

	// load animations
	Animation *anim;

	LOADANIM( "my_IplTop_a" );
	LOADANIM( "my_IplTop_b" );
	LOADANIM( "my_Clock_a_Change" );						// changes the clock from the initial "Wii Menu" text to a clock

	// create objects
	gridObj = new Object;
	gridObj->BindPane( channelFrame->FindPane( "RootPane" ) );
	gridObj->AddAnimation( brlans.find( "my_IplTop_a" )->second );

	staticObj = new Object;
	staticObj->BindPane( channelStatic->FindPane( "RootPane" ) );
	channelStatic->LoadBrlanTpls( brlans.find( "my_IplTop_b" )->second, chanSelArc );
	staticObj->BindMaterials( channelStatic->Materials() );
	staticObj->AddAnimation( brlans.find( "my_IplTop_b" )->second );
	staticObj->SetAnimation( "my_IplTop_b", 0, 2000 );
	staticObj->Start();

	clockObj = new Object;
	clockObj->BindPane( clockLyt->FindPane( "RootPane" ) );
	clockObj->AddAnimation( brlans.find( "my_Clock_a_Change" )->second );
	clockObj->SetAnimation( "my_Clock_a_Change", 0.f, -1.f, -1.f, false );
	clockObj->SetFrame( 0 );
	clockObj->Start();
	clockIntroFrames = 90;

	Pane *pane1;
	SystemMenuButton btn = ChanSel_0;
	u32 x, y, w, h;
	for( int i = 0; i < 12; i++ )
	{
		// since our layouts are drawn and animated directly from the brlyt data,
		// we need to create separate buffers for the brlyts & brlans for each of the highlites

		highliteBuffer[ i ] = chanSelArc.GetFileAllocated( "/arc/blyt/my_IplTop_d.brlyt" );
		if( !highliteBuffer[ i ] )
			return LoadFailed( "asset my_IplTop_d.brlyt [%d]", i );
		highliteBrlanBufferClick[ i ] = chanSelArc.GetFileAllocated( "/arc/anim/my_IplTop_d_Select.brlan" );
		if( !highliteBrlanBufferClick[ i ] )
			return LoadFailed( "asset my_IplTop_d_Select [%d]", i );
		highliteBrlanBufferMouseOver[ i ] = chanSelArc.GetFileAllocated( "/arc/anim/my_IplTop_d_FocusOn.brlan" );
		if( !highliteBrlanBufferMouseOver[ i ] )
			return LoadFailed( "asset my_IplTop_d_FocusOn [%d]", i );
		highliteBrlanBufferMouseOut[ i ] = chanSelArc.GetFileAllocated( "/arc/anim/my_IplTop_d_FocusOff.brlan" );
		if( !highliteBrlanBufferMouseOut[ i ] )
			return LoadFailed( "asset my_IplTop_d_FocusOff [%d]", i );

		Layout *l = new Layout;
		if( !l->Load( highliteBuffer[ i ] ) || !l->LoadTextures( chanSelArc ))
		{
			delete l;
			return LoadFailed( "highlight layout/textures [%d]", i );
		}

		// get button coords
		ButtonCoords( (SystemMenuButton)( btn + i ), x, y, w, h );

		// set position to be drawn
		if( (pane1 = l->FindPane( "RootPane" ) ) )
		{
			f32 x2 = ((((f32)screenwidth) / 2.f) - ( ((f32)screenwidth) - x )) + ( w / 2 );
			f32 y2 = ((((f32)screenheight) / 2.f) - ( ((f32)screenheight) - y )) + ( h / 2 );
			pane1->SetPosition( x2, -y2 );
		}


		// create button
		QuadButton *obj = new QuadButton( x, y, w, h );


		// setup animations
		Animation* anim = new Animation( "my_IplTop_d_Select" );
		anim->Load( (const RLAN_Header *)highliteBrlanBufferClick[ i ] );
		obj->AddAnimation( anim );
		obj->SetMouseOutAnimation( anim, 0, -1 );
		highliteAnims << anim;

		anim = new Animation( "my_IplTop_d_FocusOn" );
		anim->Load( (const RLAN_Header *)highliteBrlanBufferMouseOver[ i ] );
		obj->AddAnimation( anim );
		obj->SetMouseOverAnimation( anim, 0, -1 );
		highliteAnims << anim;

		anim = new Animation( "my_IplTop_d_FocusOff" );
		anim->Load( (const RLAN_Header *)highliteBrlanBufferMouseOut[ i ] );
		obj->AddAnimation( anim );
		obj->SetMouseOutAnimation( anim, 0, -1 );
		highliteAnims << anim;

		// bind stuff
		obj->BindPane( l->FindPane( "RootPane" ) );
		obj->BindMaterials( l->Materials() );

		obj->SetTrigger( Button::Btn_A );

		//leftBtn->Clicked.connect( this, &BannerFrame::LeftButtonClickedSlot );
		//rightBtn->Clicked.connect( this, &BannerFrame::LeftButtonClickedSlot );

		highliteLyts << l;
		channelHighlites << obj;
	}

	const PaneList &rootPanes = channelFrame->Panes();
	Pane *channelPages = channelFrame->FindPane( "N_ChAll" );
	if( !channelPages && !rootPanes.empty() )
	{
		// my_IplTop_a has one non-leaf channel branch below RootPane.  Walk
		// that authored structure without relying on legacy fixed-name lookup.
		Pane *root = rootPanes[ 0 ];
		for( u32 i = 0; i < root->panes.size() && !channelPages; ++i )
		{
			Pane *branch = root->panes[ i ];
			if( branch->panes.size() == 1 && branch->panes[ 0 ]->panes.size() >= 5 )
				channelPages = branch->panes[ 0 ];
		}
	}
	CollectChannelPageChildren( channelPages, iconPanes );
	foreach( Pane *iconPane, iconPanes )
		iconPane->SetChannelIconViewport();
	if( iconPanes.size() != 42 )// there are 42 places for icons in the grid
	{
		return LoadFailed( "icon panes %u/42", iconPanes.size() );
	}
	SetText( clockLyt, "T_WiiMenu", 1 );				// "Wii Menu"

	// theres an AM/PM thing on the right and the left.  remove the left one
	SetPaneVisible( clockLyt, "AM_PM", false );

	// The native strip has three clock anchors. Populate all three with the
	// same layout/time, advancing it once per frame, not once per anchor.
	// Otherwise a page slide brings an empty anchor into view before snapping
	// back to the populated middle one.
	for( int anchor = 0; anchor < 3; ++anchor )
	{
		char name[20];
		sprintf( name, "N_Clock%i", anchor );
		if( (pane1 = channelFrame->FindPane( name )) )
		{
			pane1->SetVisible( true );
			pane1->BindLayout( clockLyt, false );
		}
	}

	bool widescreen = ( _CONF_GetAspectRatio() > 0 );

	if( widescreen )
	{
		for( int i = 0; i < 5; i++ )
		{
			char name[ 20 ];
			sprintf( name, "Edge%i", i );
			SetMaterialIndex( channelFrame, name, 0, 2 );


			sprintf( name, "Picture_0%i", i );
			SetMaterialIndex( channelFrame, name, 0, 0 );
		}
	}

	channelHighlites[ 0 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked0 );
	channelHighlites[ 1 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked1 );
	channelHighlites[ 2 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked2 );
	channelHighlites[ 3 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked3 );

	channelHighlites[ 4 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked4 );
	channelHighlites[ 5 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked5 );
	channelHighlites[ 6 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked6 );
	channelHighlites[ 7 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked7 );

	channelHighlites[ 8 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked8 );
	channelHighlites[ 9 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked9 );
	channelHighlites[ 10 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked10 );
	channelHighlites[ 11 ]->Clicked.connect( this, &ChannelGrid::ChannelClicked11 );


	gridObj->Finished.connect( this, &ChannelGrid::GridAnimationFinished );


	loaded = true;
	state = St_IdleDown;
	dcState = DSt_None;
	lastTime = 0;
	// First Render initializes the digits before drawing, after DateChanged
	// listeners have been connected by MenuHandler::Start.
	return true;
}

void ChannelGrid::HidePage( u8 page, bool hide )
{
	char name[ 20 ];
	sprintf( name, "Edge%i", page );

	SetPaneVisible( channelFrame, name, !hide );

	// ?	2	0	1	?
	SetPaneVisible( channelFrame, page == 1 ? "Picture_02" : "Picture_01", !hide );


	sprintf( name, "N_Ch_%c", 'a' + page );
	SetPaneVisible( channelFrame, name, !hide );
}

void ChannelGrid::Render( Mtx &modelview, const Vec2f &ScreenProps, bool widescreen, bool interactive )
{
	if( !loaded || state == St_IdleUp )
	{
		return;
	}

	if( interactive )
	{
		HandleUserInput();
		if( pendingShift ) QueuePageShift( pendingShift );
	}
	BindReadyIcons();
	UpdateClock();

	// draw
	//channelFrame->Render( mv, ScreenProps, widescreen );
	channelFrame->Render( modelview, ScreenProps, widescreen );

	// update
	gridObj->Advance();
	staticObj->Advance();
	AdvanceClock( interactive );
	for( u32 i = 0; i < 12; i++ )
	{
		if( state == St_IdleDown )
		{
			highliteLyts[ i ]->Render( modelview, ScreenProps, widescreen );
		}
		if( interactive ) channelHighlites[i]->Update();
		channelHighlites[i]->Advance();
	}

	// Advance the full warm window, including the far-page columns not yet
	// bound to one of the 42 slots. Their intro must not start from an empty
	// frame when they first enter from a screen edge. Rendering remains culled.
	// During a slide also advance the destination's far column before its
	// anchors are rebound. It must not arrive at animation frame zero when the
	// page boundary is committed, even though its archive was already loaded.
	const int animationCenter = pageNo + (state == St_ShiftLeft ? 1
		: state == St_ShiftRight ? -1 : pendingShift);
	const int firstIcon = MAX(0, (MIN(pageNo, animationCenter) - 2) * 12 - 1);
	const int lastIcon = MIN((int)bannerList.size(), (MAX(pageNo, animationCenter) + 3) * 12 - 1);
	for( int whichChannelGoesHere = firstIcon; whichChannelGoesHere < lastIcon; ++whichChannelGoesHere )
	{
		if( bannerList[ whichChannelGoesHere ]->banner
		   && bannerList[ whichChannelGoesHere ]->banner->IsLoadComplete()
		   && !bannerList[ whichChannelGoesHere ]->banner->DidLoadFail()
		   && bannerList[ whichChannelGoesHere ]->banner->getIcon())
		{
			bannerList[ whichChannelGoesHere ]->banner->AdvanceIcon();
		}
	}

	// animate disc channel
	//gprintf( "dcObj->Advance();\n" );
	dcObj->Advance();
	if( dcState != DSt_None )
	{
		//gprintf( "dcIcon->GetInsertDiscObject()->Advance();\n" );
		dcIcon->GetInsertDiscObject()->Advance();
	}

	RenderDragOverlay( ScreenProps );

}

void ChannelGrid::AdvanceClock( bool interactive )
{
	// Hold the localized BMG "Wii Menu" label at the native first frame, then
	// play Nintendo's authored crossfade. Page changes never restart it, and
	// black/loading/fade-in frames do not consume the readable label interval.
	if( !interactive ) return;
	if( clockIntroFrames > 0 ) --clockIntroFrames;
	else clockObj->Advance();
}

void ChannelGrid::DiscInserted( DiHandler::DiscType dt )
{
	switch( dt )
	{
	case DiHandler::T_GC:
		dcLayout = dcIcon->GetGCLayout();
		dcObj = dcIcon->GetGCObject();
		dcState = DSt_GC;
		break;
	case DiHandler::T_Wii:
		dcState = DSt_Wii;
		break;
	case DiHandler::T_Unknown:
		dcLayout = dcIcon->GetNoDiscLayout();
		dcObj = dcIcon->GetNoDiscObject();
		dcState = DSt_None;
		break;
	}

	if( pageNo == 0 )
	{
		iconPanes[ 15 ]->UnbindAllLayouts();
		iconPanes[ 15 ]->BindLayout( dcLayout, true );

		// uberhaxx
		highliteLyts[ 0 ]->FindPane( "RootPane" )->BindLayout( dcIcon->GetInsertDiscLayout(), true );
	}
	dcIcon->StartInsertDiscAnim();
}

void ChannelGrid::DiscEjected()
{
	dcLayout = dcIcon->GetNoDiscLayout();
	dcObj = dcIcon->GetNoDiscObject();

	if( pageNo == 0 )
	{
		iconPanes[ 15 ]->UnbindAllLayouts();
		iconPanes[ 15 ]->BindLayout( dcLayout, true );

		highliteLyts[ 0 ]->FindPane( "RootPane" )->BindLayout( dcIcon->GetInsertDiscLayout(), true );
	}
	dcIcon->StartEjectDiscAnim();
}

void ChannelGrid::SetDiscChannelIcon( Layout *lyt, Object *obj )
{
	if( lyt && obj )
	{
		//gprintf( "ChannelGrid::SetDiscChannelIcon(): setting disc banner animation\n" );
		dcLayout = lyt;
		dcObj = obj;
		dcObj->Start();
	}
	else
	{
		dcLayout = dcIcon->GetNoDiscLayout();
		dcObj = dcIcon->GetNoDiscObject();
	}

	if( pageNo == 0 )
	{
		//gprintf( "ChannelGrid::SetDiscChannelIcon(): binding animation\n" );
		iconPanes[ 15 ]->UnbindAllLayouts();
		iconPanes[ 15 ]->BindLayout( dcLayout, true );

		highliteLyts[ 0 ]->FindPane( "RootPane" )->BindLayout( dcIcon->GetInsertDiscLayout(), true );
	}
}

void ChannelGrid::GridAnimationFinished()
{
	switch( state )
	{
	case St_ShiftUp:
		state = St_IdleUp;
		break;
	case St_ShiftDown:
		state = St_IdleDown;
		break;
	case St_ShiftLeft:
		SetPage( pageNo + 1 );
		state = St_IdleDown;
		break;
	case St_ShiftRight:
		SetPage( pageNo - 1 );
		state = St_IdleDown;
		break;
	default:
		break;
	}
}

void ChannelGrid::HandlePageInput()
{
	if( state == St_IdleDown )
	{
		// check for button presses
		for(int i = 0; i < 4; i++ )
		{
			if( Pad( i ).pPlus() )
			{
				RequestShiftLeft();
				break;
			}
			if( Pad( i ).pMinus() )
			{
				RequestShiftRight();
				break;
			}
		}
	}
}

void ChannelGrid::HandleUserInput()
{
	if( state != St_IdleDown )
		return;

	// A+B picks up a non-disc channel. Taking the controller here, before
	// QuadButton::Update(), prevents the initiating A press from also opening
	// that channel's banner.
	if( dragController >= 0 )
	{
		Controller &controller = Pad( dragController );
		controller.Take();
		dragHoverSlot = HoveredChannelSlot( controller );

		if( controller.hA() && controller.hB() )
		{
			// Plus/minus provide an exact cross-page option while holding the
			// channel. Lingering at either screen edge also turns the page.
			if( controller.pPlus() && pageNo < pageCnt - 1 )
			{
				RequestShiftLeft();
				dragEdgeFrames = 0;
				dragEdgeDirection = 0;
			}
			else if( controller.pMinus() && pageNo > 0 )
			{
				RequestShiftRight();
				dragEdgeFrames = 0;
				dragEdgeDirection = 0;
			}
			else
			{
				const WPADData &wpad = controller.GetData();
				int edgeDirection = 0;
				if( wpad.ir.valid && wpad.ir.x < 24.0f && pageNo > 0 )
					edgeDirection = -1;
				else if( wpad.ir.valid && wpad.ir.x > screenwidth - 24.0f && pageNo < pageCnt - 1 )
					edgeDirection = 1;
				if( edgeDirection != dragEdgeDirection )
				{
					dragEdgeDirection = edgeDirection;
					dragEdgeFrames = 0;
				}
				else if( edgeDirection && ++dragEdgeFrames >= DragEdgePageFrames )
				{
					if( edgeDirection > 0 )
						RequestShiftLeft();
					else
						RequestShiftRight();
					dragEdgeFrames = 0;
					dragEdgeDirection = 0;
				}
			}
			return;
		}

		FinishChannelDrag( dragHoverSlot );
		return;
	}

	for( int i = 0; i < 4; ++i )
	{
		Controller &controller = Pad( i );
		if( controller.hA() && controller.hB() && ( controller.pA() || controller.pB() ) )
		{
			const int slot = HoveredChannelSlot( controller );
			const int channelIndex = ChannelIndexForSlot( slot );
			if( channelIndex >= 0 && channelIndex < (int)bannerList.size()
				&& bannerList[ channelIndex ]
				&& !bannerList[ channelIndex ]->emptySlot )
			{
				dragController = i;
				dragSourceIndex = channelIndex;
				dragHoverSlot = slot;
				dragEdgeDirection = 0;
				dragEdgeFrames = 0;
				HideDraggedSourceSlot();
				controller.Take();
				gprintf( "Channel drag: picked up %s\n",
					BannerOrderKey( bannerList[ channelIndex ] ).c_str() );
				return;
			}
		}
	}

	HandlePageInput();
}

int ChannelGrid::HoveredChannelSlot( const Controller &controller ) const
{
	const WPADData &wpad = controller.GetData();
	if( !wpad.ir.valid )
		return -1;
	for( int slot = 0; slot < (int)channelHighlites.size(); ++slot )
	{
		if( channelHighlites[ slot ]->Contains( wpad.ir.x, wpad.ir.y ) )
			return slot;
	}
	return -1;
}

int ChannelGrid::ChannelIndexForSlot( int slot ) const
{
	if( slot < 0 || slot >= 12 )
		return -2;
	return pageNo * 12 + slot - 1;
}

void ChannelGrid::HideDraggedSourceSlot()
{
	if( dragSourceIndex < 0 || dragSourceIndex >= (int)bannerList.size() )
		return;
	const int visibleSlot = dragSourceIndex - pageNo * 12 + 1;
	if( visibleSlot < 0 || visibleSlot >= 12 )
		return;
	const int paneIndex = 15 + visibleSlot;
	if( paneIndex < 0 || paneIndex >= (int)iconPanes.size() )
		return;
	iconPanes[ paneIndex ]->UnbindAllLayouts();
	iconPanes[ paneIndex ]->BindLayout( channelStatic, true );
	// Render() uses this flag to avoid asynchronously rebinding the source
	// icon underneath the floating copy on the same frame.
	bannerList[ dragSourceIndex ]->IsBound = true;
}

void ChannelGrid::RenderDragOverlay( const Vec2f &ScreenProps )
{
	if( dragController < 0 || dragSourceIndex < 0
		|| dragSourceIndex >= (int)bannerList.size() )
		return;

	// The source becomes an empty Wii Menu tile, with only a restrained white
	// placement glow.  Never render the live BRLYT here: outside its authored
	// icon pane it loses the scissor and transform that keep large banner panes
	// upright and clipped.
	const int sourceSlot = dragSourceIndex - pageNo * 12 + 1;
	if( sourceSlot >= 0 && sourceSlot < (int)channelHighlites.size() )
	{
		const QuadButton *source = channelHighlites[ sourceSlot ];
		const f32 x = source->pos.x - 3.0f;
		const f32 y = source->pos.y - 3.0f;
		const f32 width = source->br.x - source->pos.x + 6.0f;
		const f32 height = source->br.y - source->pos.y + 6.0f;
		DrawRoundedScreenFrame( x, y, width, height, 10.0f, 3.0f,
			(GXColor){ 255, 255, 255, 76 }, (GXColor){ 255, 255, 255, 24 } );
	}

	// Empty tiles are valid destinations too.  Show the same restrained
	// placement glow even when the tile has no enabled channel button, so it is
	// obvious where releasing A+B/right+left click will place the channel.
	if( dragHoverSlot >= 0 && dragHoverSlot < (int)channelHighlites.size()
		&& dragHoverSlot != sourceSlot )
	{
		const QuadButton *target = channelHighlites[ dragHoverSlot ];
		const f32 targetX = target->pos.x - 4.0f;
		const f32 targetY = target->pos.y - 4.0f;
		const f32 targetWidth = target->br.x - target->pos.x + 8.0f;
		const f32 targetHeight = target->br.y - target->pos.y + 8.0f;
		DrawRoundedScreenFrame( targetX, targetY, targetWidth, targetHeight,
			11.0f, 4.0f, (GXColor){ 80, 211, 255, 190 },
			(GXColor){ 255, 255, 255, 40 } );
	}

	const WPADData &wpad = Pad( dragController ).GetData();
	if( !wpad.ir.valid )
		return;

	// A small screen-space proxy is what the real menu presents while moving a
	// channel.  It is deliberately independent of the channel artwork, so even
	// malformed or unusually large icons cannot escape their grid slot.
	const f32 width = 72.0f;
	const f32 height = 50.0f;
	const f32 maxX = MAX( 4.0f, ScreenProps.x - width - 4.0f );
	const f32 maxY = MAX( 4.0f, ScreenProps.y - height - 4.0f );
	const f32 x = MIN( maxX, MAX( 4.0f, wpad.ir.x - width * 0.5f ) );
	const f32 y = MIN( maxY, MAX( 4.0f, wpad.ir.y - height - 12.0f ) );
	DrawRoundedScreenRect( x + 3.0f, y + 4.0f, width, height, 8.0f,
		(GXColor){ 20, 79, 102, 64 } );
	DrawRoundedScreenFrame( x, y, width, height, 8.0f, 3.0f,
		(GXColor){ 0, 166, 224, 244 }, (GXColor){ 194, 239, 252, 238 } );
	DrawRoundedScreenRect( x + 7.0f, y + 7.0f, width - 14.0f, 9.0f, 3.0f,
		(GXColor){ 255, 255, 255, 132 } );
	DrawRoundedScreenRect( x + 10.0f, y + 23.0f, width - 20.0f, 15.0f, 4.0f,
		(GXColor){ 54, 183, 227, 168 } );
}

void ChannelGrid::FinishChannelDrag( int targetSlot )
{
	int targetIndex = ChannelIndexForSlot( targetSlot );

	if( dragSourceIndex >= 0 && dragSourceIndex < (int)bannerList.size()
		&& targetIndex >= 0 )
	{
		const std::string sourceKey = BannerOrderKey( bannerList[ dragSourceIndex ] );
		const std::string targetKey = BannerOrderKey( bannerList[ targetIndex ] );
		if( SwapBannerListEntries( dragSourceIndex, targetIndex ) )
		{
			gprintf( "Channel drag: swapped %s with %s\n",
				sourceKey.c_str(), targetKey.c_str() );
		}
	}

	dragController = -1;
	dragSourceIndex = -1;
	dragHoverSlot = -1;
	dragEdgeDirection = 0;
	dragEdgeFrames = 0;
	SetPage( pageNo );
}

void ChannelGrid::RequestShiftLeft()
{
	QueuePageShift( 1 );
}

void ChannelGrid::RequestShiftRight()
{
	QueuePageShift( -1 );
}

void ChannelGrid::QueuePageShift( int direction )
{
	if( state != St_IdleDown ) return;
	const int target = pageNo + direction;
	if( target < 0 || target >= pageCnt ) { pendingShift = 0; return; }
	if( pendingShift != direction )
	{
		pendingShift = direction;
		preloadDirection = direction;
		PreloadIconWindow( target );
	}
	// No fixed delay on warm pages. If NAND has not finished an incoming icon,
	// retain the complete current page and keep input/rendering alive until it
	// settles, rather than reveal a blank tile and pop the icon in mid-slide.
	if( !IconSlotsReady( pageNo ) || !IconSlotsReady( target ) ) return;
	BindReadyIcons();
	pendingShift = 0;
	for( int i = 0; i < 12; ++i ) channelHighlites[i]->SetEnabled( false );
	gridObj->SetAnimation( "my_IplTop_a", direction > 0 ? 40 : 0,
		direction > 0 ? 60 : 20, 0, false );
	gridObj->Start();
	state = direction > 0 ? St_ShiftLeft : St_ShiftRight;
	if( direction > 0 && !pageNo ) FirstPage( false );
	if( direction < 0 && pageNo >= pageCnt - 1 ) LastPage( false );
}

void ChannelGrid::ChannelClicked0()
{
	ChannelClicked( 0, 0, ( pageNo * 12 ) - 1 );
}

void ChannelGrid::ChannelClicked1()
{
	ChannelClicked( 0, 1, ( pageNo * 12 ) + 0 );
}

void ChannelGrid::ChannelClicked2()
{
	ChannelClicked( 0, 2, ( pageNo * 12 ) + 1 );
}

void ChannelGrid::ChannelClicked3()
{
	ChannelClicked( 0, 3, ( pageNo * 12 ) + 2 );
}

void ChannelGrid::ChannelClicked4()
{
	ChannelClicked( 1, 0, ( pageNo * 12 ) + 3 );
}

void ChannelGrid::ChannelClicked5()
{
	ChannelClicked( 1, 1, ( pageNo * 12 ) + 4 );
}

void ChannelGrid::ChannelClicked6()
{
	ChannelClicked( 1, 2, ( pageNo * 12 ) + 5 );
}

void ChannelGrid::ChannelClicked7()
{
	ChannelClicked( 1, 3, ( pageNo * 12 ) + 6 );
}

void ChannelGrid::ChannelClicked8()
{
	ChannelClicked( 2, 0, ( pageNo * 12 ) + 7 );
}

void ChannelGrid::ChannelClicked9()
{
	ChannelClicked( 2, 1, ( pageNo * 12 ) + 8 );
}

void ChannelGrid::ChannelClicked10()
{
	ChannelClicked( 2, 2, ( pageNo * 12 ) + 9 );
}

void ChannelGrid::ChannelClicked11()
{
	ChannelClicked( 2, 3, ( pageNo * 12 ) + 10 );
}
