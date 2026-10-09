// Original WSM Player update-restart lifecycle helper. GPL-2.0-only.
#include "updaterestart.h"
#include <string.h>
#include <ogc/ios.h>
#include <ogc/ipc.h>

namespace UpdateRestart
{
	bool Requested(int argc, char *argv[])
	{
		if(!argv) return false;
		for(int i = 1; i < argc; ++i)
			if(argv[i] && !strcmp(argv[i], "--wsm-update-restart")) return true;
		return false;
	}

	int ReleaseUsbSession()
	{
		if(IOS_GetVersion() != 58) return 0;
		// Same V5 client-shutdown request used by libogc USB_Deinitialize.
		// Opening synchronous, temporary clients avoids creating SDK async
		// notification buffers just to destroy them. No input worker is alive yet.
		// Do not reload IOS: that would lose the loader's NAND/AHB permissions.
		const int getVersion = 0, shutdown = 2;
		const char *paths[] = { "/dev/usb/hid", "/dev/usb/ven" };
		int firstError = 0;
		for(unsigned i = 0; i < 2; ++i)
		{
			const int fd = IOS_Open(paths[i], IPC_OPEN_NONE);
			if(fd < 0) { if(!firstError) firstError = fd; continue; }
			u32 version[8] ATTRIBUTE_ALIGN(32) = {};
			int result = IOS_Ioctl(fd, getVersion, NULL, 0, version, sizeof(version));
			if(result >= 0 && version[0] == 0x50001)
				result = IOS_Ioctl(fd, shutdown, NULL, 0, NULL, 0);
			else if(result >= 0) result = IPC_EINVAL;
			const int closed = IOS_Close(fd);
			if(result < 0 && !firstError) firstError = result;
			if(closed < 0 && !firstError) firstError = closed;
		}
		return firstError;
	}
}
