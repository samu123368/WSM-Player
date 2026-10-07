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
#include "Window.h"
#include "Layout.h"
#include "video.h"

void Window::Load(Pane::Header *file)
{
	const u8 *section_start = (const u8 *)file;

	Pane::Load(file);
	header = (Window::Header *) (file+1);

	// read content
	QuadPane::Load((QuadPane::Header *)(section_start + header->content_offset));

	// read frames
	const u32 *frame_offsets = (const u32 *) (section_start + header->frame_table_offset);
	for(u32 i = 0; i < header->frame_count; i++)
        frames.push_back((Frame *) (section_start + frame_offsets[i]));
}

bool Window::CopyAppearanceFrom( const Window &source )
{
	if( !header || !QuadPane::header || !source.header
		|| !source.QuadPane::header || frames.size() != source.frames.size() )
	{
		return false;
	}

	QuadPane::header->material_index = source.QuadPane::header->material_index;
	for( u32 i = 0; i < frames.size(); ++i )
	{
		if( !frames[ i ] || !source.frames[ i ] )
			return false;
		frames[ i ]->material_index = source.frames[ i ]->material_index;
		frames[ i ]->texture_flip = source.frames[ i ]->texture_flip;
	}
	return true;
}

void Window::Draw(const Resources& resources, u8 render_alpha, const float ws_scale, Mtx &view) const
{
	if( !header || !QuadPane::header )
		return;

	float frame_left = 0, frame_right = 0, frame_top = 0, frame_bottom = 0;

	// Revolution wnd1 files commonly leave the explicit frame sizes at zero.
	// NW4R then uses the first corner textures' dimensions.  Wii Room relies on
	// this: its 25x25 top-left circle is mirrored into a 50x50 round window.
	if( header->frame_count == 1 )
	{
		const u16 material_index = frames[0]->material_index;
		if( material_index < resources.materials.size() )
		{
			const u16 texture_index = resources.materials[material_index]->GetTextureIndex();
			if( texture_index < resources.textures.size() )
			{
				if( frame_left == 0 ) frame_left = resources.textures[texture_index]->GetWidth();
				if( frame_right == 0 ) frame_right = resources.textures[texture_index]->GetWidth();
				if( frame_top == 0 ) frame_top = resources.textures[texture_index]->GetHeight();
				if( frame_bottom == 0 ) frame_bottom = resources.textures[texture_index]->GetHeight();
			}
		}
	}
	else if( header->frame_count == 4 || header->frame_count == 8 )
	{
		const u16 tl_material = frames[0]->material_index;
		const u16 br_material = frames[3]->material_index;
		if( tl_material < resources.materials.size() )
		{
			const u16 texture_index = resources.materials[tl_material]->GetTextureIndex();
			if( texture_index < resources.textures.size() )
			{
				if( frame_left == 0 ) frame_left = resources.textures[texture_index]->GetWidth();
				if( frame_top == 0 ) frame_top = resources.textures[texture_index]->GetHeight();
			}
		}
		if( br_material < resources.materials.size() )
		{
			const u16 texture_index = resources.materials[br_material]->GetTextureIndex();
			if( texture_index < resources.textures.size() )
			{
				if( frame_right == 0 ) frame_right = resources.textures[texture_index]->GetWidth();
				if( frame_bottom == 0 ) frame_bottom = resources.textures[texture_index]->GetHeight();
			}
		}
	}

	const float width = GetWidth();
	const float height = GetHeight();
	frame_left = MIN( frame_left, width );
	frame_right = MIN( frame_right, width );
	frame_top = MIN( frame_top, height );
	frame_bottom = MIN( frame_bottom, height );

	const float content_left = frame_left - header->stretch_left;
	const float content_right = width - frame_right + header->stretch_right;
	const float content_bottom = frame_bottom - header->stretch_bottom;
	const float content_top = height - frame_top + header->stretch_top;
	// The three bytes following frame_count are padding in Wii WND1, not
	// modern NW4R flags. Content uses authored colors; frames do not.
	const u8 window_kind = 0;
	const bool use_frame_vertex_colors = false;

	static const float normal[4][2] = {
		{ 0.f, 0.f }, { 1.f, 0.f }, { 0.f, 1.f }, { 1.f, 1.f }
	};

	// Draw the content first.  The old player skipped it entirely and painted
	// each frame material over the full pane.
	if( window_kind != 2 )
	{
		const float bottom = window_kind == 1 ? 0.f : content_bottom;
		const float top = window_kind == 1 ? height : content_top;
		if( content_right > content_left && top > bottom )
		{
			float uv[4][2];
			for( u32 i = 0; i < 4; ++i )
			{
				uv[i][0] = header && QuadPane::header->tex_coord_count ? tex_coords[0].coords[i].s : normal[i][0];
				uv[i][1] = header && QuadPane::header->tex_coord_count ? tex_coords[0].coords[i].t : normal[i][1];
			}
			DrawQuad( resources, render_alpha, view, QuadPane::header->material_index,
				content_left, bottom, content_right, top, uv, true );
		}
	}

	if( header->frame_count == 1 && window_kind == 0 )
	{
		const float u = frame_left > 0.f ? ( width - frame_left ) / frame_left : 1.f;
		const float v = frame_top > 0.f ? ( height - frame_top ) / frame_top : 1.f;
		const float uv_tl[4][2] = { {0,0}, {u,0}, {0,1}, {u,1} };
		const float uv_tr[4][2] = { {1,0}, {0,0}, {1,v}, {0,v} };
		const float uv_bl[4][2] = { {0,v}, {1,v}, {0,0}, {1,0} };
		const float uv_br[4][2] = { {u,1}, {0,1}, {u,0}, {0,0} };
		const Frame *frame = frames[0];
		DrawQuad( resources, render_alpha, view, frame->material_index,
			0, height - frame_top, width - frame_right, height,
			uv_tl, use_frame_vertex_colors, frame->texture_flip );
		DrawQuad( resources, render_alpha, view, frame->material_index,
			width - frame_right, frame_bottom, width, height,
			uv_tr, use_frame_vertex_colors, frame->texture_flip );
		DrawQuad( resources, render_alpha, view, frame->material_index,
			0, 0, frame_left, height - frame_top,
			uv_bl, use_frame_vertex_colors, frame->texture_flip );
		DrawQuad( resources, render_alpha, view, frame->material_index,
			frame_left, 0, width, frame_bottom,
			uv_br, use_frame_vertex_colors, frame->texture_flip );
	}
	else if( ( header->frame_count == 1 || header->frame_count == 2 ) && window_kind != 0 )
	{
		const float uv_left[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };
		const float uv_right[4][2] = { {1,0}, {0,0}, {1,1}, {0,1} };
		DrawQuad( resources, render_alpha, view, frames[0]->material_index,
			0, 0, frame_left, height, uv_left, use_frame_vertex_colors,
			frames[0]->texture_flip );
		const Frame *right_frame = frames[header->frame_count == 2 ? 1 : 0];
		DrawQuad( resources, render_alpha, view, right_frame->material_index,
			width - frame_right, 0, width, height, uv_right,
			use_frame_vertex_colors, right_frame->texture_flip );
	}
	else if( header->frame_count == 4 )
	{
		const float u = frame_left > 0.f ? ( width - frame_left ) / frame_left : 1.f;
		const float v = frame_top > 0.f ? ( height - frame_top ) / frame_top : 1.f;
		const float uv_tl[4][2] = { {0,0}, {u,0}, {0,1}, {u,1} };
		const float uv_tr[4][2] = { {0,0}, {1,0}, {0,v}, {1,v} };
		const float uv_bl[4][2] = { {0,1-v}, {1,1-v}, {0,1}, {1,1} };
		const float uv_br[4][2] = { {1-u,0}, {1,0}, {1-u,1}, {1,1} };
		DrawQuad( resources, render_alpha, view, frames[0]->material_index,
			0, height - frame_top, width - frame_right, height,
			uv_tl, use_frame_vertex_colors, frames[0]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[1]->material_index,
			width - frame_right, frame_bottom, width, height,
			uv_tr, use_frame_vertex_colors, frames[1]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[2]->material_index,
			0, 0, frame_left, height - frame_top,
			uv_bl, use_frame_vertex_colors, frames[2]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[3]->material_index,
			frame_left, 0, width, frame_bottom,
			uv_br, use_frame_vertex_colors, frames[3]->texture_flip );
	}
	else if( header->frame_count >= 8 )
	{
		const float uv[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };
		DrawQuad( resources, render_alpha, view, frames[0]->material_index, 0, height-frame_top, frame_left, height, uv, use_frame_vertex_colors, frames[0]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[1]->material_index, width-frame_right, height-frame_top, width, height, uv, use_frame_vertex_colors, frames[1]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[2]->material_index, 0, 0, frame_left, frame_bottom, uv, use_frame_vertex_colors, frames[2]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[3]->material_index, width-frame_right, 0, width, frame_bottom, uv, use_frame_vertex_colors, frames[3]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[4]->material_index, frame_left, height-frame_top, width-frame_right, height, uv, use_frame_vertex_colors, frames[4]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[5]->material_index, frame_left, 0, width-frame_right, frame_bottom, uv, use_frame_vertex_colors, frames[5]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[6]->material_index, 0, frame_bottom, frame_left, height-frame_top, uv, use_frame_vertex_colors, frames[6]->texture_flip );
		DrawQuad( resources, render_alpha, view, frames[7]->material_index, width-frame_right, frame_bottom, width, height-frame_top, uv, use_frame_vertex_colors, frames[7]->texture_flip );
	}
}

