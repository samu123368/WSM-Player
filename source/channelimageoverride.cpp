#include "channelimageoverride.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <gd.h>
#include <ogc/isfs.h>

#include "Layout.h"
#include "Material.h"
#include "Picture.h"
#include "Texture.h"
#include "utils/TextureConverter.h"
#include "utils/tools.h"

namespace
{
	const unsigned long MaxCompressedBytes = 8ul * 1024ul * 1024ul;
	const unsigned long MaxDecodedPixels = 2000000ul;
	const unsigned int MaxSourceDimension = 2048;

	struct ImageInfo
	{
		enum Kind
		{
			Unsupported,
			Png,
			Jpeg
		};

		ImageInfo() : kind( Unsupported ), width( 0 ), height( 0 ), orientation( 1 ) {}
		Kind kind;
		unsigned int width;
		unsigned int height;
		unsigned int orientation;
	};

	unsigned short ReadBE16( const unsigned char *data )
	{
		return ( (unsigned short)data[ 0 ] << 8 ) | data[ 1 ];
	}

	unsigned int ReadBE32( const unsigned char *data )
	{
		return ( (unsigned int)data[ 0 ] << 24 )
			| ( (unsigned int)data[ 1 ] << 16 )
			| ( (unsigned int)data[ 2 ] << 8 ) | data[ 3 ];
	}

	unsigned short Read16( const unsigned char *data, bool littleEndian )
	{
		return littleEndian ? ( data[ 0 ] | ( (unsigned short)data[ 1 ] << 8 ) )
			: ReadBE16( data );
	}

	unsigned int Read32( const unsigned char *data, bool littleEndian )
	{
		if( !littleEndian )
			return ReadBE32( data );
		return data[ 0 ] | ( (unsigned int)data[ 1 ] << 8 )
			| ( (unsigned int)data[ 2 ] << 16 )
			| ( (unsigned int)data[ 3 ] << 24 );
	}

	bool Range( unsigned long offset, unsigned long length, unsigned long size )
	{
		return offset <= size && length <= size - offset;
	}

	unsigned int ReadExifOrientation( const unsigned char *data,
		unsigned long size )
	{
		if( !data || size < 14 || memcmp( data, "Exif\0\0", 6 ) )
			return 1;

		const unsigned char *tiff = data + 6;
		const unsigned long tiffSize = size - 6;
		if( tiffSize < 8 )
			return 1;
		const bool littleEndian = tiff[ 0 ] == 'I' && tiff[ 1 ] == 'I';
		if( !littleEndian && !( tiff[ 0 ] == 'M' && tiff[ 1 ] == 'M' ) )
			return 1;
		if( Read16( tiff + 2, littleEndian ) != 42 )
			return 1;

		const unsigned long ifd = Read32( tiff + 4, littleEndian );
		if( !Range( ifd, 2, tiffSize ) )
			return 1;
		const unsigned int count = Read16( tiff + ifd, littleEndian );
		for( unsigned int i = 0; i < count; ++i )
		{
			const unsigned long entry = ifd + 2 + i * 12ul;
			if( !Range( entry, 12, tiffSize ) )
				break;
			if( Read16( tiff + entry, littleEndian ) != 0x0112
				|| Read16( tiff + entry + 2, littleEndian ) != 3
				|| Read32( tiff + entry + 4, littleEndian ) != 1 )
				continue;
			const unsigned int orientation = Read16( tiff + entry + 8,
				littleEndian );
			return orientation >= 1 && orientation <= 8 ? orientation : 1;
		}
		return 1;
	}

