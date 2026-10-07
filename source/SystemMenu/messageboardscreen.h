#ifndef MESSAGEBOARDSCREEN_H
#define MESSAGEBOARDSCREEN_H

#include "systemmenuresource.h"

class GreyBackground;
class Texture;
class MessageBoardLive;

// Render-only Wii Message Board using the applied theme's native resources.
// WSM owns navigation/motion; it does not write NAND mail or send WC24 messages.
class MessageBoardScreen : public SystemMenuResource
{
public:
	MessageBoardScreen();
	~MessageBoardScreen();

	bool Load(const U8Archive *archive = NULL);
	void Open( GreyBackground *background );
	void BeginClose( bool openSettings = false );
	void Update( const Vec2f &screen );
	void Render( Mtx &modelview, const Vec2f &screen, bool widescreen );

	bool IsClosed() const { return closed; }
	bool IsTransitioning() const { return openingFrame > 0 || closingFrame > 0 || closed; }
	bool RequestedSettings() const { return requestedSettings; }

private:
	enum Page
	{
		PageLandingPrevious,
		PageLanding,
		PageLandingNext,
		PageCalendar,
		PageCompose,
		PageMemo,
		PageAddressBook,
		PageDisabledMessage,
		PageDisabledRegister
	};

	Texture *snapshot;
	Texture *previousSnapshot;
	Texture *menuSnapshot;
	u8 *menuPixels;
	u8 *pixels;
	u8 *previousPixels;
	GreyBackground *entryBackground;
	Page page;
	int transitionFrame;
	int transitionDirection;
	int openingFrame;
	int closingFrame;
	bool closed;
	bool requestedSettings;
	MessageBoardLive *live;
	Page previousPage;
	int dayOffset, previousDayOffset, monthOffset, previousMonthOffset;

	bool LoadPage( Page target );
	bool ChangePage( Page target, int direction = 0 );
	void ChangeDay(int direction);
	void FinishTransition();
	void GoBack();
	void ActivateAt( float x, float y, const Vec2f &screen );
	void ActivateWithoutPointer();
	void DrawTexture( Texture *texture, float x, float y, float width,
		float height, u8 alpha ) const;
	void DrawPage(bool previous, float x, float y, const Vec2f &screen, u8 alpha);
};

#endif
