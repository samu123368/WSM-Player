#ifndef WSM_MESSAGE_BOARD_LIVE_H
#define WSM_MESSAGE_BOARD_LIVE_H
#include "systemmenuresource.h"
#include <ctime>
class Object;
class Animation;

// Native graphics only. Navigation remains in MessageBoardScreen: there are
// no asynchronous button callbacks, NAND message writes or network actions.
class MessageBoardLive : public SystemMenuResource
{
public:
	MessageBoardLive();
	~MessageBoardLive();
	bool Load(const U8Archive &archive);
	void Draw(int page, int dayOffset, int monthOffset, float x, float y,
		const Vec2f &screen, u8 alpha);
	bool CalendarDateAt(float x, float y, const Vec2f &screen,
		int dayOffset, int monthOffset, int &selectedOffset) const;
private:
	Layout *board, *backdrop, *chrome, *calendar, *cell, *compose, *memo, *address;
	Animation *cellAnimation;
	Object *dayStyle, *selectedStyle;
	std::vector<u8 *> archives;
	std::vector<std::string> archiveNames;
	std::vector<u32> archiveSizes;
	std::vector<Layout *> layouts;
	char16 dateText[64], monthText[64], cellText[8];
	Layout *Read(const U8Archive &archive, const char *ash, const char *layout,
		const char *settledAnimation = NULL);
	void DrawCalendar(Mtx &view, const Vec2f &canvas, const tm &date,
		int monthOffset, u8 alpha);
	void DrawChrome(Mtx &view, bool landing, bool arrows, u8 alpha);
};
#endif
