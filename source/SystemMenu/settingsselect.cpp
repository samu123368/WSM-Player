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
#include "bmg.h"
#include "buttoncoords.h"
#include "Inputs.h"
#include "appsettingsscreen.h"
#include "settingsselect.h"
#include "video.h"
#include <ogc/lwp_watchdog.h>

SettingsSelect::SettingsSelect()
	: layout( NULL ),

	  btnSettings( NULL ),
	  btnDataMan( NULL ),
	  btnSaveData( NULL ),
	  btnChannel( NULL ),
	  btnWii( NULL ),
	  btnGC( NULL ),
	  locationObj( new Object ),
	  appSettings( new AppSettingsScreen ),
	  channelRefreshPending( false ),
	  focusedButton( 1 ),
	  settingsFadeFrame( 0 ), settingsReturning( false ), settingsBlackStarted( 0 )
{
}

SettingsSelect::~SettingsSelect()
{
	delete btnSettings;
	delete btnDataMan;
	delete btnSaveData;
	delete btnChannel;
	delete btnWii;
	delete btnGC;
	delete locationObj;
	delete appSettings;

	delete layout;

	std::map< std::string, Animation *>:: iterator it = brlans.begin(), itE = brlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
}

void SettingsSelect::SetupText()
{
	SetText( layout, "T_Datamanage0_00", 253 );	// "Data Management"
	SetText( layout, "T_DataManage_01", 253 );
	SetText( layout, "T_SaveData_00", 254 );	// "Save Data"
	SetText( layout, "T_SaveData_01", 254 );
	SetText( layout, "T_Setting_00", 316 );		// "Wii Settings"
	SetText( layout, "T_Channel_00", 255 );		// "Channels"
	SetText( layout, "T_Channel_01", 255 );
	SetText( layout, "T_Wii_00", 256 );			// "Wii"
	SetText( layout, "T_Wii_01", 256 );
	SetText( layout, "T_Cube_00", 257 );		// "Nintendo GameCube"
	SetText( layout, "T_Cube_01", 257 );
}

