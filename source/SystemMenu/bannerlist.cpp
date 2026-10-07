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
#include <algorithm>
#include <cctype>
#include <map>
#include <vector>
#include <sys/stat.h>

#include "bannerlist.h"
#include "contentbin.h"
#include "DirList.h"
#include "fileops.h"
#include "gecko.h"
#include "nandtitle.h"
#include "settings.h"

List< BannerListEntry * >bannerList;

namespace
{
	const char *const ChannelOrderPath = "sd:/config/wsm-player-channel-order.txt";
	const char *const ChannelOrderTempPath = "sd:/config/wsm-player-channel-order.tmp";
	const char *const ChannelOrderBackupPath = "sd:/config/wsm-player-channel-order.bak";
	const char *const SystemMenuOrderPath =
		"/title/00000001/00000002/data/iplsave.bin";
	const size_t MaxChannelOrderBytes = 128 * 1024;
	const size_t MaxChannelOrderKeyBytes = 511;
	const u32 MaxChannelOrderEntries = 1024;
	bool systemMenuRefreshRequested = false;
	bool savedOrderNeedsRewrite = false;
	const u32 SystemMenuSlotCount = 48;

	u64 ReadBigEndian64( const u8 *bytes );
	bool LoadSystemMenuSlotTitles( u64 slotTitle[ SystemMenuSlotCount ],
		u32 &visibleTitles );
	u64 ResolveSystemMenuSlotTitle( u64 slotTitle,
		const std::map< u64, BannerListEntry * > &installed );

	std::string NormalizeBannerPath( const std::string &path )
	{
		std::string normalized;
		normalized.reserve( path.size() );
		bool previousWasSlash = false;
		for( std::string::const_iterator it = path.begin(); it != path.end(); ++it )
		{
			unsigned char value = static_cast< unsigned char >( *it );
			if( value == '\\' )
				value = '/';
			if( value == '/' )
			{
				if( previousWasSlash )
					continue;
				previousWasSlash = true;
			}
			else
			{
				previousWasSlash = false;
				value = static_cast< unsigned char >( std::tolower( value ) );
			}
			normalized.push_back( static_cast< char >( value ) );
		}
		while( normalized.size() > 4 && normalized[ normalized.size() - 1 ] == '/' )
			normalized.erase( normalized.size() - 1 );
		return normalized;
	}

	bool IsValidOrderKey( const std::string &key )
	{
		if( key.size() > MaxChannelOrderKeyBytes )
			return false;
		if( key.compare( 0, 4, "tid:" ) == 0 )
		{
			if( key.size() != 20 )
				return false;
			for( size_t i = 4; i < key.size(); ++i )
			{
				if( !std::isxdigit( static_cast< unsigned char >( key[ i ] ) ) )
					return false;
			}
			return true;
		}
		if( key.size() >= 8 && key.size() <= 10
			&& key.compare( 0, 6, "empty:" ) == 0 )
		{
			for( size_t i = 6; i < key.size(); ++i )
				if( !std::isdigit( static_cast< unsigned char >( key[ i ] ) ) )
					return false;
			return true;
		}
		if( key.compare( 0, 5, "path:" ) != 0 || key.size() <= 5 )
			return false;
		for( size_t i = 5; i < key.size(); ++i )
		{
			if( static_cast< unsigned char >( key[ i ] ) < 0x20 )
				return false;
		}
		return true;
	}

	bool IsHomebrewOrderKey( const std::string &key )
	{
		return key.compare( 0, 14, "path:sd:/apps/" ) == 0;
	}

	bool AppendLoadedOrderKey( const std::string &key,
		std::vector< std::string > &keys, std::map< std::string, bool > &seen )
	{
		if( !IsValidOrderKey( key ) || seen.find( key ) != seen.end() )
			return false;
		seen[ key ] = true;
		keys.push_back( key );
		return true;
	}

