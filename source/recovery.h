#ifndef WSM_PLAYER_RECOVERY_H
#define WSM_PLAYER_RECOVERY_H

#include <gctypes.h>

namespace Recovery
{
	// IRQ/Bluetooth callbacks only latch requests; frame/recovery loops execute
	// the console action. Register at the established recovery initialization
	// point, after the input subsystem is ready.
	void InitializeConsoleButtons();
	void PollConsoleButtons();
	void LogConsoleAction( const char *event );
	void Initialize();
	void Shutdown();
	void Heartbeat();
	void SetCheckpoint( u32 code, const char *details );
	void Pause();
	void Resume();
}

#endif