bool SettingsSelect::Load( const u8* setupSelAshData, u32 setupSelAshSize,
	const u8 *iplSettingAshData, u32 iplSettingAshSize )
{
	if( loaded )
	{
		return true;
	}

	if( !setupSelAshData || !setupSelAshSize )
	{
		return false;
	}

	U8Archive arc( setupSelAshData, setupSelAshSize );

	// create layout
	if( !(layout = LoadLayout( arc, "it_ObjSetUp_a" ) ) )
	{
		return false;
	}

	// create buttons
	u32 x, y, w, h;

	ButtonCoords( Setup_Sel_Right_SM, x, y, w, h );
	btnSettings = new QuadButton( x, y, w, h );
	btnChannel = new QuadButton( x, y, w, h );
	btnGC = new QuadButton( x, y, w, h );


	ButtonCoords( Setup_Sel_Left_SM, x, y, w, h );
	btnDataMan = new QuadButton( x, y, w, h );
	btnSaveData = new QuadButton( x, y, w, h );
	btnWii = new QuadButton( x, y, w, h );

	// load animations
	Animation *anim;
#define LOADANIM2( x, y, z )								\
	do														\
	{														\
		if( !(anim = LoadAnimation( arc, z ) ) )			\
		{													\
			return false;									\
		}													\
		brlans[ z ] = anim;									\
		x->AddAnimation( anim );							\
		y->AddAnimation( anim );							\
	}														\
	while( 0 )

	// anims for buttons on the first page
	btnSettings->BindGroup( layout->FindGroup( "G_Setting_00" ) );
	btnDataMan->BindGroup( layout->FindGroup( "G_DataManage_00" ) );
	BindLocationPane( btnDataMan, "DataManage_02" );
	BindLocationPane( btnDataMan, "T_DataManage_01" );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpFoucusIn" );
	btnSettings->SetMouseOverAnimation( anim, 0, -1 );
	btnDataMan->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpFoucusOut" );
	btnSettings->SetMouseOutAnimation( anim, 0, -1 );
	btnDataMan->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpFoucusFlash" );
	btnSettings->SetClickAnimation( anim, 0, -1 );
	btnDataMan->SetClickAnimation( anim, 0, -1 );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpIn" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpOut" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnSettings, btnDataMan, "it_ObjSetUp_a_SetUpBack" );
	locationObj->AddAnimation( anim );
	btnSettings->SetTrigger( Button::Btn_A );
	btnDataMan->SetTrigger( Button::Btn_A );

	// anims for buttons on the data management page
	btnSaveData->BindGroup( layout->FindGroup( "G_SaveData_00" ) );
	btnChannel->BindGroup( layout->FindGroup( "G_Channel_00" ) );
	BindLocationPane( btnSaveData, "SaveData_02" );
	BindLocationPane( btnSaveData, "T_SaveData_01" );
	BindLocationPane( btnChannel, "Channel_02" );
	BindLocationPane( btnChannel, "T_Channel_01" );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelFoucusFlash" );
	btnSaveData->SetClickAnimation( anim, 0, -1 );
	btnChannel->SetClickAnimation( anim, 0, -1 );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelFoucusIn" );
	btnSaveData->SetMouseOverAnimation( anim, 0, -1 );
	btnChannel->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelFoucusOut" );
	btnSaveData->SetMouseOutAnimation( anim, 0, -1 );
	btnChannel->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelIn" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelOut" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnSaveData, btnChannel, "it_ObjSetUp_a_DataChannelBack" );
	locationObj->AddAnimation( anim );
	btnSaveData->SetTrigger( Button::Btn_A );
	btnChannel->SetTrigger( Button::Btn_A );

	// setup page 3 ( savedata wii/GC )
	btnWii->BindGroup( layout->FindGroup( "G_Wii_00" ) );
	btnGC->BindGroup( layout->FindGroup( "G_Cube_00" ) );
	BindLocationPane( btnWii, "Wii_02" );
	BindLocationPane( btnWii, "T_Wii_01" );
	BindLocationPane( btnGC, "Cube_02" );
	BindLocationPane( btnGC, "T_Cube_01" );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataFoucusFlash" );
	btnWii->SetClickAnimation( anim, 0, -1 );
	btnGC->SetClickAnimation( anim, 0, -1 );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataFoucusIn" );
	btnWii->SetMouseOverAnimation( anim, 0, -1 );
	btnGC->SetMouseOverAnimation( anim, 0, -1 );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataFoucusOut" );
	btnWii->SetMouseOutAnimation( anim, 0, -1 );
	btnGC->SetMouseOutAnimation( anim, 0, -1 );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataIn" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataOut" );
	locationObj->AddAnimation( anim );
	LOADANIM2( btnWii, btnGC, "it_ObjSetUp_a_SaveDataBack" );
	locationObj->AddAnimation( anim );
	btnWii->SetTrigger( Button::Btn_A );
	btnGC->SetTrigger( Button::Btn_A );

	// Start on the authored page-one entrance.  Input remains disabled until
	// every pane controller (including the location indicator) reaches the end.
	DisableAllButtons();
	StartPageAnimation( btnDataMan, btnSettings,
		"it_ObjSetUp_a_SetUpIn" );

	// connect some signals and slots to the buttons
	btnSettings->Clicked.connect( this, &SettingsSelect::SettingsBtnClicked );
	btnSettings->Finished.connect( this, &SettingsSelect::SettingsBtnFinished );

	btnDataMan->Clicked.connect( this, &SettingsSelect::BtnDataManClicked );
	btnDataMan->Finished.connect( this, &SettingsSelect::BtnDataManFinished );

	btnSaveData->Clicked.connect( this, &SettingsSelect::BtnSaveDataClicked );
	btnSaveData->Finished.connect( this, &SettingsSelect::BtnSaveDataFinished );

	btnWii->Clicked.connect( this, &SettingsSelect::BtnWiiSaveClicked );
	btnWii->Finished.connect( this, &SettingsSelect::BtnWiiSaveFinished );

	btnGC->Clicked.connect( this, &SettingsSelect::BtnGCSaveClicked );
	btnGC->Finished.connect( this, &SettingsSelect::BtnGCSaveFinished );

	btnChannel->Clicked.connect( this, &SettingsSelect::BtnChannelClicked );
	btnChannel->Finished.connect( this, &SettingsSelect::BtnChannelFinished );


	// setup translated strings
	SetupText();
	appSettings->LoadSystemSettingsArchive( iplSettingAshData,
		iplSettingAshSize );


	state = St_StartIn;
	loaded = true;
	return true;
}

