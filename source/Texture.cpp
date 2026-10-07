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

#include <set>
#include <malloc.h>
#include <string.h>
#include "Texture.h"
#include "video.h"

namespace
{
	// GX_TF_RGBA8 is tiled in 4x4 blocks, so one zeroed block is the smallest
	// complete transparent texture in that format.  Keep it cache-line aligned
	// because GX reads it directly through the texture unit.
	u8 TransparentTextureData[4 * 4 * 4] ATTRIBUTE_ALIGN(32) = { 0 };
	GXTexObj TransparentTextureObject;
	bool TransparentTextureInitialized = false;

	inline bool IsAligned32(const void *pointer)
	{
		return (((uintptr_t)pointer) & 31u) == 0;
	}

	u8 *AlignedTextureCopy(const u8 *source, u32 size)
	{
		if(!source || !size)
			return NULL;
		u8 *copy = (u8 *)memalign(32, RU(size, 32));
		if(!copy)
			return NULL;
		memset(copy, 0, RU(size, 32));
		memcpy(copy, source, size);
		return copy;
	}

	inline u8 NonMipmapMinFilter(u32 filter)
	{
		// Mipmap filters on a planar texture can make GX read past level zero.
		return filter == GX_NEAR ? GX_NEAR : GX_LINEAR;
	}

	inline u8 MagnificationFilter(u32 filter)
	{
		return filter == GX_NEAR ? GX_NEAR : GX_LINEAR;
	}
}

Texture::~Texture()
{
	ReleaseOwnedData();
}

void Texture::ReleaseOwnedData()
{
	free(ownedTextureData);
	free(ownedPaletteData);
	ownedTextureData = NULL;
	ownedPaletteData = NULL;
	paletteData = NULL;
}

void Texture::Load(const u8 *file )
{
	ReleaseOwnedData();
	header = NULL;
	palette = NULL;
	loaded = false;
	if(!file)
		return;

	header = (Texture::Header *) file;

	if (header->magic != MAGIC)
		return;	// bad header

	u32 texture_count = header->num_textures;
	// only support a single texture
	if (texture_count > 1)
	{
		// Never saw it happen
		texture_count = 1;
		gprintf("texture count > 1\n");
	}

	// read textures
	const TPL_Texture *tpl_list = (const TPL_Texture *) (file + header->header_size);

	for(u32 i = 0; i < texture_count; i++)
	{
		// seek to texture header
		const TPL_Texture_Header *texture = (const TPL_Texture_Header *) (file + tpl_list[i].texture_offset);

		u8 mipmap = 0;
		u8 bias_clamp = 0;

		if(texture->max_lod > 0)
			mipmap = GX_TRUE;
		if(texture->lod_bias > 0.0f)
			bias_clamp = GX_ENABLE;

		// texture data. GX masks the low five address bits, so a non-aligned
		// homebrew TPL otherwise begins with bytes preceding the image. That is
		// seen as a corrupted strip or block along the texture's top edge.
		u8 *texture_data = (u8 *) (file + texture->offset);
		// Most banner TPLs live inside an archive that has just been decompressed
		// by the CPU.  Flush the complete tiled image before GX DMA sees it; doing
		// this only for user-supplied PNGs left occasional stale pixels in normal
		// Internet Channel/NMenu textures.
		const u32 textureBytes = GX_GetTexBufferSize( texture->width,
			texture->height, texture->format, mipmap, texture->max_lod );
		if(!textureBytes)
			return;
		if(!IsAligned32(texture_data))
		{
			ownedTextureData = AlignedTextureCopy(texture_data, textureBytes);
			if(!ownedTextureData)
				return;
			texture_data = ownedTextureData;
		}
		DCFlushRange(texture_data, textureBytes);

		// seek to/read palette header
		if (tpl_list[i].palette_offset != 0)
		{
			palette = (TPL_Palette_Header *) (file + tpl_list[i].palette_offset);
			const u32 paletteBytes = (u32)palette->num_items * 2;
			if(!paletteBytes)
				return;
			paletteData = file + palette->offset;
			if(!IsAligned32(paletteData))
			{
				ownedPaletteData = AlignedTextureCopy(paletteData, paletteBytes);
				if(!ownedPaletteData)
					return;
				paletteData = ownedPaletteData;
			}
			DCFlushRange((void *)paletteData, paletteBytes);

			// load the texture
			GX_InitTexObjCI(&texobj, texture_data, texture->width, texture->height, texture->format,
							   texture->wrap_s, texture->wrap_t, mipmap, 0);
		}
		else
		{
			// load the texture
			GX_InitTexObj(&texobj, texture_data, texture->width, texture->height, texture->format,
								   texture->wrap_s, texture->wrap_t, mipmap);
		}

		// Respect the authored filter on planar textures too. More importantly,
		// never leave a mipmap minification mode active when only level zero is
		// present, because that samples unrelated memory during minification.
		if(mipmap)
		{
			GX_InitTexObjLOD(&texobj, texture->min, texture->mag,
				texture->min_lod, texture->max_lod, texture->lod_bias,
				bias_clamp, texture->edge_lod, GX_ANISO_1);
		}
		else
		{
			GX_InitTexObjLOD(&texobj, NonMipmapMinFilter(texture->min),
				MagnificationFilter(texture->mag), 0.0f, 0.0f, 0.0f,
				GX_DISABLE, GX_FALSE, GX_ANISO_1);
		}
	}
	loaded = true;
}