	bool InspectImage( const unsigned char *data, unsigned long size,
		ImageInfo &info )
	{
		static const unsigned char pngMagic[] = {
			0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
		};
		if( Range( 0, 24, size ) && !memcmp( data, pngMagic, sizeof( pngMagic ) )
			&& !memcmp( data + 12, "IHDR", 4 ) )
		{
			info.kind = ImageInfo::Png;
			info.width = ReadBE32( data + 16 );
			info.height = ReadBE32( data + 20 );
			return info.width && info.height;
		}

		if( !Range( 0, 4, size ) || data[ 0 ] != 0xff || data[ 1 ] != 0xd8 )
			return false;
		info.kind = ImageInfo::Jpeg;
		unsigned long offset = 2;
		while( offset + 4 <= size )
		{
			while( offset < size && data[ offset ] != 0xff )
				++offset;
			while( offset < size && data[ offset ] == 0xff )
				++offset;
			if( offset >= size )
				break;
			const unsigned char marker = data[ offset++ ];
			if( marker == 0xd8 || marker == 0xd9 || marker == 0x01
				|| ( marker >= 0xd0 && marker <= 0xd7 ) )
				continue;
			if( !Range( offset, 2, size ) )
				break;
			const unsigned int segmentLength = ReadBE16( data + offset );
			if( segmentLength < 2 || !Range( offset, segmentLength, size ) )
				break;
			const unsigned char *payload = data + offset + 2;
			const unsigned int payloadLength = segmentLength - 2;
			if( marker == 0xe1 && payloadLength >= 6
				&& !memcmp( payload, "Exif\0\0", 6 ) )
				info.orientation = ReadExifOrientation( payload, payloadLength );

			const bool startOfFrame = ( marker >= 0xc0 && marker <= 0xc3 )
				|| ( marker >= 0xc5 && marker <= 0xc7 )
				|| ( marker >= 0xc9 && marker <= 0xcb )
				|| ( marker >= 0xcd && marker <= 0xcf );
			if( startOfFrame && payloadLength >= 5 )
			{
				info.height = ReadBE16( payload + 1 );
				info.width = ReadBE16( payload + 3 );
				return info.width && info.height;
			}
			offset += segmentLength;
		}
		return false;
	}

	bool ReadBoundedFile( const std::string &path, unsigned char **out,
		unsigned long *outSize, std::string &error )
	{
		*out = NULL;
		*outSize = 0;
		FILE *file = fopen( path.c_str(), "rb" );
		if( !file )
		{
			error = "image file is missing";
			return false;
		}
		if( fseek( file, 0, SEEK_END ) )
		{
			fclose( file );
			error = "image file size could not be read";
			return false;
		}
		const long fileLength = ftell( file );
		if( fileLength <= 0 )
		{
			fclose( file );
			error = "image file is empty";
			return false;
		}
		if( (unsigned long)fileLength > MaxCompressedBytes
			|| fseek( file, 0, SEEK_SET ) )
		{
			fclose( file );
			error = "image file exceeds the 8 MiB safety cap";
			return false;
		}
		unsigned char *data = (unsigned char *)memalign( 32,
			(unsigned long)fileLength );
		if( !data )
		{
			fclose( file );
			error = "not enough memory for image file";
			return false;
		}
		const size_t read = fread( data, 1, fileLength, file );
		fclose( file );
		if( read != (size_t)fileLength )
		{
			free( data );
			error = "image file could not be read completely";
			return false;
		}
		*out = data;
		*outSize = (unsigned long)fileLength;
		return true;
	}

	void OrientedToSource( unsigned int orientation, int sourceWidth,
		int sourceHeight, int orientedX, int orientedY, int &sourceX,
		int &sourceY )
	{
		switch( orientation )
		{
		case 2: sourceX = sourceWidth - 1 - orientedX; sourceY = orientedY; break;
		case 3: sourceX = sourceWidth - 1 - orientedX;
			sourceY = sourceHeight - 1 - orientedY; break;
		case 4: sourceX = orientedX; sourceY = sourceHeight - 1 - orientedY; break;
		case 5: sourceX = orientedY; sourceY = orientedX; break;
		case 6: sourceX = orientedY;
			sourceY = sourceHeight - 1 - orientedX; break;
		case 7: sourceX = sourceWidth - 1 - orientedY;
			sourceY = sourceHeight - 1 - orientedX; break;
		case 8: sourceX = sourceWidth - 1 - orientedY; sourceY = orientedX; break;
		default: sourceX = orientedX; sourceY = orientedY; break;
		}
	}