bool SettingsSelect::IsSystemSettingsTransition() const
{
	return state == St_SystemSettingsFadeOut || state == St_SystemSettingsBlackHold
		|| state == St_SystemSettingsFadeIn;
}

void SettingsSelect::OpenSystemSettingsDirect( bool alreadyBlack )
{
	DisableAllButtons();
	settingsFadeFrame = 0;
	settingsReturning = false;
	state = St_SystemSettingsFadeOut;
	if( alreadyBlack )
	{
		settingsBlackStarted = gettime();
		state = St_SystemSettingsBlackHold;
	}
	CInputs::Instance()->ClearButtonsDown();
}

void SettingsSelect::Render( Mtx &modelview, const Vec2f &ScreenProps, bool widescreen, bool allowInput )
{
	if( !loaded )
	{
		return;
	}
	if( IsSystemSettingsTransition() )
	{
		const int frames = 24;
		CInputs::Instance()->ClearButtonsDown();
		if( state == St_SystemSettingsFadeOut )
		{
			if( settingsReturning ) appSettings->Render( modelview, ScreenProps, widescreen, false );
			else layout->Render( modelview, ScreenProps, widescreen );
			DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y,
				(GXColor){0, 0, 0, (u8)( ++settingsFadeFrame * 255 / frames )} );
			if( settingsFadeFrame >= frames )
			{
				settingsBlackStarted = gettime();
				state = St_SystemSettingsBlackHold;
			}
		}
		else if( state == St_SystemSettingsBlackHold )
		{
			// Keep pumping the main loop/inputs/watchdog during the five-second hold.
			DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y, (GXColor){0, 0, 0, 255} );
			if( diff_msec( settingsBlackStarted, gettime() ) >= ( settingsReturning ? 2000 : 5000 ) )
			{
				if( settingsReturning ) { SetupText(); MakeStartReady(); DisableAllButtons(); }
				else appSettings->OpenSystemSettings();
				settingsFadeFrame = frames;
				state = St_SystemSettingsFadeIn;
			}
		}
		else
		{
			if( settingsReturning ) layout->Render( modelview, ScreenProps, widescreen );
			else appSettings->Render( modelview, ScreenProps, widescreen, false );
			DrawSquare( 0, 0, ScreenProps.x, ScreenProps.y,
				(GXColor){0, 0, 0, (u8)( settingsFadeFrame * 255 / frames )} );
			if( --settingsFadeFrame <= 0 )
			{
				if( settingsReturning ) MakeStartReady();
				else state = St_AppSettings;
			}
		}
		return;
	}
	if( state == St_AppSettings )
	{
		appSettings->Render( modelview, ScreenProps, widescreen, allowInput );
		if( appSettings->TakeSystemSettingsExitRequest() )
		{
			settingsReturning = true;
			settingsFadeFrame = 0;
			state = St_SystemSettingsFadeOut;
			DisableAllButtons();
			CInputs::Instance()->ClearButtonsDown();
			return;
		}
		if( appSettings->TakeChannelRefreshRequest() )
			channelRefreshPending = true;
		if( appSettings->IsClosed() )
		{
			if( channelRefreshPending )
			{
				channelRefreshPending = false;
				ExitSettings();
				return;
			}
			SetupText();
			state = St_StartIn;
			DisableAllButtons();
			StartPageAnimation( btnDataMan, btnSettings,
				"it_ObjSetUp_a_SetUpBack" );
			CInputs::Instance()->ClearButtonsDown();
		}
		return;
	}

	// GameCube save management is not implemented in this player.  Keep the
	// authored tile visible so the page still matches the System Menu layout,
	// but render it with the normal disabled appearance and never give it focus.
	// Re-apply the alpha before each draw because the page BRLANs also animate it.
	const bool showGameCube = state == St_SaveDataIn
		|| state == St_SaveDataIdle || state == St_SaveDataSelectingWii
		|| state == St_SaveDataOut || state == St_GCSaveIn
		|| state == St_GCSaveIdle || state == St_GCSaveOut;
	const Layout::Group *gameCubeGroup = layout->FindGroup( "G_Cube_00" );
	if( gameCubeGroup )
	{
		for( PaneList::const_iterator it = gameCubeGroup->panes.begin();
			it != gameCubeGroup->panes.end(); ++it )
		{
			if( *it )
			{
				( *it )->SetVisible( showGameCube );
				if( showGameCube ) ( *it )->SetAlpha( 0x60 );
			}
		}
	}

	// draw it
	layout->Render( modelview, ScreenProps, widescreen );

	// WSM Player Settings belongs on the first Wii Options selector, alongside
	// Data Management and Wii Settings.  Keeping it here (instead of embedding
	// it in one of the retail HTML pages) makes it exactly one menu down from the
	// channel grid and leaves the real Wii System Settings archive untouched.
	if( state == St_Start )
	{
		bool hovered = false;
		for( int i = 0; i < 4; ++i )
		{
			const WPADData &wpad = Pad( i ).GetData();
			if( wpad.ir.valid && appSettings->LauncherButtonContains(
				wpad.ir.x, wpad.ir.y, ScreenProps ) )
			{
				hovered = true;
				break;
			}
		}
		appSettings->RenderLauncherButton( ScreenProps, hovered );
	}


	// animate and respond to user input
	switch( state )
	{
	case St_Start:
		btnSettings->Update();
		btnDataMan->Update();
		break;
	case St_DataManageIdle:
		btnSaveData->Update();
		btnChannel->Update();
		break;
	case St_SaveDataIdle:
		btnWii->Update();
		btnGC->Update();
		break;
	default:
		break;
	}
	if( allowInput ) HandleFallbackInput();
	btnSettings->Advance();
	btnDataMan->Advance();

	btnSaveData->Advance();
	btnChannel->Advance();

	btnWii->Advance();
	btnGC->Advance();
	locationObj->Advance();
	FinishTransitionIfReady();
}

