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
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <sys/stat.h>
#include <gccore.h>
#include <ogc/aes.h>
#include "sha1.h"
#include "BannerAsync.h"
#include "contentbin.h"
#include "fileops.h"
#include "nandtitle.h"
#include "TextureConverter.h"

extern const u8 original_homebrew_bin[];
extern const u32 original_homebrew_bin_size;

namespace
{
	const u32 MaxLooseBannerBytes = 64 * 1024 * 1024;
	const u32 MaxWadBannerBytes = 32 * 1024 * 1024;
	const u32 MaxWadMetadataBytes = 1024 * 1024;
	// libogc uses larger priorities for more urgent work. Keep archive decode
	// below the default GUI thread (64), not above it as the old value 80 did.
	const u8 IconLoaderPriority = 48;

	u16 ReadBE16( const u8 *p )
	{
		return ( (u16)p[ 0 ] << 8 ) | p[ 1 ];
	}

	u32 ReadBE32( const u8 *p )
	{
		return ( (u32)p[ 0 ] << 24 ) | ( (u32)p[ 1 ] << 16 )
			| ( (u32)p[ 2 ] << 8 ) | p[ 3 ];
	}

	u64 ReadBE64( const u8 *p )
	{
		u64 value = 0;
		for( int i = 0; i < 8; ++i )
			value = ( value << 8 ) | p[ i ];
		return value;
	}

	u64 AlignUp( u64 value, u64 alignment )
	{
		return ( value + alignment - 1 ) & ~( alignment - 1 );
	}

	bool AddWithin( u64 left, u64 right, u64 limit, u64 &result )
	{
		if( left > limit || right > limit - left )
			return false;
		result = left + right;
		return true;
	}

	bool ReadAt( FILE *file, u64 offset, void *buffer, u32 size )
	{
		if( !file || !buffer || !size || offset > (u64)LONG_MAX )
			return false;
		if( fseek( file, (long)offset, SEEK_SET ) != 0 )
			return false;
		return fread( buffer, 1, size, file ) == size;
	}

	bool HasExtension( const std::string &path, const char *extension )
	{
		const char *dot = strrchr( path.c_str(), '.' );
		return dot && !strcasecmp( dot, extension );
	}

	// WAD title-key wrapping keys are part of the file format.  Use /dev/aes
	// for both decryptions: ES_Decrypt depends on the launched title's ES
	// permissions and silently assuming common-key index zero breaks other
	// region/platform WADs.  No ticket is installed to preview an archive.
	const u8 WadCommonKeys[][16] ATTRIBUTE_ALIGN(32) =
	{
		{ 0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7 },
		{ 0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e },
		{ 0x30,0xbf,0xc7,0x6e,0x7c,0x19,0xaf,0xbb,0x23,0x16,0x33,0x30,0xce,0xd7,0xc2,0x8d }
	};
}

vector<BannerAsync *> BannerAsync::List;
queue<BannerAsync *> BannerAsync::DeleteList;
lwp_t BannerAsync::Thread = LWP_THREAD_NULL;
mutex_t BannerAsync::ListLock = LWP_THREAD_NULL;
BannerAsync * BannerAsync::InUse = NULL;
u32 BannerAsync::ThreadCount = 0;
volatile bool BannerAsync::CloseThread = false;
volatile bool BannerAsync::ThreadExited = true;

BannerAsync::BannerAsync()
	: Banner(0, 0)
	, buffer(0)
	, bufferSize(0)
	, loadComplete(false)
	, loadFailed(false)
{

}

BannerAsync::BannerAsync(const string &path, u64 tid)
	: Banner(0, 0)
	, filepath(path)
	, buffer(0)
	, bufferSize(0)
	, loadComplete(false)
	, loadFailed(false)
{
	// The worker uses the title ID while it builds News/Forecast layouts.  Set
	// it before queueing this object so a fast NAND read cannot briefly build a
	// generic icon and then race the main thread's SetTitleId() call.
	SetTitleId( tid );
	ThreadInit();
	ThreadAdd(this);
}

BannerAsync::~BannerAsync()
{
	ThreadRemove(this);
	ThreadExit();
	while(InUse == this)
		usleep(100);

	free( buffer );
}

