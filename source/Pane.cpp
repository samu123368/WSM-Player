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

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Pane.h"
#include "utils/fixedname.h"
#include "Layout.h"
#include "animation.h"
#include "sc.h"
#include "video.h"
#include "SystemMenu/renderinspection.h"

namespace
{
	// libogc exposes no GX_GetScissor. Track recursive pane clipping locally;
	// each top-level layout inherits its caller's viewport and projection.
	u32 paneClip[4];
	u32 callerClip[4];
	bool callerClipActive = false;
	Mtx44 callerProjection;
	u32 paneRenderDepth = 0;
	struct PaneClipScope
	{
		PaneClipScope()
		{
			if( paneRenderDepth++ == 0 )
			{
				paneClip[0] = callerClipActive ? callerClip[0] : 0;
				paneClip[1] = callerClipActive ? callerClip[1] : 0;
				paneClip[2] = callerClipActive ? callerClip[2] : screenwidth;
				paneClip[3] = callerClipActive ? callerClip[3] : screenheight;
			}
		}
		~PaneClipScope() { --paneRenderDepth; }
	};
	void SetPaneClip( u32 x, u32 y, u32 width, u32 height )
	{
		if( RenderInspection::Unmasked() ) { x=y=0;width=screenwidth;height=screenheight; }
		paneClip[0] = x; paneClip[1] = y;
		paneClip[2] = width; paneClip[3] = height;
		GX_SetScissor( x, y, width, height );
	}
}

void Pane::SetRenderClip(u32 x, u32 y, u32 width, u32 height,
	const Mtx44 &projection)
{
	callerClipActive = true;
	callerClip[0] = std::min(x, (u32)screenwidth);
	callerClip[1] = std::min(y, (u32)screenheight);
	callerClip[2] = std::min(width, (u32)screenwidth - callerClip[0]);
	callerClip[3] = std::min(height, (u32)screenheight - callerClip[1]);
	memcpy(callerProjection, projection, sizeof(callerProjection));
	SetPaneClip(callerClip[0], callerClip[1], callerClip[2], callerClip[3]);
}

void Pane::ResetRenderClip()
{
	callerClipActive = false;
	SetPaneClip(0, 0, screenwidth, screenheight);
}

void Pane::Load(Pane::Header *pan)
{
	this->header = pan;
	this->hide = false;
	RootPane = FixedNameEquals( header->name, sizeof( header->name ), "RootPane" );
}

Pane::~Pane()
{
	// Layout and animation owners may be destroyed independently during Apply.
	// Remove the observer while this pane and its children are still alive.
	if( animationLink ) animationLink->UnbindPane( this );
	// delete children
	for(u32 i = 0; i < panes.size(); ++i)
		delete panes[i];
}

void Pane::SetFrame(FrameNumber frame, u8 keySet)
{
	// setframe on self
	Animator::SetFrame( frame, keySet );

	// setframe on children
	for(u32 i = 0; i < panes.size(); ++i)
		panes[i]->SetFrame( frame, keySet );
}

void Pane::SetNw4rTransformOrder( bool enabled, bool recursive )
{
	useNw4rTransformOrder = enabled;
	if( recursive )
	{
		for( u32 i = 0; i < panes.size(); ++i )
			panes[ i ]->SetNw4rTransformOrder( enabled, true );
	}
}