	gdImagePtr ScaleAndOrient( gdImagePtr source, unsigned int orientation,
		int destinationWidth, int destinationHeight,
		ChannelPreview::FitMode fitMode, bool alignTop,
		double displayWidth = 0.0, double displayHeight = 0.0, bool transparentBorder = false )
	{
		if( !source || destinationWidth <= 0 || destinationHeight <= 0 )
			return NULL;
		const int sourceWidth = gdImageSX( source );
		const int sourceHeight = gdImageSY( source );
		const bool swapAxes = orientation >= 5 && orientation <= 8;
		const int orientedWidth = swapAxes ? sourceHeight : sourceWidth;
		const int orientedHeight = swapAxes ? sourceWidth : sourceHeight;
		if( displayWidth <= 0.0 ) displayWidth = destinationWidth;
		if( displayHeight <= 0.0 ) displayHeight = destinationHeight;

		gdImagePtr destination = gdImageCreateTrueColor( destinationWidth,
			destinationHeight );
		if( !destination )
			return NULL;
		gdImageAlphaBlending( destination, 0 );
		gdImageSaveAlpha( destination, 1 );
		gdImageFilledRectangle( destination, 0, 0, destinationWidth - 1,
			destinationHeight - 1, gdTrueColorAlpha( 0, 0, 0,
				transparentBorder ? gdAlphaTransparent : gdAlphaOpaque ) );

		// GX dimensions are rounded to four-pixel tiles. Compute cover-cropping
		// in the actual displayed pane, then sample that area into the texture.
		const double scaleX = displayWidth / orientedWidth;
		const double scaleY = displayHeight / orientedHeight;
		const double scale = fitMode == ChannelPreview::Fill
			? std::max( scaleX, scaleY ) : std::min( scaleX, scaleY );
		const double drawnWidth = orientedWidth * scale;
		const double drawnHeight = orientedHeight * scale;
		const double left = ( displayWidth - drawnWidth ) * 0.5;
		// Full-bleed Photo banners preserve the top of portrait and 4:3 photos.
		// Cover-cropping still removes any unused canvas, but the important upper
		// portion no longer disappears above the banner viewport.
		const double top = alignTop && drawnHeight > displayHeight
			? 0.0 : ( displayHeight - drawnHeight ) * 0.5;

		for( int y = 0; y < destinationHeight; ++y )
		{
			const double displayY = ( y + 0.5 ) * displayHeight / destinationHeight;
			const double orientedYFloat = ( displayY - top ) / scale - 0.5;
			// Upscaling maps the first pixel centre slightly outside source pixel
			// centres. Fill clamps to that edge; skipping it would leave a black seam.
			if( fitMode != ChannelPreview::Fill
				&& ( displayY < top || displayY >= top + drawnHeight ) )
				continue;
			const int orientedY = std::min( orientedHeight - 1,
				std::max( 0, (int)( orientedYFloat + 0.5 ) ) );
			for( int x = 0; x < destinationWidth; ++x )
			{
				const double displayX = ( x + 0.5 ) * displayWidth / destinationWidth;
				const double orientedXFloat = ( displayX - left ) / scale - 0.5;
				if( fitMode != ChannelPreview::Fill
					&& ( displayX < left || displayX >= left + drawnWidth ) )
					continue;
				const int orientedX = std::min( orientedWidth - 1,
					std::max( 0, (int)( orientedXFloat + 0.5 ) ) );
				int sourceX = 0;
				int sourceY = 0;
				OrientedToSource( orientation, sourceWidth, sourceHeight,
					orientedX, orientedY, sourceX, sourceY );
				if( sourceX >= 0 && sourceX < sourceWidth
					&& sourceY >= 0 && sourceY < sourceHeight )
					gdImageSetPixel( destination, x, y,
						gdImageGetTrueColorPixel( source, sourceX, sourceY ) );
			}
		}
		return destination;
	}

	bool TargetParameters( ChannelImageOverride::Target target,
		const char *const *&paneNames, int &paneCount,
		int &textureWidth, int &textureHeight,
		float &paneWidth, float &paneHeight )
	{
		static const char *photo[] = { "pic_photo" };
		static const char *forecast[] = { "J_sun_00", "W_sun_00" };
		static const char *nintendo[] = { "P_scShot_00" };
		static const char *miiContest[] = {
			"P_iconPhoto_00", "P_photo_00", "P_comPhoto_00"
		};
		static const char *miiChannel[] = { "Picture_00" };
		// Wii Fit/Plus revisions use different labels. These candidates are only
		// tried after Banner has positively identified an RFN?/RFP? title.
		static const char *wiiFitBanner[] = {
			"P_mii_00", "P_Mii_00", "pic_mii", "Picture_00"
		};
		static const char *wiiFitIcon[] = {
			"P_MiiFace_00", "P_mii_00", "P_Mii_00", "pic_mii",
			"Picture_00"
		};
		paneCount = 1;
		switch( target )
		{
		case ChannelImageOverride::ForecastImage:
			paneNames = forecast; paneCount = 2; break;
		case ChannelImageOverride::PhotoBanner:
			paneNames = photo;
			// Apply obtains the exact region from the native title-band pane.
			textureWidth = textureHeight = 0;
			paneWidth = paneHeight = 0.0f;
			return true;
		case ChannelImageOverride::PhotoIcon:
			paneNames = photo;
			// GX RGBA8 is tiled in four-pixel blocks; keep the authored pane at
			// 170x96 while padding the backing texture to the next full block.
			textureWidth = 172;
			textureHeight = 96;
			paneWidth = 170.0f;
			paneHeight = 96.0f;
			return true;
		case ChannelImageOverride::NintendoIcon:
			// Nintendo Channel's downloaded-data module fills this first hidden
			// story screenshot. The authored pane is 170x66; pad the backing GX
			// RGBA8 texture to complete four-pixel tiles without resizing the pane.
			paneNames = nintendo;
			textureWidth = 172;
			textureHeight = 68;
			paneWidth = 170.0f;
			paneHeight = 66.0f;
			return true;
		case ChannelImageOverride::MiiContestIcon:
			paneNames = miiContest; paneCount = 3; break;
		case ChannelImageOverride::MiiChannelIcon:
			paneNames = miiChannel; break;
		case ChannelImageOverride::WiiFitBanner:
			paneNames = wiiFitBanner; paneCount = 4; break;
		case ChannelImageOverride::WiiFitIcon:
			paneNames = wiiFitIcon; paneCount = 5; break;
		default:
			return false;
		}
		// Zero selects the authored pane dimensions after it has been found.
		textureWidth = textureHeight = 0;
		paneWidth = paneHeight = 0.0f;
		return true;
	}