	bool LoadSavedOrderKeys( std::vector< std::string > &keys )
	{
		keys.clear();
		FILE *order = fopen( ChannelOrderPath, "rb" );
		if( !order )
			order = fopen( ChannelOrderBackupPath, "rb" );
		if( !order )
			return false;

		std::map< std::string, bool > seen;
		std::string line;
		line.reserve( 128 );
		size_t bytesRead = 0;
		bool lineTooLong = false;
		int value = EOF;
		while( keys.size() < MaxChannelOrderEntries
			&& bytesRead < MaxChannelOrderBytes
			&& ( value = fgetc( order ) ) != EOF )
		{
			++bytesRead;
			if( value == '\n' )
			{
				if( !lineTooLong )
				{
					if( !line.empty() && line[ line.size() - 1 ] == '\r' )
						line.erase( line.size() - 1 );
					AppendLoadedOrderKey( line, keys, seen );
				}
				line.clear();
				lineTooLong = false;
			}
			else if( line.size() < MaxChannelOrderKeyBytes )
			{
				line.push_back( static_cast< char >( value ) );
			}
			else
			{
				lineTooLong = true;
			}
		}

		const bool reachedLimit = bytesRead >= MaxChannelOrderBytes
			|| keys.size() >= MaxChannelOrderEntries;
		if( !reachedLimit && value == EOF && !lineTooLong )
		{
			if( !line.empty() && line[ line.size() - 1 ] == '\r' )
				line.erase( line.size() - 1 );
			AppendLoadedOrderKey( line, keys, seen );
		}
		fclose( order );
		if( reachedLimit )
		{
			gprintf( "Channel order: input capped at %u entries/%u bytes\n",
				(unsigned)MaxChannelOrderEntries, (unsigned)MaxChannelOrderBytes );
		}
		return true;
	}

	bool BuildOrderKeysForSave( std::vector< std::string > &keys )
	{
		keys.clear();

		std::map< std::string, bool > present;
		for( u32 i = 0; i < bannerList.size(); ++i )
		{
			if( !bannerList[ i ] )
				continue;
			const std::string key = BannerOrderKey( bannerList[ i ] );
			if( !IsValidOrderKey( key ) )
			{
				gprintf( "Channel order: refusing invalid/oversized key\n" );
				return false;
			}
			if( present.find( key ) != present.end() )
				continue;
			if( keys.size() >= MaxChannelOrderEntries )
			{
				gprintf( "Channel order: refusing to save more than %u entries\n",
					(unsigned)MaxChannelOrderEntries );
				return false;
			}
			present[ key ] = true;
			keys.push_back( key );
		}

		// When Homebrew apps are hidden, they are absent from bannerList. Merge
		// their saved keys back at their former ordinal positions so a visible
		// channel reorder cannot erase them or disturb their relative order.
		if( !Settings::useHomebrewForBanners )
		{
			std::vector< std::string > savedKeys;
			if( LoadSavedOrderKeys( savedKeys ) )
			{
				for( size_t i = 0; i < savedKeys.size(); ++i )
				{
					const std::string &key = savedKeys[ i ];
					if( !IsHomebrewOrderKey( key ) || present.find( key ) != present.end() )
						continue;
					if( keys.size() >= MaxChannelOrderEntries )
						return false;
					const size_t insertAt = std::min( i, keys.size() );
					keys.insert( keys.begin() + insertAt, key );
					present[ key ] = true;
				}
			}
		}

		size_t outputBytes = 0;
		for( size_t i = 0; i < keys.size(); ++i )
		{
			const size_t lineBytes = keys[ i ].size() + 1;
			if( lineBytes > MaxChannelOrderBytes - outputBytes )
			{
				gprintf( "Channel order: refusing output larger than %u bytes\n",
					(unsigned)MaxChannelOrderBytes );
				return false;
			}
			outputBytes += lineBytes;
		}
		return true;
	}

