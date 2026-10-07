#ifndef WSM_PLAYER_RECOVERY_H
#define WSM_PLAYER_RECOVERY_H

#include <gctypes.h>

namespace Recovery
{
	void Initialize();
	void Shutdown();
	void Heartbeat();
	void SetCheckpoint( u32 code, const char *details );
	void Pause();
	void Resume();
}

#endif
