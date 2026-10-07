#ifndef WSM_CONTENTLAUNCHER_H
#define WSM_CONTENTLAUNCHER_H

#include <gctypes.h>

#include <string>
#include <vector>

namespace ContentLauncher
{
	// Errors owned by this helper.  libogc WII_* errors are returned unchanged
	// by LaunchInstalledTitle().
	enum Error
	{
		InvalidArgument = -0xa101,
		ExecutableNotFound = -0xa102,
		FileIoError = -0xa103,
		ExecutableTooLarge = -0xa104,
		InvalidExecutable = -0xa105,
		ArgumentsTooLarge = -0xa106,
		BooterReturned = -0xa107
	};

	// Both launch functions only return when launch setup fails.  A successful
	// launch transfers control away from WSM Player.
	s32 LaunchInstalledTitle( u64 titleId );

	// Looks for boot.dol first, then boot.elf.  argv[0] is always the resolved
	// executable path; the overload appends the supplied meta.xml arguments.
	s32 LaunchHomebrewDirectory( const char *directory );
	s32 LaunchHomebrewDirectory( const char *directory,
		const std::vector< std::string > &arguments );
}

#endif
