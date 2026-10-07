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
#ifndef _BANNERASYNC_H_
#define _BANNERASYNC_H_

#include <vector>
#include <queue>
#include <string>
#include "Banner.h"
#include "homebrew/homebrewbanner.h"

using namespace std;

class BannerAsync : public Banner
{
public:

	BannerAsync();// dummy constructor to avoid race condition when subclassing this

	BannerAsync( const string &filepath, u64 titleId = 0 );
	virtual ~BannerAsync();
	const std::string &Path() const { return filepath; }
	bool IsLoadComplete() const
	{
		const bool complete = loadComplete;
		__sync_synchronize();
		return complete;
	}
	bool DidLoadFail() const
	{
		__sync_synchronize();
		return loadFailed;
	}


	static void RemoveBanner(BannerAsync *banner);
	static void PrioritizeBanner(BannerAsync *banner);
	// Conservative estimate for retaining completed icons outside the grid.
	u32 CacheBytes() const;
	static void ClearQueue();
	// Stop the loader worker at a point where it is no longer touching NAND or
	// SD.  Content launchers must call this before reloading IOS or unmounting
	// the filesystem.  If launch validation fails, restart it with the matching
	// recovery call so queued icons can continue loading.
	static bool QuiesceForLaunch();
	static void RestartAfterLaunchFailure();
protected:
	string filepath;
	u8 *buffer;
	u32 bufferSize;
	volatile bool loadComplete;
	volatile bool loadFailed;

	static void * BannerAsyncThread(void *arg);
	static void ThreadAdd(BannerAsync* banner);
	static void ThreadRemove(BannerAsync* banner);
	static u32 ThreadInit();
	static u32 ThreadExit();

	static vector<BannerAsync *> List;
	static queue<BannerAsync *> DeleteList;
	static lwp_t Thread;
	static mutex_t ListLock;
	static BannerAsync * InUse;
	static u32 ThreadCount;
	static volatile bool CloseThread;
	static volatile bool ThreadExited;

	static bool LoadBannerFile( BannerAsync *bann );
	static bool LoadWadBanner( BannerAsync *bann );
	static bool LoadNandBanner( BannerAsync *bann );
	static bool LoadContentBinBanner( BannerAsync *bann );
	static bool LoadHomebrewBanner( BannerAsync *bann );
	static void PublishLoadResult( BannerAsync *bann, bool failed );
};

class BannerAsyncHB : public BannerAsync
{
public:
	BannerAsyncHB( const string &filepath, HomebrewXML *hbXml, u64 titleId = 0 );
	~BannerAsyncHB();

	Object *LoadBanner();
	Object *LoadIcon();

protected:
	HomebrewXML *xml;// not allocated or deleted in this class, just a pointer
	u8* rawTexData;// allocated with memalign here
	int pngWidth;
	int pngHeight;

	void CreatePngTexture();

	static void SetText( Layout *lyt, const char* tboxName, const char* tboxName2, const char* linePane, const char16 *txt );
};

#endif /*_GUIIMAGEASYNC_H_*/
