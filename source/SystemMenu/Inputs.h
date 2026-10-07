#ifndef INPUTS_H_
#define INPUTS_H_

#include <gccore.h>
#include <ogc/lwp.h>
#include <wiiuse/wpad.h>
#include "wiredusbmouse.h"

#define MAX_CONTROLS 4

class Controller
{
private:
	WPADData wpad;
	u8 chan;
	float posX, posY, angle;
	u32 lastActivity;
	u16 pad_btns_d, pad_btns_h;
	u32 usb_mouse_btns_d, usb_mouse_btns_h;
	u32 usb_keyboard_btns_d;
	int usb_mouse_wheel;
	bool wpadConnected, padConnected, usbMouseConnected,
		usbKeyboardConnected, taken;
	bool nunchukPointerActive;
	bool pButton( u32 pad, u32 remote ) const
		{ return ( pad_btns_d & pad ) || ( wpad.btns_d & remote )
			|| ( usb_mouse_btns_d & remote )
			|| ( usb_keyboard_btns_d & remote ); }
	bool hButton( u32 pad, u32 remote ) const
		{ return ( pad_btns_h & pad ) || ( wpad.btns_h & remote )
			|| ( usb_mouse_btns_h & remote ); }
public:
	Controller( int chan = 0 );
	void SetChannel( int c ) { chan = c; }
	int GetChan() const { return chan; }
	void Update( int physicalChannel, bool gcConnected );
	void ApplyUsbMouse( float x, float y, u8 buttonsDown,
		u8 buttonsHeld, int wheel );
	void ApplyUsbKeyboard( u32 buttonsDown );
	void ClearButtonsDown();
	void SuppressUiInput();
	// Inspector-only draw snapshot; action buttons remain suppressed. The next
	// normal Update replaces these fields with fresh IR/mouse data.
	void RestoreInspectionPointer(float x, float y, float angle)
	{
		wpad.ir.valid=1;wpad.ir.x=x;wpad.ir.y=y;wpad.ir.angle=angle;
	}
	void Take( bool take = true ) { taken = take; }
	bool IsTaken() const { return taken; }
	bool pUp() const { return pButton( PAD_BUTTON_UP, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP ); }
	bool pDown() const { return pButton( PAD_BUTTON_DOWN, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN ); }
	bool pLeft() const { return pButton( PAD_BUTTON_LEFT, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT ); }
	bool pRight() const { return pButton( PAD_BUTTON_RIGHT, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT ); }
	bool pA() const { return pButton( PAD_BUTTON_A, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A ); }
	bool pB() const { return pButton( PAD_BUTTON_B, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B ); }
	bool pPlus() const { return pButton( PAD_TRIGGER_R, WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_PLUS ); }
	bool pMinus() const { return pButton( PAD_TRIGGER_L, WPAD_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_MINUS ); }
	bool pHome() const { return pButton( PAD_BUTTON_START, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME ); }
	bool pOne() const { return pButton( 0, WPAD_BUTTON_1 ); }
	bool pTwo() const { return pButton( 0, WPAD_BUTTON_2 ); }
	bool hUp() const { return hButton( PAD_BUTTON_UP, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP ); }
	bool hDown() const { return hButton( PAD_BUTTON_DOWN, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN ); }
	bool hLeft() const { return hButton( PAD_BUTTON_LEFT, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT ); }
	bool hRight() const { return hButton( PAD_BUTTON_RIGHT, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT ); }
	bool hA() const { return hButton( PAD_BUTTON_A, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A ); }
	bool hB() const { return hButton( PAD_BUTTON_B, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B ); }
	bool hPlus() const { return hButton( PAD_TRIGGER_R, WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_PLUS ); }
	bool hMinus() const { return hButton( PAD_TRIGGER_L, WPAD_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_MINUS ); }
	bool hHome() const { return hButton( PAD_BUTTON_START, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME ); }
	bool hOne() const { return hButton( 0, WPAD_BUTTON_1 ); }
	bool hTwo() const { return hButton( 0, WPAD_BUTTON_2 ); }
	const WPADData &GetData() const { return wpad; }
	bool IsWiiRemoteConnected() const { return wpadConnected; }
	bool IsWiiRemoteReady() const { return wpad.err == WPAD_ERR_NONE; }
	bool IsGameCubeConnected() const { return padConnected; }
	bool IsUsbMouseConnected() const { return usbMouseConnected; }
	bool IsUsbKeyboardConnected() const { return usbKeyboardConnected; }
	int UsbMouseWheel() const { return usbMouseConnected ? usb_mouse_wheel : 0; }
	u8 WiiRemoteBatteryRaw() const { return IsWiiRemoteReady() ? wpad.battery_level : 0; }
	u8 WiiRemoteExpansion() const { return IsWiiRemoteReady() ? wpad.exp.type : WPAD_EXP_NONE; }
};

class CInputs
{
public:
	static CInputs *Instance() { if( !instance ) instance = new CInputs(); return instance; }
	static void DestroyInstance()
	{
		delete instance;
		instance = NULL;
	}
	static s8 WPAD_Stick( WPADData &wpad, u8 right, int axis );
	Controller &GetController( int chan ) { return controller[ chan ]; }
	void Update();
	void AllowUsbStartup( bool allow = true ) { usbStartupAllowed = allow; }
	void ClearButtonsDown();
	bool IsUsbMouseConnected() const { return usbMouseConnected; }
	bool IsUsbKeyboardConnected() const
		{ return wiredUsbMouse.IsKeyboardConnected(); }
	int GetMousePlayer() const { return mousePlayer; }
	bool SwapLogicalPlayers( int first, int second );
	bool PopUsbKey( u16 &key );
private:
	CInputs();
	~CInputs();
	static CInputs *instance;
	Controller controller[ MAX_CONTROLS ];
	Controller physicalController[ MAX_CONTROLS ];
	int physicalForLogical[ MAX_CONTROLS ];
	int pendingSwapFirst;
	int pendingSwapSecond;
	int mousePlayer;
	int keyboardPlayer;
	u16 usbStartupFrames;
	bool usbStartupAllowed;
	bool usbMouseInitialized;
	bool usbMouseConnected;
	struct MouseRoute { int player; bool connected,armed; float x,y; u8 buttons; };
	MouseRoute mice[WSM_MAX_MICE];
	void UpdateMice();
	u32 usbKeyboardButtonsDown;
	bool usbRoutingLogged;
	WiredUsbMouse wiredUsbMouse;
	u16 usbKeyQueue[ 32 ];
	u8 usbKeyRead;
	u8 usbKeyWrite;
	int ClaimNextPlayer();
};

#define Pad(x) ( CInputs::Instance()->GetController( x ) )

#endif
