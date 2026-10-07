#include "themeui.h"
#include "video.h"
#include "Material.h"
#include "themeuigeometry.h"
#include "themeuicontrast.h"
#include "utils/localuiassets.h"
#include "gd.h"
#include "utils/TextureConverter.h"
#include <algorithm>
#include <cstdlib>

ThemeUi::ThemeUi() : background(NULL), bars(NULL), button(NULL), titleStyle(NULL)
{
	buttonInk = (GXColor){100,100,100,255};
	for( int i = 0; i < 4; ++i ) { scrollTextures[i] = NULL; scrollPixels[i] = NULL; }
}
ThemeUi::~ThemeUi()
{
	delete background; delete bars; delete button; delete titleStyle;
	for( int i = 0; i < 4; ++i ) { delete scrollTextures[i]; free(scrollPixels[i]); }
	for( u8 *data : archives ) free(data);
}
Layout *ThemeUi::Read(const U8Archive &archive, const char *ash, const char *name)
{
	u32 size = 0;
	u8 *data = archive.GetFileAllocated(ash, &size);
	if( !data || !size ) { free(data); return NULL; }
	archives.push_back(data); // BRLYT/materials point into this allocation.
	return LoadLayout(U8Archive(data, size), name);
}
bool ThemeUi::Load(const U8Archive &archive)
{
	background = Read(archive, "/layout/common/setupBg.ash", "it_BgSetUp_a");
	bars = Read(archive, "/layout/common/setupBtn.ash", "it_Button_a");
	button = Read(archive, "/layout/common/dlgWdw.ash", "my_DialogWindow_a1");
	titleStyle = Read(archive, "/layout/common/setupSel.ash", "it_ObjSetUp_a");
	if( bars ) SetPaneVisible(bars, "N_Button", false);
	if( button )
	{
		Pane *pane = button->FindPane("N_BtnB_Pic");
		if( pane ) { pane->SetPosition(0,0); pane->SetRotate(0); pane->SetScale((Vec2f){1,1}); }
		if( Textbox *label = button->FindTextbox("T_BtnB") ) label->SetHide(true);
	}
	loaded = background && bars && button && button->FindPane("N_BtnB_Pic");
	ResolveButtonInk();
	// The EULA channel owns Opera's skin. Decode its ZIP in RAM directly from
	// NAND, never from an extracted agreements/skin folder on the SD card.
	std::vector<std::vector<u8> > scrollImages;
	LocalUiAssets::AgreementSkin(scrollImages);
	for( int i = 0; i < 4; ++i )
	{
		if(scrollImages.size()!=4 || scrollImages[i].empty()) continue;
		gdImagePtr image = gdImageCreateFromPngPtr(scrollImages[i].size(),scrollImages[i].data());
		if( !image ) continue;
		int w = 0, h = 0;
		scrollPixels[i] = GDImageToRGBA8(&image,&w,&h); gdImageDestroy(image);
		if( !scrollPixels[i] || w <= 0 || h <= 0 || w > 256 || h > 256 )
		{ free(scrollPixels[i]); scrollPixels[i] = NULL; continue; }
		scrollTextures[i] = new Texture;
		scrollTextures[i]->LoadFromRawData(scrollPixels[i],w,h,GX_TF_RGBA8);
	}
	return loaded;
}
const Texture *ThemeUi::ScrollArtwork(int index) const
{
	return index >= 0 && index < 4 ? scrollTextures[index] : NULL;
}
GXColor ThemeUi::Ink(u8 alpha) const
{
	GXColor result = buttonInk; result.a = alpha; return result;
}
GXColor ThemeUi::PanelInk(u8 alpha) const
{
	// N_Base is the dialog panel, not the independently themed BtnB buttons.
	// Its native T_Dialog material is the text style authored for that surface.
	Textbox *text = button ? button->FindTextbox("T_Dialog") : NULL;
	const int index = text ? text->GetMaterialIndex() : -1;
	if( index < 0 || (size_t)index >= button->Materials().size() )
		return (GXColor){45,69,82,alpha};
	Material *material = button->Materials()[index];
	const Material::Header *header = material ? material->GetHeader() : NULL;
	if( !header ) return (GXColor){45,69,82,alpha};
	const GXColorS10 &ink = header->color_regs[1];
	return (GXColor){(u8)std::max(0,std::min(255,(int)ink.r)),
		(u8)std::max(0,std::min(255,(int)ink.g)),
		(u8)std::max(0,std::min(255,(int)ink.b)),alpha};
}
void ThemeUi::ResolveButtonInk()
{
	Material *material = button ? button->FindMaterial("T_BtnB") : NULL;
	if( !material || !material->GetHeader() ) return;
	const GXColorS10 &ink = material->GetHeader()->color_regs[1];
	buttonInk = (GXColor){(u8)std::max(0,std::min(255,(int)ink.r)),
		(u8)std::max(0,std::min(255,(int)ink.g)),(u8)std::max(0,std::min(255,(int)ink.b)),255};
	Picture *center = dynamic_cast<Picture *>(button->FindPane("BtnB1"));
	const int index = center ? center->GetMaterialIndex() : -1;
	if( index < 0 || (size_t)index >= button->Materials().size() ) return;
	Material *backdrop = button->Materials()[index];
	// Do not guess contrast for arbitrary custom TEV programs. Keep authored
	// ink unless we can evaluate the button's actual native color ramp.
	if( !backdrop || !backdrop->GetHeader() || !backdrop->UsesSingleTextureColorRamp() ) return;
	const GXColorS10 &low = backdrop->GetHeader()->color_regs[0];
	const GXColorS10 &high = backdrop->GetHeader()->color_regs[1];
	const u16 textureIndex = backdrop->GetTextureIndex();
	if( textureIndex >= button->Textures().size() ) return;
	Texture *texture = button->Textures()[textureIndex];
	if( !texture || !texture->IsLoaded() ) return;
	const int w = texture->GetWidth(), h = texture->GetHeight();
	if( w < 4 || h < 4 || w > 512 || h > 512 ) return;
	const uint32_t cpuAddress = ThemeTextureCpuAddress(
		(uint32_t)GX_GetTexObjData(&texture->TexObj()),
		GX_GetTexBufferSize(w,h,GX_GetTexObjFmt(&texture->TexObj()),GX_FALSE,0));
	if( !cpuAddress ) return;
	const u8 *pixels = (const u8 *)cpuAddress;
	gdImagePtr image = NULL;
	switch( GX_GetTexObjFmt(&texture->TexObj()) )
	{
	case GX_TF_I4: I4ToGD(pixels,w,h,&image); break;
	case GX_TF_I8: I8ToGD(pixels,w,h,&image); break;
	case GX_TF_IA4: IA4ToGD(pixels,w,h,&image); break;
	case GX_TF_IA8: IA8ToGD(pixels,w,h,&image); break;
	case GX_TF_RGB565: RGB565ToGD(pixels,w,h,&image); break;
	case GX_TF_RGB5A3: RGB565A3ToGD(pixels,w,h,&image); break;
	case GX_TF_RGBA8:
		if( w % 4 == 0 ) RGBA8ToGD(pixels,w,h,&image);
		break;
	case GX_TF_CMPR: CMPToGD(pixels,w,h,&image); break;
	default: break;
	}
	if( !image ) return;
	int total = 0, samples = 0;
	// Sample behind the label, avoiding transparent edges and top gloss.
	for( int y = h * 3/8; y < h * 5/8; ++y )
		for( int x = w/4; x < w * 3/4; ++x )
		{
			const int pixel = gdImageGetTrueColorPixel(image,x,y);
			if( gdTrueColorGetAlpha(pixel) > 32 ) continue;
			total += ThemeInkLuma(
				ThemeRampChannel(gdTrueColorGetRed(pixel),low.r,high.r),
				ThemeRampChannel(gdTrueColorGetGreen(pixel),low.g,high.g),
				ThemeRampChannel(gdTrueColorGetBlue(pixel),low.b,high.b));
			++samples;
		}
	gdImageDestroy(image);
	const int background = samples ? total / samples : -1;
	if( ThemeInkNeedsContrast(buttonInk.r,buttonInk.g,buttonInk.b,background) )
		buttonInk = background < 128 ? (GXColor){255,255,255,255} : (GXColor){51,51,51,255};
	// Native Textbox uses this TEV register; a white vertex tint alone cannot
	// brighten gray material ink. WSM's direct labels use the same cached Ink().
	material->SetColorRegister(1,buttonInk);
}
GXColor ThemeUi::TitleInk(u8 alpha) const
{
	Textbox *text = titleStyle ? titleStyle->FindTextbox("T_Setting_00") : NULL;
	const int index = text ? text->GetMaterialIndex() : -1;
	if( index < 0 || (size_t)index >= titleStyle->Materials().size() )
		return (GXColor){255,255,255,alpha};
	const Material::Header *header = titleStyle->Materials()[index]->GetHeader();
	if( !header ) return (GXColor){255,255,255,alpha};
	const GXColorS10 &ink = header->color_regs[1];
	return (GXColor){(u8)std::max(0,std::min(255,(int)ink.r)),
		(u8)std::max(0,std::min(255,(int)ink.g)),(u8)std::max(0,std::min(255,(int)ink.b)),alpha};
}
bool ThemeUi::Button(float x, float y, float w, float h, u8 alpha, bool focused)
{
	if( !loaded || w <= 0 || h <= 0 ) return false;
	const ThemeButtonGeometry size = ThemeButtonSize(w,h);
	// Caps are 32x80 with right/left anchored origins, not a 204px bitmap.
	// Expand the middle and move the caps; never squash the whole button.
	const char *parts[][3] = {{"BtnB0","BtnB1","BtnB2"},
		{"BtnB0_Shade","BtnB1_Shade","BtnB2_Shade"},
		{"BtnB_Ac0","BtnB_Ac1","BtnB_Ac2"}};
	for( int layer = 0; layer < 3; ++layer )
		for( int part = 0; part < 3; ++part )
			if( Pane *pane = button->FindPane(parts[layer][part]) )
			{
				pane->SetPosition((part - 1) * size.middle * .5f,0);
				pane->SetSize(part == 1 ? size.middle : 32,80);
			}
	Mtx view;
	guMtxIdentity(view);
	guMtxScaleApply(view, view, size.scale, -size.scale, 1);
	guMtxTransApply(view, view, x + w * .5f, y + h * .5f, 0);
	button->RenderPane("N_BtnB_Pic", view, alpha);
	if( focused )
	{
		Pane *pane = button->FindPane("N_BtnB_Ac");
		if( pane )
		{
			pane->SetPosition(0,0); pane->SetHide(false); pane->SetVisible(true); pane->SetAlpha(255);
			button->RenderPane("N_BtnB_Ac", view, (u8)(alpha / 2));
			pane->SetHide(true);
		}
	}
	return true;
}
bool ThemeUi::Panel(float x, float y, float w, float h, u8 alpha)
{
	if( !loaded || w <= 0 || h <= 0 ) return false;
	// Nine-slice panel: preserve corner art rather than stretching a dialog.
	const float capX = std::min(16.0f,w * .25f), capY = std::min(32.0f,h * .25f);
	const float centerW = w - capX * 2, centerH = h - capY * 2;
	const char *parts[] = {"Picture_00","Picture_01","Picture_02","Picture_03",
		"Picture_04","Picture_05","Picture_06"};
	for( int i = 0; i < 7; ++i )
		if( Pane *pane = button->FindPane(parts[i]) )
		{
			pane->SetOrigin(4);
			const bool center = i == 3;
			const int column = i < 3 ? i : i - 4;
			pane->SetSize(center ? w : (column == 1 ? centerW : capX),center ? centerH : capY);
			pane->SetPosition(center || column == 1 ? 0 : (column == 0 ? -1 : 1) * (w-capX)*.5f,
				center ? 0 : (i < 3 ? 1 : -1) * (h-capY)*.5f);
		}
	Mtx view; guMtxIdentity(view);
	guMtxScaleApply(view,view,1,-1,1);
	guMtxTransApply(view,view,x + w * .5f,y + h * .5f,0);
	button->RenderPane("N_Base",view,alpha);
	return true;
}
bool ThemeUi::LabeledButton(float x, float y, float w, float h, u8 alpha, const char16 *text)
{
	if( !Button(x,y,w,h,alpha) ) return false;
	Textbox *label = button->FindTextbox("T_BtnB");
	if( !label ) return true;
	label->SetText(text); label->SetVisible(true); label->SetHide(false); label->SetUniformTextFit(true);
	label->SetPosition(0,0); label->SetSize(w - 20,h); label->CenterText();
	label->SetScale((Vec2f){1,1}); label->SetRotate(0);
	// The theme's TEV material provides ink. An old gray vertex tint was
	// multiplying it again and making every live Settings label unreadable.
	label->SetTextColor((GXColor){255,255,255,255}); label->SetAlpha(255);
	label->SetFontSize(24,24);
	Mtx view; guMtxIdentity(view); guMtxScaleApply(view,view,1,-1,1);
	guMtxTransApply(view,view,x + w * .5f,y + h * .5f,0);
	button->RenderPane("T_BtnB",view,alpha);
	label->SetHide(true);
	return true;
}
bool ThemeUi::Chrome(const Vec2f &screen, float slide, u8 alpha)
{
	if( !loaded ) return false;
	Mtx view;
	guMtxIdentity(view);
	guMtxScaleApply(view, view, screen.x / 608.0f, -screen.y / 456.0f, 1);
	guMtxTransApply(view, view, slide + screen.x * .5f, screen.y * .5f, 0);
	background->RenderPane("RootPane",view,alpha);
	bars->RenderPane("RootPane",view,alpha);
	return true;
}
void ThemeUi::DialogText(const Vec2f &screen, float x, float y, u8 alpha, const char16 *text)
{
	if( !loaded ) return;
	Textbox *label = button->FindTextbox("T_Dialog");
	if( !label ) return;
	label->SetText(text); label->SetUniformTextFit(true); label->SetFontSize(24,24);
	// Panel() mutates its nine-slice geometry for other controls; restore this
	// dialog's size before rendering its independently positioned text.
	Panel(x + screen.x*.5f - screen.x*256/608.0f,
		y + screen.y*.5f - screen.y*176/456.0f,
		screen.x*512/608.0f,screen.y*352/456.0f,alpha);
	Mtx view; guMtxIdentity(view);
	guMtxScaleApply(view,view,screen.x / 608.0f,-screen.y / 456.0f,1);
	guMtxTransApply(view,view,x + screen.x * .5f,y + screen.y * .5f,0);
	button->RenderPane("T_Dialog",view,alpha);
}
