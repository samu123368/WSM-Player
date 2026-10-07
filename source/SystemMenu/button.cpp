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
#include "button.h"
#include "menuaudio.h"

Button::Button()
	: state( St_Idle ),
	  trigger( Btn_None ),
	  enabled( true ),
	  fastClickAnimation( true )
{
}

void Button::SetIdleAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	idleAnim.anim = anim;
	idleAnim.start = start;
	idleAnim.end = end;
	idleAnim.loop = loop;
	idleAnim.forwardAndReverse = forwardAndReverse;
}

void Button::SetMouseOverAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	mouseOverAnim.anim = anim;
	mouseOverAnim.start = start;
	mouseOverAnim.end = end;
	mouseOverAnim.loop = loop;
	mouseOverAnim.forwardAndReverse = forwardAndReverse;
}

void Button::SetMouseOutAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	mouseOutAnim.anim = anim;
	mouseOutAnim.start = start;
	mouseOutAnim.end = end;
	mouseOutAnim.loop = loop;
	mouseOutAnim.forwardAndReverse = forwardAndReverse;
}

void Button::SetClickAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	clickAnim.anim = anim;
	clickAnim.start = start;
	clickAnim.end = end;
	clickAnim.loop = loop;
	clickAnim.forwardAndReverse = forwardAndReverse;
}

void Button::SetHeldAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	heldAnim.anim = anim;
	heldAnim.start = start;
	heldAnim.end = end;
	heldAnim.loop = loop;
	heldAnim.forwardAndReverse = forwardAndReverse;
}

void Button::SetReleaseAnimation( Animation *anim, FrameNumber start, FrameNumber end, bool loop, bool forwardAndReverse )
{
	releaseAnim.anim = anim;
	releaseAnim.start = start;
	releaseAnim.end = end;
	releaseAnim.loop = loop;
	releaseAnim.forwardAndReverse = forwardAndReverse;
}

void Button::AnimateNowOrLater( const std::string &brlan, FrameNumber start, FrameNumber end, FrameNumber wait, bool loop, bool forwardAndReversee )
{
	if( 0 )
	{
		ScheduleAnimation( brlan, start, end, wait, loop, forwardAndReversee );
	}
	else
	{
		SetAnimation( brlan, start, end, wait, loop, forwardAndReversee );
	}
}

void Button::SetEnabled( bool enable )
{
	enabled = enable;
	if( idleAnim.anim && enable )
	{
		ScheduleAnimation( idleAnim.anim->GetName(), idleAnim.loop ? idleAnim.start : idleAnim.end - 1,
						   idleAnim.end, -1.f, idleAnim.loop, idleAnim.forwardAndReverse );
	}
}

