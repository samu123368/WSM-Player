#ifndef WSM_UI_FADE_H
#define WSM_UI_FADE_H

namespace UiFade
{
// Shared by Wii System Settings and one-press console shutdown. Shutdown
// uses the same fade, but does not inherit the settings loading-screen hold.
const int SystemSettingsFrames = 24;
inline unsigned SystemSettingsAlpha( int frame )
{
	if( frame <= 0 ) return 0;
	if( frame >= SystemSettingsFrames ) return 255;
	return (unsigned)( frame * 255 / SystemSettingsFrames );
}
}
#endif
