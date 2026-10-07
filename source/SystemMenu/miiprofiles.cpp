#include "miiprofiles.h"

#include <algorithm>
#include <cstring>
#include <gccore.h>
#include <ogc/isfs.h>

namespace
{
	const char *DatabasePath = "/shared2/menu/FaceLib/RFL_DB.dat";
	const int DatabaseMagicSize = 4;
	const int MiiRecordSize = 0x4a;
	const int MiiNameOffset = 2;
	const int MiiNameCharacters = 10;
	const int MiiSlotCount = 100;
	const int DatabaseHeaderBytes = DatabaseMagicSize
		+ MiiRecordSize * MiiSlotCount;

	std::vector<std::string> names;
	bool loaded = false;
	u8 databaseHeader[ ( DatabaseHeaderBytes + 31 ) & ~31 ] ATTRIBUTE_ALIGN( 32 );

	void AppendUtf8( std::string &output, u16 value )
	{
		if( value < 0x80 ) output += (char)value;
		else if( value < 0x800 )
		{
			output += (char)( 0xc0 | ( value >> 6 ) );
			output += (char)( 0x80 | ( value & 0x3f ) );
		}
		else if( value < 0xd800 || value > 0xdfff )
		{
			output += (char)( 0xe0 | ( value >> 12 ) );
			output += (char)( 0x80 | ( ( value >> 6 ) & 0x3f ) );
			output += (char)( 0x80 | ( value & 0x3f ) );
		}
	}

	std::string ReadMiiName( const u8 *record )
	{
		std::string result;
		for( int character = 0; character < MiiNameCharacters; ++character )
		{
			const int offset = MiiNameOffset + character * 2;
			const u16 value = ( (u16)record[ offset ] << 8 )
				| record[ offset + 1 ];
			if( !value ) break;
			if( value < 0x20 ) continue;
			AppendUtf8( result, value );
		}
		return result;
	}

	void Load()
	{
		if( loaded ) return;
		loaded = true;
		names.clear();
		memset( databaseHeader, 0, sizeof( databaseHeader ) );
		const s32 descriptor = ISFS_Open( DatabasePath, ISFS_OPEN_READ );
		if( descriptor < 0 ) return;
		const s32 bytesRead = ISFS_Read( descriptor, databaseHeader,
			DatabaseHeaderBytes );
		ISFS_Close( descriptor );
		if( bytesRead < DatabaseHeaderBytes
			|| memcmp( databaseHeader, "RNOD", DatabaseMagicSize ) )
			return;
		for( int slot = 0; slot < MiiSlotCount; ++slot )
		{
			const u8 *record = databaseHeader + DatabaseMagicSize
				+ slot * MiiRecordSize;
			const std::string name = ReadMiiName( record );
			if( name.empty() ) continue;
			if( std::find( names.begin(), names.end(), name ) == names.end() )
				names.push_back( name );
		}
	}
}

const std::vector<std::string> &MiiProfiles::Names()
{
	Load();
	return names;
}

std::string MiiProfiles::Step( const std::string &current, int direction )
{
	Load();
	if( names.empty() ) return current;
	std::vector<std::string>::const_iterator found =
		std::find( names.begin(), names.end(), current );
	int index = found == names.end() ? 0 : found - names.begin();
	if( found != names.end() )
		index = ( index + ( direction < 0 ? names.size() - 1 : 1 ) )
			% names.size();
	return names[ index ];
}

int MiiProfiles::IndexOf( const std::string &name )
{
	Load();
	std::vector<std::string>::const_iterator found =
		std::find( names.begin(), names.end(), name );
	return found == names.end() ? 0 : (int)( found - names.begin() );
}

void MiiProfiles::Reload()
{
	loaded = false;
	names.clear();
}