void Button::Update()
{
	State newState = St_Idle;
	int chosenController = -1;
	int pointerOrder[ 4 ];
	int pointerCount = 0;

	// A USB mouse is an independent player, but it is also the most deliberate
	// pointer for desktop-style GUI use.  Inspect it first for hover selection;
	// action edges from every controller are checked before any hover wins.
	for( int i = 0; i < 4; ++i )
		if( Pad( i ).IsUsbMouseConnected() )
			pointerOrder[ pointerCount++ ] = i;
	for( int i = 0; i < 4; ++i )
		if( !Pad( i ).IsUsbMouseConnected() )
			pointerOrder[ pointerCount++ ] = i;

	// First pass: an actual press/hold always beats a second controller whose
	// pointer happens to be resting over the same button.
	for( int pass = 0; pass < pointerCount && enabled; ++pass )
	{
		const int i = pointerOrder[ pass ];
		const Controller &pad = Pad( i );
		if( pad.IsTaken() )
			continue;
		const WPADData &wpad = pad.GetData();
		if( !wpad.ir.valid || !Contains( wpad.ir.x, wpad.ir.y ) )
			continue;

		State action = St_Over;
		switch( trigger )
		{
		case Btn_Up:    action = pad.pUp() ? St_Clicked : pad.hUp() ? St_Held : St_Over; break;
		case Btn_Down:  action = pad.pDown() ? St_Clicked : pad.hDown() ? St_Held : St_Over; break;
		case Btn_Left:  action = pad.pLeft() ? St_Clicked : pad.hLeft() ? St_Held : St_Over; break;
		case Btn_Right: action = pad.pRight() ? St_Clicked : pad.hRight() ? St_Held : St_Over; break;
		case Btn_A:     action = pad.pA() ? St_Clicked : St_Over; break;
		case Btn_B:     action = pad.pB() ? St_Clicked : pad.hB() ? St_Held : St_Over; break;
		case Btn_Plus:  action = pad.pPlus() ? St_Clicked : pad.hPlus() ? St_Held : St_Over; break;
		case Btn_Minus: action = pad.pMinus() ? St_Clicked : pad.hMinus() ? St_Held : St_Over; break;
		case Btn_Home:  action = pad.pHome() ? St_Clicked : pad.hHome() ? St_Held : St_Over; break;
		default: break;
		}
		if( action == St_Clicked || action == St_Held )
		{
			newState = action;
			chosenController = i;
			break;
		}
	}

	// Second pass: no action occurred, so use the first pointer in the preferred
	// order for hover feedback. Hover alone must not consume a controller; doing
	// so made overlapping and later-rendered controls inaccessible to a mouse.
	if( chosenController < 0 )
	{
		for( int pass = 0; pass < pointerCount && enabled; ++pass )
		{
			const int i = pointerOrder[ pass ];
			const Controller &pad = Pad( i );
			if( pad.IsTaken() )
				continue;
			const WPADData &wpad = pad.GetData();
			if( wpad.ir.valid && Contains( wpad.ir.x, wpad.ir.y ) )
			{
				newState = St_Over;
				break;
			}
		}
	}
	else
		Pad( chosenController ).Take();

	// nothing to do
	if( newState == state )
	{
		return;
	}
	//gprintf( "newstate: %u oldstate: %u\n", newState, state );

	// handle new state
	switch( newState )
	{
	case St_Idle:
	{
		switch( state )
		{
		case St_Clicked:
		case St_Over:
		{
			if( mouseOutAnim.anim )
			{
				AnimateNowOrLater( mouseOutAnim.anim->GetName(), mouseOutAnim.start, mouseOutAnim.end, -1.f, false );
			}
			MouseOut();
		}
		break;
		// dunno how it would get to idle state from anything except over
		default:
			break;
		}

		// set idle animation to play after the mouseout one is done
		if( idleAnim.anim )
		{
			AnimateNowOrLater( idleAnim.anim->GetName(), idleAnim.loop ? idleAnim.start : idleAnim.end - 1,
							   idleAnim.end, -1.f, idleAnim.loop, idleAnim.forwardAndReverse );
		}

		// set state to idle
		state = St_Idle;
	}
	break;
	case St_Over:
	{
		bool scheduleIdle = false;
		switch( state )
		{
		case St_Idle:
		{
			// play mouseover animation
			if( mouseOverAnim.anim )
			{
				AnimateNowOrLater( mouseOverAnim.anim->GetName(), mouseOverAnim.start, mouseOverAnim.end, -1.f, false );
			}
			MouseOver();
		}
		break;
		case St_Held:
		{
			// play released animation
			if( releaseAnim.anim )
			{
				AnimateNowOrLater( releaseAnim.anim->GetName(), releaseAnim.start, releaseAnim.end, -1.f, false );
			}
			// set over animation next
			if( mouseOverAnim.anim )
			{
				ScheduleAnimation( mouseOverAnim.anim->GetName(), mouseOverAnim.loop ? mouseOverAnim.start : mouseOverAnim.end - 1,
								   mouseOverAnim.end, -1.f, mouseOverAnim.loop, mouseOverAnim.forwardAndReverse );
			}
			else
			{
				scheduleIdle = true;
			}
			Released();
		}
		break;
		default:
			break;
		}

		if( scheduleIdle && idleAnim.anim )// no mouseover animation, just use the idle one
		{
			AnimateNowOrLater( idleAnim.anim->GetName(), idleAnim.loop ? idleAnim.start : idleAnim.end - 1,
							   idleAnim.end, -1.f, idleAnim.loop, idleAnim.forwardAndReverse );
		}

		// set state to over
		state = St_Over;
	}
	break;
	case St_Held:
	{
		switch( state )
		{
		case St_Over:
		{
			// play held animation
			if( heldAnim.anim )
			{
				AnimateNowOrLater( heldAnim.anim->GetName(), heldAnim.start, heldAnim.end, -1.f, false );
			}
			Held();
		}
		break;
		default:// i guess it can only get to held state from over state
			break;
		}
		state = St_Held;
	}
	break;
	case St_Clicked:
	{
		switch( state )
		{
		case St_Idle:
			// The pointer and A press can arrive in the same input update (notably
			// on real Wii remotes and Dolphin mouse input).  Requiring a previous
			// hover frame silently discarded that first, perfectly valid click.
			MouseOver();
			// fall through
		case St_Over:
		{
			// play clicked animation
			if( clickAnim.anim )
			{
				// The old code queued the click behind the currently running
				// hover/idle animation.  A looping or very long BRLAN then made
				// HOME/settings/banner buttons appear to react 10+ seconds late.
				// Start a short click flash now, but treat the UI action as
				// completed at the input edge instead of waiting on animation I/O.
				const FrameNumber normalEnd = clickAnim.end < 0.0f
					? clickAnim.anim->FrameCount() - 1.0f : clickAnim.end;
				const FrameNumber fastEnd = !fastClickAnimation ? normalEnd
					: clickAnim.end < 0.0f
					? MIN( clickAnim.start + 6.0f, clickAnim.anim->FrameCount() - 1.0f )
					: MIN( clickAnim.end, clickAnim.start + 6.0f );
				SetAnimation( clickAnim.anim->GetName(), clickAnim.start,
					fastEnd, -1.f, false );
				if( !fastClickAnimation )
				{
					if( mouseOverAnim.anim )
						ScheduleAnimation( mouseOverAnim.anim->GetName(),
							mouseOverAnim.end - 1.0f, mouseOverAnim.end,
							-1.0f, mouseOverAnim.loop,
							mouseOverAnim.forwardAndReverse );
					else if( idleAnim.anim )
						ScheduleAnimation( idleAnim.anim->GetName(),
							idleAnim.end - 1.0f, idleAnim.end,
							-1.0f, idleAnim.loop,
							idleAnim.forwardAndReverse );
				}
				Start();
			}
			Clicked();
			Finished();
			state = St_Over;
			return;
		}
		break;
		default:// i guess it can only get to held state from over state
			break;
		}
		// just leave state as whatever it already was before the click
	}
	break;
	default:
		break;
	}
	Start();
}
