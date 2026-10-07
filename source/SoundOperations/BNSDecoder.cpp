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
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "BNSDecoder.hpp"
#include "tools.h"

SoundBlock DecodefromBNS(const u8 *buffer, u32 size);

BNSDecoder::BNSDecoder(const char * filepath)
	: SoundDecoder(filepath)
{
	SoundType = SOUND_BNS;
	memset(&SoundData, 0, sizeof(SoundBlock));

	if(!file_fd)
		return;

	OpenFile();
}

BNSDecoder::BNSDecoder(const u8 * snd, int len)
	: SoundDecoder(snd, len)
{
	SoundType = SOUND_BNS;
	memset(&SoundData, 0, sizeof(SoundBlock));

	if(!file_fd)
		return;

	OpenFile();
}

BNSDecoder::~BNSDecoder()
{
	ExitRequested = true;
	while(Decoding)
		usleep(100);

	if(SoundData.buffer != NULL)
		free(SoundData.buffer);

	SoundData.buffer = NULL;
}

void BNSDecoder::OpenFile()
{
	u8 * tempbuff = new (std::nothrow) u8[file_fd->size()];
	if(!tempbuff)
	{
		CloseFile();
		return;
	}

	int done = 0;

	while(done < file_fd->size())
	{
		int read = file_fd->read(tempbuff+done, file_fd->size()-done);
		if(read > 0)
			done += read;
		else
		{
			CloseFile();
			return;
		}
	}

	SoundData = DecodefromBNS(tempbuff, done);
	if(SoundData.buffer == NULL)
	{
		CloseFile();
		return;
	}

	delete [] tempbuff;
	tempbuff = NULL;

	Decode();
}

void BNSDecoder::CloseFile()
{
	if(file_fd)
		delete file_fd;

	file_fd = NULL;
}

int BNSDecoder::Read(u8 * buffer, int buffer_size, int pos)
{
	if(!SoundData.buffer)
		return -1;

	if(SoundData.loopFlag)
	{
		int factor = SoundData.format == VOICE_STEREO_16BIT ? 4 : 2;
		if(CurPos >= (int) SoundData.loopEnd*factor)
			CurPos = SoundData.loopStart*factor;

		if(buffer_size > (int) SoundData.loopEnd*factor-CurPos)
			buffer_size = SoundData.loopEnd*factor-CurPos;
	}
	else
	{
		if(CurPos >= (int) SoundData.size)
			return 0;

		if(buffer_size > (int) SoundData.size-CurPos)
			buffer_size = SoundData.size-CurPos;
	}

	memcpy(buffer, SoundData.buffer+CurPos, buffer_size);
	CurPos += buffer_size;

	return buffer_size;
}

struct BNSHeader
{
	u32 fccBNS;
	u32 magic;
	u32 size;
	u16 unk1;
	u16 unk2;
	u32 infoOffset;
	u32 infoSize;
	u32 dataOffset;
	u32 dataSize;
} __attribute__((packed));

struct BNSInfo
{
	u32 fccINFO;
	u32 size;
	u8 codecNum;
	u8 loopFlag;
	u8 chanCount;
	u8 zero;
	u16 freq;
	u8 pad1[2];
	u32 loopStart;
	u32 loopEnd;
	u32 offsetToChanStarts;
	u8 pad2[4];
	u32 chan1StartOffset;
	u32 chan2StartOffset;
	u32 chan1Start;
	u32 coeff1Offset;
	u8 pad3[4];
	u32 chan2Start;
	u32 coeff2Offset;
	u8 pad4[4];
	s16 coefficients1[8][2];
	u16 chan1Gain;
	u16 chan1PredictiveScale;
	s16 chan1PrevSamples[2];
	u16 chan1LoopPredictiveScale;
	s16 chan1LoopPrevSamples[2];
	u16 chan1LoopPadding;
	s16 coefficients2[8][2];
	u16 chan2Gain;
	u16 chan2PredictiveScale;
	s16 chan2PrevSamples[2];
	u16 chan2LoopPredictiveScale;
	s16 chan2LoopPrevSamples[2];
	u16 chan2LoopPadding;
} __attribute__((packed));

