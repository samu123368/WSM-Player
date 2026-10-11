#ifndef WSM_PLAYER_RECOVERY_H
#define WSM_PLAYER_RECOVERY_H

#include <gctypes.h>

namespace Recovery
{
	// IRQ/Bluetooth callbacks only latch requests; frame/recovery loops execute
	// the console action. Register hardware buttons before lengthy boot work.
	void InitializeConsoleButtons();
	void PollConsoleButtons();
	void Initialize();
	void Shutdown();
	void Heartbeat();
	void SetCheckpoint( u32 code, const char *details );
	void Pause();
	void Resume();
}

#endif