void SettingsSelect::MakeStartReady()
{
	MakePageReady( St_Start );
}

void SettingsSelect::BindLocationPane( QuadButton *owner, const char *paneName )
{
	Pane *pane = layout ? layout->FindPane( paneName ) : NULL;
	if( !pane )
	{
		gprintf( "SettingsSelect: location pane missing: %s\n", paneName );
		return;
	}
	// These panes are nested in the selectable-button groups in the retail
	// BRLYT, but their *_In/*_Out/*_Back tracks belong to the page header.  A
	// dedicated controller prevents a hover/click animation from leaving the
	// current-location graphic transparent.
	owner->UnbindPane( pane, false );
	locationObj->BindPane( pane, false );
}

void SettingsSelect::DisableAllButtons()
{
	btnSettings->SetEnabled( false );
	btnDataMan->SetEnabled( false );
	btnSaveData->SetEnabled( false );
	btnChannel->SetEnabled( false );
	btnWii->SetEnabled( false );
	btnGC->SetEnabled( false );
}

void SettingsSelect::MakePageReady( State readyState )
{
	DisableAllButtons();
	btnSettings->SetState( Button::St_Idle );
	btnDataMan->SetState( Button::St_Idle );
	btnSaveData->SetState( Button::St_Idle );
	btnChannel->SetState( Button::St_Idle );
	btnWii->SetState( Button::St_Idle );
	btnGC->SetState( Button::St_Idle );

	state = readyState;
	switch( readyState )
	{
	case St_Start:
	case St_AppSettings:
		btnSettings->SetEnabled( true );
		btnDataMan->SetEnabled( true );
		focusedButton = 1;
		break;
	case St_DataManageIdle:
		btnSaveData->SetEnabled( true );
		btnChannel->SetEnabled( true );
		focusedButton = 0;
		break;
	case St_SaveDataIdle:
		btnWii->SetEnabled( true );
		// The legacy GameCube-management branch has no backing implementation.
		// Leave its tile visibly disabled instead of accepting a click that does
		// nothing.
		btnGC->SetEnabled( false );
		focusedButton = 0;
		break;
	default:
		break;
	}
}

