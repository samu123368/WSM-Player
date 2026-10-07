#ifndef WIIREMOTEINPUT_H_
#define WIIREMOTEINPUT_H_

#include <wiiuse/wpad.h>

// The Wii Remote backend deliberately lives outside Inputs.cpp.  USB HID code
// must not initialize, reconfigure, or scan WPAD; this module is the sole owner
// of the normal libogc WPAD startup/scan/report path.
namespace WiiRemoteInput
{
	void Initialize( int virtualWidth, int virtualHeight );
	void Scan();
	bool CopyReport( int channel, WPADData &report );
}

#endif