void Window::DrawQuad(const Resources& resources, u8 render_alpha, Mtx &view,
		u16 material_index, float left, float bottom, float right, float top,
		const float input_tex_coords[4][2], bool use_vertex_colors,
		u8 texture_flip) const
{
	if( right <= left || top <= bottom || material_index >= resources.materials.size() )
		return;

	GXColor colors[4];
	for( u32 i = 0; i < 4; ++i )
	{
		colors[i] = use_vertex_colors ? QuadPane::header->vertex_colors[i]
			: (GXColor){ 0xff, 0xff, 0xff, 0xff };
	}
	const bool modulate = QuadPane::IsModulateColor( colors, render_alpha );
	resources.materials[material_index]->Apply( resources, render_alpha, modulate );

	Mtx local, modelview;
	guMtxIdentity( local );
	guMtxTransApply( local, local,
		-0.5f * GetOriginX() * GetWidth(),
		-0.5f * GetOriginY() * GetHeight(), 0.f );
	guMtxConcat( view, local, modelview );
	LoadMenuPositionMatrix(modelview, GX_PNMTX0);

	GX_ClearVtxDesc();
	GX_InvVtxCache();
	GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
	const u8 tex_count = MAX( (u8)1, resources.materials[material_index]->GetTextureMapCount() );
	for( u32 i = 0; i < tex_count && i < 8; ++i )
		GX_SetVtxDesc( GX_VA_TEX0 + i, GX_DIRECT );

	float tex[4][2];
	for( u32 i = 0; i < 4; ++i )
	{
		float s = input_tex_coords[i][0];
		float t = input_tex_coords[i][1];
		switch( texture_flip )
		{
			case 1: s = 1.f - s; break;
			case 2: t = 1.f - t; break;
			case 3: { const float old_s = s; s = 1.f - t; t = old_s; break; }
			case 4: s = 1.f - s; t = 1.f - t; break;
			case 5: { const float old_s = s; s = t; t = 1.f - old_s; break; }
		}
		tex[i][0] = s;
		tex[i][1] = t;
	}

	const u8 indices[4] = { 2, 3, 1, 0 }; // bottom-left, bottom-right, top-right, top-left
	const float positions[4][2] = {
		{ left, bottom }, { right, bottom }, { right, top }, { left, top }
	};
	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	for( u32 vertex = 0; vertex < 4; ++vertex )
	{
		const u8 color_index = indices[vertex];
		GX_Position3f32( positions[vertex][0], positions[vertex][1], 0.f );
		GX_Color4u8( colors[color_index].r, colors[color_index].g,
			colors[color_index].b, MultiplyAlpha( colors[color_index].a, render_alpha ) );
		for( u32 map = 0; map < tex_count && map < 8; ++map )
			GX_TexCoord2f32( tex[color_index][0], tex[color_index][1] );
	}
	GX_End();
}
