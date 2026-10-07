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
#ifndef BMG_H
#define BMG_H

#include <gctypes.h>
#include "utils/char16.h"

// simple implimentation of "ipl::message::Message"

//! the way nintendo does it, each string has a hardcoded index.
//! but they change indexes with different versions of the system menu.  this implimtation expects string
//! indexes from the bmg in v514.  it will try to determine which bmg it is given by checking the crc,
//! and then convert the requested index into one for the version of bmg it thinks it has

//! the whole index resolving stuff is pretty specific to the "ipl_message.bmg" files used in the system menu

class Bmg
{
public:
	Bmg( const u8* stuff = NULL, u32 len = 0 );

	bool SetResource( const u8* stuff, u32 len );

	u16 GetMessageCount() const { return inf1Header ? IndexResolver ? 458 : inf1Header->numMessages : 0; }

	// get a pointer to a u16 string for the given index
	//! expects a message index from the bmg files in system menu 514
	const char16* GetMessage( u16 index ) const;

	// this seems like a good enough place to put a global translator
	static Bmg *Instance()
	{
		if( !instance )
		{
			instance = new Bmg;
		}
		return instance;
	}

protected:
	// header is 0x20 bytes
	struct BmgHeader
	{
		u64 magic;		// "MESGbmg1"
		u32 size;		// total file size
		u32 sectionCnt;	// number of sections

		u32 pad1;		// 0x10 bytes of padding
		u32 pad2;
		u32 pad3;
		u32 pad4;
	}__attribute__((packed));

	// 0x10 bytes
	struct Inf1Header
	{
		u32 magic;			// INF1 - 0x494e4631
		u32 size;			// section size
		u16 numMessages;	// number of messages
		u16 entrySize;		// i guess this is the size of each entry in the INF1 section?
		u32 pad;
	}__attribute__((packed));

	// 0x8 bytes
	struct Dat1Header
	{
		u32 magic;		// DAT1 - 0x44415431
		u32 size;		// section size
	}__attribute__((packed));


	BmgHeader *bmgHeader;
	Inf1Header *inf1Header;
	Dat1Header *dat1Header;
	u16 (*IndexResolver)( u16 index );

	void Reset();

	static Bmg *instance;
};

// Message text is read from the user-owned NAND BMG at runtime.


#endif // BMG_H
