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

#include "buttoncoords.h"
#include "homemenu.h"
#include "localization.h"
#include "appsettingsscreen.h"
#include "sc.h"
#include "video.h"
#include "../ModernWSM/TinyFont.h"

namespace
{
	u32 WiiRemoteBatteryPercent( u8 rawLevel )
	{
		// libogc exposes the Wii Remote's native 0..0xc8 charge value.
		u32 level = rawLevel > 0xc8 ? 0xc8 : rawLevel;
		return ( level * 100u + 100u ) / 200u;
	}

	const char *ExpansionSuffix( u8 type )
	{
		switch( type )
		{
		case WPAD_EXP_NUNCHUK:     return " NC";
		case WPAD_EXP_CLASSIC:     return " CC";
		case WPAD_EXP_GUITARHERO3: return " GH";
		case WPAD_EXP_WIIBOARD:    return " WB";
		case WPAD_EXP_UNKNOWN:     return " EX";
		default:                   return "";
		}
	}

	bool PointInRect( float x, float y, float left, float top,
		float width, float height )
	{
		return x >= left && x < left + width && y >= top && y < top + height;
	}

	void ControllerOrderRect( const Vec2f &screen, int slot, float &x,
		float &y, float &w, float &h )
	{
		const float sx = screen.x / 640.0f;
		const float sy = screen.y / 480.0f;
		x = 145.0f * sx;
		y = ( 142.0f + slot * 48.0f ) * sy;
		w = 350.0f * sx;
		h = 38.0f * sy;
	}

	bool ControllerSettingsHit( const Controller &pad )
	{
		const WPADData &data = pad.GetData();
		if( !data.ir.valid )
			return false;
		// On the real HOME Menu the complete lower controller strip is one large
		// controller-settings target, not only the small icon at the left.
		return PointInRect( data.ir.x, data.ir.y, 0.0f, 330.0f,
			640.0f, 150.0f );
	}
}

HomeMenu::HomeMenu()
	: lyt( new Layout ),
	  overlayBackdropActive( false ),
	  btn1( NULL ),
	  btnTopBar( NULL ),
	  btnBottomBar( NULL ),
	  btnWiimote( NULL ),
	  btnDlg1( NULL ),
	  btnDlg2( NULL ),
	  testObj( new Object ),
	  dlgObj( new Object ),
	  dialogReady( false ),
	  waitForARelease( false ),
	  menuTransitionFrames( 0 ),
	  dialogFrames( 0 ),
	  topBarAuthoredFinalY( 0.0f ),
	  topBarFinalKnown( false ),
	  bottomBarAuthoredFinalY( 0.0f ),
	  bottomBarFinalKnown( false ),
	  controllerOrderCursor( 0 ),
	  controllerOrderPicked( -1 )
{
	dlgTxt[ 0 ] = 0;
	for( int i = 0; i < 4; i++ )
	{
		btryObj[ i ] = new Object;
		btryLevel[ i ] = -1;
		btryCasePane[ i ] = NULL;
		for( int segment = 0; segment < 4; ++segment )
			btryPowerPane[ i ][ segment ] = NULL;
	}
}

HomeMenu::~HomeMenu()
{
	delete btn1;
	delete btnTopBar;
	delete btnBottomBar;
	delete btnWiimote;
	delete btnDlg1;
	delete btnDlg2;
	delete btryObj[0];
	delete btryObj[1];
	delete btryObj[2];
	delete btryObj[3];

	delete dlgObj;
	delete testObj;

	delete lyt;

	std::map< std::string, Animation *>:: iterator it = brlans.begin(), itE = brlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
}

void MakeAllVisible( Pane *pane )
{
	//bool changed = false;
	if( pane->GetAlpha() != 0xff || pane->GetHide() || !pane->GetVisible() )
	{
		gprintf( "changing pane: \"%s\" %02x, %u, %u \n", pane->getName(), pane->GetAlpha(), pane->GetHide(), pane->GetVisible() );
	}
	pane->SetAlpha( 0xff );
	pane->SetVisible( true );
	pane->SetHide( false );

	foreach( Pane *p, pane->panes )
	{
		MakeAllVisible( p );
	}
}

void HomeMenu::SetBGTexture( Texture *tex )
{
	overlayBackdropActive = tex != NULL;
	const char* bgpane = "back_00";
	Pane *snapshotPane = lyt->FindPane( bgpane );
	Material *mat = NULL;
	if( snapshotPane )
	{
		const int materialIndex = snapshotPane->GetMaterialIndex();
		const MaterialList &materials = lyt->Materials();
		if( materialIndex >= 0 && (u32)materialIndex < materials.size() )
			mat = materials[ materialIndex ];
	}
	// Retain the name lookup for alternate HOME resources whose material was
	// deliberately named after the picture pane.
	if( !mat )
		mat = lyt->FindMaterial( bgpane );
	if( mat )
		mat->SetForcedTexture( tex );
	ApplyOverlayBackground();
}

void HomeMenu::ApplyOverlayBackground()

{
	if( !overlayBackdropActive )
		return;

	// MenuHandler draws and darkens the frozen framebuffer itself before this
	// layout. Hide both stock background panes completely: back_02 is the Wii
	// HOME striped texture, which must not appear in the pause-overlay style.
	// Applying this every frame prevents the entrance BRLAN restoring either
	// pane's authored alpha.
	Pane *snapshotPane = lyt->FindPane( "back_00" );
	if( snapshotPane )
	{
		snapshotPane->SetAlpha( 0 );
	}

	Pane *linesPane = lyt->FindPane( "back_02" );
	if( linesPane )
	{
		linesPane->SetAlpha( 0 );
	}
}

void HomeMenu::ApplyDialogShadeBounds( const Vec2f &ScreenProps )
{
	if( state != St_Dialog )
		return;

	Pane *shade = lyt->FindPane( "back_01" );
	if( !shade )
		return;

	// back_01 is authored as a smaller content shade in several HOME archives.
	// Expand it around its own origin so it covers the complete framebuffer but
	// remains in the native layout order between HOME and N_Dialog.
	const float width = ScreenProps.x + 8.0f;
	const float height = ScreenProps.y + 8.0f;
	const float x = ( (float)shade->GetOriginX() - 1.0f ) * width * 0.5f;
	const float y = ( (float)shade->GetOriginY() - 1.0f ) * height * 0.5f;
	const Vec2f unitScale = { 1.0f, 1.0f };
	shade->SetSize( width, height );
	shade->SetScale( unitScale );
	shade->SetPosition( x, y );
	shade->SetVisible( true );
	shade->SetHide( false );
}