void BannerAsync::ThreadAdd(BannerAsync *Image)
{
	LWP_MutexLock(ListLock);
	List.push_back(Image);
	LWP_MutexUnlock(ListLock);
	if( Thread != LWP_THREAD_NULL )
		LWP_ResumeThread(Thread);
}

void BannerAsync::ThreadRemove(BannerAsync *Image)
{
	LWP_MutexLock(ListLock);
	for(u32 i = 0; i < List.size(); ++i)
	{
		if(List[i] == Image)
		{
			List.erase(List.begin()+i);
			break;
		}
	}
	LWP_MutexUnlock(ListLock);
}

void BannerAsync::RemoveBanner(BannerAsync *img)
{
	if( !img ) return;
	LWP_MutexLock(ListLock);
    DeleteList.push(img);
	LWP_MutexUnlock(ListLock);
	if( Thread != LWP_THREAD_NULL )
		LWP_ResumeThread(Thread);
}

void BannerAsync::PrioritizeBanner(BannerAsync *banner)
{
	if( !banner || banner->IsLoadComplete() ) return;
	LWP_MutexLock(ListLock);
	for( u32 i = 0; i < List.size(); ++i )
		if( List[i] == banner )
		{
			List.erase(List.begin() + i);
			List.insert(List.begin(), banner);
			break;
		}
	LWP_MutexUnlock(ListLock);
}

u32 BannerAsync::CacheBytes() const
{
	if( !IsLoadComplete() ) return UINT_MAX;
	// Actual backing allocations plus a conservative allowance for objects,
	// animations and decoded font/layout metadata. Count all texture texels as
	// RGBA8 even when GX stores them more compactly or inside icon_bin.
	u64 bytes = 256 * 1024;
	if( buffer ) bytes += malloc_usable_size(buffer);
	if( icon_bin ) bytes += malloc_usable_size(icon_bin);
	if( banner_bin ) bytes += malloc_usable_size(banner_bin);
	if( layout_icon )
	{
		const TextureList &textures = layout_icon->Textures();
		for( TextureList::const_iterator it = textures.begin(); it != textures.end(); ++it )
			if( *it ) bytes += (u64)RU((*it)->GetWidth(), 4) * RU((*it)->GetHeight(), 4) * 4;
	}
	return bytes > UINT_MAX ? UINT_MAX : (u32)bytes;
}

void BannerAsync::ClearQueue()
{
	LWP_MutexLock(ListLock);
	List.clear();
	LWP_MutexUnlock(ListLock);
}

void * BannerAsync::BannerAsyncThread(void *arg)
{
	while(!CloseThread)
	{
		BannerAsync *next = NULL;
		vector< BannerAsync * > pendingDeletes;
		LWP_MutexLock(ListLock);
		while(!DeleteList.empty())
		{
			BannerAsync *doomed = DeleteList.front();
			pendingDeletes.push_back( doomed );
			DeleteList.pop();
			for( u32 i = 0; i < List.size(); ++i )
			{
				if( List[ i ] == doomed )
				{
					List.erase( List.begin() + i );
					break;
				}
			}
		}
		if( !List.empty() )
		{
			next = List.front();
			List.erase(List.begin());
			InUse = next;
		}
		LWP_MutexUnlock(ListLock);
		// Destruction calls ThreadRemove(), which takes ListLock itself.  Delete
		// only after releasing the queue lock to avoid a self-deadlock.
		for( u32 i = 0; i < pendingDeletes.size(); ++i )
			delete pendingDeletes[ i ];

		if( next )
		{
			if( next->filepath[ 0 ] == '/' )// cheating...  if the path starts with a '/', assume it is from the nand
			{
				LoadNandBanner( next );
			}
			else if( HasExtension( next->filepath, ".wad" ) )
			{
				LoadWadBanner( next );
			}
			else if( HasExtension( next->filepath, ".app" )
				|| HasExtension( next->filepath, ".bnr" ) )
			{
				LoadBannerFile( next );
			}
			else if( strcasestr( next->filepath.c_str(), "/apps/" ) )
			{
				LoadHomebrewBanner( next );
			}
			else if( strcasestr( next->filepath.c_str(), "content.bin" ) )
			{
				LoadContentBinBanner( next );
			}
			else
			{
				LoadBannerFile( next );
			}
			InUse = NULL;
			// Yield between archives rather than consuming a whole page's decode
			// work back-to-back while the GUI is animating.
			usleep(1000);
			continue;
		}

		if( !CloseThread )
			LWP_SuspendThread(Thread);
	}

	InUse = NULL;
	ThreadExited = true;
	return NULL;
}

