#ifndef WSM_WII_HTML_CONTENT_H
#define WSM_WII_HTML_CONTENT_H
#include <string>
#include <vector>
#include <cstddef>

// Read-only table flow for Nintendo's fixed-width Settings documents. This is
// deliberately not a browser or a JavaScript/NAND settings implementation.
struct WiiHtmlCell
{
	float x, y, w, h;
	std::string markup, cssClass, background;
	bool centered;
	bool listButton; // Authored button-backed cell, not a paragraph styled List.
};
std::vector<WiiHtmlCell> WiiHtmlTableCells(const std::string &html);
std::size_t WiiHtmlButtonCell(const std::vector<WiiHtmlCell> &cells,
	float x,float y,float w,float h,const std::vector<bool> &used);
std::string WiiNicknameUtf8(const unsigned char *data, std::size_t bytes);
std::string WiiHtmlUtf8(const char *data,std::size_t bytes);
std::string WiiHtmlElementMarkup(const std::string &html,const std::string &id);
#endif