void HomeMenu::ApplyTopBarBounds( const Vec2f &ScreenProps )
{
	// Shift the authored entrance/exit path by the difference between its own
	// final frame and the real framebuffer top. This preserves the animation's
	// movement and size while making it end at y=0 instead of snapping there.
	Pane *topBar = lyt->FindPane( "bar_00" );
	if( !topBar )
		return;
	const float localTop = topBar->GetHeight()
		* ( 1.0f - 0.5f * (float)topBar->GetOriginY() );
	const float alignedY = ScreenProps.y * 0.5f - localTop;
	if( ( state == St_FadeIn || state == St_FadeOut ) && topBarFinalKnown )
	{
		const float correction = alignedY - topBarAuthoredFinalY;
		topBar->SetPosition( topBar->GetPosX(), topBar->GetPosY() + correction );
	}
	else
	{
		topBar->SetPosition( topBar->GetPosX(), alignedY );
	}
}

void HomeMenu::ApplyBottomBarBounds( const Vec2f &ScreenProps )
{
	Pane *bottomBar = lyt->FindPane( "bar_10" );
	if( !bottomBar || !bottomBarFinalKnown )
		return;

	// The stock entrance finishes at -236, leaving four pixels below
	// the controller strip on a 480-line framebuffer. Translate the whole
	// authored motion so its final bottom edge meets the framebuffer edge.
	// The caller restores the authored position after rendering, so paused
	// dialogs and hover animations never accumulate the correction.
	const float localBottom = -0.5f * (float)bottomBar->GetOriginY()
		* bottomBar->GetHeight();
	const float alignedY = -ScreenProps.y * 0.5f - localBottom;
	const float correction = alignedY - bottomBarAuthoredFinalY;
	bottomBar->SetPosition( bottomBar->GetPosX(),
		bottomBar->GetPosY() + correction );
}

