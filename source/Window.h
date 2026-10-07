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
#ifndef WII_BNR_WINDOW_H_
#define WII_BNR_WINDOW_H_

#include "QuadPane.h"

class Window : public QuadPane
{
public:
	typedef QuadPane Base;

	static const u32 MAGIC = MAKE_FOURCC('w', 'n', 'd', '1');

	void Load(Pane::Header *file);

	// Reuse another authored window's content/corner materials without changing
	// the shared Material objects themselves.  This is useful when a layout has
	// the right animated pane but needs the appearance of a sibling pane.
	bool CopyAppearanceFrom( const Window &source );

private:
	void Draw(const Resources& resources, u8 render_alpha, const float ws_scale, Mtx &view) const;
	void DrawQuad(const Resources& resources, u8 render_alpha, Mtx &view,
			u16 material_index, float left, float bottom, float right, float top,
			const float tex_coords[4][2], bool use_vertex_colors,
			u8 texture_flip = 0) const;

	struct Header
	{
		// WND1 v8 stores four float content-inflation values here. Frame
		// dimensions come from the frame textures, not eight u16 fields.
		f32 stretch_left;
		f32 stretch_right;
		f32 stretch_top;
		f32 stretch_bottom;
		u8 frame_count;
		u8 pad[3];
		u32 content_offset;
		u32 frame_table_offset;
	} __attribute__((packed));

	struct Frame
	{
	    u16 material_index;
		u8 texture_flip;
	} __attribute__((packed));

	Header *header;
	std::vector<Frame *> frames;
};

#endif
