#include "freecamerapreview.h"
#include "video.h"
#include <cstdlib>
#include <algorithm>

void FreeCameraPreview::Clear()
{
	if(pixels) { GX_DrawDone();std::free(pixels);pixels=NULL; }
	width=height=0;
}
void FreeCameraPreview::Capture()
{
	Clear();
	u32 bytes=0;
	pixels=CreateTextureFromFrameBuffer(GX_TF_RGBA8,bytes,width,height);
	if(!pixels || !width || !height) { Clear();return; }
	GX_InitTexObj(&texture,pixels,width,height,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
	GX_InitTexObjLOD(&texture,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
	GX_InvalidateTexAll();
}
bool FreeCameraPreview::Draw(const RenderInspection::Element &e,float x,float y,float w,float h) const
{
	if(!pixels || !e.previewVisible || e.kind==RenderInspection::Container || w<=0 || h<=0) return false;
	const float cw=(e.right-e.left)*width,ch=(e.bottom-e.top)*height;
	if(cw<=0 || ch<=0) return false;
	const float scale=std::min(w/cw,h/ch),dw=cw*scale,dh=ch*scale;
	x+=(w-dw)/2;y+=(h-dh)/2;
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
	GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
	GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_TRUE);
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetNumTexGens(1);GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX3x4,GX_TG_TEX0,GX_IDENTITY);
	GX_SetNumTevStages(1);GX_SetNumIndStages(0);GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevOp(GX_TEVSTAGE0,GX_MODULATE);
	GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
	GX_SetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
	GX_SetTevSwapModeTable(GX_TEV_SWAP0,GX_CH_RED,GX_CH_GREEN,GX_CH_BLUE,GX_CH_ALPHA);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_SET);
	GX_ClearVtxDesc();GX_InvVtxCache();
	GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
	GX_LoadTexObj(const_cast<GXTexObj*>(&texture),GX_TEXMAP0);
	GX_Begin(GX_QUADS,GX_VTXFMT0,4);
	GX_Position3f32(x,y,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(e.left,e.top);
	GX_Position3f32(x+dw,y,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(e.right,e.top);
	GX_Position3f32(x+dw,y+dh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(e.right,e.bottom);
	GX_Position3f32(x,y+dh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(e.left,e.bottom);
	GX_End();return true;
}
