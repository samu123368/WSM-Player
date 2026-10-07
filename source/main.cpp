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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <ogc/ios.h>
#include <ogc/context.h>
#include <ogc/stm.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include "bannerlist.h"
#include "SoundOperations/audio.h"
#include "SoundOperations/gui_sound.h"
#include "utils/nandtitle.h"
#include "DirList.h"
#include "video.h"
#include "Banner.h"
#include "menuhandler.h"
#include "settings.h"
#include "SystemMenu/SystemFont.h"
#include "SystemMenu/BannerAsync.h"
#include "SystemMenu/dihandler.h"
#include "SystemMenu/Inputs.h"
#include "SoundOperations/SoundHandler.hpp"
#include "recovery.h"
#include "replacementactivity.h"

#include "utils/ash.h"
#include "U8Archive.h"
#include "SystemMenu/SystemMenuResources.h"

extern "C"
{
	extern s32 MagicPatches(s32);
	void __exception_setreload(int t);
}

namespace
{
	static const u32 LoaderStubAddress = 0x80001800;
	static const u32 LoaderStubBytes = 0x1800;
	u8 *savedLoaderStub = NULL;

	bool HasLoaderStub( const void *address )
	{
		const volatile u32 *words = (const volatile u32 *)address;
		return words[ 1 ] == 0x53545542 && words[ 2 ] == 0x48415858;
	}

	void PreserveLoaderStub()
	{
		if( savedLoaderStub || !HasLoaderStub( (const void *)LoaderStubAddress ) )
			return;
		savedLoaderStub = (u8 *)memalign( 32, LoaderStubBytes );
		if( savedLoaderStub )
			memcpy( savedLoaderStub, (const void *)LoaderStubAddress,
				LoaderStubBytes );
	}

	void RestoreLoaderStub()
	{
		if( !savedLoaderStub || !HasLoaderStub( savedLoaderStub ) ) return;
		memcpy( (void *)LoaderStubAddress, savedLoaderStub, LoaderStubBytes );
		DCFlushRange( (void *)LoaderStubAddress, LoaderStubBytes );
		ICInvalidateRange( (void *)LoaderStubAddress, LoaderStubBytes );
		__sync_synchronize();
	}

	void ReturnToLoader()
	{
		// Keep every libogc frontend alive and use its normal application-exit
		// path.  Calling __lwp_thread_stopmultitasking ourselves bypassed libogc's
		// registered handoff and DSI'd with some forwarder stubs on real hardware.
		RestoreLoaderStub();
		exit( 0 );
		for( ;; ) __asm__ volatile( "sync" );
	}

	void AppendRuntimeLog( const char *event )
	{
		const std::string path = Settings::applicationPath + "wsm-runtime.log";
		FILE *log = fopen( path.c_str(), "ab" );
		if( !log ) return;
		time_t now = time( NULL );
		fprintf( log, "%lu IOS%d r%d %s\n", (unsigned long)now,
			IOS_GetVersion(), IOS_GetRevision(), event ? event : "unknown" );
		fclose( log );
	}

	bool SafeModeFlagPresent()
	{
		const std::string path = Settings::applicationPath + "safe-mode.flag";
		FILE *flag = fopen( path.c_str(), "rb" );
		if( !flag ) return false;
		fclose( flag );
		return true;
	}

	bool SafeModeChordHeld()
	{
		// Give already-synchronised Wii Remotes and GC pads a short window to
		// request recovery without adding another boot screen.  B+Minus (or
		// B+L on a GC pad) avoids colliding with the Health-screen A press.
		for( int frame = 0; frame < 45; ++frame )
		{
			CInputs::Instance()->Update();
			for( int player = 0; player < MAX_CONTROLS; ++player )
				if( Pad( player ).hB() && Pad( player ).hMinus() ) return true;
			VIDEO_WaitVSync();
		}
		return false;
	}
}

extern "C" void __wrap___reload()
{
	// Never jump directly into a loader stub from an exception.  At that point
	// GX, WPAD or USB can still own live callbacks and the stub commonly leaves a
	// real Wii on a permanent black screen.  /dev/stm performs a hardware reboot
	// without depending on those application subsystems.
	STM_RebootSystem();
	const u64 started = gettime();
	while( diff_msec( started, gettime() ) < 2000 )
		__asm__ volatile( "sync" );
	// This is reached only if STM rejected the request.  Do not force the System
	// Menu so Priiloader can still apply the user's configured autoboot target.
	SYS_ResetSystem( SYS_RESTART, 0, 0 );
	for( ;; ) __asm__ volatile( "sync" );
}

int	main( int argc, char *argv[] )
{
	__exception_setreload(5);
	// The launching homebrew loader leaves its return stub in low memory. Some
	// IOS, disc and USB operations also use low-memory scratch space, so preserve
	// the complete stub before any subsystem has a chance to touch it.
	PreserveLoaderStub();

	// good enough.  we're not using rand for any 1337 crpto stuff
	srand( time( NULL ) );

	InitGecko();
	InitVideo();
	// USB keyboards and mice use Nintendo's complete IOS58 VEN/HID stack. IOS
	// selection belongs to the Homebrew Channel/forwarder: reloading IOS after GX
	// has started can strand a real Wii on a green/black screen. If another IOS
	// launches WSM Player, the menu remains usable and USB input simply stays off.
	const int entryIos = IOS_GetVersion();

	// load sd before settings
	const bool sdMounted = fatMount("sd", &__io_wiisd, 0, 32, 64);
	Settings::Load( argc, argv );
	if( sdMounted ) ReplacementActivity::Initialize();
	if( sdMounted )
	{
		char bootEvent[ 64 ];
		snprintf( bootEvent, sizeof( bootEvent ),
			"boot entry-ios=%d usb-runtime=%s", entryIos,
			entryIos == 58 ? "ios58" : "disabled" );
		AppendRuntimeLog( bootEvent );
	}
	// WPAD and the standard USB input libraries share libogc's host owner.
	// No raw HID/VEN handles are reserved before Bluetooth initialization.
	CInputs::Instance();
	// Install the global CPU exception handlers as soon as video and controller
	// recovery are usable. Keep hang detection paused only for the bounded boot
	// initialization below; DSI/ISI/alignment/program monitoring remains active.
	Recovery::Initialize();
	Recovery::Pause();
	Recovery::SetCheckpoint( 0x000, "initializing WSM Player services" );
	if( SafeModeFlagPresent() || SafeModeChordHeld() )
	{
		Settings::ApplySafeModeOverrides();
		AppendRuntimeLog( "safe-mode" );
	}
	InitAudio();
	MagicPatches(1);
	ISFS_Initialize();
	NandTitles.Get();


	if( !SystemMenuResources::Instance()->Init() )
	{
		gprintf( "error initializing system menu resources.  exiting\n" );
		AppendRuntimeLog( "fatal: system-menu-resources" );
		exit( 0 );
	}

	Recovery::SetCheckpoint( 0x001, "starting WSM Player menu" );
	Recovery::Resume();
	AppendRuntimeLog( "menu-start" );
	MenuHandler::Instance()->Start();
	Recovery::Shutdown();
	AppendRuntimeLog( "return-begin" );

	// Do not tear down WPAD, USB, DI, ISFS, audio, or background workers here.
	// libogc's exit path stops multitasking and transfers to the loader atomically.
	AppendRuntimeLog( ( savedLoaderStub
		&& HasLoaderStub( savedLoaderStub ) )
		? "return-ready-stub" : "return-ready-no-stub" );
	ReturnToLoader();
}