	u16 ReadTiledRgb5A3( const unsigned char *data, int width, int x, int y )
	{
		const unsigned int block = ( ( y >> 2 ) * ( width >> 2 ) + ( x >> 2 ) )
			* 32;
		const unsigned int pixel = ( ( y & 3 ) * 4 + ( x & 3 ) ) * 2;
		return ( (u16)data[ block + pixel ] << 8 )
			| data[ block + pixel + 1 ];
	}

	void DecodeRgb5A3( u16 pixel, u8 &red, u8 &green, u8 &blue, u8 &alpha )
	{
		if( pixel & 0x8000 )
		{
			red = ( ( pixel >> 10 ) & 0x1f ) * 255 / 31;
			green = ( ( pixel >> 5 ) & 0x1f ) * 255 / 31;
			blue = ( pixel & 0x1f ) * 255 / 31;
			alpha = 255;
		}
		else
		{
			alpha = ( ( pixel >> 12 ) & 7 ) * 255 / 7;
			red = ( ( pixel >> 8 ) & 15 ) * 17;
			green = ( ( pixel >> 4 ) & 15 ) * 17;
			blue = ( pixel & 15 ) * 17;
		}
	}

	void WriteTiledRgba8( unsigned char *data, int width, int x, int y,
		u8 red, u8 green, u8 blue, u8 alpha )
	{
		const unsigned int block = ( ( y >> 2 ) * ( width >> 2 ) + ( x >> 2 ) )
			* 64;
		const unsigned int pixel = ( ( y & 3 ) * 4 + ( x & 3 ) ) * 2;
		data[ block + pixel ] = alpha;
		data[ block + pixel + 1 ] = red;
		data[ block + 32 + pixel ] = green;
		data[ block + 32 + pixel + 1 ] = blue;
	}

	bool ReadWiiFitCapture( unsigned char *capture, unsigned int captureBytes,
		std::string &error )
	{
		static const char *paths[] = {
			"/title/00010004/52464e50/data/RPFitCap.dat", // PAL RFNP
			"/title/00010004/52464e45/data/RPFitCap.dat", // USA RFNE
			"/title/00010004/52464e4a/data/RPFitCap.dat", // Japan RFNJ
			"/title/00010004/52464e4b/data/RPFitCap.dat", // Korea RFNK
			"/title/00010004/52465050/data/RPFitCap.dat", // Wii Fit Plus PAL
			"/title/00010004/52465045/data/RPFitCap.dat",
			"/title/00010004/5246504a/data/RPFitCap.dat"
		};
		u8 header[ 32 ] ATTRIBUTE_ALIGN( 32 );
		for( unsigned int i = 0; i < sizeof( paths ) / sizeof( paths[ 0 ] ); ++i )
		{
			const s32 descriptor = ISFS_Open( paths[ i ], ISFS_OPEN_READ );
			if( descriptor < 0 ) continue;
			memset( header, 0, sizeof( header ) );
			const s32 headerRead = ISFS_Read( descriptor, header, sizeof( header ) );
			const bool valid = headerRead >= 8 && !memcmp( header, "RPCP0000", 8 );
			// Eight complete 64x64 RGB5A3 Mii portraits occupy 0x22000..0x31fff.
			// 0x32000 begins a different capture table and must not be decoded as a
			// face (that was the source of WSM Player's scrambled/squashed Mii).
			const s32 seek = valid ? ISFS_Seek( descriptor, 0x22000, SEEK_SET ) : -1;
			const s32 read = seek == 0x22000
				? ISFS_Read( descriptor, capture, captureBytes ) : -1;
			ISFS_Close( descriptor );
			if( read == (s32)captureBytes ) return true;
		}
		error = "no valid Wii Fit Mii capture was found on NAND";
		return false;
	}

}

