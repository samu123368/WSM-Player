#include <cmath>
#include <cstring>

#include "Inputs.h"
#include "recovery.h"
#include "settings.h"
#include "utils/tools.h"
#include "video.h"
#include "wiiremoteinput.h"

#define PADCAL 50
#define PI 3.14159265f
#define SPEED_CALIBRATION 0.1f

CInputs *CInputs::instance = NULL;

Controller::Controller( int controllerChannel )
	: chan( controllerChannel ), posX( screenwidth * 0.5f ), posY( screenheight * 0.5f ), angle( 0.0f ),
	  lastActivity( 0xffffffff ), pad_btns_d( 0 ), pad_btns_h( 0 ),
	  usb_mouse_btns_d( 0 ), usb_mouse_btns_h( 0 ),
	  usb_keyboard_btns_d( 0 ),
	  usb_mouse_wheel( 0 ),
	  wpadConnected( false ), padConnected( false ),
	  usbMouseConnected( false ), usbKeyboardConnected( false ), taken( false ),
	  nunchukPointerActive( false )
{
	memset( &wpad, 0, sizeof( wpad ) );
	wpad.err = WPAD_ERR_NO_CONTROLLER;
}

void Controller::Update( int physicalChannel, bool gcConnected )
{
	usb_mouse_btns_d = 0;
	usb_mouse_btns_h = 0;
	usb_keyboard_btns_d = 0;
	usb_mouse_wheel = 0;
	usbMouseConnected = false;
	usbKeyboardConnected = false;
	taken = false;
	++lastActivity;

	if( physicalChannel < 0 || physicalChannel >= MAX_CONTROLS )
	{
		memset( &wpad, 0, sizeof( wpad ) );
		wpad.err = WPAD_ERR_NO_CONTROLLER;
		wpadConnected = false;
		padConnected = false;
		pad_btns_d = 0;
		pad_btns_h = 0;
		nunchukPointerActive = false;
		return;
	}

	chan = physicalChannel;
	// This is the normal libogc WPAD access pattern: scan once in CInputs and
	// copy the channel's published report once.  Do not add a second probe or a
	// connection state machine around the report callback.
	wpadConnected = WiiRemoteInput::CopyReport( chan, wpad );
	padConnected = gcConnected;
	// libogc publishes no useful PAD report for an absent channel. Avoid
	// touching the channel buffer unless PAD_ScanPads marked it present.
	pad_btns_d = gcConnected ? PAD_ButtonsDown( chan ) : 0;
	pad_btns_h = gcConnected ? PAD_ButtonsHeld( chan ) : 0;

	if( wpad.ir.valid )
	{
		nunchukPointerActive = false;
		lastActivity = 0;
		posX = wpad.ir.x;
		posY = wpad.ir.y;
		angle = wpad.ir.angle;
		return;
	}

	angle = 0.0f;
	// Ordinary menu pointing falls back to the Nunchuk when the sensor-bar IR
	// is absent. Leave the expansion report intact for spectator flight; that
	// mode consumes UI input before any menu buttons can act on this pointer.
	const bool nunchukConnected = wpadConnected && wpad.exp.type == WPAD_EXP_NUNCHUK;
	if( !nunchukConnected ) nunchukPointerActive = false;
	else
	{
		float magnitude = wpad.exp.nunchuk.js.mag;
		const float direction = wpad.exp.nunchuk.js.ang;
		if( std::isfinite( magnitude ) && std::isfinite( direction ) )
		{
			magnitude = LIMIT( magnitude, 0.0f, 1.0f );
			if( magnitude > 0.18f )
			{
				const float distance = ( magnitude - 0.18f ) / 0.82f * 8.0f;
				posX += std::sin( direction * PI / 180.0f ) * distance;
				posY -= std::cos( direction * PI / 180.0f ) * distance;
				nunchukPointerActive = true;
				lastActivity = 0;
			}
		}
	}
	const int padX = gcConnected ? PAD_StickX( chan ) : 0;
	const int padY = gcConnected ? PAD_StickY( chan ) : 0;
	if( padX < -PADCAL ) { posX += ( padX + PADCAL ) * SPEED_CALIBRATION; lastActivity = 0; }
	else if( padX > PADCAL ) { posX += ( padX - PADCAL ) * SPEED_CALIBRATION; lastActivity = 0; }
	if( padY < -PADCAL ) { posY -= ( padY + PADCAL ) * SPEED_CALIBRATION; lastActivity = 0; }
	else if( padY > PADCAL ) { posY -= ( padY - PADCAL ) * SPEED_CALIBRATION; lastActivity = 0; }

	if( pad_btns_h ) lastActivity = 0;

	posX = LIMIT( posX, -5.0f, screenwidth + 5.0f );
	posY = LIMIT( posY, -5.0f, screenheight + 5.0f );
	// A Wii Remote without current IR must not retain a stale hand or hover
	// target. Only an explicitly used Nunchuk or connected GameCube pad may
	// synthesize a stick pointer;
	// ApplyUsbMouse supplies its own valid position later in the input frame.
	if( nunchukPointerActive || ( padConnected && lastActivity < 300 ) )
	{
		wpad.ir.valid = 1;
		wpad.ir.x = posX;
		wpad.ir.y = posY;
		wpad.ir.angle = angle;
	}
}