bool HomeMenu::Load( const u8* homeBtn1AshData, u32 homeBtn1AshSize, const u8* lang_homeBtn1AshData, u32 lang_homeBtn1AshSize )
{
	if( loaded )
	{
		return true;
	}

	if( !homeBtn1AshData || !homeBtn1AshSize || !lang_homeBtn1AshData || !lang_homeBtn1AshSize )
	{
		return false;
	}

	Pane *pane;
	U8Archive arc1( homeBtn1AshData, homeBtn1AshSize );
	U8Archive arc2( lang_homeBtn1AshData, lang_homeBtn1AshSize );

	// since the textures are stread out over 2 archives, load them manually
	// read layout data
	u8 *stuff = arc2.GetFile( "/arc/blyt/th_HomeBtn_d.brlyt" );
	if( !stuff )
	{
		return false;
	}

	// load layout
	if( !lyt->Load( stuff ) )
	{
		return false;
	}

	// load textures from an archive based on their name
	foreach( Texture *t, lyt->Textures() )
	{
		// arc2 may be the English fallback when the NAND lacks this locale.
		// Resolve against the actual archives, not the requested language suffix.
		if( !LoadTpl( t, arc2 ) && !LoadTpl( t, arc1 ) )
		{
			return false;
		}
	}

	SetPaneVisible( lyt, "let_icn_00", false );

	// just show the striped background for now
	if( ( pane = lyt->FindPane( "back_02" ) ) )
	{
		pane->SetAlpha( 0xff );
	}

	u32 x, y, w, h;
	ButtonCoords( HBMTopBar, x, y, w, h );
	btnTopBar = new QuadButton( x, y, w, h );

	ButtonCoords( HBMBottomBar, x, y, w, h );
	btnBottomBar = new QuadButton( x, y, w, h );

	ButtonCoords( HBMWiimoteDown, x, y, w, h );
	btnWiimote = new QuadButton( x, y, w, h );

	ButtonCoords( HBMCenterBtn, x, y, w, h );
	btn1 = new QuadButton( x, y, w, h );

	ButtonCoords( Dlg_A2_Btn1, x, y, w, h );
	btnDlg1 = new QuadButton( x, y, w, h );

	ButtonCoords( Dlg_A2_Btn2, x, y, w, h );
	btnDlg2 = new QuadButton( x, y, w, h );


	Animation* anim;

#define LOADANIM( x, y )									\
	do														\
	{														\
		if( !(anim = LoadAnimation( arc1, y ) ) )			\
		{													\
			return false;									\
		}													\
		brlans[ y ] = anim;									\
		x->AddAnimation( anim );							\
	}														\
	while( 0 )

#define LOADANIM2( x, y, z )								\
	do														\
	{														\
		if( !(anim = LoadAnimation( arc1, z ) ) )			\
		{													\
			return false;									\
		}													\
		brlans[ z ] = anim;									\
		x->AddAnimation( anim );							\
		y->AddAnimation( anim );							\
	}														\
	while( 0 )

#define LOADANIMBTRY( x )									\
	do														\
	{														\
		if( !(anim = LoadAnimation( arc1, x ) ) )			\
		{													\
			return false;									\
		}													\
		brlans[ x ] = anim;									\
		btryObj[0]->AddAnimation( anim );					\
		btryObj[1]->AddAnimation( anim );					\
		btryObj[2]->AddAnimation( anim );					\
		btryObj[3]->AddAnimation( anim );					\
	}														\
	while( 0 )

	btnTopBar->BindPane( lyt->FindPane( "bar_00" ) );// top bar
	btnBottomBar->BindPane( lyt->FindPane( "bar_10" ) );// bottom bar
	//testObj->BindPane( lyt->FindPane( "back_02" ) );//	background with the horizontal lines
	btn1->BindPane( lyt->FindPane( "N_cntBtn_all" ) );// center "Wii Menu" button, it has the stupid letter image on it
	btnWiimote->BindPane( lyt->FindPane( "N_cntrl_00" ) );// the huge wiimote that slides in from the bottom of the page


#define BTRY_MAT( x )											\
	do															\
	{															\
		snprintf( name, sizeof( name ), x, i );					\
		btryObj[i]->BindMaterial( lyt->FindMaterial( name ) );	\
	}															\
	while( 0 )

	// setup batteries
	for( int i = 0; i < 4; i++ )
	{
		char name[ 20 ];
		BTRY_MAT( "btryCase_%.2u" );
		BTRY_MAT( "btryPwr_%.2u_0" );
		BTRY_MAT( "btryPwr_%.2u_1" );
		BTRY_MAT( "btryPwr_%.2u_2" );
		BTRY_MAT( "btryPwr_%.2u_3" );
		BTRY_MAT( "tx_plyr_%.2u" );

		snprintf( name, sizeof( name ), "btryCase_%.2u", (unsigned)i );
		btryCasePane[ i ] = lyt->FindPane( name );
		for( int segment = 0; segment < 4; ++segment )
		{
			snprintf( name, sizeof( name ), "btryPwr_%.2u_%u",
				(unsigned)i, (unsigned)segment );
			btryPowerPane[ i ][ segment ] = lyt->FindPane( name );
		}
	}

	LOADANIMBTRY( "th_HomeBtn_d_btry_gry" );
	LOADANIMBTRY( "th_HomeBtn_d_btry_red" );// make sure to set the battery to white before red.  if you do grey -> red it looks bad
	LOADANIMBTRY( "th_HomeBtn_d_btry_wht" );

	// Start disconnected. Runtime input snapshots select grey/red/white and
	// reveal exactly the number of authored battery segments that are charged.
	for( int i = 0; i < 4; ++i )
	{
		btryObj[ i ]->SetAnimation( "th_HomeBtn_d_btry_gry", 0, 0, 0, false );
		btryObj[ i ]->SetFrame( 0 );
		btryObj[ i ]->Pause( true );
		for( int segment = 0; segment < 4; ++segment )
		{
			if( btryPowerPane[ i ][ segment ] )
			{
				btryPowerPane[ i ][ segment ]->SetVisible( false );
				btryPowerPane[ i ][ segment ]->SetHide( true );
			}
		}
	}



	//btnBottomBar->BindMaterials( lyt->Materials() );
	//LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_strt" );

	//dlgObj->BindPane( lyt->FindPane( "N_Dialog" ) );

	//LOADANIM( testObj, "th_HomeBtn_d_hmMenu_bar_in" );
	//LOADANIM( testObj, "th_HomeBtn_d_optn_bar_in" );		// looks like the big wiimote and the settings it has are starting to come up from the bottom of the screen
	//LOADANIM( testObj, "th_HomeBtn_d_hmMenu_strt" );		// everything fades in and slides onto the screen asd stuff like that

	LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_strt" );		// everything fades in and slides onto the screen asd stuff like that
	btnBottomBar->AddAnimation( anim );
	btnWiimote->AddAnimation( anim );
	btn1->AddAnimation( anim );

	LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_fnsh" );		// fade out and go back to what we were doing
	btnBottomBar->AddAnimation( anim );
	btnWiimote->AddAnimation( anim );
	btn1->AddAnimation( anim );


	// setup "wii menu" button
	LOADANIM( btn1, "th_HomeBtn_d_cntBtn_in" );
	btn1->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM( btn1, "th_HomeBtn_d_cntBtn_out" );
	btn1->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM( btn1, "th_HomeBtn_d_cntBtn_psh" );
	btn1->SetClickAnimation( anim, 0, -1 );
	btn1->SetTrigger( Button::Btn_A );

	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_L" ) );
	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_R" ) );
	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_M" ) );
	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_L_shdw" ) );
	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_R_shdw" ) );
	btn1->BindMaterial( lyt->FindMaterial( "btnL_00_M_shdw" ) );

	// setup top bar button
	LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_bar_in" );
	btnTopBar->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_bar_out" );
	btnTopBar->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM( btnTopBar, "th_HomeBtn_d_hmMenu_bar_psh" );
	btnTopBar->SetClickAnimation( anim, 0, -1 );
	btnTopBar->SetTrigger( Button::Btn_A );

	// setup bottom bar button
	LOADANIM2( btnBottomBar, btnWiimote, "th_HomeBtn_d_close_bar_in" );
	btnBottomBar->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM( btnBottomBar, "th_HomeBtn_d_close_bar_out" );
	btnBottomBar->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM2( btnBottomBar, btnWiimote, "th_HomeBtn_d_close_bar_psh" );
	btnBottomBar->SetClickAnimation( anim, 0, -1 );
	//btnBottomBar->SetTrigger( Button::Btn_A );

	// setup huge wiimote button
	LOADANIM( btnWiimote, "th_HomeBtn_d_cntrl_dwn" );	// the settings menu slides into the wiimote and then it slides down to the bottom of the screen
	LOADANIM( btnWiimote, "th_HomeBtn_d_cntrl_up" );	// the controller slides up
	LOADANIM( btnWiimote, "th_HomeBtn_d_cntrl_wndw_opn" );	// wiimote settinsg menu slides out

	// setup dialog
	dlgObj->BindPane( lyt->FindPane( "back_01" ) );		// shade that makes everything behind the dialog darker
	dlgObj->BindPane( lyt->FindPane( "N_Dialog" ) );
	//dlgObj->BindMaterials( lyt->Materials() );
	LOADANIM( dlgObj, "th_HomeBtn_d_cmn_msg_in" );		// slides in from the top
	LOADANIM( dlgObj, "th_HomeBtn_d_cmn_msg_out" );		// slides out off the bottom
	LOADANIM( dlgObj, "th_HomeBtn_d_cmn_msg_rtrn" );	// slides out off the top

	// we arent drawing the shadow correctly anyways, so hide it
	SetPaneVisible( lyt, "W_DlgShade", false );
	ShowDialogPanes( false );

	// main text for the dialog
	strlcpy16( dlgTxt, Localization::Get( Localization::ReturnToLoader ), 128 );
	SetText( lyt, "T_Dialog", dlgTxt );

	// setup dialog buttons
	btnDlg1->BindPane( lyt->FindPane( "N_BtnA" ) );
	btnDlg2->BindPane( lyt->FindPane( "N__BtnB" ) );
	LOADANIM2( btnDlg1, btnDlg2, "th_HomeBtn_d_cmn_msg_btn_in" );
	btnDlg1->SetMouseOverAnimation( anim, 0, -1 );
	btnDlg2->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM2( btnDlg1, btnDlg2, "th_HomeBtn_d_cmn_msg_btn_out" );
	btnDlg1->SetMouseOutAnimation( anim, 0, -1 );
	btnDlg2->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM2( btnDlg1, btnDlg2, "th_HomeBtn_d_cmn_msg_btn_psh" );
	btnDlg1->SetClickAnimation( anim, 0, -1 );
	btnDlg2->SetClickAnimation( anim, 0, -1 );
	btnDlg1->SetTrigger( Button::Btn_A );
	btnDlg2->SetTrigger( Button::Btn_A );

	btnDlg1->SetEnabled( false );
	btnDlg2->SetEnabled( false );


	// Sample the authored opening animation's final top-bar position before
	// restarting it at frame zero. ApplyTopBarBounds uses this to translate the
	// complete path, avoiding a correction only after the animation finishes.
	std::map< std::string, Animation *>::iterator homeStart =
		brlans.find( "th_HomeBtn_d_hmMenu_strt" );
	Pane *authoredTopBar = lyt->FindPane( "bar_00" );
	if( homeStart != brlans.end() && homeStart->second && authoredTopBar )
	{
		btnTopBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt",
			0, -1, -1, false );
		btnTopBar->SetFrame( std::max( 0.0f,
			homeStart->second->FrameCount() - 1.0f ) );
		topBarAuthoredFinalY = authoredTopBar->GetPosY();
		topBarFinalKnown = true;
	}
	Pane *authoredBottomBar = lyt->FindPane( "bar_10" );
	if( homeStart != brlans.end() && homeStart->second && authoredBottomBar )
	{
		btnBottomBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt",
			0, -1, -1, false );
		btnBottomBar->SetFrame( std::max( 0.0f,
			homeStart->second->FrameCount() - 1.0f ) );
		bottomBarAuthoredFinalY = authoredBottomBar->GetPosY();
		bottomBarFinalKnown = true;
	}

	// set initial fadein
	btnTopBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btnBottomBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btnWiimote->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btn1->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );

	// connect to button slots
	btnTopBar->Finished.connect( this, &HomeMenu::TopBarAnimDone );
	btnTopBar->Clicked.connect( this, &HomeMenu::TopBarClicked );
	btn1->Clicked.connect( this, &HomeMenu::CenterBtnClicked );
	btn1->Finished.connect( this, &HomeMenu::CenterBtnAnimDone );
	dlgObj->Finished.connect( this, &HomeMenu::DialogAnimDone );

	btnDlg1->Finished.connect( this, &HomeMenu::DlgBtn1AnimDone );
	btnDlg1->Clicked.connect( this, &HomeMenu::DlgBtn1Clicked );
	btnDlg2->Finished.connect( this, &HomeMenu::DlgBtn2AnimDone );
	btnDlg2->Clicked.connect( this, &HomeMenu::DlgBtn2Clicked );

	btnTopBar->SetEnabled( false );
	btnBottomBar->SetEnabled( false );
	btnWiimote->SetEnabled( false );
	btn1->SetEnabled( false );

	btnTopBar->Start();
	btnBottomBar->Start();
	btnWiimote->Start();
	btn1->Start();

	loaded = true;
	state = St_FadeIn;
	choice = Ch_None;
	dialogReady = false;
	dlgState = DSt_Hidden;
	menuTransitionFrames = 0;
	return true;
}

