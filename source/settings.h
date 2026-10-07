/*
Copyright (c) 2012 - Dimok and giantpune

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
#ifndef SETTINGS_H
#define SETTINGS_H

#include "tinyxml/tinyxml.h"

class GuiSound;

namespace Settings
{

// read preview banners (.bnr / .app / .wad) from sd card
extern bool useDumpedBanners;

// show banners of channels on the nand
extern bool useNandBanners;

// load sd:/app/... homebrew and create banners
extern bool useHomebrewForBanners;

// user-facing WSM Player options
extern bool directHomeExit;
// Opt-in because launching replaces the preview process and can expose broken
// or unsupported titles.  Off preserves the original safe preview behavior.
extern bool launchOnStart;
extern int musicVolume;
// This switch only affects WSM Player's System Menu ambience.
// Banner/channel audio remains controlled by Music volume.
extern bool menuMusicEnabled;

// Standard IOS58 HID input.  This includes wired devices and 2.4 GHz receiver
// sets which expose mouse/keyboard report descriptors.
extern bool usbInputEnabled;
// Relative mouse speed in percent.  200 matches WSM Player's established 2x
// desktop-mouse motion; the setting is bounded to 50..400.
extern int mouseSpeed;

// False uses the Wii Menu's original 12-hour clock. True shows 00-23 and
// hides the AM/PM marker.
extern bool clock24Hour;
// -1 follows the console; other values are WSM UI language IDs (not SYSCONF).
extern int uiLanguage;
// Developer camera is opt-in for this session only; never activate it on boot.
extern bool freeCameraEnabled;

// Registering the current music stream lets live slider changes affect it
// without introducing interface sound effects.
void RegisterMusicSound( GuiSound *sound );
void UnregisterMusicSound( GuiSound *sound );
void ApplyMusicVolume();

// try to load the banner from DVD for the disc channel
extern bool mountDVD;

// path to read banners from on SD card.  default is SD:/banners/
extern std::string sdBannerPath;


// path to read resource file.  if this is empty, use the one in the system menu.
// otherwise, treat this as a file like "sd:/mySystemMenuResources.app"
extern std::string resourcePath;

// User-selected theme package. This is kept separate from the low-level
// resource override so the Themes screen can remember a disabled theme.
extern std::string customThemePath;

// Directory containing the running boot.dol, including its trailing slash.
// Companion assets such as the NAND-rendered System Menu audio live beside it.
extern std::string applicationPath;
// Explicit public signed manifest URL (HTTP), or an sd:/ manifest for offline use.
// Empty until a release feed is supplied; never stores GitHub credentials.
extern std::string updateManifestUrl;

// Runtime-only recovery mode.  It never overwrites the user's saved choices;
// it simply disables optional inputs for the current boot.
extern bool safeModeActive;
void ApplySafeModeOverrides();


// save and load
void Load( int argc, char *argv[] );
bool Save();
}

#endif // SETTINGS_H