void Texture::Apply(u8 &tlutName, u8 map_id, u8 wrap_s, u8 wrap_t) const
{
	if( !loaded )
	{
		gprintf( "Texture::Apply(): not loaded yet\n" );
		return;
	}

	if(tlutName >= 20 || map_id >= 8)
	{
		gprintf( "Texture::Apply(): bad parameters\n" );
		return;
	}

    // create a temporary texture object to not modify the original with the wrap_s and wrap_t parameters
	GXTexObj tempTexObj;
	for(int i = 0; i < 8; ++i)
		tempTexObj.val[i] = texobj.val[i];

	// assume that if there is a palette header, then this format is a CIx one
	if(palette)
	{
		// seek to/read palette data
		// load tlut
		GXTlutObj tlutobj;
		GX_InitTlutObj(&tlutobj, (void *)paletteData, palette->format,
			palette->num_items );
		GX_LoadTlut(&tlutobj, tlutName);
		GX_InitTexObjTlut(&tempTexObj, tlutName);
		tlutName++;
	}

    GX_InitTexObjWrapMode(&tempTexObj, wrap_s, wrap_t);
	GX_LoadTexObj(&tempTexObj, map_id);
}

void Texture::ApplyTransparentFallback(u8 map_id, u8 wrap_s, u8 wrap_t)
{
	if(map_id >= 8)
		return;

	if(!TransparentTextureInitialized)
	{
		DCFlushRange(TransparentTextureData, sizeof(TransparentTextureData));
		GX_InitTexObj(&TransparentTextureObject, TransparentTextureData,
			4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjLOD(&TransparentTextureObject, GX_NEAR, GX_NEAR,
			0.0f, 0.0f, 0.0f, GX_DISABLE, GX_FALSE, GX_ANISO_1);
		TransparentTextureInitialized = true;
	}

	GXTexObj textureObject = TransparentTextureObject;
	GX_InitTexObjWrapMode(&textureObject, wrap_s, wrap_t);
	GX_LoadTexObj(&textureObject, map_id);
}

void Texture::LoadFromRawData( u8 *data, u16 width, u16 height, u8 fmt )
{
	ReleaseOwnedData();
	header = NULL;
	palette = NULL;
	loaded = false;
	if( !data )
	{
		return;
	}
	// GX reads texture memory through DMA.  libgd/custom image buffers have just
	// been written by the CPU, so flush their complete tiled allocation before
	// publishing the object.  Without this, the top cache line can intermittently
	// contain pixels from the previous texture on real hardware.
	const u32 bytes = GX_GetTexBufferSize( width, height, fmt, GX_FALSE, 0 );
	if(!bytes)
		return;
	if(!IsAligned32(data))
	{
		ownedTextureData = AlignedTextureCopy(data, bytes);
		if(!ownedTextureData)
			return;
		data = ownedTextureData;
	}
	DCFlushRange(data, bytes);
	loaded = true;
	GX_InitTexObj( &texobj, data, width, height, fmt, 0, 0, false );
	GX_InitTexObjLOD( &texobj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f,
		GX_DISABLE, GX_FALSE, GX_ANISO_1 );
	//GX_InitTexObjLOD( &texObjs[ texIdx ], 0, 0, 0, 0,
	//						  0, 0, 0, 0 );
}