ChannelImageOverride::ChannelImageOverride()
	: texture( NULL ), textureData( NULL ), ready( false )
{
}

bool ChannelImageOverride::PhotoBannerContentBounds( Layout *layout,
	float &width, float &height, float &centerY )
{
	Pane *band = layout ? layout->FindPane( "belt_c" ) : NULL;
	if( !band ) return false;
	width = layout->GetWidth();
	// Cover the complete layout, not just the area below belt_c. The authored
	// belt and both logo layers are drawn over this background by Banner.
	height = layout->GetHeight();
	centerY = 0.0f;
	return std::isfinite( width ) && std::isfinite( height )
		&& std::isfinite( centerY ) && width > 0.0f && width <= 1024.0f
		&& height > 0.0f && height <= layout->GetHeight()
		&& height <= 1024.0f;
}

ChannelImageOverride::~ChannelImageOverride()
{
	// Banner destroys its Layout explicitly in its destructor body. Do not
	// dereference the old material here; it can already be gone.
	Reset();
}

void ChannelImageOverride::Reset()
{
	delete texture;
	free( textureData );
	texture = NULL;
	textureData = NULL;
	ready = false;
	lastError.clear();
}

bool ChannelImageOverride::ValidForecastDimensions( unsigned int width,
	unsigned int height, unsigned int orientation )
{
	if( !width || !height || width > MaxSourceDimension || height > MaxSourceDimension
		|| (unsigned long)width * height > MaxDecodedPixels ) return false;
	if( orientation >= 5 && orientation <= 8 ) std::swap( width, height );
	return width == height || width * 3u == height * 4u;
}

bool ChannelImageOverride::ValidateForecastImage( const std::string &path )
{
	unsigned char *data = NULL;
	unsigned long size = 0;
	std::string error;
	if( !ReadBoundedFile( path, &data, &size, error ) ) return false;
	ImageInfo info;
	const bool valid = InspectImage( data, size, info )
		&& ValidForecastDimensions( info.width, info.height, info.orientation );
	free( data );
	return valid;
}

