#pragma once
#include <cmath>
#include <ctime>

// Gregorian calendar days, independent of daylight-saving clock changes.
inline int BoardCivilDay(const tm &date)
{
	int y = date.tm_year + 1900, m = date.tm_mon + 1;
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned year = y - era * 400;
	const unsigned day = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + date.tm_mday - 1;
	return era * 146097 + year * 365 + year / 4 - year / 100 + day;
}

// Local coordinates of the straightened calendar. The caller inverts the
// actual rendered frame matrix, including its native widescreen correction.
inline int BoardCalendarCell(float x, float y)
{
	const int col = (int)std::floor((x + 245) / 70);
	const int row = (int)std::floor((162 - y) / 50);
	return col >= 0 && col < 7 && row >= 0 && row < 6 ? row * 7 + col : -1;
}