void Controller::ApplyUsbMouse( float x, float y, u8 buttonsDown,
	u8 buttonsHeld, int wheel )
{
	usbMouseConnected = true;
	usb_mouse_wheel = wheel;
	posX = LIMIT( x, -5.0f, screenwidth + 5.0f );
	posY = LIMIT( y, -5.0f, screenheight + 5.0f );
	angle = 0.0f;
	lastActivity = 0;
	wpad.ir.valid = 1;
	wpad.ir.x = posX;
	wpad.ir.y = posY;
	wpad.ir.angle = 0.0f;

	// Left and right must remain independent A/B controls.  Holding both is
	// used by the channel-grid drag path and must never be converted to HOME.
	if( buttonsHeld & 0x01 ) usb_mouse_btns_h |= WPAD_BUTTON_A;
	if( buttonsHeld & 0x02 ) usb_mouse_btns_h |= WPAD_BUTTON_B;
	if( buttonsDown & 0x01 ) usb_mouse_btns_d |= WPAD_BUTTON_A;
	if( buttonsDown & 0x02 ) usb_mouse_btns_d |= WPAD_BUTTON_B;
	if( buttonsHeld & 0x04 ) usb_mouse_btns_h |= WPAD_BUTTON_HOME;
	// The middle mouse button is the mouse controller's HOME button.  Keep it
	// distinct from right-click/B so mice with three buttons can reach the same
	// Home Menu path as a Wii Remote without a keyboard-specific shortcut.
	if( buttonsDown & 0x04 ) usb_mouse_btns_d |= WPAD_BUTTON_HOME;
	// Extra side buttons provide the same library shortcuts as Wii Remote 1/2.
	if( buttonsDown & 0x08 ) usb_mouse_btns_d |= WPAD_BUTTON_1;
	if( buttonsDown & 0x10 ) usb_mouse_btns_d |= WPAD_BUTTON_2;
	// Wheel pulses mirror the Wii Remote's Plus/Minus page controls.
	if( wheel > 0 ) usb_mouse_btns_d |= WPAD_BUTTON_PLUS;
	else if( wheel < 0 ) usb_mouse_btns_d |= WPAD_BUTTON_MINUS;
}

void Controller::ApplyUsbKeyboard( u32 buttonsDown )
{
	usbKeyboardConnected = true;
	usb_keyboard_btns_d |= buttonsDown;
	if( buttonsDown ) lastActivity = 0;
}

void Controller::SuppressUiInput()
{
	ClearButtonsDown();
	pad_btns_h = 0;
	wpad.btns_h = 0;
	wpad.ir.valid = 0;
	usb_mouse_btns_h = 0;
	usb_mouse_wheel = 0;
	taken = true;
}

void Controller::ClearButtonsDown()
{
	pad_btns_d = 0;
	wpad.btns_d = 0;
	usb_mouse_btns_d = 0;
	usb_keyboard_btns_d = 0;
	taken = false;
}

CInputs::CInputs()
	: pendingSwapFirst( -1 ), pendingSwapSecond( -1 ),
	  mousePlayer( -1 ), keyboardPlayer( -1 ), usbStartupFrames( 0 ),
	  usbStartupAllowed( false ),
	  usbMouseInitialized( false ),
	  usbMouseConnected( false ),
	  usbKeyboardButtonsDown( 0 ),
	  usbRoutingLogged( false ),
	  usbKeyRead( 0 ), usbKeyWrite( 0 )
{
	memset( usbKeyQueue, 0, sizeof( usbKeyQueue ) );
    for(unsigned i=0;i<WSM_MAX_MICE;++i) {
        mice[i].player=-1;mice[i].connected=mice[i].armed=false;
        mice[i].buttons=0;mice[i].x=screenwidth*.5f;mice[i].y=screenheight*.5f;
    }
	PAD_Init();
	WiiRemoteInput::Initialize( screenwidth, screenheight );
	for( int i = 0; i < MAX_CONTROLS; ++i )
	{
		controller[ i ].SetChannel( i );
		physicalForLogical[ i ] = i;
	}
	// WPAD/BTE already initializes shared libogc USB here. Delay only our mouse
	// worker, leaving Bluetooth's initialization and outstanding requests intact.
}

