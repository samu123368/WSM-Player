#include <stdio.h>
#include <unistd.h>
#include <gccore.h>
#include <ogc/context.h>
#include <ogc/lwp.h>
#include <ogc/stm.h>
#include <wiiuse/wpad.h>

#include "recovery.h"
#include "video.h"

extern "C"
{
	void __exception_sethandler( u32 exception,
		void ( *handler )( frame_context *frame ) );
}

namespace
{
	volatile u32 heartbeatSequence = 0;
	volatile u32 checkpointCode = 0;
	const char *volatile checkpointDetails = "starting";
	volatile bool watchdogEnabled = false;
	volatile bool watchdogPaused = true;
	volatile bool recoveryActive = false;
	volatile bool exceptionPending = false;
	volatile bool resetRequested = false;
	bool resetCallbackInstalled = false;
	resetcallback previousResetCallback = NULL;
	lwp_t watchdogThread = LWP_THREAD_NULL;
	lwp_t mainThread = LWP_THREAD_NULL;
	u8 watchdogStack[ 32768 ] ATTRIBUTE_ALIGN( 32 );

	struct ExceptionSnapshot
	{
		u32 exception;
		u32 srr0;
		u32 lr;
		u32 dar;
		u32 srr1;
		lwp_t thread;
	};
	ExceptionSnapshot exceptionSnapshot;

	void WsmResetCallback( u32, void * )
	{
		// System callbacks must remain signal-only.  The main/recovery loop owns
		// video and IOS reset calls.
		resetRequested = true;
	}

	const char *ExceptionName( u32 exception )
	{
		switch( exception )
		{
		case EX_DSI: return "Data Storage Interrupt (DSI)";
		case EX_ISI: return "Instruction Storage Interrupt (ISI)";
		case EX_ALIGN: return "Alignment Exception";
		case EX_PRG: return "Program Exception";
		default: return "CPU Exception";
		}
	}

	void RestartFromRecovery()
	{
		ShowRecoveryFramebuffer( "Restarting WSM Player...", "", "" );
		VIDEO_WaitVSync();
		// A hardware warm reboot is safe even after a DSI. With WSM Player's
		// intended Priiloader/forwarder setup it re-enters the app from its start.
		SYS_ResetSystem( SYS_RESTART, 0, 0 );
		usleep( 500000 );
		// SYS_ResetSystem normally does not return. Keep STM as a fallback for IOS
		// environments which reject the first reset request after an exception.
		STM_RebootSystem();
		for( ;; ) __asm__ volatile( "sync" );
	}

	bool RecoveryAButtonPressed()
	{
		const u32 remoteA = WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A;
		for( int channel = 0; channel < 4; ++channel )
		{
			// Read the event queue directly. Calling WPAD_ScanPads here can enter
			// the same path in which the suspended UI thread was watchdog-stalled.
			WPADData event;
			while( WPAD_ReadEvent( channel, &event ) >= WPAD_ERR_NONE )
				if( event.btns_h & remoteA ) return true;
		}

		const u32 gc = PAD_ScanPads();
		for( int channel = 0; channel < 4; ++channel )
			if( ( gc & ( PAD_CHAN0_BIT >> channel ) )
				&& ( ( PAD_ButtonsDown( channel ) | PAD_ButtonsHeld( channel ) )
					& PAD_BUTTON_A ) )
				return true;
		return false;
	}

	void WaitForRestartButton()
	{
		for( ;; )
		{
			if( resetRequested || SYS_ResetButtonDown()
				|| RecoveryAButtonPressed() )
				RestartFromRecovery();
			VIDEO_WaitVSync();
		}
	}

	void ShowRecovery( const char *code, const char *summary,
		const char *details )
	{
		if( recoveryActive )
		{
			STM_RebootSystem();
			for( ;; ) __asm__ volatile( "sync" );
		}
		recoveryActive = true;
		watchdogPaused = true;

		const lwp_t self = LWP_GetSelf();
		// The exception trampoline has already suspended the faulting thread. Do
		// not ask libogc to suspend that same LWP a second time.
		if( mainThread != LWP_THREAD_NULL && mainThread != self
			&& ( !exceptionPending || mainThread != exceptionSnapshot.thread ) )
			LWP_SuspendThread( mainThread );

		ShowRecoveryFramebuffer( code, summary, details );
		VIDEO_WaitVSync();
		WaitForRestartButton();
	}

	extern "C" void WsmExceptionHandler( frame_context *frame )
	{
		// A CPU exception is not a safe place to format strings, touch GX, scan
		// WPAD or manipulate unrelated LWPs. The old handler did all four and a
		// second fault inside it fell through to libogc's ordinary DSI dump. Only
		// capture fixed-size register values here; the dedicated recovery thread
		// owns every UI and input operation.
		if( recoveryActive || exceptionPending || LWP_GetSelf() == watchdogThread )
		{
			STM_RebootSystem();
			for( ;; ) __asm__ volatile( "sync" );
		}
		exceptionSnapshot.exception = frame ? frame->EXCPT_Number : 0xffffffff;
		exceptionSnapshot.srr0 = frame ? frame->SRR0 : 0;
		exceptionSnapshot.lr = frame ? frame->LR : 0;
		exceptionSnapshot.dar = frame ? frame->DAR : 0;
		exceptionSnapshot.srr1 = frame ? frame->SRR1 : 0;
		exceptionSnapshot.thread = LWP_GetSelf();
		__sync_synchronize();
		exceptionPending = true;

		// Park the damaged LWP permanently. Suspending it yields immediately to
		// the high-priority recovery monitor, which presents the blue screen.
		if( watchdogThread != LWP_THREAD_NULL )
			LWP_SuspendThread( exceptionSnapshot.thread );
		for( ;; ) LWP_YieldThread();
	}