	bool SaveBannerOrder()
	{
		std::vector< std::string > keys;
		if( !BuildOrderKeysForSave( keys ) )
			return false;

		// mkdir is intentionally best-effort: EEXIST is the common result.
		mkdir( "sd:/config", 0777 );
		remove( ChannelOrderTempPath );

		FILE *order = fopen( ChannelOrderTempPath, "wb" );
		if( !order )
		{
			gprintf( "Channel order: cannot create %s\n", ChannelOrderTempPath );
			return false;
		}

		bool ok = true;
		for( size_t i = 0; i < keys.size(); ++i )
		{
			if( fprintf( order, "%s\n", keys[ i ].c_str() ) < 0 )
			{
				ok = false;
				break;
			}
		}
		if( fflush( order ) != 0 )
			ok = false;
		if( fclose( order ) != 0 )
			ok = false;
		if( !ok )
		{
			remove( ChannelOrderTempPath );
			gprintf( "Channel order: write failed; old order retained\n" );
			return false;
		}

		// Retain a recoverable old copy until the complete temporary file is
		// in place rather than ever writing a partial preference in-place.
		FILE *currentOrder = fopen( ChannelOrderPath, "rb" );
		if( currentOrder )
			fclose( currentOrder );
		else
			// Recover an interrupted previous replacement before rotating it.
			rename( ChannelOrderBackupPath, ChannelOrderPath );
		remove( ChannelOrderBackupPath );
		const bool hadOldOrder = rename( ChannelOrderPath, ChannelOrderBackupPath ) == 0;
		if( rename( ChannelOrderTempPath, ChannelOrderPath ) != 0 )
		{
			if( hadOldOrder )
				rename( ChannelOrderBackupPath, ChannelOrderPath );
			remove( ChannelOrderTempPath );
			gprintf( "Channel order: could not install new order file\n" );
			return false;
		}
		if( hadOldOrder )
			remove( ChannelOrderBackupPath );
		return true;
	}
}

std::string BannerOrderKey( const BannerListEntry *entry )
{
	if( !entry )
		return std::string();
	if( entry->emptySlot )
	{
		char emptyKey[ 16 ];
		snprintf( emptyKey, sizeof( emptyKey ), "empty:%04d",
			std::max( 0, std::min( 9999, entry->systemMenuSlot ) ) );
		return emptyKey;
	}
	if( entry->tid )
	{
		char titleKey[ 24 ];
		snprintf( titleKey, sizeof( titleKey ), "tid:%016llx",
			static_cast< unsigned long long >( entry->tid ) );
		return titleKey;
	}
	return std::string( "path:" ) + NormalizeBannerPath( entry->filepath );
}

void ApplySavedBannerOrder()
{
	savedOrderNeedsRewrite = false;
	if( bannerList.size() < 2 )
		return;

	std::vector< std::string > savedKeys;
	if( !LoadSavedOrderKeys( savedKeys ) || savedKeys.empty() )
		return;
	std::map< std::string, BannerListEntry * > available;
	std::map< u64, BannerListEntry * > installed;
	for( u32 i = 0; i < bannerList.size(); ++i )
		if( bannerList[ i ] && !bannerList[ i ]->emptySlot )
		{
			available[ BannerOrderKey( bannerList[ i ] ) ] = bannerList[ i ];
			if( bannerList[ i ]->tid )
				installed[ bannerList[ i ]->tid ] = bannerList[ i ];
		}

	u64 systemMenuTitles[ SystemMenuSlotCount ];
	u32 visibleSystemMenuTitles = 0;
	const bool haveSystemMenuSlots = LoadSystemMenuSlotTitles(
		systemMenuTitles, visibleSystemMenuTitles );

	List< BannerListEntry * > ordered;
	std::map< BannerListEntry *, bool > used;
	for( u32 i = 0; i < savedKeys.size(); ++i )
	{
		const std::string &key = savedKeys[ i ];
		if( key.compare( 0, 6, "empty:" ) == 0 )
		{
			const int systemSlot = atoi( key.c_str() + 6 );
			if( haveSystemMenuSlots && systemSlot > 0
				&& systemSlot < (int)SystemMenuSlotCount )
			{
				const u64 resolvedTitle = ResolveSystemMenuSlotTitle(
					systemMenuTitles[ systemSlot ], installed );
				const std::map< u64, BannerListEntry * >::iterator replacement =
					installed.find( resolvedTitle );
				if( resolvedTitle && replacement != installed.end()
					&& !used[ replacement->second ] )
				{
					ordered << replacement->second;
					used[ replacement->second ] = true;
					savedOrderNeedsRewrite = true;
					gprintf( "Channel order: restored title %016llx in former empty slot %d\n",
						static_cast< unsigned long long >( resolvedTitle ), systemSlot );
					continue;
				}
			}
			BannerListEntry *empty = new BannerListEntry( std::string() );
			empty->emptySlot = true;
			empty->systemMenuSlot = systemSlot;
			ordered << empty;
			continue;
		}
		const std::map< std::string, BannerListEntry * >::iterator found =
			available.find( key );
		if( found != available.end() && !used[ found->second ] )
		{
			ordered << found->second;
			used[ found->second ] = true;
		}
	}
	for( u32 i = 0; i < bannerList.size(); ++i )
		if( bannerList[ i ] && !bannerList[ i ]->emptySlot
			&& !used[ bannerList[ i ] ] )
			ordered << bannerList[ i ];
	bannerList.swap( ordered );
}