CInputs::~CInputs()
{
	if( usbMouseInitialized ) wiredUsbMouse.Shutdown();
}

void CInputs::Update()
{
	// Count the WPAD settling period across every screen.  The old code reset
	// this counter whenever a banner or settings page opened, so a user who did
	// not remain on the channel grid for three uninterrupted seconds could keep
	// USB disabled for the entire session.  The actual IOS58 claim is still
	// restricted to a stable grid frame below.
	if( !usbMouseInitialized && Settings::usbInputEnabled
		&& usbStartupFrames < 180 )
		++usbStartupFrames;
	// Standard libogc WPAD path: one scan and one report copy per physical
	// channel. Logical player order is applied afterwards, never in Bluetooth.
	Recovery::SetCheckpoint( 0x111, "scanning Wii Remotes" );
	WiiRemoteInput::Scan();
	Recovery::SetCheckpoint( 0x112, "scanning GameCube controllers" );
	const u32 gcMask = PAD_ScanPads();

	// Snapshot every physical controller immediately after its matching scan.
	// In particular, do this before servicing USB IOS requests. Keeping an OH0
	// request between WPAD_ScanPads() and its report read caused real hardware
	// to stall at the old broad H114 checkpoint when mouse and remote input were
	// both active.
	static const char *const reportCheckpoints[ MAX_CONTROLS ] =
	{
		"copying controller report P1", "copying controller report P2",
		"copying controller report P3", "copying controller report P4"
	};
	for( int channel = 0; channel < MAX_CONTROLS; ++channel )
	{
		Recovery::SetCheckpoint( 0x116 + channel,
			reportCheckpoints[ channel ] );
		const bool gcConnected =
			( gcMask & ( PAD_CHAN0_BIT >> channel ) ) != 0;
		physicalController[ channel ].Update( channel, gcConnected );
	}
	const bool swapped = pendingSwapFirst >= 0;
	if( swapped )
	{
		const int first = pendingSwapFirst, second = pendingSwapSecond;
		const int physical = physicalForLogical[first];
		physicalForLogical[first] = physicalForLogical[second];
		physicalForLogical[second] = physical;
        for(unsigned i=0;i<WSM_MAX_MICE;++i) {
            if(mice[i].player==first)mice[i].player=second;
            else if(mice[i].player==second)mice[i].player=first;
        }
		if( mousePlayer == first ) mousePlayer = second;
		else if( mousePlayer == second ) mousePlayer = first;
		if( keyboardPlayer == first ) keyboardPlayer = second;
		else if( keyboardPlayer == second ) keyboardPlayer = first;
		pendingSwapFirst = pendingSwapSecond = -1;
		usbRoutingLogged = false;
	}
	for( int player = 0; player < MAX_CONTROLS; ++player )
	{
		controller[player] = physicalController[physicalForLogical[player]];
		controller[player].SetChannel( player );
	}
	Recovery::SetCheckpoint( 0x11A, "preparing optional USB reports" );

	usbKeyboardButtonsDown = 0;
	if( usbMouseInitialized )
	{
		Recovery::SetCheckpoint( 0x113, "servicing USB input reports" );
		wiredUsbMouse.Update( usbStartupAllowed, Settings::usbInputEnabled );

		u16 key = WiredUsbMouse::KeyNone;
		while( wiredUsbMouse.ReadKey( key ) )
		{
			switch( key )
			{
			case WiredUsbMouse::KeyEnter:
				usbKeyboardButtonsDown |= WPAD_BUTTON_A; break;
			case WiredUsbMouse::KeyEscape:
				usbKeyboardButtonsDown |= WPAD_BUTTON_B; break;
			case WiredUsbMouse::KeyHome:
				usbKeyboardButtonsDown |= WPAD_BUTTON_HOME; break;
			case WiredUsbMouse::KeyUp:
				usbKeyboardButtonsDown |= WPAD_BUTTON_UP; break;
			case WiredUsbMouse::KeyDown:
				usbKeyboardButtonsDown |= WPAD_BUTTON_DOWN; break;
			case WiredUsbMouse::KeyLeft:
				usbKeyboardButtonsDown |= WPAD_BUTTON_LEFT; break;
			case WiredUsbMouse::KeyRight:
				usbKeyboardButtonsDown |= WPAD_BUTTON_RIGHT; break;
			case WiredUsbMouse::KeyTab:
				usbKeyboardButtonsDown |= WPAD_BUTTON_PLUS; break;
			case 'f': case 'F':
				usbKeyboardButtonsDown |= WPAD_BUTTON_1; break;
			case 'r': case 'R':
				usbKeyboardButtonsDown |= WPAD_BUTTON_2; break;
			default: break;
			}
			const u8 next = ( usbKeyWrite + 1 ) & 31;
			if( next == usbKeyRead ) break;
			usbKeyQueue[ usbKeyWrite ] = key;
			usbKeyWrite = next;
		}
	}
	else
	{
		usbMouseConnected = false;

		keyboardPlayer = -1;
	}

	UpdateMice();

	const bool usbKeyboardConnected = usbMouseInitialized
		&& Settings::usbInputEnabled && wiredUsbMouse.IsKeyboardConnected();
	Recovery::SetCheckpoint( 0x11C, "assigning logical controller slots" );
	if( keyboardPlayer >= 0 && keyboardPlayer < MAX_CONTROLS
		&& ( controller[ keyboardPlayer ].IsWiiRemoteConnected()
			|| controller[ keyboardPlayer ].IsGameCubeConnected() ) )
		keyboardPlayer = -1;
	if( !usbKeyboardConnected ) keyboardPlayer = -1;

	// Assign a valid keyboard immediately and keep both halves of a combo
	// receiver on the same logical player.
	if( keyboardPlayer < 0 && usbKeyboardConnected )
	{
		const int available = mousePlayer >= 0 ? mousePlayer : ClaimNextPlayer();
		keyboardPlayer = available;
	}
	if( keyboardPlayer < 0 && mousePlayer >= 0 && usbKeyboardConnected )
		keyboardPlayer = mousePlayer;
	if( !usbMouseConnected && !usbKeyboardConnected ) usbRoutingLogged = false;
	if( !usbRoutingLogged && ( usbMouseConnected || usbKeyboardConnected ) )
	{
		u32 occupiedMask = 0;
		for( int player = 0; player < MAX_CONTROLS; ++player )
			if( controller[ player ].IsWiiRemoteConnected()
				|| controller[ player ].IsGameCubeConnected() )
				occupiedMask |= 1u << player;
		wiredUsbMouse.LogRouting( mousePlayer, keyboardPlayer, occupiedMask );
		usbRoutingLogged = true;
	}
	if( usbKeyboardConnected && keyboardPlayer >= 0
		&& keyboardPlayer < MAX_CONTROLS )
		controller[ keyboardPlayer ].ApplyUsbKeyboard(
			usbKeyboardButtonsDown );
	// Finish the WPAD scan and copy reports before starting our mouse worker.
	// Shared USB can already be initialized by BTE; do not reset it here.
	if( !usbMouseInitialized && usbStartupAllowed && Settings::usbInputEnabled
		&& usbStartupFrames >= 180 )
	{
		Recovery::SetCheckpoint( 0x115, "initializing serialized USB HID" );
		ClearButtonsDown();
		usbMouseInitialized = wiredUsbMouse.Init();
		usbStartupFrames = 0;
		// Flush the new diagnostic header immediately and service a device-change
		// completion if IOS already delivered one during initialization.
		if( usbMouseInitialized ) wiredUsbMouse.Update( true );
	}

	// The click used to finish a swap must not activate another control under
	// the new player number. Positions/held state remain intact.
	if( swapped ) ClearButtonsDown();
	Recovery::SetCheckpoint( 0x11F, "input update complete" );
}