bool HomeMenu::LoadTpl( Texture *tex, const U8Archive &arc )
{
	const u8 *file = arc.GetFile( "/arc/timg/" + tex->getName() );
	if( file )
	{
		tex->Load( file );
		return true;
	}
	return false;
}

void HomeMenu::Render( Mtx &modelview, const Vec2f &ScreenProps, bool widescreen )
{
	if( !loaded )
	{
		return;
	}

	switch( state )
	{
	case St_BtnIdle:
		// Opening BRLANs have finished before this state becomes active.
		// An already stationary pointer may now enter the normal hover state.
		{
			bool openedControllerOrder = false;
			for( int i = 0; i < 4; ++i )
			{
				Controller &pad = Pad( i );
				if( !pad.IsTaken() && pad.pA() && ControllerSettingsHit( pad ) )
				{
					pad.Take();
					OpenControllerOrder();
					openedControllerOrder = true;
					break;
				}
			}
			if( openedControllerOrder )
				break;
			btnTopBar->Update();
			btnBottomBar->Update();
			btnWiimote->Update();
			btn1->Update();
		}

		btnTopBar->Advance();
		btnBottomBar->Advance();
		btnWiimote->Advance();
		btn1->Advance();

		btryObj[0]->Advance();
		btryObj[1]->Advance();
		btryObj[2]->Advance();
		btryObj[3]->Advance();
		if( waitForARelease )
		{
			bool held = false;
			for( int i = 0; i < 4; i++ )
				held |= Pad( i ).hA();
			if( !held )
				waitForARelease = false;
		}
		for( int i = 0; i < 4; i++ )
		{
			if( Pad( i ).pHome() )
			{
				BeginClose();
				Pad( i ).Take();
			}
			else if( Pad( i ).pB() )
			{
				BeginClose();
				Pad( i ).Take();
			}
			// Do not treat every A press as the center Wii Menu button.  The actual
			// QuadButton handles pointer hits, so empty areas stay non-clickable.
		}
		break;
	case St_ControllerOrder:
	{
		if( waitForARelease )
		{
			bool held = false;
			for( int i = 0; i < 4; ++i )
				held |= Pad( i ).hA();
			if( !held )
				waitForARelease = false;
		}
		// Follow the mouse as it moves across the rows. A stationary Wii Remote
		// IR coordinate must not steal this desktop-style hover selection.
		int activateSlot = -1;
		for( int i = 0; i < 4; ++i )
		{
			Controller &pointer = Pad( i );
			if( !pointer.IsUsbMouseConnected() )
				continue;
			const WPADData &data = pointer.GetData();
			if( !data.ir.valid )
				continue;
			for( int slot = 0; slot < 4; ++slot )
			{
				float x, y, w, h;
				ControllerOrderRect( ScreenProps, slot, x, y, w, h );
				if( PointInRect( data.ir.x, data.ir.y, x, y, w, h ) )
					controllerOrderCursor = slot;
			}
			break;
		}

		for( int i = 0; i < 4; ++i )
		{
			Controller &pad = Pad( i );
			if( pad.pHome() )
			{
				pad.Take();
				CloseControllerOrder();
				BeginClose();
				break;
			}
			if( pad.pB() )
			{
				pad.Take();
				CloseControllerOrder();
				break;
			}
			if( pad.pUp() )
			{
				controllerOrderCursor = ( controllerOrderCursor + 3 ) % 4;
				pad.Take();
				break;
			}
			else if( pad.pDown() )
			{
				controllerOrderCursor = ( controllerOrderCursor + 1 ) % 4;
				pad.Take();
				break;
			}

			if( waitForARelease || !pad.pA() || pad.IsTaken() )
				continue;
			const WPADData &data = pad.GetData();
			if( data.ir.valid )
			{
				bool found = false;
				for( int slot = 0; slot < 4; ++slot )
				{
					float x, y, w, h;
					ControllerOrderRect( ScreenProps, slot, x, y, w, h );
					if( PointInRect( data.ir.x, data.ir.y, x, y, w, h ) )
					{
						controllerOrderCursor = slot;
						activateSlot = slot;
						found = true;
						break;
					}
				}
				const float sx = ScreenProps.x / 640.0f;
				const float sy = ScreenProps.y / 480.0f;
				if( !found && PointInRect( data.ir.x, data.ir.y,
					250.0f * sx, 352.0f * sy, 140.0f * sx, 38.0f * sy ) )
				{
					CloseControllerOrder();
				}
			}
			else
			{
				activateSlot = controllerOrderCursor;
			}
			pad.Take();
			break;
		}
		if( activateSlot >= 0 && state == St_ControllerOrder )
			ActivateControllerOrderSlot( activateSlot );
		break;
	}
	case St_FadeIn:
	case St_FadeOut:
		// The HOME menu owns input while either authored transition is active.
		// Advance every pane controller before testing completion so no single
		// button can end the transition while a sibling is still on screen.
		btnTopBar->Advance();
		btnBottomBar->Advance();
		btnWiimote->Advance();
		btn1->Advance();
		btryObj[0]->Advance();
		btryObj[1]->Advance();
		btryObj[2]->Advance();
		btryObj[3]->Advance();
		menuTransitionFrames++;
		if( MainAnimationsFinished() || menuTransitionFrames > 90 )
		{
			if( state == St_FadeIn )
				MakeButtonsReady();
			else
				Done();
		}
		break;
	case St_Dialog:
		dlgObj->Advance();
		dialogFrames++;

		if( waitForARelease )
		{
			bool held = false;
			for( int i = 0; i < 4; i++ )
				held |= Pad( i ).hA();
			if( !held )
				waitForARelease = false;
		}

		if( dlgState == DSt_Active )
		{
			btnDlg1->Update();
			btnDlg2->Update();
		}
		btnDlg1->Advance();
		btnDlg2->Advance();
		for( int i = 0; i < 4; i++ )
		{
			if( dlgState == DSt_Active && ( Pad( i ).pB() || Pad( i ).pHome() ) )
			{
				CancelDialog();
				Pad( i ).Take();
			}
			else if( dlgState == DSt_Active && !waitForARelease
				&& Pad( i ).pA() && !Pad( i ).IsTaken()
				&& !Pad( i ).IsUsbMouseConnected() )
			{
				DlgBtn2Clicked();
				Pad( i ).Take();
			}
		}
		if( dlgState == DSt_Opening && dialogFrames > 8 )
		{
			MakeDialogReady();
		}
		else if( dlgState == DSt_ClosingNo && dialogFrames > 18 )
		{
			FinishCancelDialog();
		}
		else if( dlgState == DSt_ClosingYes && dialogFrames > 18 )
		{
			FinishConfirmDialog();
		}
		break;
	}

	UpdateControllerIndicators();
	ApplyOverlayBackground();
	ApplyDialogShadeBounds( ScreenProps );
	ApplyTopBarBounds( ScreenProps );
	Pane *bottomBar = lyt->FindPane( "bar_10" );
	const float bottomBarY = bottomBar ? bottomBar->GetPosY() : 0.0f;
	ApplyBottomBarBounds( ScreenProps );
	lyt->Render( modelview, ScreenProps, widescreen );
	if( bottomBar )
		bottomBar->SetPosition( bottomBar->GetPosX(), bottomBarY );
	DrawControllerIndicators( ScreenProps );
	DrawControllerOrder( ScreenProps );
}