bool ChannelImageOverride::Apply( Layout *layout, Target target,
	const std::string &path, ChannelPreview::FitMode fitMode )
{
	ready = false;
	lastError.clear();
	if( !layout || path.empty() )
	{
		lastError = path.empty() ? "no image selected" : "layout is unavailable";
		return false;
	}

	const char *const *paneNames = NULL;
	int paneCount = 0;
	int textureWidth = 0;
	int textureHeight = 0;
	float paneWidth = 0.0f;
	float paneHeight = 0.0f;
	if( !TargetParameters( target, paneNames, paneCount, textureWidth, textureHeight,
		paneWidth, paneHeight ) )
	{
		lastError = "unsupported image target";
		return false;
	}
	Picture *picture = NULL;
	const char *paneName = NULL;
	for( int i = 0; i < paneCount && !picture; ++i )
	{
		picture = dynamic_cast< Picture * >( layout->FindPane( paneNames[ i ] ) );
		if( picture ) paneName = paneNames[ i ];
	}
	if( !picture )
	{
		lastError = "native image pane was not found";
		return false;
	}
	if( textureWidth <= 0 || textureHeight <= 0 )
	{
		if( target == PhotoBanner )
		{
			float centerY;
			if( !PhotoBannerContentBounds( layout, paneWidth, paneHeight, centerY ) )
			{
				lastError = "native Photo title-band bounds are invalid";
				return false;
			}
		}
		else
		{
			paneWidth = picture->GetWidth();
			paneHeight = picture->GetHeight();
		}
		textureWidth = ( (int)( paneWidth + 0.999f ) + 3 ) & ~3;
		textureHeight = ( (int)( paneHeight + 0.999f ) + 3 ) & ~3;
		if( textureWidth < 4 || textureHeight < 4
			|| textureWidth > 1024 || textureHeight > 1024 )
		{
			lastError = "native image pane has unsafe dimensions";
			return false;
		}
	}
	const int materialIndex = picture->GetMaterialIndex();
	const MaterialList &materials = layout->Materials();
	if( materialIndex < 0 || (unsigned int)materialIndex >= materials.size()
		|| !materials[ materialIndex ] )
	{
		lastError = "native image material was not found";
		return false;
	}

	unsigned char *compressed = NULL;
	unsigned long compressedSize = 0;
	if( !ReadBoundedFile( path, &compressed, &compressedSize, lastError ) )
		return false;

	ImageInfo info;
	if( !InspectImage( compressed, compressedSize, info ) )
	{
		free( compressed );
		lastError = "only valid JPEG and PNG images are supported";
		return false;
	}
	const unsigned long pixels = (unsigned long)info.width * info.height;
	if( target == ForecastImage
		&& !ValidForecastDimensions( info.width, info.height, info.orientation ) )
	{
		free( compressed );
		lastError = "Forecast images must be 1:1 or 4:3 and within the Wii safety cap";
		return false;
	}
	if( info.width > MaxSourceDimension || info.height > MaxSourceDimension
		|| pixels > MaxDecodedPixels )
	{
		free( compressed );
		lastError = "image dimensions exceed the 2-megapixel Wii safety cap";
		return false;
	}

	gdImagePtr source = info.kind == ImageInfo::Png
		? gdImageCreateFromPngPtr( compressedSize, compressed )
		: gdImageCreateFromJpegPtr( compressedSize, compressed );
	free( compressed );
	if( !source )
	{
		lastError = "JPEG/PNG decoder rejected the image";
		return false;
	}
	if( gdImageSX( source ) != (int)info.width
		|| gdImageSY( source ) != (int)info.height )
	{
		gdImageDestroy( source );
		lastError = "decoded image dimensions do not match its header";
		return false;
	}

	gdImagePtr canvas = ScaleAndOrient( source, info.orientation,
		textureWidth, textureHeight, fitMode,
		target == ChannelImageOverride::PhotoBanner
			&& fitMode == ChannelPreview::Fill,
		target == PhotoBanner || target == ForecastImage ? paneWidth : 0.0,
		target == PhotoBanner || target == ForecastImage ? paneHeight : 0.0,
		target == ForecastImage );
	gdImageDestroy( source );
	if( !canvas )
	{
		lastError = "not enough memory to resize the image";
		return false;
	}
	int convertedWidth = 0;
	int convertedHeight = 0;
	unsigned char *candidateData = GDImageToRGBA8( &canvas, &convertedWidth,
		&convertedHeight );
	gdImageDestroy( canvas );
	if( !candidateData || convertedWidth != textureWidth
		|| convertedHeight != textureHeight )
	{
		free( candidateData );
		lastError = "could not create a Wii RGBA8 texture";
		return false;
	}

	Texture *candidateTexture = new Texture;
	if( !candidateTexture )
	{
		free( candidateData );
		lastError = "not enough memory for the GX texture";
		return false;
	}
	candidateTexture->LoadFromRawData( candidateData, convertedWidth,
		convertedHeight, GX_TF_RGBA8 );

	// This is the commit point. Nothing above changed the authored layout.
	// Swap the material first so deleting an older texture cannot leave the
	// current pane with a dangling forced-texture pointer.
	materials[ materialIndex ]->SetForcedTexture( candidateTexture );
	delete texture;
	free( textureData );
	texture = candidateTexture;
	textureData = candidateData;
	// Set the target's native replacement size.  Banner-specific presentation
	// code may subsequently reposition the pane (Photo banner uses full bleed).
	picture->SetSize( paneWidth, paneHeight );
	picture->SetVisible( true );
	picture->SetHide( false );
	ready = true;
	gprintf( "Channel image: %s -> %s (%dx%d, %s)\n", path.c_str(),
		paneName, convertedWidth, convertedHeight,
		fitMode == ChannelPreview::Fill ? "fill" : "fit" );
	return true;
}