void SettingsSelect::StartPageAnimation( QuadButton *left, QuadButton *right,
	const char *animation, bool animateLocation )
{
	left->SetAnimation( animation, 0, -1, -1, false );
	right->SetAnimation( animation, 0, -1, -1, false );
	left->Start();
	right->Start();
	if( animateLocation )
	{
		locationObj->SetAnimation( animation, 0, -1, -1, false );
		locationObj->Start();
	}
}

void SettingsSelect::StartSelectionAnimation( QuadButton *selected,
	const char *animation, State selectingState )
{
	// Button::Update emits Clicked immediately so menu actions stay responsive.
	// Re-start the retail focus-flash here at its authored 40-frame length, then
	// let FinishTransitionIfReady begin the fly-off on a later frame.  This also
	// gives D-pad activation the same visual sequence as a pointer click.
	state = selectingState;
	DisableAllButtons();
	selected->SetAnimation( animation, 0, -1, -1, false );
	selected->Start();
}

bool SettingsSelect::TransitionAnimationsFinished() const
{
	return btnSettings->IsFinished() && btnDataMan->IsFinished()
		&& btnSaveData->IsFinished() && btnChannel->IsFinished()
		&& btnWii->IsFinished() && btnGC->IsFinished()
		&& locationObj->IsFinished();
}

bool SettingsSelect::IsTransitioning() const
{
	switch( state )
	{
	case St_StartIn:
	case St_StartSelectingData:
	case St_StartSelectingSettings:
	case St_StartSettingsOut:
	case St_StartOut:
	case St_DataManageIn:
	case St_DataManageSelectingSave:
	case St_DataManageSelectingChannel:
	case St_DataManageFadeOut:
	case St_SaveDataIn:
	case St_SaveDataSelectingWii:
	case St_SaveDataOut:
	case St_WiiSaveIn:
	case St_WiiSaveOut:
	case St_GCSaveIn:
	case St_GCSaveOut:
	case St_ChannelIn:
	case St_ChannelOut:
	case St_ReturningFromHome:
		return true;
	default:
		return false;
	}
}

bool SettingsSelect::IsAppSettingsActive() const
{
	return state == St_AppSettings && appSettings && appSettings->IsOpen();
}

bool SettingsSelect::IsUsbDevicesActive() const
{
	return IsAppSettingsActive() && appSettings->IsUsbDevicesOpen();
}

bool SettingsSelect::IsApplyingTheme() const
{
	return IsAppSettingsActive() && appSettings->IsApplyingTheme();
}

bool SettingsSelect::TakeThemeRestartRequest()
{
	return IsAppSettingsActive() && appSettings->TakeThemeRestartRequest();
}

void SettingsSelect::ThemeRestartFailed( s32 error )
{
	if( appSettings ) appSettings->ThemeRestartFailed( error );
}