void HomeMenu::UpdateControllerIndicators()
{
	for( int i = 0; i < 4; ++i )
	{
		Controller &controller = Pad( i );
		const bool ready = controller.IsWiiRemoteReady();
		const u32 percent = ready
			? WiiRemoteBatteryPercent( controller.WiiRemoteBatteryRaw() ) : 0;
		const bool low = ready && percent <= 20;
		int segments = ready ? ( percent ? ( percent + 24 ) / 25 : 1 ) : 0;
		if( segments > 4 )
			segments = 4;

		const int visualState = ready ? ( low ? 0x10 : 0x20 ) | segments : 0;
		if( btryLevel[ i ] == visualState )
			continue;

		for( int segment = 0; segment < 4; ++segment )
		{
			Pane *pane = btryPowerPane[ i ][ segment ];
			if( !pane )
				continue;
			const bool show = ready && segment < segments;
			pane->SetVisible( show );
			pane->SetHide( !show );
		}

		if( !ready )
		{
			btryObj[ i ]->SetAnimation( "th_HomeBtn_d_btry_gry", 0, 0, 0, false );
		}
		else if( low )
		{
			// Nintendo's material tracks do not transition cleanly grey -> red.
			btryObj[ i ]->SetAnimation( "th_HomeBtn_d_btry_wht", 0, 0, 0, false );
			btryObj[ i ]->SetFrame( 0 );
			btryObj[ i ]->SetAnimation( "th_HomeBtn_d_btry_red", 0, 0, 0, false );
		}
		else
		{
			btryObj[ i ]->SetAnimation( "th_HomeBtn_d_btry_wht", 0, 0, 0, false );
		}
		btryObj[ i ]->SetFrame( 0 );
		btryObj[ i ]->Pause( true );
		btryLevel[ i ] = visualState;
	}
}

