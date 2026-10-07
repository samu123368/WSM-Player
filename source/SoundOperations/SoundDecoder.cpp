/***************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * 3Band resampling thanks to libmad
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
#include <gccore.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include "SoundDecoder.hpp"

namespace
{
const u32 FixedPointShift = 15;
const u32 FixedPointScale = 1 << FixedPointShift;
const int AsndOutputRate = 48000;
}

SoundDecoder::SoundDecoder()
{
	file_fd = NULL;
	Init();
}

SoundDecoder::SoundDecoder(const char * filepath)
{
	file_fd = new CFile(filepath, "rb");
	Init();
}

SoundDecoder::SoundDecoder(const u8 * buffer, int size)
{
	file_fd = new CFile(buffer, size);
	Init();
}

SoundDecoder::~SoundDecoder()
{
	ExitRequested = true;
	while(Decoding)
		usleep(100);

	if(file_fd)
		delete file_fd;
	file_fd = NULL;
	if(ResampleBuffer)
		free(ResampleBuffer);
	ResampleBuffer = NULL;
}

void SoundDecoder::Init()
{
	SoundType = SOUND_RAW;
	SoundBlocks = 8;
	SoundBlockSize = 8192;
	CurPos = 0;
	Loop = false;
	EndOfFile = false;
	Decoding = false;
	ExitRequested = false;
	ResampleBuffer = NULL;
	ResampleRatio = 0;
	ResamplePhase = 0;
	ResamplePrevious[ 0 ] = ResamplePrevious[ 1 ] = 0;
	ResampleFiltered[ 0 ] = ResampleFiltered[ 1 ] = 0;
	ResampleHasPrevious = false;
	ResampleFilterReady = false;
	ResampleOutputBlockSize = SoundBlockSize;
	OutputSampleRate = AsndOutputRate;
	ResamplerConfigured = false;
	SoundBuffer.SetBufferBlockSize(SoundBlockSize);
	SoundBuffer.Resize(SoundBlocks);
}

int SoundDecoder::Rewind()
{
	CurPos = 0;
	EndOfFile = false;
	file_fd->rewind();
	ResamplePhase = 0;
	ResampleHasPrevious = false;
	ResampleFilterReady = false;

	return 0;
}

int SoundDecoder::Read(u8 * buffer, int buffer_size, int pos)
{
	int ret = file_fd->read(buffer, buffer_size);
	CurPos += ret;

	return ret;
}

void SoundDecoder::ConfigureResampler()
{
	if(ResamplerConfigured)
		return;
	ResamplerConfigured = true;

	const int sourceRate = GetSampleRate();
	OutputSampleRate = sourceRate;
	// Banner BNS audio is normally 32 kHz stereo.  ASND fixes the Wii audio
	// hardware at 48 kHz, and its live pitch conversion makes these streams
	// sound thin.  Convert the decoded, big-endian stereo PCM once here, as
	// done by the maintained Wii loader audio path.
	if(!IsStereo() || !Is16Bit() || sourceRate <= 0 || sourceRate >= AsndOutputRate)
		return;

	ResampleBuffer = (u8 *)memalign(32, SoundBlockSize);
	if(!ResampleBuffer)
		return;

	ResampleRatio = (FixedPointScale * (u32)sourceRate) / AsndOutputRate;
	if(!ResampleRatio)
	{
		free(ResampleBuffer);
		ResampleBuffer = NULL;
		return;
	}

	// SoundBuffer retains its original 8192-byte capacity.  Read only the
	// amount of source PCM which expands back into that capacity.
	ResampleOutputBlockSize = SoundBlockSize;
	SoundBlockSize = (SoundBlockSize * ResampleRatio) / FixedPointScale;
	SoundBlockSize &= ~0x03; // complete 16-bit stereo frames only
	OutputSampleRate = AsndOutputRate;
}

u32 SoundDecoder::UpsampleStereo16(const s16 *source, s16 *destination,
	u32 sourceSamples, u32 destinationCapacity)
{
	const u32 sourceFrames = sourceSamples / 2;
	if(!source || !destination || !sourceFrames || destinationCapacity < 2)
		return 0;

	u32 sourceFrame = 0;
	if(!ResampleHasPrevious)
	{
		ResamplePrevious[ 0 ] = source[ 0 ];
		ResamplePrevious[ 1 ] = source[ 1 ];
		ResampleHasPrevious = true;
		sourceFrame = 1;
	}

	u32 written = 0;
	while(sourceFrame < sourceFrames && written + 1 < destinationCapacity)
	{
		const s16 *current = source + sourceFrame * 2;
		s32 sample[ 2 ];
		for(int channel = 0; channel < 2; ++channel)
		{
			const s32 previous = ResamplePrevious[ channel ];
			const s32 interpolated = previous
				+ (((s32)current[ channel ] - previous) * (s32)ResamplePhase
					>> FixedPointShift);
			// A very gentle one-pole high cut removes the metallic edge introduced
			// by 32-to-48 kHz conversion without muffling speech or banner music.
			sample[ channel ] = ResampleFilterReady
				? ( 3 * interpolated + ResampleFiltered[ channel ] ) / 4
				: interpolated;
			ResampleFiltered[ channel ] = sample[ channel ];
			destination[ written + channel ] = (s16)sample[ channel ];
		}
		ResampleFilterReady = true;
		written += 2;

		ResamplePhase += ResampleRatio;
		while(ResamplePhase >= FixedPointScale && sourceFrame < sourceFrames)
		{
			ResamplePrevious[ 0 ] = source[ sourceFrame * 2 ];
			ResamplePrevious[ 1 ] = source[ sourceFrame * 2 + 1 ];
			++sourceFrame;
			ResamplePhase -= FixedPointScale;
		}
	}
	return written;
}

void SoundDecoder::Decode()
{
	if(!file_fd || ExitRequested || EndOfFile)
		return;

	u16 newWhich = SoundBuffer.Which();
	u16 i = 0;
	for (i = 0; i < SoundBuffer.Size()-2; i++)
	{
		if(!SoundBuffer.IsBufferReady(newWhich))
			break;

		newWhich = (newWhich+1) % SoundBuffer.Size();
	}

	if(i == SoundBuffer.Size()-2)
		return;

	Decoding = true;
	ConfigureResampler();

	int done  = 0;
	u8 * write_buf = SoundBuffer.GetBuffer(newWhich);
	if(!write_buf)
	{
		ExitRequested = true;
		Decoding = false;
		return;
	}

	while(done < SoundBlockSize)
	{
		int ret = Read(&write_buf[done], SoundBlockSize-done, Tell());

		if(ret <= 0)
		{
			if(Loop)
			{
				Rewind();
				continue;
			}
			else
			{
				EndOfFile = true;
				break;
			}
		}

		done += ret;
	}

	if(done > 0)
	{
		if(ResampleBuffer && ResampleRatio)
		{
			// A partial stereo frame cannot be submitted to ASND or safely
			// interpolated.  Valid BNS data is already aligned; this also makes
			// malformed/truncated input fail quietly instead of reading past it.
			done &= ~0x03;
			if(done == 0)
			{
				EndOfFile = true;
				Decoding = false;
				return;
			}
			memcpy(ResampleBuffer, write_buf, done);
			const u32 sourceSamples = done / sizeof(s16);
			const u32 destinationSamples = UpsampleStereo16(
				(const s16 *)ResampleBuffer, (s16 *)write_buf, sourceSamples,
				ResampleOutputBlockSize / sizeof(s16));
			done = destinationSamples * sizeof(s16);
		}
		SoundBuffer.SetBufferSize(newWhich, done);
		SoundBuffer.SetBufferReady(newWhich, true);
	}

	if(!SoundBuffer.IsBufferReady((newWhich+1) % SoundBuffer.Size()))
		Decode();

	Decoding = false;
}