bool SettingsSelect::IsFakeOobeActive() const
{
	return state == St_AppSettings && appSettings
		&& appSettings->IsOpen() && appSettings->IsFakeOobeActive();
}

void SettingsSelect::FinishTransitionIfReady()
{
	// Selection feedback and page movement are deliberately separate phases.
	// Starting an *_Out BRLAN from the Clicked signal used to overwrite the
	// focus-flash on the same frame, making the retail fly-off sequence appear
	// to be missing.  Wait only for the selected button here; sibling hover
	// animations are irrelevant and input is already disabled.
	switch( state )
	{
	case St_StartSelectingData:
		if( !btnDataMan->IsFinished() )
			return;
		state = St_StartOut;
		StartPageAnimation( btnDataMan, btnSettings,
			"it_ObjSetUp_a_SetUpOut" );
		return;
	case St_StartSelectingSettings:
		if( !btnSettings->IsFinished() )
			return;
		state = St_StartSettingsOut;
		StartPageAnimation( btnDataMan, btnSettings,
			"it_ObjSetUp_a_SetUpOut" );
		return;
	case St_DataManageSelectingSave:
		if( !btnSaveData->IsFinished() )
			return;
		state = St_DataManageFadeOut;
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelOut" );
		return;
	case St_DataManageSelectingChannel:
		if( !btnChannel->IsFinished() )
			return;
		state = St_ChannelIn;
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelOut" );
		return;
	case St_SaveDataSelectingWii:
		if( !btnWii->IsFinished() )
			return;
		state = St_WiiSaveIn;
		StartPageAnimation( btnWii, btnGC,
			"it_ObjSetUp_a_SaveDataOut" );
		return;
	default:
		break;
	}

	if( !IsTransitioning() || !TransitionAnimationsFinished() )
		return;

	switch( state )
	{
	case St_StartIn:
		MakeStartReady();
		break;
	case St_StartOut:
		state = St_DataManageIn;
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelIn" );
		break;
	case St_StartSettingsOut:
		state = St_AppSettings;
		appSettings->OpenSystemSettings();
		break;
	case St_DataManageIn:
		MakePageReady( St_DataManageIdle );
		break;
	case St_DataManageFadeOut:
		state = St_SaveDataIn;
		StartPageAnimation( btnWii, btnGC,
			"it_ObjSetUp_a_SaveDataIn" );
		break;
	case St_SaveDataIn:
		MakePageReady( St_SaveDataIdle );
		break;
	case St_WiiSaveIn:
		DisableAllButtons();
		btnWii->SetState( Button::St_Idle );
		state = St_WiiSaveIdle;
		HideWii( true );
		AppendWiiSaveData( true );
		break;
	case St_GCSaveIn:
		DisableAllButtons();
		btnGC->SetState( Button::St_Idle );
		state = St_GCSaveIdle;
		HideWii( true );
		AppendGCSaveData( true );
		break;
	case St_ChannelIn:
		DisableAllButtons();
		btnChannel->SetState( Button::St_Idle );
		state = St_ChannelIdle;
		HideWii( true );
		AppendChannelManager( true );
		break;
	case St_ReturningFromHome:
		MakePageReady( homeReturnState );
		break;
	default:
		break;
	}
}

bool SettingsSelect::BeginHomeMenuReturn()
{
	const char *animation = NULL;
	QuadButton *left = NULL;
	QuadButton *right = NULL;

	switch( state )
	{
	case St_Start:
		animation = "it_ObjSetUp_a_SetUpBack";
		left = btnDataMan;
		right = btnSettings;
		break;
	case St_DataManageIdle:
		animation = "it_ObjSetUp_a_DataChannelBack";
		left = btnSaveData;
		right = btnChannel;
		break;
	case St_SaveDataIdle:
		animation = "it_ObjSetUp_a_SaveDataBack";
		left = btnWii;
		right = btnGC;
		break;
	default:
		return false;
	}

	homeReturnState = state;
	state = St_ReturningFromHome;
	DisableAllButtons();
	StartPageAnimation( left, right, animation );
	return true;
}

