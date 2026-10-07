#include "messageboardscreen.h"
#include "messageboardlive.h"

#include <cstdlib>
#include <algorithm>
#include <gd.h>

#include "greybackground.h"
#include "Inputs.h"
#include "Texture.h"
#include "utils/TextureConverter.h"
#include "video.h"

namespace
{
	const int OpenFrames = 30;
	const int CloseFrames = 30;
	const int PageFrames = 10;

	bool InRect( float x, float y, float left, float top,
		float right, float bottom )
	{
		return x >= left && x <= right && y >= top && y <= bottom;
	}

	void PrepareTextureGX()
	{
		LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
		GX_SetCullMode( GX_CULL_NONE );
		GX_SetZMode( GX_DISABLE, GX_ALWAYS, GX_FALSE );
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
	}
}

MessageBoardScreen::MessageBoardScreen()
	: snapshot( NULL ), previousSnapshot( NULL ), menuSnapshot( NULL ),
	  menuPixels( NULL ), pixels( NULL ),
	  previousPixels( NULL ), entryBackground( NULL ), page( PageLanding ),
	  transitionFrame( 0 ), transitionDirection( 0 ), openingFrame( 0 ),
	  closingFrame( 0 ), closed( true ), requestedSettings( false ), live(NULL),
	  previousPage(PageLanding), dayOffset(0), previousDayOffset(0),
	  monthOffset(0), previousMonthOffset(0)
{
}

MessageBoardScreen::~MessageBoardScreen()
{
	delete live;
	delete snapshot;
	delete previousSnapshot;
	free( pixels );
	free( previousPixels );
	delete menuSnapshot;
	free( menuPixels );
}

bool MessageBoardScreen::LoadPage( Page target )
{
	if( live )
	{
		previousPage = page; previousDayOffset = dayOffset;
		previousMonthOffset = monthOffset;
		page = target;
		// Returning from a submenu keeps the selected date.
		loaded = true;
		return true;
	}
	// Missing native board resources must not resurrect copied screenshots.
	return false;
}

bool MessageBoardScreen::Load(const U8Archive *archive)
{
	if( archive )
	{
		live = new MessageBoardLive;
		if( !live->Load(*archive) )
		{
			delete live; live = NULL;
			// A partial theme must not silently show stock screenshots.
			return false;
		}
	}
	return LoadPage( PageLanding );
}

void MessageBoardScreen::Open( GreyBackground *background )
{
	entryBackground = background;
	delete menuSnapshot;
	menuSnapshot = NULL;
	free( menuPixels );
	u32 bytes = 0;
	u16 width = 0, height = 0;
	menuPixels = CreateTextureFromDisplayBuffer( bytes, width, height );
	if( menuPixels )
	{
		menuSnapshot = new Texture;
		menuSnapshot->LoadFromRawData( menuPixels, width, height, GX_TF_RGB565 );
	}
	closed = false;
	requestedSettings = false;
	openingFrame = OpenFrames;
	closingFrame = 0;
	transitionFrame = 0;
	FinishTransition();
	CInputs::Instance()->ClearButtonsDown();
}

void MessageBoardScreen::BeginClose( bool openSettings )
{
	if( closingFrame > 0 || closed ) return;
	requestedSettings = openSettings;
	closingFrame = CloseFrames;
	openingFrame = 0;
	transitionFrame = 0;
	FinishTransition();
	CInputs::Instance()->ClearButtonsDown();
}

void MessageBoardScreen::FinishTransition()
{
	delete previousSnapshot;
	previousSnapshot = NULL;
	free( previousPixels );
	previousPixels = NULL;
}

bool MessageBoardScreen::ChangePage( Page target, int direction )
{
	if( target == page )
	{
		transitionFrame = 3;
		transitionDirection = 0;
		CInputs::Instance()->ClearButtonsDown();
		return true;
	}
	if( !LoadPage( target ) ) return false;
	transitionFrame = PageFrames;
	transitionDirection = direction;
	CInputs::Instance()->ClearButtonsDown();
	return true;
}

void MessageBoardScreen::ChangeDay(int direction)
{
	if( !live )
	{
		// Legacy image-only fallback has only three photographed dates.
		const int target = std::max(0,std::min(2,(int)page + direction));
		ChangePage((Page)target,direction);
		return;
	}
	previousPage = page; previousDayOffset = dayOffset;
	previousMonthOffset = monthOffset;
	dayOffset += direction;
	page = PageLanding;
	monthOffset = 0;
	transitionFrame = PageFrames; transitionDirection = direction;
	CInputs::Instance()->ClearButtonsDown();
}

void MessageBoardScreen::GoBack()
{
	switch( page )
	{
	case PageLandingPrevious:
	case PageLanding:
	case PageLandingNext:
		BeginClose();
		break;
	case PageCalendar:
	case PageCompose:
		ChangePage( PageLanding );
		break;
	case PageMemo:
	case PageAddressBook:
		ChangePage( PageCompose );
		break;
	case PageDisabledMessage:
		ChangePage( PageCompose );
		break;
	case PageDisabledRegister:
		ChangePage( PageAddressBook );
		break;
	}
}

