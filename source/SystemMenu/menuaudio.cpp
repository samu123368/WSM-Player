#include "menuaudio.h"

#include <cstdio>

#include "SoundOperations/gui_sound.h"
#include "settings.h"

MenuAudio *MenuAudio::instance = NULL;

MenuAudio *MenuAudio::Instance()
{
	if( !instance )
		instance = new MenuAudio;
	return instance;
}

MenuAudio::MenuAudio()
	: music( NULL )
{
}

MenuAudio::~MenuAudio()
{
	Reset();
}

bool MenuAudio::LocateAsset( const char *name, std::string &path ) const
{
	if( !name || !name[ 0 ] )
		return false;

	const std::string candidates[] =
	{
		Settings::applicationPath + name,
		std::string( "sd:/apps/wsmplayer/" ) + name,
		std::string( "sd:/apps/wsm-player-renderer-test/" ) + name
	};

	for( u32 i = 0; i < sizeof( candidates ) / sizeof( candidates[ 0 ] ); ++i )
	{
		FILE *file = fopen( candidates[ i ].c_str(), "rb" );
		if( !file )
			continue;
		fclose( file );
		path = candidates[ i ];
		return true;
	}
	return false;
}

bool MenuAudio::EnsureMusic()
{
	if( music )
		return music->IsLoaded();

	std::string path;
	if( !LocateAsset( "wii_menu_bgm.wav", path ) )
		return false;

	music = new GuiSound( path.c_str(), Settings::musicVolume );
	if( !music || !music->IsLoaded() )
	{
		delete music;
		music = NULL;
		return false;
	}
	music->SetLoop( 1 );
	return true;
}

void MenuAudio::StartMusic()
{
	if( !Settings::menuMusicEnabled )
	{
		PauseMusic();
		return;
	}
	if( !EnsureMusic() )
		return;
	Settings::RegisterMusicSound( music );
	if( !music->IsPlaying() )
		music->Resume();
}

void MenuAudio::PauseMusic()
{
	if( !music )
		return;
	Settings::UnregisterMusicSound( music );
	if( music->IsPlaying() )
		music->Pause();
}

void MenuAudio::ApplySettings()
{
	if( Settings::menuMusicEnabled )
		StartMusic();
	else
		PauseMusic();
}

void MenuAudio::Reset()
{
	if( music )
		Settings::UnregisterMusicSound( music );
	delete music;
	music = NULL;
}
