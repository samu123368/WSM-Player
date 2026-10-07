#ifndef WSM_THEME_UI_H
#define WSM_THEME_UI_H
#include "systemmenuresource.h"

// Shared, render-only Wii widgets. Owned by the applied resource generation;
// never retains objects from a previous theme or installs button callbacks.
class ThemeUi : public SystemMenuResource
{
public:
	ThemeUi();
	~ThemeUi();
	bool Load(const U8Archive &archive);
	bool Button(float x, float y, float w, float h, u8 alpha, bool focused = false);
	bool Panel(float x, float y, float w, float h, u8 alpha);
	bool LabeledButton(float x, float y, float w, float h, u8 alpha, const char16 *text);
	void DialogText(const Vec2f &screen, float x, float y, u8 alpha, const char16 *text);
	bool Chrome(const Vec2f &screen, float slide, u8 alpha);
	GXColor Ink(u8 alpha) const;
	GXColor PanelInk(u8 alpha) const;
	GXColor TitleInk(u8 alpha) const;
	const Texture *ScrollArtwork(int index) const;
private:
	Layout *background, *bars, *button, *titleStyle;
	Texture *scrollTextures[4];
	u8 *scrollPixels[4];
	GXColor buttonInk;
	std::vector<u8 *> archives;
	void ResolveButtonInk();
	Layout *Read(const U8Archive &archive, const char *ash, const char *layout);
};
#endif