void CInputs::UpdateMice()
{
    Recovery::SetCheckpoint(0x11B,"routing independent USB mice");
    usbMouseConnected=false;mousePlayer=-1;
    // Release disconnected/conflicting assignments first, so no mouse can
    // inherit another device's buttons or overwrite a newly paired remote.
    for(unsigned i=0;i<WSM_MAX_MICE;++i) {
        MouseRoute &m=mice[i];
        if(m.player>=0 && (controller[m.player].IsWiiRemoteConnected() ||
           controller[m.player].IsGameCubeConnected()))m.player=-1;
    }
    for(unsigned i=0;i<WSM_MAX_MICE;++i) {
        MouseRoute &m=mice[i];int dx=0,dy=0,wheel=0;u8 buttons=0;
        // Always consume accumulated motion, even while disabled. Suppress only
        // UI routing; never deinitialize the shared Bluetooth/USB host.
        const bool detected=usbMouseInitialized && wiredUsbMouse.ReadState(dx,dy,wheel,buttons,i);
        const bool online=detected && Settings::usbInputEnabled;
        if(!online) {
            m.connected=m.armed=false;m.buttons=0;m.player=-1;
            continue;
        }
        usbMouseConnected=true;
        if(!m.connected)m.armed=false;
        m.connected=true;
        u8 down=0,held=0;
        if(!m.armed) {
            if(wiredUsbMouse.HasMouseStateReport(i) && !buttons)m.armed=true;
        } else {down=buttons & ~m.buttons;held=buttons;}
        m.buttons=buttons;
        if(m.player<0)m.player=ClaimNextPlayer();
        const float scale=Settings::mouseSpeed/100.f;
        m.x=LIMIT(m.x+dx*scale,0.f,(float)screenwidth);
        m.y=LIMIT(m.y+dy*scale,0.f,(float)screenheight);
        if(m.player<0)continue; // Four occupied slots: no stealing or sharing.
        if(mousePlayer<0)mousePlayer=m.player;
        controller[m.player].ApplyUsbMouse(m.x,m.y,down,held,wheel);
    }
}