bool ChannelImageOverride::ApplyWiiFitMiiCapture( Layout *layout, Target target,
	int profileIndex )
{
	ready = false;
	lastError.clear();
	if( !layout || ( target != WiiFitBanner && target != WiiFitIcon ) )
	{
		lastError = !layout ? "layout is unavailable" : "target is not Wii Fit";
		return false;
	}

	const char *const *paneNames = NULL;
	int paneCount = 0;
	int textureWidth = 0;
	int textureHeight = 0;
	float paneWidth = 0.0f;
	float paneHeight = 0.0f;
	if( !TargetParameters( target, paneNames, paneCount, textureWidth,
		textureHeight, paneWidth, paneHeight ) )
		return false;
	Picture *picture = NULL;
	const char *paneName = NULL;
	for( int i = 0; i < paneCount && !picture; ++i )
	{
		picture = dynamic_cast< Picture * >( layout->FindPane( paneNames[ i ] ) );
		if( picture ) paneName = paneNames[ i ];
	}
	if( !picture )
	{
		lastError = "native Wii Fit Mii pane was not found";
		return false;
	}
	paneWidth = picture->GetWidth();
	paneHeight = picture->GetHeight();
	textureWidth = ( (int)( paneWidth + 0.999f ) + 3 ) & ~3;
	textureHeight = ( (int)( paneHeight + 0.999f ) + 3 ) & ~3;
	if( textureWidth < 4 || textureHeight < 4
		|| textureWidth > 128 || textureHeight > 128 )
	{
		lastError = "native Wii Fit Mii pane has unsafe dimensions";
		return false;
	}
	const int materialIndex = picture->GetMaterialIndex();
	const MaterialList &materials = layout->Materials();
	if( materialIndex < 0 || (unsigned int)materialIndex >= materials.size()
		|| !materials[ materialIndex ] )
	{
		lastError = "native Wii Fit Mii material was not found";
		return false;
	}

	// RPFitCap stores eight independently tiled 64x64 RGB5A3 portraits starting
	// at 0x22000. Each cell already contains the complete, rendered Mii face.
	const int sourceWidth = 64;
	const int sourceHeight = 64;
	const int profileCount = 8;
	const unsigned int cellBytes = sourceWidth * sourceHeight * 2;
	const unsigned int captureBytes = profileCount * cellBytes;
	unsigned char *capture = (unsigned char *)memalign( 32, captureBytes );
	if( !capture )
	{
		lastError = "not enough memory for Wii Fit Mii capture";
		return false;
	}
	if( !ReadWiiFitCapture( capture, captureBytes, lastError ) )
	{
		free( capture );
		return false;
	}

	profileIndex = std::max( 0, std::min( profileCount - 1, profileIndex ) );
	// A selected Mii can exist in RFL_DB before Wii Fit has captured that profile.
	// Prefer its matching cell, then fall back to the first populated profile
	// rather than binding an all-transparent texture.
	int selectedCell = profileIndex;
	bool populated = false;
	for( int pass = 0; pass < profileCount && !populated; ++pass )
	{
		int slot = profileIndex;
		if( pass > 0 )
		{
			slot = pass - 1;
			if( slot >= profileIndex ) ++slot;
		}
		if( slot < 0 || slot >= profileCount ) continue;
		const unsigned char *cell = capture + slot * cellBytes;
		int visibleSourcePixels = 0;
		for( int y = 0; y < sourceHeight; ++y )
			for( int x = 0; x < sourceWidth; ++x )
			{
				u8 red, green, blue, alpha;
				DecodeRgb5A3( ReadTiledRgb5A3( cell, sourceWidth, x, y ),
					red, green, blue, alpha );
				if( alpha > 8 ) ++visibleSourcePixels;
			}
		// Empty Wii Fit slots can retain a small shirt/edge capture. A complete
		// face occupies far more than 256 pixels; reject those fragments so the
		// first real Mii is used as the safe fallback.
		if( visibleSourcePixels >= 256 )
		{
			populated = true;
			selectedCell = slot;
		}
	}
	if( !populated )
	{
		free( capture );
		lastError = "Wii Fit Mii capture contains no populated profiles";
		return false;
	}
	const unsigned char *cell = capture + selectedCell * cellBytes;

	const unsigned int outputBytes = GX_GetTexBufferSize( textureWidth,
		textureHeight, GX_TF_RGBA8, GX_FALSE, 0 );
	unsigned char *candidateData = (unsigned char *)memalign( 32, outputBytes );
	if( !candidateData )
	{
		free( capture );
		lastError = "not enough memory for Wii Fit Mii texture";
		return false;
	}
	memset( candidateData, 0, outputBytes );
	// Crop only transparent padding, then use one uniform scale for both axes.
	// This preserves the selected Mii's proportions in the 64x64 icon instead
	// of applying non-uniform pane scaling or pinning it to the top.
	// EFB copies can leave a few isolated horizontal pixels above the face.
	// Locate the first sustained run of portrait rows so those capture crumbs do
	// not enlarge the bounding box and push the real Mii toward the icon bottom.
	int portraitTop = 0;
	int portraitRun = 0;
	for( int sourceY = 0; sourceY < sourceHeight; ++sourceY )
	{
		int rowPixels = 0;
		for( int sourceX = 0; sourceX < sourceWidth; ++sourceX )
		{
			u8 red, green, blue, alpha;
			DecodeRgb5A3( ReadTiledRgb5A3( cell, sourceWidth,
				sourceX, sourceY ), red, green, blue, alpha );
			if( alpha > 8 ) ++rowPixels;
		}
		portraitRun = rowPixels >= 8 ? portraitRun + 1 : 0;
		if( portraitRun >= 4 )
		{
			portraitTop = sourceY - portraitRun + 1;
			break;
		}
	}
	int cropLeft = sourceWidth;
	int cropTop = sourceHeight;
	int cropRight = -1;
	int cropBottom = -1;
	for( int sourceY = portraitTop; sourceY < sourceHeight; ++sourceY )
		for( int sourceX = 0; sourceX < sourceWidth; ++sourceX )
		{
			u8 red, green, blue, alpha;
			DecodeRgb5A3( ReadTiledRgb5A3( cell, sourceWidth,
				sourceX, sourceY ), red, green, blue, alpha );
			if( alpha <= 8 ) continue;
			cropLeft = std::min( cropLeft, sourceX );
			cropTop = std::min( cropTop, sourceY );
			cropRight = std::max( cropRight, sourceX );
			cropBottom = std::max( cropBottom, sourceY );
		}
	if( cropRight < cropLeft || cropBottom < cropTop )
	{
		free( capture );
		free( candidateData );
		lastError = "Wii Fit Mii capture is empty";
		return false;
	}
	cropLeft = std::max( 0, cropLeft - 1 );
	cropTop = std::max( 0, cropTop - 1 );
	cropRight = std::min( sourceWidth - 1, cropRight + 1 );
	cropBottom = std::min( sourceHeight - 1, cropBottom + 1 );
	const int croppedWidth = cropRight - cropLeft + 1;
	const int croppedHeight = cropBottom - cropTop + 1;
	// The icon's stock MiiFace artwork leaves more air around the head than the
	// banner portrait. Keep the same correct aspect ratio, but inset only the
	// icon capture by another ten percent so it fits the authored balloon/frame.
	const float portraitInset = target == WiiFitIcon ? 0.90f : 1.0f;
	const int availableWidth = std::max( 1,
		(int)( ( textureWidth - 4 ) * portraitInset + 0.5f ) );
	const int availableHeight = std::max( 1,
		(int)( ( textureHeight - 4 ) * portraitInset + 0.5f ) );
	const float scale = std::min( availableWidth / (float)croppedWidth,
		availableHeight / (float)croppedHeight );
	const int drawnWidth = std::max( 1,
		(int)( croppedWidth * scale + 0.5f ) );
	const int drawnHeight = std::max( 1,
		(int)( croppedHeight * scale + 0.5f ) );
	const int left = ( textureWidth - drawnWidth ) / 2;
	const int top = ( textureHeight - drawnHeight ) / 2;
	int visiblePixels = 0;
	for( int y = 0; y < drawnHeight; ++y )
		for( int x = 0; x < drawnWidth; ++x )
		{
			const int sourceX = std::min( cropRight,
				cropLeft + x * croppedWidth / drawnWidth );
			const int sourceY = std::min( cropBottom,
				cropTop + y * croppedHeight / drawnHeight );
			u8 red, green, blue, alpha;
			DecodeRgb5A3( ReadTiledRgb5A3( cell, sourceWidth, sourceX, sourceY ),
				red, green, blue, alpha );
			if( alpha > 8 ) ++visiblePixels;
			WriteTiledRgba8( candidateData, textureWidth, left + x, top + y,
				red, green, blue, alpha );
		}
	free( capture );
	if( visiblePixels < 8 )
	{
		free( candidateData );
		lastError = "Wii Fit Mii capture is empty";
		return false;
	}

	Texture *candidateTexture = new Texture;
	if( !candidateTexture )
	{
		free( candidateData );
		lastError = "not enough memory for Wii Fit Mii GX texture";
		return false;
	}
	candidateTexture->LoadFromRawData( candidateData, textureWidth,
		textureHeight, GX_TF_RGBA8 );
	materials[ materialIndex ]->SetForcedTexture( candidateTexture );
	delete texture;
	free( textureData );
	texture = candidateTexture;
	textureData = candidateData;
	picture->SetSize( paneWidth, paneHeight );
	picture->SetVisible( true );
	picture->SetHide( false );
	ready = true;
	gprintf( "Wii Fit Mii profile %d -> %s (%dx%d, aspect preserved)\n",
		selectedCell, paneName, textureWidth, textureHeight );
	return true;
}