void Pane::Render(const Resources& resources, u8 parent_alpha, Mtx &modelview,
				  bool widescreen, bool modify_alpha) const
{
	if( channelIconViewport ) iconViewportVisible = false;
	const RenderInspection::Policy inspection = RenderInspection::Active()
		? RenderInspection::Observe(header->name,
			RenderInspection::Classify(header->magic,header->name,channelIconViewport),
			GetWidth(),GetHeight(),GetInspectionText()) : RenderInspection::Auto;
	RenderInspection::DrawLayerScope inspectionLayer(header->name);
	if( inspection == RenderInspection::Hidden ) return;
	if (inspection != RenderInspection::Shown && (!GetVisible() || GetHide()))
		return;
	PaneClipScope clipScope;

	u8 render_alpha = header->alpha;

    if(RootPane && parent_alpha != 0xFF)
    {
        render_alpha = MultiplyAlpha(header->alpha, parent_alpha);
        modify_alpha = true;
    }
	else if(!RootPane && modify_alpha)
	{
		render_alpha = MultiplyAlpha(header->alpha, parent_alpha);
	}
	else if(GetInfluencedAlpha() && header->alpha != 0xff)
	{
		modify_alpha = true;
		parent_alpha = MultiplyAlpha(header->alpha, parent_alpha);
	}

	float ws_scale = 1.0f;

	if( widescreen && GetWidescren() )
	{
		ws_scale *= 0.82f; // should actually be 0.75?
		widescreen = false;
	}

	Mtx m1,m2,m3,m4;
	guMtxIdentity (m1);

	if( useNw4rTransformOrder )
	{
		// NW4R panes compose T * Rz * Ry * Rx * S. ApplyScale must be used
		// here: ScaleApply pre-multiplies and pulls separately modelled strips
		// apart when their parent/child rotations differ.
		guMtxRotDeg ( m2, 'x', header->rotate.x );
		guMtxRotDeg ( m3, 'y', header->rotate.y );
		guMtxRotDeg ( m4, 'z', header->rotate.z );
		guMtxConcat(m4, m3, m4);
		guMtxConcat(m4, m2, m1);
		guMtxApplyScale(m1, m1,
			header->scale.x * ws_scale, header->scale.y, 1.f);
	}
	else
	{
		// Keep the historical WSM composition for layouts already tuned to it.
		// Switching every retail pane to NW4R order regresses other banners.
		guMtxScaleApply(m1,m1,
			header->scale.x * ws_scale, header->scale.y, 1.f);
		guMtxRotDeg ( m2, 'x', header->rotate.x );
		guMtxRotDeg ( m3, 'y', header->rotate.y );
		guMtxRotDeg ( m4, 'z', header->rotate.z );
		guMtxConcat(m2, m3, m2);
		guMtxConcat(m2, m4, m2);
		guMtxConcat(m1, m2, m1);
	}

	// Translate
	guMtxTransApply(m1,m1, header->translate.x, header->translate.y, header->translate.z);

	guMtxConcat (modelview, m1, pane_view);
	if(RenderInspection::WantsBounds() && render_alpha && drawSelf)
	{
		// Copy only numerical bounds, never the pane/material/resource pointers.
		f32 viewport[6],projection[7];GX_GetViewportv(viewport);
		Mtx44 previewProjection={};
		if(RenderInspection::PerspectiveProjection(previewProjection,callerClipActive ? callerProjection : MainProjection,MainProjection))
			GX_GetProjectionv(previewProjection,projection,GX_PERSPECTIVE);
		else GX_GetProjectionv(callerClipActive ? callerProjection : MainProjection,projection,GX_ORTHOGRAPHIC);
		const float left=-.5f*GetOriginX()*GetWidth(),top=-.5f*GetOriginY()*GetHeight();
		const float corners[4][2]={{left,top},{left+GetWidth(),top},
			{left+GetWidth(),top+GetHeight()},{left,top+GetHeight()}};
		float minX=screenwidth,minY=screenheight,maxX=0,maxY=0;
		Mtx previewView;std::memcpy(previewView,pane_view,sizeof(previewView));
		RenderInspection::TransformView(previewView,MainProjection);
		for(unsigned i=0;i<4;++i)
		{
			if(RenderInspection::ThreeDActive() && previewProjection[3][2]==-1.f
				&& previewView[2][0]*corners[i][0]+previewView[2][1]*corners[i][1]+previewView[2][3]>=-1.f)continue;
			float x,y,z;GX_Project(corners[i][0],corners[i][1],0,previewView,projection,viewport,&x,&y,&z);
			minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);
		}
		RenderInspection::SetBounds(header->name,minX/screenwidth,minY/screenheight,
			maxX/screenwidth,maxY/screenheight);
	}

	if( inspection == RenderInspection::Shown ) { render_alpha=parent_alpha=255;modify_alpha=true; }
	bool scissor = gxScissorForBindedLayouts && !RenderInspection::Unmasked();
	if( channelIconViewport && !scissor ) iconViewportVisible = true;
	u32 scissorX = 0;
	u32 scissorY = 0;
	u32 scissorW = 0;
	u32 scissorH = 0;
	const u32 parentX = paneClip[0], parentY = paneClip[1];
	const u32 parentW = paneClip[2], parentH = paneClip[3];

	// calculate scissors if they will be used
	if( scissor )
	{
		f32 viewport[6];
		f32 projection[7];
		GX_GetViewportv(viewport);
        GX_GetProjectionv(callerClipActive ? callerProjection : MainProjection,
			projection, GX_ORTHOGRAPHIC);

		// The old code projected two points and assumed a centred, unrotated pane.
		// During page movement that assumption is false (and right/bottom origins
		// were mathematically wrong), allowing bound channel icons to escape their
		// slots.  Project every authored corner and build an order-independent box.
		const f32 left = -0.5f * GetOriginX() * GetWidth();
		const f32 top = -0.5f * GetOriginY() * GetHeight();
		const f32 right = left + GetWidth();
		const f32 bottom = top + GetHeight();
		const f32 corners[4][2] = {
			{ left, top }, { right, top }, { right, bottom }, { left, bottom }
		};
		f32 minX = (f32)screenwidth;
		f32 minY = (f32)screenheight;
		f32 maxX = 0.0f;
		f32 maxY = 0.0f;
		for( u32 corner = 0; corner < 4; ++corner )
		{
			f32 x, y, z;
			GX_Project( corners[corner][0], corners[corner][1], 0.0f,
				pane_view, projection, viewport, &x, &y, &z );
			minX = std::min( minX, x );
			minY = std::min( minY, y );
			maxX = std::max( maxX, x );
			maxY = std::max( maxY, y );
		}
		minX = std::max( 0.0f, std::min( minX, (f32)screenwidth ) );
		minY = std::max( 0.0f, std::min( minY, (f32)screenheight ) );
		maxX = std::max( 0.0f, std::min( maxX, (f32)screenwidth ) );
		maxY = std::max( 0.0f, std::min( maxY, (f32)screenheight ) );

		// GX scissor rectangles are integer pixel cells.  Expanding with
		// floor/ceil leaks a one-pixel row from oversized icon layouts into the
		// neighbouring channel frame.  Round inward so the authored slot is a
		// hard containment boundary even while its page is moving.
		const f32 clippedLeft = ceilf( minX );
		const f32 clippedTop = ceilf( minY );
		const f32 clippedRight = floorf( maxX );
		const f32 clippedBottom = floorf( maxY );
		scissorX = (u32)clippedLeft;
		scissorY = (u32)clippedTop;
		scissorW = (u32)std::max( clippedRight - clippedLeft, 0.0f );
		scissorH = (u32)std::max( clippedBottom - clippedTop, 0.0f );

		const u32 rightEdge = std::min( scissorX + scissorW, parentX + parentW );
		const u32 bottomEdge = std::min( scissorY + scissorH, parentY + parentH );
		scissorX = std::max( scissorX, parentX );
		scissorY = std::max( scissorY, parentY );
		scissorW = rightEdge > scissorX ? rightEdge - scissorX : 0;
		scissorH = bottomEdge > scissorY ? bottomEdge - scissorY : 0;
		// GX scissor registers encode an inclusive end (size - 1). Do not
		// submit an empty off-screen icon viewport: it underflows the rectangle,
		// and wastes rendering/texture/font work for invisible neighbouring pages.
		if( channelIconViewport )
		{
			iconViewportVisible = scissorW > 0 && scissorH > 0;
			if( !iconViewportVisible ) return;
		}
		SetPaneClip( scissorX, scissorY, scissorW, scissorH );
	}

	// binded layouts dont inheiret the modified widescreen setting
	bool realWS = ( _CONF_GetAspectRatio() == CONF_ASPECT_16_9 );
	// Data Management's N_Data16x9/N_Banner16x9 parent already applies the
	// widescreen correction. Its wide TV is 168/176x96, not the grid's 128x96.
	// Reapplying that correction in the icon squeezes it; scaling against 128
	// also zooms and crops its height. Retain the parent's aspect state here.
	const bool childWidescreen = dataManagementIcon ? widescreen : realWS;
	const f32 iconWidth = dataManagementIcon && realWS ? 170.0f : 128.0f;

	// draw binded layouts that appear under this one
	foreach( Layout *l, bindedLayoutsUnder )
	{
		if( fitBindedLayoutsToPane && l->GetWidth() > 0.0f
			&& l->GetHeight() > 0.0f )
		{
			Mtx fitted;
			// Icons render in the System Menu's 128x96 viewport, regardless
			// of an icon BRLYT declaring the full 608x456 banner canvas.
			const f32 scale = std::max( GetWidth() / ( channelIconViewport ? iconWidth : l->GetWidth() ),
				GetHeight() / ( channelIconViewport ? 96.0f : l->GetHeight() ) );
			guMtxApplyScale( pane_view, fitted, scale, scale, 1.0f );
			l->RenderWithCurrentMtx( fitted, childWidescreen );
		}
		else
			l->RenderWithCurrentMtx( pane_view, realWS );
	}
	if( scissor )
	{
		SetPaneClip( parentX, parentY, parentW, parentH );
	}

	// render self
	if(drawSelf && !(RenderInspection::Unmasked() && RenderInspection::IsMask(header->name)))
		Draw(resources, render_alpha, ws_scale, pane_view);

	// render children
	for(u32 i = 0; i < panes.size(); ++i)
		panes[i]->Render(resources, render_alpha, pane_view, widescreen, modify_alpha);

	// draw binded panes that appear on top of this one
	if( scissor )
	{
		SetPaneClip( scissorX, scissorY, scissorW, scissorH );
	}
	foreach( Layout *l, bindedLayoutsOver )
	{
		if( fitBindedLayoutsToPane && l->GetWidth() > 0.0f
			&& l->GetHeight() > 0.0f )
		{
			Mtx fitted;
			const f32 scale = std::max( GetWidth() / ( channelIconViewport ? iconWidth : l->GetWidth() ),
				GetHeight() / ( channelIconViewport ? 96.0f : l->GetHeight() ) );
			guMtxApplyScale( pane_view, fitted, scale, scale, 1.0f );
			l->RenderWithCurrentMtx( fitted, childWidescreen );
		}
		else
			l->RenderWithCurrentMtx( pane_view, realWS );
	}
	if( scissor )
	{
		SetPaneClip( parentX, parentY, parentW, parentH );
	}
}

