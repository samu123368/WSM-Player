// Original WSM Player update-restart lifecycle helper. GPL-2.0-only.
#ifndef WSM_UPDATE_RESTART_H
#define WSM_UPDATE_RESTART_H

namespace UpdateRestart
{
	bool Requested(int argc, char *argv[]);
	// Fresh-process startup only, BEFORE WPAD/MOUSE/USB owners are created.
	// Returns the first frontend error, or zero after both clients were released.
	int ReleaseUsbSession();
}
#endif