u32 BannerAsync::ThreadInit()
{
	if (Thread == LWP_THREAD_NULL)
	{
		if( ListLock == LWP_MUTEX_NULL )
			LWP_MutexInit(&ListLock, false);
		CloseThread = false;
		ThreadExited = false;
		LWP_CreateThread(&Thread, BannerAsyncThread, NULL, NULL, 32768, IconLoaderPriority);
	}
	return ++ThreadCount;
}

bool BannerAsync::QuiesceForLaunch()
{
	if( Thread == LWP_THREAD_NULL )
		return false;

	CloseThread = true;
	// A resume can race the worker's final self-suspend.  Keep waking it until
	// it has published its exit flag, then join without any suspended-thread
	// deadlock window.
	while( !ThreadExited )
	{
		LWP_ResumeThread( Thread );
		usleep( 1000 );
	}
	LWP_JoinThread( Thread, NULL );
	Thread = LWP_THREAD_NULL;
	InUse = NULL;
	return true;
}

void BannerAsync::RestartAfterLaunchFailure()
{
	if( Thread != LWP_THREAD_NULL )
		return;
	CloseThread = false;
	ThreadExited = false;
	if( LWP_CreateThread(&Thread, BannerAsyncThread, NULL, NULL, 32768, IconLoaderPriority) < 0 )
	{
		Thread = LWP_THREAD_NULL;
		ThreadExited = true;
		return;
	}
	LWP_ResumeThread( Thread );
}

u32 BannerAsync::ThreadExit()
{
	//! We don't need to always shutdown and startup the thread, especially
	//! since this is a nested startup/shutdown from the gui thread.
	//! It's fine with being put to suspended only.
	/*
	if (--ThreadCount == 0)
	{
		CloseThread = true;
		LWP_ResumeThread(Thread);
		LWP_JoinThread(Thread, NULL);
		LWP_MutexUnlock(ListLock);
		LWP_MutexDestroy(ListLock);
		Thread = LWP_THREAD_NULL;
		ListLock = LWP_MUTEX_NULL;
		ListLock = LWP_MUTEX_NULL;
	}
	*/
	return --ThreadCount;
}

void BannerAsync::PublishLoadResult( BannerAsync *bann, bool failed )
{
	if( !bann )
		return;
	// Publish failure and every object/layout write before the completion flag.
	// The menu thread pairs this with the barrier in IsLoadComplete().
	bann->loadFailed = failed;
	__sync_synchronize();
	bann->loadComplete = true;
}

bool BannerAsync::LoadBannerFile( BannerAsync *bann )
{
	//gprintf( "BannerAsync::LoadBannerFile( \"%s\" )\n", bann->filepath.c_str() );
	FILE * f = fopen( bann->filepath.c_str(), "rb" );
	if(!f)
	{
		gprintf( "  error opening %s\n", bann->filepath.c_str() );
		PublishLoadResult( bann, true );
		return false;
	}

	if( fseek( f, 0, SEEK_END ) != 0 )
	{
		fclose( f );
		PublishLoadResult( bann, true );
		return false;
	}
	const long fileSize = ftell( f );
	rewind( f );
	if( fileSize <= 0 || (u64)fileSize > MaxLooseBannerBytes )
	{
		gprintf( "  rejected loose banner size %ld: %s\n", fileSize,
			bann->filepath.c_str() );
		fclose( f );
		PublishLoadResult( bann, true );
		return false;
	}
	const u32 size = (u32)fileSize;

	u8 *buffer = (u8*)memalign( 32, size );
	if( !buffer )
	{
		fclose( f );
		PublishLoadResult( bann, true );
		return false;
	}
	const bool readOk = fread( buffer, 1, size, f ) == size;
	fclose( f );
	if( !readOk )
	{
		free( buffer );
		PublishLoadResult( bann, true );
		return false;
	}

	const bool ok = bann->Load(buffer, size);
	if( ok )
	{
		bann->LoadIcon();
	}
	bann->buffer = buffer;
	bann->bufferSize = size;
	PublishLoadResult( bann, !ok );
	return ok;
}