void RequestSystemMenuBannerRefresh()
{
	systemMenuRefreshRequested = true;
}

bool SystemMenuBannerRefreshPending()
{
	return systemMenuRefreshRequested;
}

namespace
{
u64 ReadBigEndian64( const u8 *bytes )
{
	u64 value = 0;
	for( int i = 0; i < 8; ++i )
		value = ( value << 8 ) | bytes[ i ];
	return value;
}

bool LoadSystemMenuSlotTitles( u64 slotTitle[ SystemMenuSlotCount ],
	u32 &visibleTitles )
{
	memset( slotTitle, 0, sizeof( u64 ) * SystemMenuSlotCount );
	visibleTitles = 0;
	u8 *data = NULL;
	u32 size = 0;
	const int result = NandTitles.LoadFileFromNand( SystemMenuOrderPath,
		&data, &size );
	const u32 headerBytes = 16;
	const u32 slotBytes = 16;
	if( result < 0 || !data
		|| size < headerBytes + slotBytes * SystemMenuSlotCount
		|| data[ 0 ] != 'R' || data[ 1 ] != 'I'
		|| data[ 2 ] != 'P' || data[ 3 ] != 'L' )
	{
		free( data );
		gprintf( "Channel order: System Menu iplsave unavailable (%d, %u bytes)\n",
			result, size );
		return false;
	}

	for( u32 slot = 0; slot < SystemMenuSlotCount; ++slot )
	{
		const u32 offset = headerBytes + slot * slotBytes;
		const u8 type = data[ offset ];
		const u64 titleId = ReadBigEndian64( data + offset + 8 );
		if( type != 0 && titleId != 0 )
		{
			slotTitle[ slot ] = titleId;
			++visibleTitles;
		}
	}
	free( data );
	return visibleTitles != 0;
}

u64 ResolveSystemMenuSlotTitle( u64 slotTitle,
	const std::map< u64, BannerListEntry * > &installed )
{
	// Photo Channel 1.1 keeps its visible banner in HAYA, while many System
	// Menu iplsave files continue to identify that slot as the original HAAA.
	// Prefer HAYA when installed, but retain HAAA for an un-updated Wii.
	const u64 PhotoChannel10 = 0x0001000248414141ull;
	const u64 PhotoChannel11 = 0x0001000248415941ull;
	if( slotTitle == PhotoChannel10
		&& installed.find( PhotoChannel11 ) != installed.end() )
		return PhotoChannel11;
	return slotTitle;
}
}

static bool ApplySystemMenuBannerOrder()
{
	// The System Menu stores four pages of twelve slots in iplsave.bin. Use its
	// title IDs directly so WSM shows the same visible channels in the same order;
	// the special Disc Channel slot has no title ID and remains WSM's fixed slot.
	u64 slotTitle[ SystemMenuSlotCount ];
	u32 visibleTitles = 0;
	if( !LoadSystemMenuSlotTitles( slotTitle, visibleTitles ) )
		return false;

	std::map< u64, BannerListEntry * > installed;
	List< BannerListEntry * > extras;
	for( u32 i = 0; i < bannerList.size(); ++i )
	{
		BannerListEntry *entry = bannerList[ i ];
		if( entry && entry->tid )
		{
			if( installed.find( entry->tid ) == installed.end() )
				installed[ entry->tid ] = entry;
			else
				delete entry;
		}
		else if( entry )
			extras << entry;
	}

	List< BannerListEntry * > ordered;
	// Slot zero is WSM's separately rendered Disc Channel. Every later IPL slot
	// maps directly to bannerList[slot - 1], including genuine empty positions.
	for( u32 slot = 1; slot < SystemMenuSlotCount; ++slot )
	{
		const u64 resolvedTitle = ResolveSystemMenuSlotTitle(
			slotTitle[ slot ], installed );
		const std::map< u64, BannerListEntry * >::iterator found =
			installed.find( resolvedTitle );
		if( resolvedTitle && found != installed.end() )
		{
			ordered << found->second;
			installed.erase( found );
		}
		else
		{
			BannerListEntry *empty = new BannerListEntry( std::string() );
			empty->emptySlot = true;
			empty->systemMenuSlot = slot;
			ordered << empty;
		}
	}
	for( std::map< u64, BannerListEntry * >::iterator it = installed.begin();
		it != installed.end(); ++it )
		delete it->second;
	ordered.Append( extras );
	bannerList.swap( ordered );
	gprintf( "Channel order: imported %u visible System Menu slots\n",
		(unsigned)visibleTitles );
	return true;
}