void HomeMenu::DrawControllerIndicators( const Vec2f &ScreenProps )
{
	// Keep the diagnostic labels off authored entrance/exit frames and dialogs;
	// the native battery art still follows those animations normally.
	if( state != St_BtnIdle )
		return;

	for( int i = 0; i < 4; ++i )
	{
		Pane *pane = btryCasePane[ i ];
		if( !pane )
			continue;

		Controller &controller = Pad( i );
		const bool ready = controller.IsWiiRemoteReady();
		const bool wiiConnected = controller.IsWiiRemoteConnected();
		const bool gcConnected = controller.IsGameCubeConnected();
		char label[ 128 ];
		if( ready )
		{
			snprintf( label, sizeof( label ), "P%d %u%%%s%s", i + 1,
				(unsigned)WiiRemoteBatteryPercent( controller.WiiRemoteBatteryRaw() ),
				ExpansionSuffix( controller.WiiRemoteExpansion() ),
				gcConnected ? " +GC" : "" );
		}
		else if( wiiConnected )
		{
			snprintf( label, sizeof( label ), "P%d WII%s", i + 1,
				gcConnected ? " +GC" : "" );
		}
		else if( gcConnected )
		{
			snprintf( label, sizeof( label ), "P%d GC", i + 1 );
		}
		else if( controller.IsUsbMouseConnected() )
		{
			snprintf( label, sizeof( label ), "P%d %s", i + 1, Localization::GetUtf8("USB mouse") );
		}
		else
		{
			snprintf( label, sizeof( label ), "P%d --", i + 1 );
		}

		const Mtx &view = pane->GetView();
		const float localX = 0.5f * ( 1.0f - pane->GetOriginX() ) * pane->GetWidth();
		const float localY = 0.5f * ( 1.0f - pane->GetOriginY() ) * pane->GetHeight();
		const float centerX = view[ 0 ][ 0 ] * localX + view[ 0 ][ 1 ] * localY + view[ 0 ][ 3 ];
		const float centerY = view[ 1 ][ 0 ] * localX + view[ 1 ][ 1 ] * localY + view[ 1 ][ 3 ];
		const float scale = 1.25f;
		const float width = ModernWSM::TinyFont::Width( label, scale );
		float x = centerX - width * 0.5f;
		float y = centerY + 12.0f;
		if( x < 2.0f )
			x = 2.0f;
		if( x + width > ScreenProps.x - 2.0f )
			x = ScreenProps.x - width - 2.0f;
		if( y + 5.0f * scale > ScreenProps.y - 2.0f )
			y = centerY - 12.0f;

		GXColor color = { 125, 125, 125, 220 };
		if( ready && WiiRemoteBatteryPercent( controller.WiiRemoteBatteryRaw() ) <= 20 )
			color = (GXColor){ 220, 50, 50, 255 };
		else if( wiiConnected || gcConnected )
			color = (GXColor){ 20, 105, 155, 255 };
		if( controller.IsUsbMouseConnected() )
			AppSettingsScreen::DrawInterfaceText( centerX - 48, y, 96,
				label, 11.0f, 8.0f, color );
		else ModernWSM::TinyFont::Draw( x, y, label, scale, color );
	}
}

void HomeMenu::DrawControllerOrder( const Vec2f &ScreenProps )
{
	if( state != St_ControllerOrder )
		return;

	const float sx = ScreenProps.x / 640.0f;
	const float sy = ScreenProps.y / 480.0f;
	const GXColor shade = { 0, 0, 0, 150 };
	const GXColor panel = { 242, 248, 250, 250 };
	const GXColor border = { 0, 174, 232, 255 };
	const GXColor idle = { 222, 231, 236, 255 };
	const GXColor selected = { 166, 225, 247, 255 };
	const GXColor picked = { 110, 205, 160, 255 };
	const GXColor ink = { 40, 53, 60, 255 };
	const GXColor white = { 255, 255, 255, 255 };

	DrawSquare( 0.0f, 0.0f, ScreenProps.x, ScreenProps.y, shade );
	DrawSquare( 112.0f * sx, 72.0f * sy, 416.0f * sx, 340.0f * sy, panel );
	DrawSquare( 112.0f * sx, 72.0f * sy, 416.0f * sx, 4.0f * sy, border );
	AppSettingsScreen::DrawInterfaceText( 140.0f * sx, 86.0f * sy, 370.0f * sx,
		"Controller order", 24.0f * sy, 15.0f * sy, ink );
	AppSettingsScreen::DrawInterfaceText( 140.0f * sx, 118.0f * sy, 370.0f * sx,
		"Select a device, then its new player slot", 15.0f * sy, 10.0f * sy, ink );

	for( int slot = 0; slot < 4; ++slot )
	{
		float x, y, w, h;
		ControllerOrderRect( ScreenProps, slot, x, y, w, h );
		GXColor row = slot == controllerOrderPicked ? picked
			: ( slot == controllerOrderCursor ? selected : idle );
		DrawSquare( x, y, w, h, row );
		char player[ 128 ];
		snprintf( player, sizeof( player ), Localization::GetUtf8("Player %d"), slot + 1 );
		AppSettingsScreen::DrawInterfaceText( x + 14.0f * sx, y + 10.0f * sy, 132.0f * sx,
			player, 17.0f * sy, 11.0f * sy, ink );

		Controller &controller = Pad( slot );
		char device[ 256 ];
		if( controller.IsUsbMouseConnected() )
			snprintf( device, sizeof( device ), "%s", Localization::GetUtf8("USB mouse") );
		else if( controller.IsWiiRemoteReady() )
			snprintf( device, sizeof( device ), Localization::GetUtf8("Wii Remote %u%%"),
				(unsigned)WiiRemoteBatteryPercent( controller.WiiRemoteBatteryRaw() ) );
		else if( controller.IsWiiRemoteConnected() )
			snprintf( device, sizeof( device ), "%s", Localization::GetUtf8("Wii Remote") );
		else if( controller.IsGameCubeConnected() )
			snprintf( device, sizeof( device ), "GameCube" );
		else
			snprintf( device, sizeof( device ), "%s", Localization::GetUtf8("Empty") );
		AppSettingsScreen::DrawInterfaceText( x + 158.0f * sx, y + 10.0f * sy, 177.0f * sx,
			device, 17.0f * sy, 11.0f * sy, ink );
	}

	DrawSquare( 250.0f * sx, 352.0f * sy, 140.0f * sx, 38.0f * sy, border );
	AppSettingsScreen::DrawInterfaceText( 266.0f * sx, 361.0f * sy, 108.0f * sx,
		"Done", 18.0f * sy, 12.0f * sy, white );
	AppSettingsScreen::DrawInterfaceText( 140.0f * sx, 393.0f * sy, 370.0f * sx,
		"A: Select    B: Back    HOME: Close", 14.0f * sy, 10.0f * sy, ink );
}