struct BNSDataHeader
{
	u32 fccDATA;
	u32 size;
} __attribute__((packed));

struct BNSADPCMBlock
{
	u8 header;
	u8 samples[7];
} __attribute__((packed));

struct BNSDecObj
{
	s16 prevSamples[2];
	s16 coeff[8][2];
};

static bool rangeContains(u32 size, u32 offset, u32 length)
{
	return offset <= size && length <= size - offset;
}

static bool readU32(const u8 *buffer, u32 size, u32 offset, u32 &value)
{
	if (!buffer || !rangeContains(size, offset, sizeof(value)))
		return false;
	memcpy(&value, buffer + offset, sizeof(value));
	return true;
}

static bool loadBNSInfo(BNSInfo &bnsInfo, const u8 *buffer, u32 size)
{
	if (!buffer || size < 0x20)
		return false;

	memset(&bnsInfo, 0, sizeof(bnsInfo));
	memcpy(&bnsInfo, buffer, std::min(size, (u32)sizeof(bnsInfo)));
	if (bnsInfo.fccINFO != MAKE_FOURCC('I','N','F','O')
		|| bnsInfo.size != size || bnsInfo.chanCount < 1
		|| bnsInfo.chanCount > 2 || size < 8)
	{
		return false;
	}

	// All offsets inside INFO are relative to the byte immediately following
	// its eight-byte section header.  Parse them even for the common fixed
	// layout so alternate channel tables never fall through to struct offsets.
	const u8 *ptr = buffer + 8;
	const u32 ptrSize = size - 8;
	for (u32 channel = 0; channel < bnsInfo.chanCount; ++channel)
	{
		u32 channelStartOffset = 0;
		u32 channelStart = 0;
		u32 coefficientOffset = 0;
		if (!readU32(ptr, ptrSize, bnsInfo.offsetToChanStarts + channel * 4,
				channelStartOffset)
			|| !readU32(ptr, ptrSize, channelStartOffset, channelStart)
			|| !readU32(ptr, ptrSize, channelStartOffset + 4, coefficientOffset)
			|| !rangeContains(ptrSize, coefficientOffset, 40))
		{
			return false;
		}

		if (channel == 0)
		{
			memcpy(bnsInfo.coefficients1, ptr + coefficientOffset,
				sizeof(bnsInfo.coefficients1));
			// Gain and predictive scale occupy four bytes after the coefficients.
			memcpy(bnsInfo.chan1PrevSamples, ptr + coefficientOffset + 36,
				sizeof(bnsInfo.chan1PrevSamples));
			bnsInfo.chan1StartOffset = channelStartOffset;
			bnsInfo.chan1Start = channelStart;
			bnsInfo.coeff1Offset = coefficientOffset;
		}
		else
		{
			memcpy(bnsInfo.coefficients2, ptr + coefficientOffset,
				sizeof(bnsInfo.coefficients2));
			memcpy(bnsInfo.chan2PrevSamples, ptr + coefficientOffset + 36,
				sizeof(bnsInfo.chan2PrevSamples));
			bnsInfo.chan2StartOffset = channelStartOffset;
			bnsInfo.chan2Start = channelStart;
			bnsInfo.coeff2Offset = coefficientOffset;
		}
	}
	return true;
}

static s8 signExtendNibble(u8 value)
{
	value &= 0x0f;
	return (value & 0x08) ? (s8)(value | 0xf0) : (s8)value;
}

