#include "uifontfallback.h"
extern const u8 ui_font_fallback_bin[];
extern const u32 ui_font_fallback_bin_size;
namespace {
struct Glyph { u16 code, sheet, x, y; float advance; s16 bearing; };
const Glyph glyphs[] = {
#include "ui_font_glyphs.inc"
};
const Glyph *Find(u16 code) {
 unsigned first=0,last=sizeof(glyphs)/sizeof(glyphs[0]);
 while(first<last) { unsigned mid=(first+last)/2;
  if(glyphs[mid].code<code) first=mid+1; else last=mid; }
 return first<sizeof(glyphs)/sizeof(glyphs[0]) && glyphs[first].code==code ? &glyphs[first] : NULL;
}
}
bool UiFontFallback::HasGlyph(u16 code) {return Find(code)!=NULL;}
float UiFontFallback::Advance(u16 code,float height) {
 const Glyph *g=Find(code); return g?g->advance*height/32.0f:0;
}
void UiFontFallback::Draw(u16 code,float x,float y,float height,GXColor color,
                         bool layoutCoordinates,float horizontalScale) {
 const Glyph *g=Find(code); if(!g) return;
 const u32 offset=g->sheet*512u*512u;
 if(offset+512u*512u>ui_font_fallback_bin_size) return;
 static bool flushed=false;
 if(!flushed){DCFlushRange((void*)ui_font_fallback_bin,ui_font_fallback_bin_size);flushed=true;}
 GXTexObj texture;
 GX_InitTexObj(&texture,(void*)(ui_font_fallback_bin+offset),512,512,GX_TF_IA4,GX_CLAMP,GX_CLAMP,GX_FALSE);
 GX_InitTexObjLOD(&texture,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
 GX_LoadTexObj(&texture,GX_TEXMAP0);
 const float scale=height/32.0f;
 x+=(g->bearing-3)*scale*horizontalScale; y+=(layoutCoordinates?3:-3)*scale;
 const float width=48*scale*horizontalScale, size=48*scale*(layoutCoordinates?-1:1);
 const float u=g->x/512.0f, v=g->y/512.0f, step=48/512.0f;
 GX_Begin(GX_QUADS,GX_VTXFMT0,4);
 GX_Position3f32(x,y,0);GX_Color4u8(color.r,color.g,color.b,color.a);GX_TexCoord2f32(u,v);
 GX_Position3f32(x+width,y,0);GX_Color4u8(color.r,color.g,color.b,color.a);GX_TexCoord2f32(u+step,v);
 GX_Position3f32(x+width,y+size,0);GX_Color4u8(color.r,color.g,color.b,color.a);GX_TexCoord2f32(u+step,v+step);
 GX_Position3f32(x,y+size,0);GX_Color4u8(color.r,color.g,color.b,color.a);GX_TexCoord2f32(u,v+step);
 GX_End();
}
