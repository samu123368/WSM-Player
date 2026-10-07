/***************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * for WiiXplorer 2010
 ***************************************************************************/
#include <string.h>
#include "WavDecoder.hpp"
#include "tools.h"

#define le16(i) ((((u16) ((i) & 0xFF)) << 8) | ((u16) (((i) & 0xFF00) >> 8)))
#define le32(i) ((((u32)le16((i) & 0xFFFF)) << 16) | ((u32)le16(((i) & 0xFFFF0000) >> 16)))

namespace
{
u32 WavLE32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
u16 WavLE16(const u8 *p) { return (u16)p[0] | ((u16)p[1] << 8); }
}

WavDecoder::WavDecoder(const char * filepath)
	: SoundDecoder(filepath)
{
	SoundType = SOUND_WAV;
	SampleRate = 48000;
	Format = VOICE_STEREO_16BIT;

	if(!file_fd)
		return;

	OpenFile();
}

WavDecoder::WavDecoder(const u8 * snd, int len)
	: SoundDecoder(snd, len)
{
	SoundType = SOUND_WAV;
	SampleRate = 48000;
	Format = VOICE_STEREO_16BIT;

	if(!file_fd)
		return;

	OpenFile();
}

WavDecoder::~WavDecoder()
{
}

void WavDecoder::OpenFile()
{
	DataOffset = DataSize = LoopStartBytes = LoopEndBytes = 0;
	EmbeddedLoop = false;
	u8 header[12];
	if(file_fd->size() < 12 || file_fd->read(header, sizeof(header)) != sizeof(header)
		|| memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4))
	{
		CloseFile();
		return;
	}
	const u32 riffSize = WavLE32(header + 4);
	// Bound every chunk before seeking; a corrupt size must never wrap around
	// or leave a decoder thread spinning through a malformed banner sound.
	if(riffSize < 4 || riffSize > (u32)file_fd->size() - 8)
	{
		CloseFile();
		return;
	}
	const u32 end = riffSize + 8;
	u16 channels = 0, bits = 0, alignment = 0;
	u32 loopStart = 0, loopEnd = 0;
	bool hasFormat = false, hasData = false, hasLoop = false;
	for(u32 offset = 12; offset <= end && end - offset >= 8; )
	{
		u8 chunk[8];
		file_fd->seek(offset, SEEK_SET);
		if(file_fd->read(chunk, sizeof(chunk)) != sizeof(chunk)) { CloseFile(); return; }
		const u32 size = WavLE32(chunk + 4), payload = offset + 8;
		if(size > end - payload) { CloseFile(); return; }
		if(!memcmp(chunk, "fmt ", 4) && !hasFormat)
		{
			u8 fmt[16];
			if(size < sizeof(fmt) || file_fd->read(fmt, sizeof(fmt)) != sizeof(fmt)
				|| WavLE16(fmt) != 1) { CloseFile(); return; }
			channels = WavLE16(fmt + 2); SampleRate = WavLE32(fmt + 4);
			alignment = WavLE16(fmt + 12); bits = WavLE16(fmt + 14);
			hasFormat = true;
		}
		else if(!memcmp(chunk, "data", 4) && !hasData)
		{
			DataOffset = payload; DataSize = size; hasData = true;
		}
		else if(!memcmp(chunk, "smpl", 4) && !hasLoop && size >= 60)
		{
			// smpl may follow DATA (as in NEEK2o). Its sample-end is inclusive.
			u8 smpl[60];
			if(file_fd->read(smpl, sizeof(smpl)) == sizeof(smpl)
				&& WavLE32(smpl + 28) > 0 && WavLE32(smpl + 40) == 0
				&& WavLE32(smpl + 52) == 0 && WavLE32(smpl + 56) == 0)
			{
				loopStart = WavLE32(smpl + 44); loopEnd = WavLE32(smpl + 48);
				hasLoop = true;
			}
		}
		offset = payload + size;
		if(size & 1) { if(offset == end) break; ++offset; }
	}
	if(!hasFormat || !hasData || !SampleRate || channels < 1 || channels > 2
		|| (bits != 8 && bits != 16) || alignment != channels * (bits / 8)
		|| !DataSize || DataSize > 0x7fffffffu || DataSize % alignment)
	{
		CloseFile();
		return;
	}
	Is16Bit = bits == 16;
	Format = channels == 1 ? (Is16Bit ? VOICE_MONO_16BIT : VOICE_MONO_8BIT)
		: (Is16Bit ? VOICE_STEREO_16BIT : VOICE_STEREO_8BIT);
	if(hasLoop && loopStart <= loopEnd && loopEnd < DataSize / alignment)
	{
		LoopStartBytes = loopStart * alignment;
		LoopEndBytes = (loopEnd + 1) * alignment;
		EmbeddedLoop = true;
	}
	Decode();
}

void WavDecoder::CloseFile()
{
	if(file_fd)
		delete file_fd;

	file_fd = NULL;
}

int WavDecoder::Read(u8 * buffer, int buffer_size, int pos)
{
	if(!file_fd)
		return -1;

	// Honor the asset's embedded repeat independently of GuiSound's optional
	// whole-file repeat, matching BNS loop behavior and preserving one-shots.
	const u32 end = EmbeddedLoop ? LoopEndBytes : DataSize;
	if(EmbeddedLoop && CurPos >= (int)end) CurPos = LoopStartBytes;
	if(CurPos < 0 || CurPos >= (int)end || buffer_size <= 0)
		return 0;

	file_fd->seek(DataOffset+CurPos, SEEK_SET);

	if(buffer_size > (int)end-CurPos)
		buffer_size = end-CurPos;

	int read = file_fd->read(buffer, buffer_size);
	if(read > 0)
	{
		if (Is16Bit)
		{
			read &= ~0x0001;

			for (u32 i = 0; i < (u32) (read / sizeof (u16)); ++i)
				((u16 *) buffer)[i] = le16(((u16 *) buffer)[i]);
		}
		CurPos += read;
	}

	return read;
}