void SettingsSelect::HandleFallbackInput()
{
	// The save/channel management views are modal children rendered after this
	// selector.  They share the same Controller snapshots, so consuming A/B
	// here while one of those children is active prevents every child button
	// from ever seeing the press.  Keep the selector visible for the authored
	// transition, but leave input ownership to the active child view.
	if( state != St_Start
		&& state != St_DataManageIdle && state != St_SaveDataIdle )
	{
		return;
	}

	for( int i = 0; i < 4; i++ )
	{
		const WPADData &wpad = Pad( i ).GetData();
		const Vec2f screen = { (f32)screenwidth, (f32)screenheight };
		if( state == St_Start && Pad( i ).pA() && wpad.ir.valid
			&& appSettings->LauncherButtonContains( wpad.ir.x, wpad.ir.y,
				screen ) )
		{
			DisableAllButtons();
			state = St_AppSettings;
			appSettings->Open();
			Pad( i ).Take();
			continue;
		}
		// Plus is the non-pointer shortcut for the same first-page button.
		if( state == St_Start && Pad( i ).pPlus()
			&& Pad( i ).UsbMouseWheel() == 0 )
		{
			DisableAllButtons();
			state = St_AppSettings;
			appSettings->Open();
			Pad( i ).Take();
			continue;
		}
		if( Pad( i ).pB() )
		{
			BackBtnClicked();
			Pad( i ).Take();
			continue;
		}
		if( Pad( i ).pLeft() )
		{
			focusedButton = 0;
			Pad( i ).Take();
		}
		else if( Pad( i ).pRight() )
		{
			// There is only one usable target on the Save Data selector.
			focusedButton = state == St_SaveDataIdle ? 0 : 1;
			Pad( i ).Take();
		}
		if( Pad( i ).pA() && !Pad( i ).IsTaken() )
		{
			// Authored buttons consume a pointer click in Button::Update(). A mouse
			// OR Wii Remote IR click left here is blank space, not permission to fire
			// whichever D-pad fallback happened to be focused previously.
			if( !wpad.ir.valid && !Pad( i ).IsUsbMouseConnected() )
				ActivateFocusedButton();
			Pad( i ).Take();
		}
	}
}

void SettingsSelect::ActivateFocusedButton()
{
	switch( state )
	{
	case St_Start:
		if( focusedButton == 0 )
			BtnDataManClicked();
		else
			SettingsBtnClicked();
		break;
	case St_DataManageIdle:
		if( focusedButton == 0 )
			BtnSaveDataClicked();
		else
			BtnChannelClicked();
		break;
	case St_SaveDataIdle:
		if( focusedButton == 0 )
			BtnWiiSaveClicked();
		else
			BtnGCSaveClicked();
		break;
	default:
		break;
	}
}

void SettingsSelect::SettingsBtnClicked()
{
	if( state == St_Start )
	{
		OpenSystemSettingsDirect();
	}
}

void SettingsSelect::SettingsBtnFinished()
{
}

void SettingsSelect::BtnDataManClicked()
{
	if( state == St_Start )
	{
		StartSelectionAnimation( btnDataMan,
			"it_ObjSetUp_a_SetUpFoucusFlash", St_StartSelectingData );
	}
}

void SettingsSelect::BtnDataManFinished()
{
	// Transitions are polled once after every controller advances in Render().
}

void SettingsSelect::BtnSaveDataClicked()
{
	if( state == St_DataManageIdle )
	{
		StartSelectionAnimation( btnSaveData,
			"it_ObjSetUp_a_DataChannelFoucusFlash",
			St_DataManageSelectingSave );
	}
}

void SettingsSelect::BtnSaveDataFinished()
{
	// See BtnDataManFinished().
}