bool SwapBannerListEntries( u32 first, u32 second )
{
	if( first >= bannerList.size() || second >= MaxChannelOrderEntries )
		return false;
	if( !bannerList[ first ]
		|| ( second < bannerList.size() && !bannerList[ second ] ) )
		return false;
	const u32 originalSize = bannerList.size();
	int nextEmptyIdentity = 0;
	for( u32 i = 0; i < bannerList.size(); ++i )
		if( bannerList[ i ] && bannerList[ i ]->emptySlot )
			nextEmptyIdentity = std::max( nextEmptyIdentity,
				bannerList[ i ]->systemMenuSlot + 1 );
	while( bannerList.size() <= second )
	{
		if( nextEmptyIdentity > 9999 )
		{
			while( bannerList.size() > originalSize )
				delete bannerList.TakeLast();
			return false;
		}
		BannerListEntry *empty = new BannerListEntry( std::string() );
		empty->emptySlot = true;
		empty->systemMenuSlot = nextEmptyIdentity++;
		bannerList << empty;
	}
	if( first == second )
		return true;
	std::swap( bannerList[ first ], bannerList[ second ] );
	if( !SaveBannerOrder() )
	{
		std::swap( bannerList[ first ], bannerList[ second ] );
		while( bannerList.size() > originalSize )
			delete bannerList.TakeLast();
		gprintf( "Channel order: save failed; in-memory swap reverted\n" );
		return false;
	}
	return true;
}

static void AddNandBannerPaths( u32 whichOnes )
{
	for( u32 j = 0; j < 3; j++ )
	{
		if( ( ( j == 0 ) || ( j == 2 ) ) && !( whichOnes & NandUserChannels ) )
		{
			continue;
		}
		else if( ( j == 1 ) && !( whichOnes & NandSystemChannels ) )
		{
			continue;
		}
		u32 type = ( j == 0 ) ? 0x10001 : ( ( j == 1 ) ? 0x10002 : 0x10004 );
		u32 cnt = NandTitles.SetType( type );
		for( u32 i = 0; i < cnt; i++ )
		{
			u64 tid = NandTitles.Next();

			// The all-region News/Forecast data titles are not visible channels.
			// HAAA is different: old Wiis use it as Photo Channel itself, while
			// Photo Channel 1.1 installs HAYA and leaves HAAA in iplsave.  Keep
			// HAAA only when HAYA is genuinely absent so either revision renders.
			const u32 lowTitle = TITLE_LOWER( tid );
			if( j == 1 && ( lowTitle == 0x48414741 || lowTitle == 0x48414641 ) )
			{
				continue;
			}
			if( j == 1 && lowTitle == 0x48414141
				&& NandTitles.IndexOf( 0x0001000248415941ull ) >= 0 )
				continue;

			tmd* titleTmd = NandTitles.GetTMD( tid );
			if( !titleTmd )
			{
				continue;
			}

			u16 contentIndex;
			bool ok = false;
			for( contentIndex = 0; contentIndex < titleTmd->num_contents; contentIndex++ )
			{
				if( !titleTmd->contents[ contentIndex ].index )
				{
					ok = true;
					break;
				}
			}
			if( !ok )
			{
				continue;
			}
			char pathBuf[ 65 ]__attribute__((aligned( 32 )));

			snprintf( pathBuf, sizeof( pathBuf ), "/title/%08x/%08x/content/%08x.app", TITLE_UPPER( tid ), TITLE_LOWER( tid ),
					titleTmd->contents[contentIndex].cid );

			// check for channels that have been deleted
			s32 fd = ISFS_Open( pathBuf, ISFS_OPEN_READ );
			if( fd < 0 )
			{
				continue;
			}
			ISFS_Close( fd );

			BannerListEntry *entry = new BannerListEntry( pathBuf );

			// get size
			u32 s1 = 0, s2 = 0;
			s32 ret;
			snprintf( pathBuf, sizeof( pathBuf ), "/title/%08x/%08x/content", TITLE_UPPER( tid ), TITLE_LOWER( tid ) );
			if( !(ret = ISFS_GetUsage( pathBuf, &s1, &s2 )) )
			{
				entry->blocks = RU( s1, 8 ) / 8;
			}
			else
			{
				gprintf( "ISFS_GetUsage( \"%s\" ): %i\n", pathBuf, ret );
			}
			entry->tid = tid;
			bannerList << entry;
		}
	}
}

