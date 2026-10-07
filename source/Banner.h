/*
Copyright (c) 2010 - Wii Banner Player Project
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

#ifndef _BANNER_H_
#define _BANNER_H_

#include "Layout.h"
#include "SystemMenu/object.h"
#include "channelimageoverride.h"
#include "channelpreviewconfig.h"
#include "utils/U8Archive.h"

class Banner
{
public:
	Banner( const u8* data, u32 len );
	virtual ~Banner();

	bool Load( const u8 *data, u32 len );

	virtual Object *LoadBanner();
	virtual Object *LoadIcon();
	// Advance the selected banner plus any channel-specific animation
	// controllers which cannot safely share Object's single AnimationLink.
	void AdvanceBanner();
	// Advance a grid icon, then restore generated News/Forecast panes which the
	// native BRLAN may have hidden or moved on that frame.
	void AdvanceIcon();
	bool LoadSound();

	// delete the banner and icon in case you wanna save memory or something stupid like that
	void UnloadBanner();
	void UnloadIcon();
	void UnloadSound();

	Layout *getBanner() const { return layout_banner; }
	// BannerAsync builds icons on a worker thread. Never expose a partially
	// parsed Layout to the grid while image decode/material and BRLAN setup are
	// still mutating it.
	Layout *getIcon() const { return iconReady ? layout_icon : NULL; }
	const u8 *getSound() const { return sound_bin; }
	u32 getSoundSize() const { return sound_bin_size; }
	// Set this only after a supported Photo Channel image and its GX texture
	// have both loaded successfully. The selected state is applied by the next
	// LoadBanner(); the normal banner is always the default.
	void SetInsertedPhotoImageReady( bool ready ) { hasInsertedPhotoImage = ready; }
	bool HasInsertedPhotoImage() const { return hasInsertedPhotoImage; }
	void SetTitleId( u64 tid )
	{
		const bool changed = titleId != tid;
		titleId = tid;
		// BannerAsync may finish loading before MenuHandler assigns the title ID.
		// In that case LoadBanner() could not identify Forecast/News and skipped
		// the native BRLYT textbox replacement. Apply it as soon as the real ID
		// arrives. Repeated assignments of the same ID do not regenerate content.
		if( changed )
			RefreshGeneratedChannelText();
	}

	const char16 *GetTitle() const;
	const char16 *GetSubTitle() const;
	// Reapply generated Forecast/News data after BRLAN animation updates.
	void RefreshGeneratedChannelText();

protected:
	U8Archive *arc;
	Object *bannerObj;
	Object *iconObj;
	Object *marioKartCarouselObj[ 2 ];
	float marioKartCarouselDelay;
	float marioKartCarouselLoopEnd;
	bool marioKartCarouselStarted;

	std::map< std::string, Animation *>iconBrlans;
	std::map< std::string, Animation *>bannerBrlans;

	//Layout* LoadLayout(const u8 *banner_file, const std::string& lyt_name, const std::string &language);

	Layout *LoadLayout( const U8Archive &theArc, const std::string &lytName );
	Animation *LoadAnimation( const U8Archive &theArc, const std::string &lanName );

	u8 *banner_bin, *icon_bin, *sound_bin;
	u32 sound_bin_size;
	Layout *layout_banner, *layout_icon;
	u64 titleId;
	bool hasInsertedPhotoImage;
	bool bannerCustomImageAttempted;
	bool iconCustomImageAttempted;
	int appliedWiiFitIconProfile;
	volatile bool iconReady;
	u32 appliedPreviewRevision;
	// Keep banner and grid-icon textures independent: the icon remains live
	// while the selected large banner is repeatedly loaded and unloaded.
	ChannelImageOverride bannerImageOverride;
	ChannelImageOverride iconImageOverride;
	std::string forecastBannerImagePath;
	std::string forecastIconImagePath;
	// PAL layouts do not contain compact Japanese materials. Acquire them from
	// the user's local HAFJ resources and retain their backing bytes here.
	Texture forecastJapaneseBannerTextures[ 5 ];
	Texture forecastJapaneseIconTextures[ 5 ];
	std::vector<u8> forecastLocalArt[2][5];
	bool forecastLocalArtAttempted[2];
	void LoadLocalForecastTextures(bool icon);
	// Slot zero is channel metadata; slots 1..12 are the twelve News ticker
	// entries.  Keep this storage owned by Banner because Textbox::SetText stores
	// the pointer rather than copying its contents.
	// News headlines plus Forecast's arbitrary temperature + condition line.
	// The latter can legally combine the 32-byte temperature and 160-byte
	// condition settings, so retain the complete user text in stable storage.
	char16 generatedText[ 13 ][ 256 ];
	char16 nintendoIconText[ 260 ];
	bool nintendoStoryCycle;
	int nintendoStoryIndex;
	void StartNintendoStory();
	// Stable storage for user-authored panes in Today & Tomorrow, Mii Contest
	// and Wii Fit. Textbox::SetText keeps these pointers instead of copying.
	char16 customPreviewText[ 8 ][ 256 ];
	// Textbox::SetText retains its pointer. Keep the three Everybody Votes
	// messages alive for the entire icon-layout lifetime; foreground and shadow
	// panes intentionally share each corresponding immutable buffer.
	char16 everybodyVotesText[ 3 ][ ChannelPreview::MaxEverybodyVotesTextBytes ];

	u8* imetHeader;

	// stupid rso# groups
	void ApplyGeneratedChannelText();
	void ApplyGeneratedChannelIcon();
	void EnsureGeneratedChannelDataCurrent( bool force = false );
};

#endif