bool BannerAsync::LoadWadBanner( BannerAsync *bann )
{
	if( !bann )
		return false;
	FILE *file = fopen( bann->filepath.c_str(), "rb" );
	if( !file )
	{
		gprintf( "WAD banner: cannot open %s\n", bann->filepath.c_str() );
		PublishLoadResult( bann, true );
		return false;
	}

	bool ok = false;
	bool aesOpened = false;
	u8 *ticketData = NULL;
	u8 *tmdData = NULL;
	u8 *bannerData = NULL;
	u8 *encryptedChunk = NULL;

	do
	{
	if( fseek( file, 0, SEEK_END ) != 0 )
		break;
	const long measuredSize = ftell( file );
	if( measuredSize < 0x40 )
		break;
	const u64 fileSize = (u64)measuredSize;

	u8 header[ 0x40 ] ATTRIBUTE_ALIGN(32);
	if( !ReadAt( file, 0, header, sizeof( header ) ) )
		break;
	const u32 headerSize = ReadBE32( header + 0x00 );
	const u32 certSize = ReadBE32( header + 0x08 );
	const u32 crlSize = ReadBE32( header + 0x0c );
	const u32 ticketSize = ReadBE32( header + 0x10 );
	const u32 tmdSize = ReadBE32( header + 0x14 );
	const u32 dataSize = ReadBE32( header + 0x18 );
	if( headerSize < 0x20 || headerSize > 0x1000
		|| ticketSize < 0x200 || ticketSize > MaxWadMetadataBytes
		|| tmdSize < 0x1e4 || tmdSize > MaxWadMetadataBytes )
	{
		gprintf( "WAD banner: invalid section sizes in %s\n",
			bann->filepath.c_str() );
		break;
	}

	u64 certOffset = AlignUp( headerSize, 0x40 );
	u64 crlOffset = 0;
	u64 ticketOffset = 0;
	u64 tmdOffset = 0;
	u64 dataOffset = 0;
	u64 sectionEnd = 0;
	if( !AddWithin( certOffset, certSize, fileSize, sectionEnd ) )
		break;
	crlOffset = AlignUp( sectionEnd, 0x40 );
	if( !AddWithin( crlOffset, crlSize, fileSize, sectionEnd ) )
		break;
	ticketOffset = AlignUp( sectionEnd, 0x40 );
	if( !AddWithin( ticketOffset, ticketSize, fileSize, sectionEnd ) )
		break;
	tmdOffset = AlignUp( sectionEnd, 0x40 );
	if( !AddWithin( tmdOffset, tmdSize, fileSize, sectionEnd ) )
		break;
	dataOffset = AlignUp( sectionEnd, 0x40 );
	if( !AddWithin( dataOffset, dataSize, fileSize, sectionEnd ) )
		break;

	ticketData = (u8*)memalign( 32, AlignUp( ticketSize, 32 ) );
	tmdData = (u8*)memalign( 32, AlignUp( tmdSize, 32 ) );
	if( !ticketData || !tmdData
		|| !ReadAt( file, ticketOffset, ticketData, ticketSize )
		|| !ReadAt( file, tmdOffset, tmdData, tmdSize ) )
		break;

	const u32 ticketSignatureSize =
		ReadBE32( ticketData ) == 0x00010000 ? 0x240
		: ReadBE32( ticketData ) == 0x00010001 ? 0x140
		: ReadBE32( ticketData ) == 0x00010002 ? 0x80 : 0;
	const u32 tmdSignatureSize =
		ReadBE32( tmdData ) == 0x00010000 ? 0x240
		: ReadBE32( tmdData ) == 0x00010001 ? 0x140
		: ReadBE32( tmdData ) == 0x00010002 ? 0x80 : 0;
	if( !ticketSignatureSize || !tmdSignatureSize
		|| ticketSignatureSize + 0xb2 > ticketSize
		|| tmdSignatureSize + 0xa4 > tmdSize )
		break;

	const u8 *ticket = ticketData + ticketSignatureSize;
	const u8 *titleMetadata = tmdData + tmdSignatureSize;
	const u64 titleId = ReadBE64( titleMetadata + 0x4c );
	const u64 ticketTitleId = ReadBE64( ticket + 0x9c );
	const u16 contentCount = ReadBE16( titleMetadata + 0x9e );
	if( !titleId || ticketTitleId != titleId
		|| !contentCount || contentCount > 512
		|| (u64)tmdSignatureSize + 0xa4
			+ (u64)contentCount * 0x24 > tmdSize )
		break;

	u64 contentOffset = dataOffset;
	u64 bannerSize = 0;
	u16 bannerIndex = 0xffff;
	const u8 *bannerHash = NULL;
	bool contentOffsetsValid = true;
	for( u16 i = 0; i < contentCount; ++i )
	{
		const u8 *record = titleMetadata + 0xa4 + (u32)i * 0x24;
		const u16 index = ReadBE16( record + 0x04 );
		const u64 size = ReadBE64( record + 0x08 );
		if( index == 0 )
		{
			bannerSize = size;
			bannerIndex = index;
			bannerHash = record + 0x10;
			break;
		}
		const u64 paddedSize = AlignUp( size, 0x40 );
		if( !AddWithin( contentOffset, paddedSize, sectionEnd, contentOffset ) )
		{
			contentOffsetsValid = false;
			break;
		}
	}
	if( !contentOffsetsValid )
		break;
	if( bannerIndex == 0xffff || !bannerSize || bannerSize > MaxWadBannerBytes )
	{
		gprintf( "WAD banner: missing/oversized content index 0 in %s\n",
			bann->filepath.c_str() );
		break;
	}
	const u64 encryptedSize64 = AlignUp( bannerSize, 16 );
	if( encryptedSize64 > UINT_MAX
		|| contentOffset > sectionEnd
		|| encryptedSize64 > sectionEnd - contentOffset )
		break;
	const u32 encryptedSize = (u32)encryptedSize64;
	const u8 commonKeyIndex = ticket[ 0xb1 ];
	if( commonKeyIndex >= sizeof( WadCommonKeys ) / sizeof( WadCommonKeys[0] ) )
	{
		gprintf( "WAD banner: unsupported common-key index %u in %s\n",
			commonKeyIndex, bann->filepath.c_str() );
		break;
	}
	const s32 aesInitResult = AES_Init();
	if( aesInitResult < 0 )
	{
		gprintf( "WAD banner: AES_Init failed: %ld\n", (long)aesInitResult );
		break;
	}
	aesOpened = true;

	u8 commonKey[ 32 ] ATTRIBUTE_ALIGN(32);
	u8 encryptedTitleKey[ 32 ] ATTRIBUTE_ALIGN(32);
	u8 titleKey[ 32 ] ATTRIBUTE_ALIGN(32);
	u8 titleIv[ 32 ] ATTRIBUTE_ALIGN(32);
	memset( commonKey, 0, sizeof( commonKey ) );
	memcpy( commonKey, WadCommonKeys[ commonKeyIndex ], 16 );
	memset( encryptedTitleKey, 0, sizeof( encryptedTitleKey ) );
	memset( titleKey, 0, sizeof( titleKey ) );
	memset( titleIv, 0, sizeof( titleIv ) );
	memcpy( encryptedTitleKey, ticket + 0x7f, 16 );
	for( int i = 0; i < 8; ++i )
		titleIv[ i ] = (u8)( titleId >> ( 56 - i * 8 ) );
	DCFlushRange( encryptedTitleKey, 32 );
	DCFlushRange( titleIv, 32 );
	DCInvalidateRange( titleKey, 32 );
	const s32 titleKeyResult = AES_Decrypt( commonKey, 16, titleIv, 16,
		encryptedTitleKey, titleKey, 16 );
	DCInvalidateRange( titleKey, 32 );
	if( titleKeyResult < 0 )
	{
		gprintf( "WAD banner: title-key decrypt failed: %ld\n",
			(long)titleKeyResult );
		break;
	}
	// Invalidating the final partial cache line must not discard an adjacent
	// allocation's heap metadata when the AES size is 16 (but not 32) aligned.
	bannerData = (u8*)memalign( 32, AlignUp( encryptedSize, 32 ) );
	encryptedChunk = (u8*)memalign( 32, AES_BLOCK_SIZE );
	if( !bannerData || !encryptedChunk )
		break;
	u8 contentIv[ 32 ] ATTRIBUTE_ALIGN(32);
	memset( contentIv, 0, sizeof( contentIv ) );
	contentIv[ 0 ] = (u8)( bannerIndex >> 8 );
	contentIv[ 1 ] = (u8)bannerIndex;
	DCFlushRange( titleKey, 32 );
	DCFlushRange( contentIv, 32 );
	bool decryptOk = true;
	for( u32 done = 0; done < encryptedSize; )
	{
		const u32 chunk = std::min( (u32)AES_BLOCK_SIZE,
			encryptedSize - done );
		if( !ReadAt( file, contentOffset + done, encryptedChunk, chunk ) )
		{
			decryptOk = false;
			break;
		}
		// AES-CBC uses the last ciphertext block as the IV for the next call.
		// Save it explicitly before the call so chunk chaining also works with
		// IOS implementations that differ in whether they write the IV back.
		u8 nextIv[ 16 ] ATTRIBUTE_ALIGN(32);
		memcpy( nextIv, encryptedChunk + chunk - 16, 16 );
		DCFlushRange( encryptedChunk, chunk );
		DCInvalidateRange( bannerData + done, chunk );
		const s32 decryptResult = AES_Decrypt( titleKey, 16, contentIv, 16,
			encryptedChunk, bannerData + done, chunk );
		DCInvalidateRange( bannerData + done, chunk );
		if( decryptResult < 0 )
		{
			gprintf( "WAD banner: content decrypt failed: %ld\n",
				(long)decryptResult );
			decryptOk = false;
			break;
		}
		memcpy( contentIv, nextIv, 16 );
		done += chunk;
	}
	if( !decryptOk )
		break;
	u8 actualHash[20];
	GetSha1( bannerData, (u32)bannerSize, actualHash );
	if( memcmp( actualHash, bannerHash, sizeof( actualHash ) ) )
	{
		gprintf( "WAD banner: decrypted content hash mismatch in %s\n",
			bann->filepath.c_str() );
		break;
	}

	// Keep the WAD title ID only for rendering/channel-specific behavior.  The
	// BannerList entry intentionally retains tid == 0, so Start remains a safe
	// preview and never launches or installs the dropped WAD.
	bann->SetTitleId( titleId );
	ok = bann->Load( bannerData, (u32)bannerSize );
	if( ok )
		bann->LoadIcon();
	if( ok )
	{
		bann->buffer = bannerData;
		bann->bufferSize = (u32)bannerSize;
		bannerData = NULL;
		gprintf( "WAD banner: loaded %016llx from %s\n",
			(unsigned long long)titleId, bann->filepath.c_str() );
	}
	} while( false );

	free( encryptedChunk );
	if( aesOpened )
		AES_Close();
	free( bannerData );
	free( tmdData );
	free( ticketData );
	fclose( file );
	PublishLoadResult( bann, !ok );
	return ok;
}