static void AddSDBannersToList()
{
	// Make the documented drop folder available on a fresh SD card.  mkdir is
	// best-effort because EEXIST is the normal result after the first boot.
	mkdir( Settings::sdBannerPath.c_str(), 0777 );
	DirList dir( Settings::sdBannerPath.c_str(), 0, DirList::Files);

	int BannersCount = dir.GetFilecount();
	for( int i = 0; i < BannersCount; i++ )
	{
		const char *ext = strrchr( dir.GetFilepath( i ), '.');
		if( !ext || ( strcasecmp( ext, ".bnr") && strcasecmp( ext, ".app" )
			&& strcasecmp( ext, ".wad" ) ) )
		{
			gprintf( "  skipping %s\n", dir.GetFilepath( i ) );
			continue;
		}
		//if( !strcasestr( dir.GetFilepath( i ), "SGV" ) )
		//{
		//	continue;
		//}

		BannerListEntry *entry = new BannerListEntry( dir.GetFilepath( i ) );
		entry->externalPreview = true;
		bannerList << entry;
	}
}

static void PlaceExternalBannersAfterLastInstalled(
	const List< BannerListEntry * > &previews )
{
	if( previews.empty() )
		return;

	std::map< BannerListEntry *, bool > shouldMove;
	for( u32 i = 0; i < previews.size(); ++i )
		if( previews[ i ] ) shouldMove[ previews[ i ] ] = true;

	List< BannerListEntry * > moved;
	List< BannerListEntry * > remaining;
	for( u32 i = 0; i < bannerList.size(); ++i )
	{
		BannerListEntry *entry = bannerList[ i ];
		if( entry && shouldMove.find( entry ) != shouldMove.end() )
			moved << entry;
		else
			remaining << entry;
	}
	if( moved.empty() )
		return;

	// The highest installed title is the last legitimate Wii channel slot.
	// Insert previews immediately after it, not after the IPL's unused tail.
	u32 insertAt = 0;
	for( u32 i = 0; i < remaining.size(); ++i )
		if( remaining[ i ] && remaining[ i ]->tid ) insertAt = i + 1;

	// A preview occupies an existing blank channel tile whenever possible.
	// Removing those trailing placeholders keeps the preview on the same page
	// instead of manufacturing a mostly empty fifth page.
	for( u32 i = 0; i < moved.size(); ++i )
	{
		for( u32 slot = insertAt; slot < remaining.size(); ++slot )
		{
			if( !remaining[ slot ] || !remaining[ slot ]->emptySlot )
				continue;
			delete remaining[ slot ];
			remaining.erase( remaining.begin() + slot );
			break;
		}
	}

	remaining.insert( remaining.begin() + std::min( insertAt,
		(u32)remaining.size() ), moved.begin(), moved.end() );
	bannerList.swap( remaining );
}

static List< BannerListEntry * > ExternalBannersMissingFromOrder(
	const std::vector< std::string > &savedKeys )
{
	std::map< std::string, bool > saved;
	for( u32 i = 0; i < savedKeys.size(); ++i ) saved[ savedKeys[ i ] ] = true;
	List< BannerListEntry * > missing;
	for( u32 i = 0; i < bannerList.size(); ++i )
	{
		BannerListEntry *entry = bannerList[ i ];
		if( entry && entry->externalPreview
			&& saved.find( BannerOrderKey( entry ) ) == saved.end() )
			missing << entry;
	}
	return missing;
}