void HomeMenu::OpenControllerOrder()
{
	if( state != St_BtnIdle )
		return;
	state = St_ControllerOrder;
	controllerOrderPicked = -1;
	const int mouse = CInputs::Instance()->GetMousePlayer();
	controllerOrderCursor = mouse >= 0 && mouse < 4 ? mouse : 0;
	waitForARelease = true;
	btnTopBar->SetEnabled( false );
	btnBottomBar->SetEnabled( false );
	btnWiimote->SetEnabled( false );
	btn1->SetEnabled( false );
	CInputs::Instance()->ClearButtonsDown();
}

void HomeMenu::CloseControllerOrder()
{
	if( state != St_ControllerOrder )
		return;
	controllerOrderPicked = -1;
	waitForARelease = true;
	CInputs::Instance()->ClearButtonsDown();
	MakeButtonsReady();
}

void HomeMenu::ActivateControllerOrderSlot( int slot )
{
	if( slot < 0 || slot >= 4 )
		return;
	if( controllerOrderPicked < 0 )
	{
		Controller &candidate = Pad( slot );
		if( !candidate.IsUsbMouseConnected()
			&& !candidate.IsWiiRemoteConnected()
			&& !candidate.IsGameCubeConnected() )
		{
			return;
		}
		controllerOrderPicked = slot;
		return;
	}
	if( controllerOrderPicked == slot )
	{
		controllerOrderPicked = -1;
		return;
	}
	if( CInputs::Instance()->SwapLogicalPlayers( controllerOrderPicked, slot ) )
	{
		controllerOrderCursor = slot;
		controllerOrderPicked = -1;
		waitForARelease = true;
	}
}

void HomeMenu::Reset()
{
	// set initial fadein
	btnTopBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btnBottomBar->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btnWiimote->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );
	btn1->SetAnimation( "th_HomeBtn_d_hmMenu_strt", 0, -1, -1, false );

	btnTopBar->Start();
	btnBottomBar->Start();
	btnWiimote->Start();
	btn1->Start();

	btnTopBar->SetEnabled( false );
	btnBottomBar->SetEnabled( false );
	btnWiimote->SetEnabled( false );
	btn1->SetEnabled( false );

	btnDlg1->SetEnabled( false );
	btnDlg2->SetEnabled( false );
	ShowDialogPanes( false );

	state = St_FadeIn;
	choice = Ch_None;
	dialogReady = false;
	dlgState = DSt_Hidden;
	waitForARelease = false;
	menuTransitionFrames = 0;
	dialogFrames = 0;
	controllerOrderCursor = 0;
	controllerOrderPicked = -1;
	for( int i = 0; i < 4; ++i )
		btryLevel[ i ] = -1;
}

void HomeMenu::TopBarClicked()
{
	if( state == St_BtnIdle )
	{
		BeginClose();
	}
}

void HomeMenu::TopBarAnimDone()
{
	if( state == St_FadeIn && MainAnimationsFinished() )
	{
		MakeButtonsReady();
	}
}

bool HomeMenu::MainAnimationsFinished() const
{
	return btnTopBar->IsFinished()
		&& btnBottomBar->IsFinished()
		&& btnWiimote->IsFinished()
		&& btn1->IsFinished();
}

void HomeMenu::BeginClose()
{
	if( state != St_BtnIdle )
		return;

	btnTopBar->SetEnabled( false );
	btnBottomBar->SetEnabled( false );
	btnWiimote->SetEnabled( false );
	btn1->SetEnabled( false );
	btnTopBar->SetState( Button::St_Idle );
	btnBottomBar->SetState( Button::St_Idle );
	btnWiimote->SetState( Button::St_Idle );
	btn1->SetState( Button::St_Idle );

	btnTopBar->SetAnimation( "th_HomeBtn_d_hmMenu_fnsh", 0, -1, -1, false );
	btnBottomBar->SetAnimation( "th_HomeBtn_d_hmMenu_fnsh", 0, -1, -1, false );
	btnWiimote->SetAnimation( "th_HomeBtn_d_hmMenu_fnsh", 0, -1, -1, false );
	btn1->SetAnimation( "th_HomeBtn_d_hmMenu_fnsh", 0, -1, -1, false );
	btnTopBar->Start();
	btnBottomBar->Start();
	btnWiimote->Start();
	btn1->Start();

	choice = Ch_None;
	state = St_FadeOut;
	menuTransitionFrames = 0;
	waitForARelease = true;
	CInputs::Instance()->ClearButtonsDown();
}

void HomeMenu::CenterBtnClicked()
{
	if( state == St_BtnIdle )
	{
		// Do not wait for the center-button press animation to finish; on some
		// themes it animates but never reaches the Finished callback.
		choice = Ch_None;
		state = St_Dialog;
		dlgState = DSt_Opening;
		dialogFrames = 0;
		waitForARelease = true;
		ShowDialogPanes( true );

		btnTopBar->SetEnabled( false );
		btnBottomBar->SetEnabled( false );
		btnWiimote->SetEnabled( false );
		btn1->SetEnabled( false );

		dlgObj->SetAnimation( "th_HomeBtn_d_cmn_msg_in", 0, -1, -1, false );
		dlgObj->Start();
		CInputs::Instance()->ClearButtonsDown();
		gprintf( "ReturnDialog: Hidden -> Opening\n" );
	}
}