void SettingsSelect::BackBtnClicked()
{
	//gprintf( "SettingsSelect::BackBtnClicked(): %u\n", state );
	if( state == St_Start )
	{
		ExitSettings();
	}
	else if( state == St_AppSettings )
	{
		// The shared setupBtn Back control must move back one level inside the
		// WSM/HTML browser.  Closing the entire modal here caused subpages to
		// jump all the way to the Wii Options selector.
		appSettings->HandleBack();
	}
	else if( state == St_DataManageIdle )
	{
		state = St_StartIn;
		DisableAllButtons();
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelOut", false );
		StartPageAnimation( btnDataMan, btnSettings,
			"it_ObjSetUp_a_SetUpIn" );
	}
	else if( state == St_SaveDataIdle )
	{
		state = St_DataManageIn;
		DisableAllButtons();
		StartPageAnimation( btnWii, btnGC,
			"it_ObjSetUp_a_SaveDataOut", false );
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelIn" );
	}
	/*else if( state == St_WiiSaveIdle || state == St_GCSaveIdle )
	{
		// bring back the buttons for save data page
		btnWii->SetAnimation( "it_ObjSetUp_a_SaveDataIn", 0, -1, -1, false );
		btnGC->SetAnimation( "it_ObjSetUp_a_SaveDataIn", 0, -1, -1, false );
		btnWii->Start();
		btnGC->Start();

		// show the "Wii" logo
		HideWii( false );

		// signal to remove save data layouts
		if( state == St_WiiSaveIdle )
		{
			AppendWiiSaveData( false );
		}
		else
		{
			AppendGCSaveData( false );
		}
		state = St_SaveDataIn;
	}
	else if( state == St_ChannelIdle )
	{
		state = St_DataManageIn;

		// bring back the buttons for page 2
		btnSaveData->SetAnimation( "it_ObjSetUp_a_DataChannelIn", 0, -1, -1, false );
		btnChannel->SetAnimation( "it_ObjSetUp_a_DataChannelIn", 0, -1, -1, false );
		btnSaveData->Start();
		btnChannel->Start();

		// show the "Wii" logo
		HideWii( false );

		// signal to remove the channel management screen
		AppendChannelManager( false );
	}*/

}

void SettingsSelect::WiiSaveDone()
{
	if( state == St_WiiSaveIdle )
	{
		HideWii( false );
		AppendWiiSaveData( false );
		state = St_SaveDataIn;
		DisableAllButtons();
		StartPageAnimation( btnWii, btnGC,
			"it_ObjSetUp_a_SaveDataIn" );
	}
}

void SettingsSelect::ChannelEditDone()
{
	if( state == St_ChannelIdle )
	{
		HideWii( false );
		AppendChannelManager( false );
		state = St_DataManageIn;
		DisableAllButtons();
		StartPageAnimation( btnSaveData, btnChannel,
			"it_ObjSetUp_a_DataChannelIn" );
	}
}

void SettingsSelect::BtnWiiSaveClicked()
{
	if( state == St_SaveDataIdle )
	{
		StartSelectionAnimation( btnWii,
			"it_ObjSetUp_a_SaveDataFoucusFlash", St_SaveDataSelectingWii );
	}
}

void SettingsSelect::BtnWiiSaveFinished()
{
	// See BtnDataManFinished().
}

void SettingsSelect::BtnGCSaveClicked()
{
	// Deliberately unavailable: no GameCube save grid or transfer backend exists.
}

void SettingsSelect::BtnGCSaveFinished()
{
	// See BtnDataManFinished().
}

void SettingsSelect::BtnChannelClicked()
{
	if( state == St_DataManageIdle )
	{
		StartSelectionAnimation( btnChannel,
			"it_ObjSetUp_a_DataChannelFoucusFlash",
			St_DataManageSelectingChannel );
	}
}

void SettingsSelect::BtnChannelFinished()
{
	// See BtnDataManFinished().
}