bool BannerAsync::LoadNandBanner( BannerAsync *bann )
{
	u8* buffer;
	u32 size;

	if( NandTitle::LoadFileFromNand( bann->filepath.c_str(), &buffer, &size ) )
	{
		gprintf( "error loading: \"%s\"\n", bann->filepath.c_str() );
		PublishLoadResult( bann, true );
		return false;
	}
	const bool ok = bann->Load( buffer, size );
	if( ok )
	{
		bann->LoadIcon();
	}
	bann->buffer = buffer;
	bann->bufferSize = size;
	PublishLoadResult( bann, !ok );
	return ok;
}

bool BannerAsync::LoadContentBinBanner( BannerAsync *bann )
{
	const char *path = bann->filepath.c_str();
	u32 tid32 = *(u32*)( path + 22 );
	FILE *file = fopen( path, "rb" );
	if( !file )
	{
		PublishLoadResult( bann, true );
		return false;
	}

	u32 bannerLen;
	u32 installedBytes = 0;
	u8* bannerData = ContentBin::GetIconAsBannerData( file, tid32, bannerLen, installedBytes );
	fclose( file );
	if( !bannerData )
	{
		PublishLoadResult( bann, true );
		return false;
	}
	const bool ok = bann->Load( bannerData, bannerLen );
	if( ok )
	{
		bann->LoadIcon();
	}
	bann->buffer = bannerData;
	bann->bufferSize = bannerLen;
	PublishLoadResult( bann, !ok );
	return ok;
}