static List< BannerListEntry * > AllExternalBanners()
{
	List< BannerListEntry * > previews;
	for( u32 i = 0; i < bannerList.size(); ++i )
		if( bannerList[ i ] && bannerList[ i ]->externalPreview )
			previews << bannerList[ i ];
	return previews;
}

static void AddHomebrewAppsToList()
{
	DirList dir("sd:/apps/", 0, DirList::Dirs );

	int BannersCount = dir.GetFilecount();
	for( int i = 0; i < BannersCount; i++ )
	{
		char path[ 0x80 ];
		char path2[ 0x80 ];
		snprintf( path, sizeof( path ), "%s/boot.dol", dir.GetFilepath( i ) );
		FILE *f = fopen( path, "rb" );
		if( !f )
		{
			snprintf( path, sizeof( path ), "%s/boot.elf", dir.GetFilepath( i ) );
			f = fopen( path, "rb" );
			if( !f )
			{
				continue;
			}
		}
		fclose( f );

		// create xml
		snprintf( path2, sizeof( path2 ), "%s/meta.xml", dir.GetFilepath( i ) );

		BannerListEntry *entry = new BannerListEntry( dir.GetFilepath( i ) );
		entry->hbXml = new HomebrewXML( path2 );
		if( !entry->hbXml->GetName() )
		{
			entry->hbXml->SetName( path );
		}
		bannerList << entry;
	}
}

static void AddChannelsMovedToSD()
{
	DirList dir( "sd:/private/wii/title", NULL, DirList::Dirs );
	int cnt = dir.GetFilecount();
	for( int i = 0; i < cnt; i++ )
	{
		// check for content.bin
		char path[ 65 ];
		snprintf( path, sizeof( path ), "%s/content.bin", dir.GetFilepath( i ) );

		FILE *f = fopen( path, "rb" );
		if( !f )
		{
			continue;
		}

		u32 len = ContentBin::GetInstalledSize( f );
		fclose( f );

		BannerListEntry *entry = new BannerListEntry( path );
#define KiB		( 1024 )
#define MiB		( KiB * 1024 )
#define BLOCK	( MiB / 8 )

		entry->blocks = RU( len, BLOCK ) / BLOCK;

		bannerList << entry;
	}
}

void BuildBannerList( u32 whichOnes )
{
	const bool appendSdBanners = ( whichOnes & DumpedSDBanners )
		&& Settings::useDumpedBanners;
	if( Settings::useNandBanners )
	{
		AddNandBannerPaths( whichOnes );
	}

	if( ( whichOnes & HomebrewApps ) && Settings::useHomebrewForBanners )
	{
		AddHomebrewAppsToList();
	}
	if( whichOnes & ChannelsMovedToSD )
	{
		AddChannelsMovedToSD();
	}
	if( appendSdBanners )
		AddSDBannersToList();
	const bool fullNandMenu = ( whichOnes & AllNandChannels ) == AllNandChannels;
	std::vector< std::string > savedKeys;
	const bool hasSavedOrder = LoadSavedOrderKeys( savedKeys )
		&& !savedKeys.empty();
	const bool refreshNow = fullNandMenu && ( systemMenuRefreshRequested
		|| !hasSavedOrder );
	if( fullNandMenu ) systemMenuRefreshRequested = false;
	if( refreshNow && ApplySystemMenuBannerOrder() )
	{
		// Refresh only the installed-title order, then put previews into the
		// first available channel tiles after the final real title.
		PlaceExternalBannersAfterLastInstalled( AllExternalBanners() );
		if( !SaveBannerOrder() )
			gprintf( "Channel order: could not save refreshed System Menu slots\n" );
	}
	else
	{
		const List< BannerListEntry * > newlyDiscovered =
			ExternalBannersMissingFromOrder( savedKeys );
		ApplySavedBannerOrder();
		PlaceExternalBannersAfterLastInstalled( newlyDiscovered );
		if( ( savedOrderNeedsRewrite || !newlyDiscovered.empty() )
			&& !SaveBannerOrder() )
			gprintf( "Channel order: could not save repaired System Menu slots\n" );
	}
}

void FreeBannerList()
{
	foreach( BannerListEntry *e, bannerList )
	{
		delete e;
	}
	bannerList.clear();
}