Pane* Pane::FindPane(const std::string& find_name)
{
	if( FixedNameEquals( getName(), sizeof( header->name ), find_name ) )
		return this;

	for(u32 i = 0; i < panes.size(); ++i)
	{
		Pane *found = panes[i]->FindPane(find_name);
		if (found)
			return found;
	}

	return NULL;
}

Pane* Pane::FindPane( const char* find_name )
{
	if( FixedNameEquals( getName(), sizeof( header->name ), find_name ) )
	{
		return this;
	}

	for(u32 i = 0; i < panes.size(); ++i)
	{
		Pane *found = panes[i]->FindPane( find_name );
		if (found)
			return found;
	}

	return NULL;
}

void Pane::BindLayout( Layout *layout, bool under )
{
	if( under )
	{
		bindedLayoutsUnder << layout;
	}
	else
	{
		bindedLayoutsOver << layout;
	}
}

void Pane::UnbindAllLayouts()
{
	bindedLayoutsUnder.clear();
	bindedLayoutsOver.clear();
}

void Pane::ProcessHermiteKey(const KeyType& type, float value)
{
	if (type.type == ANIMATION_TYPE_VERTEX_COLOR)	// vertex color
	{
		// only alpha is supported for Panes afaict
		if (0x10 == type.target)
		{
			header->alpha = FLOAT_2_U8(value);
			return;
		}
	}
	else if (type.type == ANIMATION_TYPE_PANE)	// pane animation
	{
		if (type.target < 10)
		{
			(&header->translate.x)[type.target] = value;
			return;
		}
	}

	Base::ProcessHermiteKey(type, value);
}

void Pane::ProcessStepKey(const KeyType& type, StepKeyHandler::KeyData data)
{
	if (type.type == ANIMATION_TYPE_VISIBILITY)	// visibility
	{
		SetVisible(!!data.data2);
		return;
	}

	Base::ProcessStepKey(type, data);
}
