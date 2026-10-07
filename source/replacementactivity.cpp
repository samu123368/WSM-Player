#include "replacementactivity.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "settings.h"

namespace
{
	const off_t MaxActivityBytes = 512 * 1024;
	const u32 MaxRecords = 512;
	const time_t MaxCreditedSession = 12 * 60 * 60;
	std::vector< ReplacementActivity::Record > records;
	std::string pendingKey;
	time_t pendingStart = 0;

	std::string ActivityPath()
	{
		return Settings::applicationPath + "wsm-activity-v1.tsv";
	}

	std::string PendingPath()
	{
		return Settings::applicationPath + "wsm-launch-pending.txt";
	}

	std::string CleanKey( const std::string &key )
	{
		std::string clean = key.substr( 0, 1024 );
		for( size_t i = 0; i < clean.size(); ++i )
			if( clean[ i ] == '\t' || clean[ i ] == '\r' || clean[ i ] == '\n' )
				clean[ i ] = ' ';
		return clean;
	}

	ReplacementActivity::Record *Find( const std::string &key )
	{
		for( size_t i = 0; i < records.size(); ++i )
			if( records[ i ].key == key ) return &records[ i ];
		return NULL;
	}

	ReplacementActivity::Record *FindOrAdd( const std::string &key )
	{
		ReplacementActivity::Record *record = Find( key );
		if( record ) return record;
		if( key.empty() || records.size() >= MaxRecords ) return NULL;
		ReplacementActivity::Record added;
		added.key = key;
		added.launches = 0;
		added.seconds = 0;
		added.lastLaunch = 0;
		added.favorite = false;
		records.push_back( added );
		return &records.back();
	}

	bool ReplaceFile( const std::string &path, const std::string &temporary )
	{
		const std::string backup = path + ".bak";
		remove( backup.c_str() );
		const bool hadPrevious = rename( path.c_str(), backup.c_str() ) == 0;
		if( rename( temporary.c_str(), path.c_str() ) != 0 )
		{
			if( hadPrevious ) rename( backup.c_str(), path.c_str() );
			remove( temporary.c_str() );
			return false;
		}
		remove( backup.c_str() );
		return true;
	}

	bool SaveRecords()
	{
		mkdir( Settings::applicationPath.c_str(), 0777 );
		const std::string path = ActivityPath();
		const std::string temporary = path + ".tmp";
		FILE *file = fopen( temporary.c_str(), "wb" );
		if( !file ) return false;
		fprintf( file, "WSM-ACTIVITY\t1\n" );
		for( size_t i = 0; i < records.size(); ++i )
		{
			const ReplacementActivity::Record &record = records[ i ];
			fprintf( file, "%s\t%lu\t%lu\t%lu\t%u\n", record.key.c_str(),
				(unsigned long)record.launches,
				(unsigned long)record.seconds,
				(unsigned long)record.lastLaunch,
				record.favorite ? 1u : 0u );
		}
		const bool flushed = fflush( file ) == 0;
		const bool closed = fclose( file ) == 0;
		const bool complete = flushed && closed;
		if( !complete )
		{
			remove( temporary.c_str() );
			return false;
		}
		return ReplaceFile( path, temporary );
	}

	void LoadRecords()
	{
		records.clear();
		struct stat info;
		const std::string path = ActivityPath();
		if( stat( path.c_str(), &info ) != 0 || info.st_size < 0
			|| info.st_size > MaxActivityBytes ) return;
		FILE *file = fopen( path.c_str(), "rb" );
		if( !file ) return;
		char line[ 1400 ];
		if( !fgets( line, sizeof( line ), file )
			|| strncmp( line, "WSM-ACTIVITY\t1", 14 ) )
		{
			fclose( file );
			return;
		}
		while( records.size() < MaxRecords && fgets( line, sizeof( line ), file ) )
		{
			char *save = NULL;
			char *key = strtok_r( line, "\t\r\n", &save );
			char *launches = strtok_r( NULL, "\t\r\n", &save );
			char *seconds = strtok_r( NULL, "\t\r\n", &save );
			char *last = strtok_r( NULL, "\t\r\n", &save );
			char *favorite = strtok_r( NULL, "\t\r\n", &save );
			if( !key || !launches || !seconds || !last || !favorite ) continue;
			ReplacementActivity::Record record;
			record.key = CleanKey( key );
			record.launches = strtoul( launches, NULL, 10 );
			record.seconds = strtoul( seconds, NULL, 10 );
			record.lastLaunch = (time_t)strtoul( last, NULL, 10 );
			record.favorite = strtoul( favorite, NULL, 10 ) != 0;
			if( !record.key.empty() ) records.push_back( record );
		}
		fclose( file );
	}