bool CInputs::PopUsbKey( u16 &key )
{
	key = WiredUsbMouse::KeyNone;
	if( usbKeyRead == usbKeyWrite ) return false;
	key = usbKeyQueue[ usbKeyRead ];
	usbKeyRead = ( usbKeyRead + 1 ) & 31;
	return true;
}

bool CInputs::SwapLogicalPlayers( int first, int second )
{
	if( first < 0 || first >= MAX_CONTROLS || second < 0
		|| second >= MAX_CONTROLS || first == second )
	{
		return false;
	}

	if( pendingSwapFirst >= 0 ) return false;
	// Do not mutate reports while HOME is iterating over them. Update applies
	// the permutation atomically at the next frame boundary.
	pendingSwapFirst = first;
	pendingSwapSecond = second;
	return true;
}

int CInputs::ClaimNextPlayer()
{
	bool used[ MAX_CONTROLS ] = { false, false, false, false };
    for(unsigned i=0;i<WSM_MAX_MICE;++i) {
        if(mice[i].player>=0 && mice[i].player<MAX_CONTROLS)used[mice[i].player]=true;
    }
	if( keyboardPlayer >= 0 && keyboardPlayer < MAX_CONTROLS )
		used[ keyboardPlayer ] = true;
	for( int player = 0; player < MAX_CONTROLS; ++player )
		if( controller[ player ].IsWiiRemoteConnected()
			|| controller[ player ].IsGameCubeConnected() )
			used[ player ] = true;
	for( int player = 0; player < MAX_CONTROLS; ++player )
		if( !used[ player ] ) return player;
	return -1;
}

void CInputs::ClearButtonsDown()
{
	for( int i = 0; i < MAX_CONTROLS; ++i ) controller[ i ].ClearButtonsDown();
}

s8 CInputs::WPAD_Stick( WPADData &data, u8 right, int axis )
{
	float magnitude = 0.0f;
	float angle = 0.0f;
	switch( data.exp.type )
	{
	case WPAD_EXP_NUNCHUK:
	case WPAD_EXP_GUITARHERO3:
		if( !right ) { magnitude = data.exp.nunchuk.js.mag; angle = data.exp.nunchuk.js.ang; }
		break;
	case WPAD_EXP_CLASSIC:
		if( !right ) { magnitude = data.exp.classic.ljs.mag; angle = data.exp.classic.ljs.ang; }
		else { magnitude = data.exp.classic.rjs.mag; angle = data.exp.classic.rjs.ang; }
		break;
	default:
		break;
	}
	if( magnitude > 1.0f ) magnitude = 1.0f;
	else if( magnitude < -1.0f ) magnitude = -1.0f;
	const float value = axis == 0
		? magnitude * sinf( PI * angle / 180.0f )
		: magnitude * cosf( PI * angle / 180.0f );
	return (s8)( value * 128.0f );
}