static void decodeADPCMBlock(s16 *buffer, const BNSADPCMBlock &block, BNSDecObj &bnsDec)
{
	int h1 = bnsDec.prevSamples[0];
	int h2 = bnsDec.prevSamples[1];
	const int coeffIndex = (block.header >> 4) & 0x07;
	const int lshift = block.header & 0x0f;
	int c1 = bnsDec.coeff[coeffIndex][0];
	int c2 = bnsDec.coeff[coeffIndex][1];
	for (int i = 0; i < 14; ++i)
	{
		const u8 pair = block.samples[i / 2];
		int nibSample = signExtendNibble((i & 1) == 0 ? (pair >> 4) : pair);
		// Shifting a negative signed nibble is undefined in C++.  Modern GCC
		// may optimize that expression differently at -O2, which turns valid
		// DSP-ADPCM into harsh high-frequency noise.  Multiplication expresses
		// the same Nintendo DSP scale operation with defined signed arithmetic.
		int sampleDeltaHP = nibSample * (1 << lshift) * 2048;
		int predictedSampleHP = c1 * h1 + c2 * h2;
		int sampleHP = predictedSampleHP + sampleDeltaHP;
		buffer[i] = std::min(std::max(-32768, (sampleHP + 1024) >> 11), 32767);
		h2 = h1;
		h1 = buffer[i];
	}
	bnsDec.prevSamples[0] = h1;
	bnsDec.prevSamples[1] = h2;
}

static u8 * decodeBNS(u32 &size, const BNSInfo &bnsInfo,
	const u8 *data, u32 dataSize)
{
	size = 0;
	if (!data || !dataSize || bnsInfo.chanCount < 1 || bnsInfo.chanCount > 2)
		return NULL;

	const u32 channelStart[2] = { bnsInfo.chan1Start, bnsInfo.chan2Start };
	u32 channelEnd[2] = { dataSize, dataSize };
	if (!rangeContains(dataSize, channelStart[0], 0))
		return NULL;
	if (bnsInfo.chanCount == 2)
	{
		if (!rangeContains(dataSize, channelStart[1], 0)
			|| channelStart[0] == channelStart[1])
		{
			return NULL;
		}
		if (channelStart[0] < channelStart[1])
			channelEnd[0] = channelStart[1];
		else
			channelEnd[1] = channelStart[0];
	}

	u32 channelBlocks[2] = { 0, 0 };
	for (u32 channel = 0; channel < bnsInfo.chanCount; ++channel)
	{
		if (channelEnd[channel] < channelStart[channel])
			return NULL;
		channelBlocks[channel] = (channelEnd[channel] - channelStart[channel])
			/ sizeof(BNSADPCMBlock);
		if (!channelBlocks[channel])
			return NULL;
	}
	const u32 blocksPerChannel = bnsInfo.chanCount == 2
		? std::min(channelBlocks[0], channelBlocks[1]) : channelBlocks[0];
	if (blocksPerChannel > 0xffffffffu / 14u / bnsInfo.chanCount
		/ sizeof(s16))
	{
		return NULL;
	}

	const u32 frames = blocksPerChannel * 14;
	const u32 outputSamples = frames * bnsInfo.chanCount;
	u8 *buffer = (u8 *)malloc(outputSamples * sizeof(s16));
	if (!buffer)
		return NULL;

	s16 smplBlock[14];
	BNSDecObj decObj;
	s16 *outputBuf = (s16 *)buffer;
	const BNSADPCMBlock *inputBuf =
		(const BNSADPCMBlock *)(data + channelStart[0]);
	memcpy(decObj.coeff, bnsInfo.coefficients1, sizeof decObj.coeff);
	memcpy(decObj.prevSamples, bnsInfo.chan1PrevSamples, sizeof decObj.prevSamples);
	if (bnsInfo.chanCount == 1)
		for (u32 i = 0; i < blocksPerChannel; ++i)
		{
			decodeADPCMBlock(smplBlock, inputBuf[i], decObj);
			memcpy(outputBuf, smplBlock, sizeof smplBlock);
			outputBuf += 14;
		}
	else
	{
		for (u32 i = 0; i < blocksPerChannel; ++i)
		{
			decodeADPCMBlock(smplBlock, inputBuf[i], decObj);
			for (int j = 0; j < 14; ++j)
				outputBuf[j * 2] = smplBlock[j];
			outputBuf += 2 * 14;
		}
		outputBuf = (s16 *)buffer + 1;
		inputBuf = (const BNSADPCMBlock *)(data + channelStart[1]);
		memcpy(decObj.coeff, bnsInfo.coefficients2, sizeof decObj.coeff);
		memcpy(decObj.prevSamples, bnsInfo.chan2PrevSamples, sizeof decObj.prevSamples);
		for (u32 i = 0; i < blocksPerChannel; ++i)
		{
			decodeADPCMBlock(smplBlock, inputBuf[i], decObj);
			for (int j = 0; j < 14; ++j)
				outputBuf[j * 2] = smplBlock[j];
			outputBuf += 2 * 14;
		}
	}
	size = outputSamples * sizeof(s16);
	return buffer;
}