void MessageBoardScreen::ActivateAt( float x, float y,
	const Vec2f &screen )
{
	if( screen.x <= 0.0f || screen.y <= 0.0f ) return;
	const float px = x * 640.0f / screen.x;
	const float py = y * 480.0f / screen.y;

	switch( page )
	{
	case PageLandingPrevious:
	case PageLanding:
	case PageLandingNext:
		if( InRect( px, py, 0, 120, 85, 260 ) )
		{
			ChangeDay(-1);
		}
		else if( InRect( px, py, 555, 120, 640, 260 ) )
		{
			ChangeDay(1);
		}
		else if( InRect( px, py, 15, 345, 100, 475 ) )
			ChangePage( PageCalendar );
		else if( InRect( px, py, 85, 345, 170, 475 ) )
			ChangePage( PageCompose );
		else if( InRect( px, py, 535, 345, 640, 480 ) )
			BeginClose();
		break;
	case PageCalendar:
		// The visible native Back button is in the footer. A tall old image
		// hitbox intercepted dates in the calendar's sixth row.
		if( InRect( px, py, 0, 375, 185, 480 ) )
			ChangePage( PageLanding );
		else if( InRect( px, py, 0, 120, 85, 275 ) )
			monthOffset = std::max(-1200,monthOffset - 1);
		else if( InRect( px, py, 555, 120, 640, 275 ) )
			monthOffset = std::min(1200,monthOffset + 1);
		else if( live )
		{
			int selected;
			if( live->CalendarDateAt(x,y,screen,dayOffset,monthOffset,selected) )
			{
				ChangePage(PageLanding);
				dayOffset = selected; monthOffset = 0;
			}
		}
		break;
	case PageCompose:
		if( InRect( px, py, 0, 345, 185, 480 ) )
			ChangePage( PageLanding );
		else if( InRect( px, py, 35, 70, 210, 340 ) )
			ChangePage( PageMemo );
		else if( InRect( px, py, 225, 65, 420, 340 ) )
			ChangePage( PageDisabledMessage );
		else if( InRect( px, py, 430, 55, 625, 340 ) )
			ChangePage( PageAddressBook );
		break;
	case PageMemo:
		if( InRect( px, py, 0, 345, 185, 480 ) )
			ChangePage( PageCompose );
		else if( InRect( px, py, 455, 345, 640, 480 ) )
			ChangePage( PageLanding );
		// The user excluded the on-screen keyboard from image backing.
		break;
	case PageAddressBook:
		if( InRect( px, py, 0, 345, 185, 480 ) )
			ChangePage( PageCompose );
		else if( InRect( px, py, 455, 345, 640, 480 ) )
			ChangePage( PageDisabledRegister );
		else if( InRect( px, py, 0, 120, 85, 275 )
			|| InRect( px, py, 555, 120, 640, 275 ) )
			ChangePage( PageAddressBook );
		break;
	case PageDisabledMessage:
	case PageDisabledRegister:
		if( InRect( px, py, 125, 310, 330, 445 ) )
			GoBack();
		else if( InRect( px, py, 315, 310, 535, 445 ) )
			BeginClose( true );
		break;
	}
}

void MessageBoardScreen::ActivateWithoutPointer()
{
	switch( page )
	{
	case PageLandingPrevious:
	case PageLanding:
	case PageLandingNext:
		ChangePage( PageCompose );
		break;
	case PageCalendar:
		ChangePage( PageLanding );
		break;
	case PageCompose:
		ChangePage( PageMemo );
		break;
	case PageMemo:
		ChangePage( PageCompose );
		break;
	case PageAddressBook:
		ChangePage( PageDisabledRegister );
		break;
	case PageDisabledMessage:
	case PageDisabledRegister:
		GoBack();
		break;
	}
}

void MessageBoardScreen::Update( const Vec2f &screen )
{
	if( closed ) return;
	if( openingFrame > 0 )
	{
		--openingFrame;
		return;
	}
	if( closingFrame > 0 )
	{
		--closingFrame;
		if( closingFrame == 0 ) closed = true;
		return;
	}
	if( transitionFrame > 0 )
	{
		--transitionFrame;
		if( transitionFrame == 0 ) FinishTransition();
		return;
	}

	for( int i = 0; i < 4; ++i )
	{
		Controller &controller = Pad( i );
		if( controller.pHome() )
		{
			BeginClose();
			return;
		}
		if( controller.pB() )
		{
			GoBack();
			return;
		}
		if( page == PageLandingPrevious || page == PageLanding
			|| page == PageLandingNext )
		{
			if( controller.pLeft() || controller.pMinus() )
			{
				ChangeDay(-1);
				return;
			}
			if( controller.pRight() || controller.pPlus() )
			{
				ChangeDay(1);
				return;
			}
		}
		if( page == PageCalendar )
		{
			if( controller.pLeft() || controller.pMinus() )
			{ monthOffset = std::max(-1200,monthOffset - 1); return; }
			if( controller.pRight() || controller.pPlus() )
			{ monthOffset = std::min(1200,monthOffset + 1); return; }
		}
		if( !controller.pA() ) continue;
		const WPADData &data = controller.GetData();
		if( data.ir.valid ) ActivateAt( data.ir.x, data.ir.y, screen );
		else ActivateWithoutPointer();
		return;
	}
}

