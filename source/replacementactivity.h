#ifndef WSM_PLAYER_REPLACEMENT_ACTIVITY_H
#define WSM_PLAYER_REPLACEMENT_ACTIVITY_H

#include <gctypes.h>
#include <ctime>
#include <string>
#include <vector>

namespace ReplacementActivity
{
	struct Record
	{
		std::string key;
		u32 launches;
		u32 seconds;
		time_t lastLaunch;
		bool favorite;
	};

	// Finalizes a launch left pending by the previous WSM Player process.  The
	// pending marker is intentionally stored on SD, so Priiloader returning to
	// WSM Player can account for time spent in a channel or homebrew app.
	void Initialize();
	void BeginLaunch( const std::string &key );
	void CancelLaunch();
	bool ToggleFavorite( const std::string &key );
	bool IsFavorite( const std::string &key );
	bool GetRecord( const std::string &key, Record &record );
	std::vector< Record > Recent( u32 limit );
	std::vector< Record > Favorites();
}

#endif
