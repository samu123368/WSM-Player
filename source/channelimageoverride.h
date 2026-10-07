#ifndef CHANNEL_IMAGE_OVERRIDE_H
#define CHANNEL_IMAGE_OVERRIDE_H

#include <string>

#include "channelpreviewconfig.h"

class Layout;
class Texture;

// Owns one decoded GX texture for as long as its Banner object can render it.
// The retail BRLYT still supplies the pane, material, UVs and animation; this
// class only replaces that material's image after a complete, bounded decode.
class ChannelImageOverride
{
public:
		enum Target
		{
			PhotoBanner,
			PhotoIcon,
			ForecastImage,
			NintendoIcon,
			MiiContestIcon,
			MiiChannelIcon,
			WiiFitBanner,
			WiiFitIcon
		};

	ChannelImageOverride();
	~ChannelImageOverride();
	// Shared by decoding and rendering so the image covers precisely the space
	// below the native Photo Channel title band, including regional layouts.
	static bool PhotoBannerContentBounds( Layout *layout, float &width,
		float &height, float &centerY );

	// Returns false without touching the retail pane/material when the path is
	// missing, the file is unsupported, or the image exceeds the Wii-safe cap.
	bool Apply( Layout *layout, Target target, const std::string &path,
		ChannelPreview::FitMode fitMode );
	// Wii Fit stores the real Mii portrait used by its channel in RPFitCap.dat.
	// Read that NAND capture and bind it to the retail Mii pane without creating
	// an SD image or uploading the user's Mii data anywhere.
	bool ApplyWiiFitMiiCapture( Layout *layout, Target target,
		int profileIndex );
	// Call only after the associated Layout has stopped rendering (normally
	// from Banner::UnloadBanner/UnloadIcon after deleting that layout).
	void Reset();

	bool Ready() const { return ready; }
	Texture *GetTexture() const { return ready ? texture : NULL; }
	static bool ValidForecastDimensions( unsigned int width, unsigned int height,
		unsigned int orientation = 1 );
	static bool ValidateForecastImage( const std::string &path );
	const std::string &LastError() const { return lastError; }

private:
	ChannelImageOverride( const ChannelImageOverride & );
	ChannelImageOverride &operator=( const ChannelImageOverride & );

	Texture *texture;
	unsigned char *textureData;
	bool ready;
	std::string lastError;
};

#endif // CHANNEL_IMAGE_OVERRIDE_H