void MessageBoardScreen::DrawTexture( Texture *texture, float x, float y,
	float width, float height, u8 alpha ) const
{
	if( !texture || !texture->IsLoaded() || width <= 0.0f || height <= 0.0f )
		return;
	u8 tlut = 0;
	texture->Apply( tlut, GX_TEXMAP0, GX_CLAMP, GX_CLAMP );
	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	GX_Position3f32( x, y, 0.0f );
	GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 0.0f, 0.0f );
	GX_Position3f32( x + width, y, 0.0f );
	GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 1.0f, 0.0f );
	GX_Position3f32( x + width, y + height, 0.0f );
	GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 1.0f, 1.0f );
	GX_Position3f32( x, y + height, 0.0f );
	GX_Color4u8( 255, 255, 255, alpha ); GX_TexCoord2f32( 0.0f, 1.0f );
	GX_End();
}

void MessageBoardScreen::Render( Mtx &modelview, const Vec2f &screen,
	bool widescreen )
{
	if( !live && (!snapshot || !snapshot->IsLoaded()) ) return;
	// Update marks the screen closed immediately before MenuHandler disposes it.
	// Repaint the underlying board for that last frame so closing never flashes
	// a black frame on real hardware.
	if( closed )
	{
		if( requestedSettings )
		{
			DrawSquare( 0, 0, screen.x, screen.y, (GXColor){0, 0, 0, 255} );
			return;
		}
		if( menuSnapshot )
		{
			PrepareTextureGX();
			DrawTexture( menuSnapshot, 0.0f, 0.0f, screen.x, screen.y, 255 );
		}
		else if( entryBackground ) entryBackground->Render( modelview, screen, widescreen );
		return;
	}
	if( ( openingFrame > 0 || closingFrame > 0 ) && entryBackground )
		entryBackground->Render( modelview, screen, widescreen );
	PrepareTextureGX();

	if( openingFrame > 0 )
	{
		float progress = (float)( OpenFrames - openingFrame ) / OpenFrames;
		progress = progress * progress * ( 3.0f - 2.0f * progress );
		DrawPage(false,0,screen.y * (1 - progress),screen,255);
		if( menuSnapshot )
		{
			PrepareTextureGX();
			DrawTexture( menuSnapshot, 0.0f, -screen.y * progress, screen.x, screen.y, 255 );
		}
		return;
	}
	if( closingFrame > 0 )
	{
		if( requestedSettings )
		{
			DrawPage(false,0,0,screen,255);
			DrawSquare( 0, 0, screen.x, screen.y,
				(GXColor){0, 0, 0, (u8)(255 * (CloseFrames - closingFrame) / CloseFrames)} );
			return;
		}
		float progress = (float)closingFrame / CloseFrames;
		progress = progress * progress * ( 3.0f - 2.0f * progress );
		DrawPage(false,0,screen.y * (1 - progress),screen,255);
		if( menuSnapshot )
		{
			PrepareTextureGX();
			DrawTexture( menuSnapshot, 0.0f, -screen.y * progress, screen.x, screen.y, 255 );
		}
		return;
	}
	if( transitionFrame > 0 && (live || previousSnapshot) )
	{
		const float progress = (float)( PageFrames - transitionFrame ) / PageFrames;
		if( transitionDirection != 0 )
		{
			const float direction = (float)transitionDirection;
			DrawPage(true,-direction * progress * screen.x,0,screen,255);
			DrawPage(false,direction * (1 - progress) * screen.x,0,screen,255);
		}
		else
		{
			DrawPage(true,0,0,screen,(u8)(255 * (1 - progress)));
			DrawPage(false,0,0,screen,(u8)(255 * progress));
		}
		return;
	}
	DrawPage(false,0,0,screen,255);
}

void MessageBoardScreen::DrawPage(bool previous, float x, float y,
	const Vec2f &screen, u8 alpha)
{
	if( live ) live->Draw(previous ? previousPage : page,
		previous ? previousDayOffset : dayOffset,
		previous ? previousMonthOffset : monthOffset,x,y,screen,alpha);
	else
	{
		PrepareTextureGX();
		DrawTexture(previous ? previousSnapshot : snapshot,x,y,screen.x,screen.y,alpha);
	}
}
