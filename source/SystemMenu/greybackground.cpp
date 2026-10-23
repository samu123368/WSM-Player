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
#include "bmg.h"
#include "greybackground.h"
#include "dateformat.h"
#include "localization.h"
#include "settings.h"
#include "Textbox.h"

GreyBackground::GreyBackground()
	: layout( NULL ),
	  obj( new Object )
{
	dateTextBuffer[ 0 ] = 0;
}

GreyBackground::~GreyBackground()
{
	delete obj;
	delete layout;

	std::map< std::string, Animation *>::iterator it = brlans.begin(), itE = brlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
}

void GreyBackground::SetDate( u8 weekDay, u8 dayOfTheMonth, u8 month )
{
	switch( weekDay )
	{
	case 0: weekDay = 116;break;// the bmg has all the days of the week in order except sunday, which is at the end
	default: weekDay += 109;break;
	}
	char16 *str = dateTextBuffer;

	// get weekday text
	int len = strlcpy16( str, Bmg::Instance()->GetMessage( weekDay ), 64 );

	// Display preference only; leave the Wii's actual date/time unchanged.
	char numeric[24];
	DateFormat::Format(numeric, sizeof(numeric), Settings::dateOrder,
		month, dayOfTheMonth, Localization::MonthFirstDate());
	snprintf16( str + len, 64 - len, " %s", numeric );
	SetText( layout, "T_Day_b", str );
	if(layout) if(Textbox *text = layout->FindTextbox("T_Day_b")) text->SetUniformTextFit(true);
}

bool GreyBackground::Load( const u8* boardAshData, u32 boardAshSize )
{
	if( loaded )
	{
		return true;
	}

	if( !boardAshData || !boardAshSize )
	{
		return false;
	}
	U8Archive boardArc( boardAshData, boardAshSize );

	if( !( layout = LoadLayout( boardArc, "my_IplTop_c" ) ) )
	{
		return false;
	}


	Animation *my_IplTop_c = LoadAnimation( boardArc, "my_IplTop_c" );
	if( !my_IplTop_c )
	{
		return false;
	}
	layout->LoadBrlanTpls( my_IplTop_c, boardArc );
	brlans[ "my_IplTop_c" ] = my_IplTop_c;
	obj->BindPane( layout->FindPane( "RootPane" ) );
	obj->AddAnimation( my_IplTop_c );
	obj->SetAnimation( "my_IplTop_c" );
	//obj->Start();

	SetText( layout, "T_Day_b", dateTextBuffer );

	loaded = true;

	return true;
}

void GreyBackground::RenderBackdrop( Mtx &modelview, const Vec2f &ScreenProps, bool widescreen )
{
	// Do not expose a stationary date below the menu snapshot as it slides up.
	// Include the offscreen native date copies in case a theme moves them.
	const char *names[] = {"T_Day_a", "T_Day_b", "T_Day_c"};
	Pane *dates[3] = {NULL,NULL,NULL};
	bool hidden[3] = {false,false,false};
	for( int n = 0; n < 3; ++n )
	{
		dates[n] = layout ? layout->FindPane(names[n]) : NULL;
		if( dates[n] ) { hidden[n] = dates[n]->GetHide(); dates[n]->SetHide(true); }
	}
	Render(modelview,ScreenProps,widescreen);
	for( int n = 0; n < 3; ++n )
		if( dates[n] ) dates[n]->SetHide(hidden[n]);
}

void GreyBackground::Render( Mtx &modelview, const Vec2f &ScreenProps, bool widescreen )
{
	if( !loaded )
	{
		return;
	}
	layout->Render( modelview, ScreenProps, widescreen );
	obj->Advance();
}
