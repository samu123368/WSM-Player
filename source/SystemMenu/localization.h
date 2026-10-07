#ifndef WSM_LOCALIZATION_H
#define WSM_LOCALIZATION_H

#include "utils/char16.h"

namespace Localization
{
	enum { Portuguese = 10, Swedish = 11, LanguageCount = 46 };
	int CurrentLanguage();
	int NativeLanguage();
	const char *LanguageCode();
	const char *LanguageName( int language );
	enum Key
	{
		ReturnToLoader,
		SettingsTitle,
		HomeButtonAction,
		BackToLoader,
		HomeMenu,
		HideHomebrewApps,
		LaunchOnStart,
		Yes,
		No,
		Music,
		Back,
		SettingsSaveFailed,
		MarioKartChannel,
		KeyCount
	};

	// Return a UTF-16 string for the configured Wii language.  Unknown
	// languages and missing keys deliberately fall back to English.
	const char16 *Get( Key key );
	const char16 *Get( Key key, int language );
	// Translate immediate-mode UTF-8 labels used by the WSM/System Settings
	// pages. Unknown strings (including user-entered content) pass through.
	const char *GetUtf8( const char *english );
	const char *GetUtf8( const char *english, int language );
	// Stable storage for archive-backed UI textboxes (never a temporary buffer).
	const char16 *GetText( const char *english );
	// Exact authored-text matches only; NULL leaves unknown channel copy intact.
	const char16 *TranslateChannelText( const char16 *authored );
	const char16 *GetMenuMessage( unsigned int index );
}

#endif // WSM_LOCALIZATION_H