SoundBlock DecodefromBNS(const u8 *buffer, u32 size)
{
	SoundBlock OutBlock;
	memset(&OutBlock, 0, sizeof(SoundBlock));

	if (!buffer || size < sizeof(BNSHeader))
		return OutBlock;
	BNSHeader hdr;
	memcpy(&hdr, buffer, sizeof(hdr));
	if (hdr.fccBNS != MAKE_FOURCC('B','N','S',' '))
		return OutBlock;
	if (hdr.size < sizeof(hdr) || hdr.size > size
		|| hdr.infoSize < 8 || hdr.dataSize < sizeof(BNSDataHeader)
		|| !rangeContains(hdr.size, hdr.infoOffset, hdr.infoSize)
		|| !rangeContains(hdr.size, hdr.dataOffset, hdr.dataSize))
	{
		return OutBlock;
	}

	// Find and validate INFO before following any of its relative offsets.
	BNSInfo infoChunk;
	if (!loadBNSInfo(infoChunk, buffer + hdr.infoOffset, hdr.infoSize))
		return OutBlock;
	BNSDataHeader dataChunk;
	memcpy(&dataChunk, buffer + hdr.dataOffset, sizeof(dataChunk));
	if (dataChunk.fccDATA != MAKE_FOURCC('D','A','T','A')
		|| dataChunk.size < sizeof(dataChunk) || dataChunk.size > hdr.dataSize)
	{
		return OutBlock;
	}
	// Check format
	if (infoChunk.codecNum != 0)	// Only codec i've found : 0 = ADPCM. Maybe there's also 1 and 2 for PCM 8 or 16 bits ?
		return OutBlock;
	u8 format = (u8)-1;
	if (infoChunk.chanCount == 1 && infoChunk.codecNum == 0)
		format = VOICE_MONO_16BIT;
	else if (infoChunk.chanCount == 2 && infoChunk.codecNum == 0)
		format = VOICE_STEREO_16BIT;
	if (format == (u8)-1)
		return OutBlock;
	u32 freq = (u32) infoChunk.freq;
	u32 length = 0;
	// Copy data
	if (infoChunk.codecNum == 0)
	{
		// hdr.dataSize describes the complete DATA section.  Some retail/homebrew
		// BNS files under-report DATA's duplicate inner size by one alignment
		// block; using the validated header extent preserves the final channel
		// block while still remaining inside hdr.size.
		OutBlock.buffer = decodeBNS(length, infoChunk,
			buffer + hdr.dataOffset + sizeof(dataChunk),
			hdr.dataSize - sizeof(dataChunk));
		if (!OutBlock.buffer)
			return OutBlock;
	}
	else
	{
		OutBlock.buffer = (u8*) malloc(hdr.dataSize - sizeof(dataChunk));
		if (!OutBlock.buffer)
			return OutBlock;
		length = hdr.dataSize - sizeof(dataChunk);
		memcpy(OutBlock.buffer, buffer + hdr.dataOffset + sizeof(dataChunk), length);
	}

	const u32 decodedFrames = length / (infoChunk.chanCount * sizeof(s16));
	if (infoChunk.loopFlag
		&& (infoChunk.loopStart >= infoChunk.loopEnd
			|| infoChunk.loopEnd > decodedFrames))
	{
		free(OutBlock.buffer);
		memset(&OutBlock, 0, sizeof(OutBlock));
		return OutBlock;
	}

	OutBlock.frequency = freq;
	OutBlock.format = format;
	OutBlock.size = length;
	OutBlock.loopStart = infoChunk.loopStart;
	OutBlock.loopEnd = infoChunk.loopEnd;
	OutBlock.loopFlag = infoChunk.loopFlag;

	return OutBlock;
}