	void InstallExceptionHandlers()
	{
		// Reassert these lightweight table entries from the monitor as well as at
		// startup. No later subsystem or IOS frontend can silently leave WSM Player
		// without its recovery handlers for more than one monitor tick.
		__exception_sethandler( EX_DSI, WsmExceptionHandler );
		__exception_sethandler( EX_ISI, WsmExceptionHandler );
		__exception_sethandler( EX_ALIGN, WsmExceptionHandler );
		__exception_sethandler( EX_PRG, WsmExceptionHandler );
	}

	void *WatchdogMain( void * )
	{
		u32 previous = heartbeatSequence;
		u32 stalledChecks = 0;
		u32 monitorTicks = 0;
		while( watchdogEnabled )
		{
			usleep( 100000 );
			if( exceptionPending )
			{
				__sync_synchronize();
				static char code[ 24 ];
				static char summary[ 96 ];
				static char details[ 320 ];
				snprintf( code, sizeof( code ), "Error: WSM-E%02lu",
					(unsigned long)exceptionSnapshot.exception );
				snprintf( summary, sizeof( summary ), "%s",
					ExceptionName( exceptionSnapshot.exception ) );
				snprintf( details, sizeof( details ),
					"Checkpoint %03lX: %s\nSRR0 %08lX   LR %08lX\nDAR  %08lX   SRR1 %08lX",
					(unsigned long)checkpointCode,
					checkpointDetails ? checkpointDetails : "unknown",
					(unsigned long)exceptionSnapshot.srr0,
					(unsigned long)exceptionSnapshot.lr,
					(unsigned long)exceptionSnapshot.dar,
					(unsigned long)exceptionSnapshot.srr1 );
				ShowRecovery( code, summary, details );
			}
			// Heartbeat timeout accounting remains at 500 ms while exception
			// delivery is checked every 100 ms.
			if( ++monitorTicks < 5 )
				continue;
			monitorTicks = 0;
			if( watchdogPaused )
			{
				previous = heartbeatSequence;
				stalledChecks = 0;
				continue;
			}
			const u32 current = heartbeatSequence;
			if( current != previous )
			{
				previous = current;
				stalledChecks = 0;
				continue;
			}
			if( ++stalledChecks < 24 )
				continue;

			static char code[ 24 ];
			static char details[ 256 ];
			snprintf( code, sizeof( code ), "Error: WSM-H%03lX",
				(unsigned long)( checkpointCode & 0xfff ) );
			snprintf( details, sizeof( details ),
				"The UI produced no completed frame for 12 seconds.\n"
				"Last checkpoint %03lX: %s",
				(unsigned long)checkpointCode,
				checkpointDetails ? checkpointDetails : "unknown" );
			ShowRecovery( code, "WSM Player stopped responding.", details );
		}
		return NULL;
	}
}

void Recovery::Initialize()
{
	if( watchdogEnabled ) return;
	mainThread = LWP_GetSelf();
	heartbeatSequence = 1;
	checkpointCode = 1;
	checkpointDetails = "entering menu";
	recoveryActive = false;
	exceptionPending = false;
	resetRequested = false;
	watchdogEnabled = true;
	watchdogPaused = false;
	if( LWP_CreateThread( &watchdogThread, WatchdogMain, NULL, watchdogStack,
		sizeof( watchdogStack ), 40 ) < 0 )
	{
		watchdogThread = LWP_THREAD_NULL;
		watchdogEnabled = false;
	}
	InstallExceptionHandlers();
	previousResetCallback = SYS_SetResetCallback( WsmResetCallback );
	resetCallbackInstalled = true;
}

void Recovery::Shutdown()
{
	watchdogPaused = true;
	watchdogEnabled = false;
	if( watchdogThread != LWP_THREAD_NULL )
	{
		LWP_JoinThread( watchdogThread, NULL );
		watchdogThread = LWP_THREAD_NULL;
	}
	if( resetCallbackInstalled )
	{
		SYS_SetResetCallback( previousResetCallback );
		previousResetCallback = NULL;
		resetCallbackInstalled = false;
	}
}

void Recovery::Heartbeat()
{
	// Exception-handler table updates are process-global.  Reinstall from the
	// UI thread at each completed frame rather than racing WPAD/IOS from the
	// watchdog LWP.  The watchdog remains dedicated to observing progress and
	// presenting recovery UI.
	InstallExceptionHandlers();
	++heartbeatSequence;
	if( resetRequested || SYS_ResetButtonDown() ) RestartFromRecovery();
}

void Recovery::SetCheckpoint( u32 code, const char *details )
{
	checkpointDetails = details ? details : "unknown";
	__sync_synchronize();
	checkpointCode = code;
}

void Recovery::Pause() { watchdogPaused = true; }

void Recovery::Resume()
{
	++heartbeatSequence;
	watchdogPaused = false;
}