void HomeMenu::CenterBtnAnimDone()
{
	if( choice == Ch_CenterBtn )
	{
		choice = Ch_None;
		state = St_Dialog;
		dlgState = DSt_Opening;
		dialogFrames = 0;
		waitForARelease = true;
		ShowDialogPanes( true );

		// disable background buttons
		btnTopBar->SetEnabled( false );
		btnBottomBar->SetEnabled( false );
		btnWiimote->SetEnabled( false );
		btn1->SetEnabled( false );

		// bring in the dialog
		dlgObj->SetAnimation( "th_HomeBtn_d_cmn_msg_in", 0, -1, -1, false );
		dlgObj->Start();
		CInputs::Instance()->ClearButtonsDown();
		gprintf( "ReturnDialog: Hidden -> Opening\n" );
	}
}

void HomeMenu::DlgBtn1Clicked()
{
	if( state == St_Dialog && dlgState == DSt_Active )
	{
		btnDlg1->SetEnabled( false );
		btnDlg2->SetEnabled( false );
		choice = Ch_None;
		dlgState = DSt_ClosingYes;
		dialogFrames = 0;
		dlgObj->SetAnimation( "th_HomeBtn_d_cmn_msg_out", 0, -1, -1, false );
		dlgObj->Start();
		CInputs::Instance()->ClearButtonsDown();
		gprintf( "ReturnDialog: Active -> ClosingYes\n" );
	}
}

void HomeMenu::DlgBtn1AnimDone()
{
	if( choice == Ch_DlgLeft )
	{
		choice = Ch_None;

		// get rid of the dialog
		dlgObj->SetAnimation( "th_HomeBtn_d_cmn_msg_out", 0, -1, -1, false );
		dlgObj->Start();

		// disable dialog buttons
		btnDlg1->SetEnabled( false );
		btnDlg2->SetEnabled( false );

		dlgState = DSt_ClosingYes;
	}
}

void HomeMenu::DlgBtn2Clicked()
{
	if( state == St_Dialog && dlgState == DSt_Active )
	{
		CancelDialog();
	}
}

void HomeMenu::DlgBtn2AnimDone()
{
	if( choice == Ch_DlgRight )
	{
		choice = Ch_None;

		// get rid of the dialog
		dlgObj->SetAnimation( "th_HomeBtn_d_cmn_msg_out", 0, -1, -1, false );
		dlgObj->Start();

		// disable dialog buttons
		btnDlg1->SetEnabled( false );
		btnDlg2->SetEnabled( false );

		dlgState = DSt_ClosingNo;
	}
}

void HomeMenu::DialogAnimDone()
{
	if( dlgState == DSt_Opening )
	{
		// enable dialog buttons
		MakeDialogReady();
	}
	else if( dlgState == DSt_ClosingNo )
	{
		FinishCancelDialog();
	}
	else if( dlgState == DSt_ClosingYes )
	{
		FinishConfirmDialog();
	}
}

void HomeMenu::MakeButtonsReady()
{
	// Reset edge state only after the opening animation, then let Update()
	// resolve hover from the current position without requiring pointer motion.
	btnTopBar->SetState( Button::St_Idle );
	btnBottomBar->SetState( Button::St_Idle );
	btnWiimote->SetState( Button::St_Idle );
	btn1->SetState( Button::St_Idle );
	btnTopBar->SetEnabled( true );
	btnBottomBar->SetEnabled( true );
	btnWiimote->SetEnabled( true );
	btn1->SetEnabled( true );
	state = St_BtnIdle;
	choice = Ch_None;
}

void HomeMenu::MakeDialogReady()
{
	if( dlgState != DSt_Opening )
		return;
	dialogReady = true;
	btnDlg1->SetEnabled( true );
	btnDlg2->SetEnabled( true );
	dlgState = DSt_Active;
	dialogFrames = 0;
	CInputs::Instance()->ClearButtonsDown();
	gprintf( "ReturnDialog: Opening -> Active\n" );
}

void HomeMenu::CancelDialog()
{
	if( state != St_Dialog || dlgState != DSt_Active )
		return;

	// Some HOME-menu archives never finish (or corrupt pane state during) the
	// common message-out animation.  Cancelling is not destructive, so dismiss
	// it atomically instead of leaving a half-animated modal over the menu.
	btnDlg1->SetEnabled( false );
	btnDlg2->SetEnabled( false );
	dialogReady = false;
	choice = Ch_None;
	dlgState = DSt_ClosingNo;
	dialogFrames = 0;
	CInputs::Instance()->ClearButtonsDown();
	gprintf( "ReturnDialog No callback fired\n" );
	FinishCancelDialog();
}

void HomeMenu::ShowDialogPanes( bool show )
{
	SetPaneVisible( lyt, "back_01", show );
	SetPaneVisible( lyt, "N_Dialog", show );
}

void HomeMenu::FinishCancelDialog()
{
	if( dlgState != DSt_ClosingNo )
		return;
	btnDlg1->SetEnabled( false );
	btnDlg2->SetEnabled( false );
	btnDlg1->SetState( Button::St_Idle );
	btnDlg2->SetState( Button::St_Idle );
	ShowDialogPanes( false );
	dialogReady = false;
	dlgState = DSt_Hidden;
	waitForARelease = true;
	CInputs::Instance()->ClearButtonsDown();
	MakeButtonsReady();
	gprintf( "ReturnDialog hidden and input restored\n" );
}

void HomeMenu::FinishConfirmDialog()
{
	if( dlgState != DSt_ClosingYes )
		return;
	dlgState = DSt_Hidden;
	ShowDialogPanes( false );
	btnDlg1->SetEnabled( false );
	btnDlg2->SetEnabled( false );
	gprintf( "ReturnDialog: ClosingYes -> exit\n" );
	ExitToWiiMenu();
}