bool BannerAsync::LoadHomebrewBanner( BannerAsync *bann )
{
	const bool ok = bann->Load( original_homebrew_bin, original_homebrew_bin_size );
	if( ok )
	{
		bann->LoadIcon();
	}
	PublishLoadResult( bann, !ok );
	return ok;
}

BannerAsyncHB::BannerAsyncHB( const string &path, HomebrewXML *hbXml, u64 tid )
	: xml( hbXml ),
	  rawTexData( NULL )
{
	// not using BannerAsync's second constructor because it creates a race condition where
	// this class gets treated as the base class for a brief second on the working thread until the
	// main thread catches up

	filepath = path;
	SetTitleId( tid );
	ThreadInit();
	ThreadAdd(this);
}

BannerAsyncHB::~BannerAsyncHB()
{
	free( rawTexData );
}

Object *BannerAsyncHB::LoadBanner()
{
	if( !arc || !xml )
		return NULL;

	if( bannerObj )
	{
		return bannerObj;
	}
	u32 arcLen;
	if( !( banner_bin = arc->GetFileAllocated( "/meta/banner.bin", &arcLen ) ) )
	{
		return NULL;
	}

	U8Archive theArc( banner_bin, arcLen );

	// create layout
	if( !( layout_banner = LoadLayout( theArc, "banner" ) ) )
	{
		return NULL;
	}

	// create object
	bannerObj = new Object;
	bannerObj->BindPane( layout_banner->FindPane( "RootPane" ) );
	bannerObj->BindMaterials( layout_banner->Materials() );

	// get animations
	std::string brlanName = "banner_Start";
	Animation *anim = LoadAnimation( theArc, brlanName );

	// we have a starting animation
	if( anim )
	{
		layout_banner->LoadBrlanTpls( anim, theArc );
		bannerBrlans[ brlanName ] = anim;
		bannerObj->AddAnimation( anim );
		bannerObj->SetAnimation( brlanName, 0, -1, -1, false );
		bannerObj->Start();
	}

	brlanName = "banner_Loop";
	anim = LoadAnimation( theArc, brlanName );

	// we have a loop animation
	if( anim )
	{
		layout_banner->LoadBrlanTpls( anim, theArc );
		bannerBrlans[ brlanName ] = anim;
		bannerObj->AddAnimation( anim );
		bannerObj->ScheduleAnimation( brlanName );
		bannerObj->Start();
	}

	// now patch in text from the xml
	SetText( layout_banner, "T_coder", "T_Coded_by", "Line", xml->GetCoder() );
	SetText( layout_banner, "T_release_date", "T_Released", "Line1", xml->GetReleasedate() );
	SetText( layout_banner, "T_version", "T_versiontext", "Line2", xml->GetVersion() );
	SetText( layout_banner, "T_name", NULL, NULL, xml->GetName() );
	SetText( layout_banner, "T_short_descript", NULL, NULL, xml->GetShortDescription() );

	// patch in the png icon
	CreatePngTexture();

	if( rawTexData )
	{
		Texture *tex;
		if( ( tex = layout_banner->FindTexture( "HBPic.tpl" ) ) )
		{
			tex->LoadFromRawData( rawTexData, pngWidth, pngHeight, GX_TF_RGBA8 );
		}

		f32 sc = 1.0f;
		Vec2f v;
		v.x = sc;
		v.y = sc;

		Pane *pane;
		if( (pane = layout_banner->FindPane( "HBPic" ) ) )
		{
			pane->SetScale( v );
			pane->SetSize((f32)pngWidth,(f32)pngHeight);
		}
		if( (pane = layout_banner->FindPane( "HBPicSha" ) ) )
		{
			pane->SetScale( v );
		}
	}
	else
	{
		// theres no icon.png, so hide some panes
		Pane *pane;
		if( (pane = layout_banner->FindPane( "HBPic" ) ) )
		{
			pane->SetVisible( false );
		}
		if( (pane = layout_banner->FindPane( "N_HBPicSha" ) ) )
		{
			pane->SetVisible( false );
		}
	}

	return bannerObj;
}

