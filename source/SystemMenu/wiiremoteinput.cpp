#include "wiiremoteinput.h"

#include <cstring>

namespace
{
	bool initialized = false;
	bool connected[ 4 ] = { false, false, false, false };
	u32 expansion[ 4 ] = { WPAD_EXP_NONE, WPAD_EXP_NONE,
		WPAD_EXP_NONE, WPAD_EXP_NONE };
}

void WiiRemoteInput::Initialize( int virtualWidth, int virtualHeight )
{
	if( initialized ) return;
	WPAD_Init();
	WPAD_SetDataFormat( WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR );
	WPAD_SetVRes( WPAD_CHAN_ALL, virtualWidth, virtualHeight );
	initialized = true;
}

void WiiRemoteInput::Scan()
{
	if( !initialized ) return;
	WPAD_ScanPads();
	// The unused libogc channel buffers can retain err == WPAD_ERR_NONE, which
	// made WSM report four connected remotes and left no logical slot for USB.
	// WPAD_Probe is the public connection-status API; use it for ownership and
	// remember the expansion type for the field-level report snapshot below.
	for( int channel = WPAD_CHAN_0; channel <= WPAD_CHAN_3; ++channel )
	{
		expansion[ channel ] = WPAD_EXP_NONE;
		connected[ channel ] = WPAD_Probe( channel,
			&expansion[ channel ] ) == WPAD_ERR_NONE;
	}
}

bool WiiRemoteInput::CopyReport( int channel, WPADData &report )
{
	memset( &report, 0, sizeof( report ) );
	report.err = WPAD_ERR_NO_CONTROLLER;
	if( !initialized || channel < WPAD_CHAN_0 || channel > WPAD_CHAN_3 )
		return false;
	if( !connected[ channel ] ) return false;

	// Do not memcpy libogc's complete callback-owned WPADData buffer here.
	// Bluetooth can publish the next report while WSM is copying that large
	// structure, which produced a real-hardware lock while a USB mouse and Wii
	// Remote were active together. Take only the fields WSM actually consumes
	// through libogc's public snapshot accessors.
	report.err = WPAD_ERR_NONE;
	report.battery_level = WPAD_BatteryLevel( channel );
	report.btns_d = WPAD_ButtonsDown( channel );
	report.btns_h = WPAD_ButtonsHeld( channel );
	report.btns_u = WPAD_ButtonsUp( channel );
	WPAD_IR( channel, &report.ir );
	if( expansion[ channel ] != WPAD_EXP_NONE )
		WPAD_Expansion( channel, &report.exp );
	else
		report.exp.type = WPAD_EXP_NONE;
	return true;
}
