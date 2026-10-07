#include "ash.h"
#include "gecko.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

// Clean ASH0 decoder for current devkitPPC.
//
// The original implementation was a literal transcription of PowerPC
// registers and branches.  Besides being impossible to bounds-check, small
// compiler/optimizer changes could corrupt the decoded U8 archive.  This
// implementation expresses the same two Huffman trees and LZ copy stream
// directly.  The structure is based on the documented ASH algorithm and the
// MIT-licensed rustii implementation by NinjaCheetah & contributors.
// Attribution and upstream MIT permission are retained in LICENSES/Rustwii.txt.

namespace
{
	static const u32 TreeRight = 0x80000000u;
	static const u32 TreeLeft = 0x40000000u;
	static const u32 TreeValueMask = 0x3fffffffu;

	u32 ReadBe32( const u8 *data )
	{
		return ( (u32)data[ 0 ] << 24 ) |
		       ( (u32)data[ 1 ] << 16 ) |
		       ( (u32)data[ 2 ] << 8 ) |
		       (u32)data[ 3 ];
	}

	struct BitReader
	{
		const u8 *data;
		u32 size;
		u32 position;
		u32 word;
		u32 consumed;
		bool valid;

		BitReader( const u8 *source, u32 sourceSize, u32 start )
			: data( source ), size( sourceSize ), position( start ),
			  word( 0 ), consumed( 0 ), valid( true )
		{
			Feed();
		}

		bool Feed()
		{
			if( !data || position > size || size - position < 4 )
			{
				valid = false;
				return false;
			}
			word = ReadBe32( data + position );
			position += 4;
			consumed = 0;
			return true;
		}

		bool ReadBit( u32 &value )
		{
			if( !valid )
				return false;
			if( consumed == 32 && !Feed() )
				return false;
			value = word >> 31;
			++consumed;
			word <<= 1;
			return true;
		}

		bool ReadBits( u32 count, u32 &value )
		{
			if( !valid || !count || count > 31 )
				return false;
			value = 0;
			for( u32 i = 0; i < count; ++i )
			{
				u32 bit = 0;
				if( !ReadBit( bit ) )
					return false;
				value = ( value << 1 ) | bit;
			}
			return true;
		}
	};

	struct DecodeTree
	{
		u32 *left;
		u32 *right;
		u32 count;
		u32 leafCount;
		u32 root;

		DecodeTree()
			: left( NULL ), right( NULL ), count( 0 ), leafCount( 0 ), root( 0 )
		{
		}

		~DecodeTree()
		{
			free( left );
			free( right );
		}

		bool Allocate( u32 width )
		{
			leafCount = 1u << width;
			count = 2 * leafCount - 1;
			left = (u32*)calloc( count, sizeof( u32 ) );
			right = (u32*)calloc( count, sizeof( u32 ) );
			return left && right;
		}
	};

	bool ReadTree( BitReader &reader, u32 width, DecodeTree &tree )
	{
		if( !tree.Allocate( width ) )
			return false;

		const u32 workCapacity = 2 * tree.leafCount;
		u32 *work = (u32*)malloc( workCapacity * sizeof( u32 ) );
		if( !work )
			return false;

		u32 workPosition = 0;
		u32 nextNode = tree.leafCount;
		u32 pending = 0;
		bool success = false;

		for( ;; )
		{
			u32 bit = 0;
			if( !reader.ReadBit( bit ) )
				break;

			if( bit )
			{
				if( workPosition + 2 > workCapacity || nextNode >= tree.count )
					break;
				work[ workPosition++ ] = nextNode | TreeRight;
				work[ workPosition++ ] = nextNode | TreeLeft;
				pending += 2;
				++nextNode;
				continue;
			}

			u32 value = 0;
			if( !reader.ReadBits( width, value ) || value >= tree.leafCount )
				break;

			if( !pending )
			{
				tree.root = value;
				success = true;
				break;
			}

			for( ;; )
			{
				if( !workPosition || !pending )
					break;
				const u32 node = work[ --workPosition ];
				const u32 index = node & TreeValueMask;
				--pending;
				if( index >= tree.count )
					break;

				if( node & TreeRight )
				{
					tree.right[ index ] = value;
					value = index;
					if( !pending )
					{
						tree.root = value;
						success = true;
						break;
					}
					continue;
				}

				tree.left[ index ] = value;
				if( !pending )
				{
					tree.root = index;
					success = true;
				}
				break;
			}

			if( success )
				break;
		}

		free( work );
		return success;
	}

	bool DecodeSymbol( BitReader &reader, const DecodeTree &tree, u32 &symbol )
	{
		symbol = tree.root;
		while( symbol >= tree.leafCount )
		{
			if( symbol >= tree.count )
				return false;
			u32 bit = 0;
			if( !reader.ReadBit( bit ) )
				return false;
			symbol = bit ? tree.right[ symbol ] : tree.left[ symbol ];
		}
		return true;
	}
}

bool IsAshCompressed( const u8 *stuff, u32 len )
{
	return stuff && len >= 0x10 && ReadBe32( stuff ) == 0x41534830u;
}

u8 *DecompressAsh( const u8 *stuff, u32 &len )
{
	const u32 sourceSize = len;
	if( !IsAshCompressed( stuff, sourceSize ) )
		return NULL;

	const u32 outputSize = ReadBe32( stuff + 4 ) & 0x00ffffffu;
	const u32 distanceOffset = ReadBe32( stuff + 8 );
	if( !outputSize || outputSize > 0x04000000u ||
		distanceOffset < 0x10 || distanceOffset > sourceSize ||
		sourceSize - distanceOffset < 4 )
	{
		gprintf( "ASH: invalid header\n" );
		return NULL;
	}

	BitReader symbolReader( stuff, sourceSize, 0x0c );
	BitReader distanceReader( stuff, sourceSize, distanceOffset );
	DecodeTree symbolTree;
	DecodeTree distanceTree;
	if( !symbolReader.valid || !distanceReader.valid ||
		!ReadTree( symbolReader, 9, symbolTree ) ||
		!ReadTree( distanceReader, 11, distanceTree ) )
	{
		gprintf( "ASH: invalid tree data\n" );
		return NULL;
	}

	u8 *output = (u8*)memalign( 32, outputSize );
	if( !output )
	{
		gprintf( "ASH: no memory\n" );
		return NULL;
	}

	u32 outputPosition = 0;
	while( outputPosition < outputSize )
	{
		u32 symbol = 0;
		if( !DecodeSymbol( symbolReader, symbolTree, symbol ) )
			break;

		if( symbol < 0x100 )
		{
			output[ outputPosition++ ] = (u8)symbol;
			continue;
		}

		u32 distance = 0;
		if( !DecodeSymbol( distanceReader, distanceTree, distance ) )
			break;

		const u32 copyLength = symbol - 0x100 + 3;
		if( distance >= outputPosition || copyLength > outputSize - outputPosition )
			break;

		u32 sourcePosition = outputPosition - distance - 1;
		for( u32 i = 0; i < copyLength; ++i )
			output[ outputPosition++ ] = output[ sourcePosition++ ];
	}

	if( outputPosition != outputSize )
	{
		gprintf( "ASH: malformed stream at %u/%u bytes\n", outputPosition, outputSize );
		free( output );
		return NULL;
	}

	len = outputSize;
	return output;
}