Object *BannerAsyncHB::LoadIcon()
{
	if(!arc)
		return NULL;

	if( iconObj )
	{
		return iconObj;
	}
	iconReady = false;

	u32 arcLen;
	if( !( icon_bin = arc->GetFileAllocated( "/meta/icon.bin", &arcLen ) ) )
	{
		return NULL;
	}

	U8Archive theArc( icon_bin, arcLen );

	// create layout
	if( !( layout_icon = LoadLayout( theArc, "icon" ) ) )
	{
		return NULL;
	}

	// create object
	iconObj = new Object;
	iconObj->BindPane( layout_icon->FindPane( "RootPane" ) );
	iconObj->BindMaterials( layout_icon->Materials() );

	// get animations
	std::string brlanName = "icon";
	Animation *anim = LoadAnimation( theArc, brlanName );

	// we have a loop animation
	if( anim )
	{
		layout_icon->LoadBrlanTpls( anim, theArc );
		iconBrlans[ brlanName ] = anim;
		iconObj->AddAnimation( anim );
		iconObj->ScheduleAnimation( brlanName );
        if((u32)anim->FrameCount() != 0) // avoid division by 0 -> crash
            iconObj->SetFrame( ((u32)rand()) % ((u32)anim->FrameCount()) );
		iconObj->Start();
	}

	// patch in the png icon
	CreatePngTexture();

	if( rawTexData )
	{
		Texture *tex;
		if( ( tex = layout_icon->FindTexture( "Iconpng.tpl" ) ) )
		{
			tex->LoadFromRawData( rawTexData, pngWidth, pngHeight, GX_TF_RGBA8 );
		}
	}
	else
	{
		// Keep the original WSM blue tile; no stock generic artwork is needed.
	}


	__sync_synchronize();
	iconReady = true;
	return iconObj;
}