	void RemovePending()
	{
		remove( PendingPath().c_str() );
		pendingKey.clear();
		pendingStart = 0;
	}

	void FinalizePending()
	{
		FILE *file = fopen( PendingPath().c_str(), "rb" );
		if( !file ) return;
		char line[ 1200 ];
		if( fgets( line, sizeof( line ), file ) )
		{
			char *tab = strchr( line, '\t' );
			if( tab )
			{
				*tab++ = '\0';
				char *end = tab + strlen( tab );
				while( end > tab && ( end[ -1 ] == '\r' || end[ -1 ] == '\n' ) )
					*--end = '\0';
				const time_t started = (time_t)strtoul( line, NULL, 10 );
				const time_t now = time( NULL );
				ReplacementActivity::Record *record = FindOrAdd( CleanKey( tab ) );
				if( record && now >= started )
				{
					time_t elapsed = now - started;
					if( elapsed > MaxCreditedSession ) elapsed = MaxCreditedSession;
					record->seconds += (u32)elapsed;
				}
			}
		}
		fclose( file );
		RemovePending();
		SaveRecords();
	}

	bool RecentFirst( const ReplacementActivity::Record &a,
		const ReplacementActivity::Record &b )
	{
		return a.lastLaunch > b.lastLaunch;
	}

	bool FavoriteFirst( const ReplacementActivity::Record &a,
		const ReplacementActivity::Record &b )
	{
		if( a.lastLaunch != b.lastLaunch ) return a.lastLaunch > b.lastLaunch;
		return a.key < b.key;
	}
}

void ReplacementActivity::Initialize()
{
	LoadRecords();
	FinalizePending();
}

void ReplacementActivity::BeginLaunch( const std::string &rawKey )
{
	const std::string key = CleanKey( rawKey );
	Record *record = FindOrAdd( key );
	if( !record ) return;
	pendingKey = key;
	pendingStart = time( NULL );
	++record->launches;
	record->lastLaunch = pendingStart;
	SaveRecords();
	const std::string path = PendingPath();
	const std::string temporary = path + ".tmp";
	FILE *file = fopen( temporary.c_str(), "wb" );
	if( !file ) return;
	fprintf( file, "%lu\t%s\n", (unsigned long)pendingStart,
		pendingKey.c_str() );
	const bool flushed = fflush( file ) == 0;
	const bool closed = fclose( file ) == 0;
	const bool complete = flushed && closed;
	if( complete ) ReplaceFile( path, temporary );
	else remove( temporary.c_str() );
}

void ReplacementActivity::CancelLaunch()
{
	if( !pendingKey.empty() )
	{
		Record *record = Find( pendingKey );
		if( record && record->lastLaunch == pendingStart && record->launches )
			--record->launches;
	}
	RemovePending();
	SaveRecords();
}

bool ReplacementActivity::ToggleFavorite( const std::string &rawKey )
{
	Record *record = FindOrAdd( CleanKey( rawKey ) );
	if( !record ) return false;
	record->favorite = !record->favorite;
	SaveRecords();
	return record->favorite;
}

bool ReplacementActivity::IsFavorite( const std::string &rawKey )
{
	Record *record = Find( CleanKey( rawKey ) );
	return record && record->favorite;
}

bool ReplacementActivity::GetRecord( const std::string &rawKey, Record &record )
{
	Record *found = Find( CleanKey( rawKey ) );
	if( !found ) return false;
	record = *found;
	return true;
}

std::vector< ReplacementActivity::Record > ReplacementActivity::Recent( u32 limit )
{
	std::vector< Record > result;
	for( size_t i = 0; i < records.size(); ++i )
		if( records[ i ].lastLaunch ) result.push_back( records[ i ] );
	std::sort( result.begin(), result.end(), RecentFirst );
	if( result.size() > limit ) result.resize( limit );
	return result;
}

std::vector< ReplacementActivity::Record > ReplacementActivity::Favorites()
{
	std::vector< Record > result;
	for( size_t i = 0; i < records.size(); ++i )
		if( records[ i ].favorite ) result.push_back( records[ i ] );
	std::sort( result.begin(), result.end(), FavoriteFirst );
	return result;
}