void BannerAsyncHB::SetText( Layout *lyt, const char* tboxName, const char *tboxName2, const char *linePane, const char16 *txt )
{
	Textbox *tbox = lyt->FindTextbox( tboxName );
	Pane *pane = NULL;
	if( !tbox )
	{
		return;
	}
	if( !txt || !txt[ 0 ] )
	{
		tbox->SetVisible( false );
		if( tboxName2 && ( pane = lyt->FindPane( tboxName2 ) ) )
		{
			pane->SetVisible( false );
		}
		if( linePane && ( pane = lyt->FindPane( linePane ) ) )
		{
			pane->SetVisible( false );
		}
		return;
	}
	tbox->SetText( txt );
}

void BannerAsyncHB::CreatePngTexture()
{
	if( rawTexData )
	{
		return;
	}
	// load png
	char path[ 0x80 ];
	snprintf( path, sizeof( path ), "%s/icon.png", filepath.c_str() );

	u8* pngData = NULL;
	u32 pngLen;
	if( LoadFileToMem( path, &pngData, &pngLen ) < 1 )
	{
		return;
	}

	// convert png to raw data
	gdImagePtr gdImg = gdImageCreateFromPngPtr( pngLen, pngData );
	if( !gdImg )
	{
		free( pngData );
		return;
	}

	rawTexData = GDImageToRGBA8( &gdImg, &pngWidth, &pngHeight );
	gdImageDestroy( gdImg );

	// done with the png
	free( pngData );
}
