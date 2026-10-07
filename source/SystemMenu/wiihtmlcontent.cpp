#include "wiihtmlcontent.h"
#include <algorithm>
#include <cstdlib>
#include <cctype>

std::string WiiHtmlUtf8(const char *data,std::size_t bytes)
{
	std::string out;
	if( !data ) return out;
	// Several EU documents declare UTF-8 but contain Windows-1252 letters.
	// Preserve valid UTF-8 sequences; convert isolated legacy bytes, rather
	// than letting a cedilla swallow the next two ASCII letters in Utf8ToChar16.
	static const unsigned cp1252[32] = {0x20ac,0xfffd,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
		0x2c6,0x2030,0x160,0x2039,0x152,0xfffd,0x17d,0xfffd,
		0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
		0x2dc,0x2122,0x161,0x203a,0x153,0xfffd,0x17e,0x178};
	for( std::size_t i = 0; i < bytes; )
	{
		const unsigned ch = (unsigned char)data[i];
		if( ch < 128 ) { out += data[i++]; continue; }
		const std::size_t n = ch >= 0xc2 && ch <= 0xdf ? 2
			: ch >= 0xe0 && ch <= 0xef ? 3 : ch >= 0xf0 && ch <= 0xf4 ? 4 : 0;
		bool valid = n && i + n <= bytes;
		for( std::size_t k = 1; valid && k < n; ++k )
			valid = ((unsigned char)data[i+k] & 0xc0) == 0x80;
		if( valid && n >= 3 )
		{
			const unsigned second = (unsigned char)data[i+1];
			valid = !(ch == 0xe0 && second < 0xa0) && !(ch == 0xed && second >= 0xa0)
				&& !(ch == 0xf0 && second < 0x90) && !(ch == 0xf4 && second >= 0x90);
		}
		if( valid ) { out.append(data+i,n); i += n; continue; }
		const unsigned cp = ch < 0xa0 ? cp1252[ch-0x80] : ch;
		if( cp < 0x800 ) { out += (char)(0xc0 | (cp>>6)); out += (char)(0x80 | (cp&63)); }
		else { out += (char)(0xe0 | (cp>>12)); out += (char)(0x80 | ((cp>>6)&63)); out += (char)(0x80 | (cp&63)); }
		++i;
	}
	return out;
}

std::string WiiNicknameUtf8(const unsigned char *data, std::size_t bytes)
{
	std::string text;
	if(!data) return text;
	// IPL.NIK is ten UTF-16BE code units followed by a terminator.
	bytes = std::min(bytes, (std::size_t)20);
	for(std::size_t i = 0; i + 1 < bytes; i += 2)
	{
		unsigned cp = (data[i] << 8) | data[i+1];
		if(!cp) break;
		if(cp >= 0xd800 && cp <= 0xdbff && i + 3 < bytes)
		{
			const unsigned low = (data[i+2] << 8) | data[i+3];
			if(low >= 0xdc00 && low <= 0xdfff)
			{ cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00; i += 2; }
		}
		if(cp >= 0xd800 && cp <= 0xdfff) cp = 0xfffd;
		if(cp < 0x80) text += (char)cp;
		else if(cp < 0x800)
		{ text += (char)(0xc0 | (cp >> 6)); text += (char)(0x80 | (cp & 63)); }
		else if(cp < 0x10000)
		{ text += (char)(0xe0 | (cp >> 12)); text += (char)(0x80 | ((cp >> 6) & 63)); text += (char)(0x80 | (cp & 63)); }
		else
		{ text += (char)(0xf0 | (cp >> 18)); text += (char)(0x80 | ((cp >> 12) & 63)); text += (char)(0x80 | ((cp >> 6) & 63)); text += (char)(0x80 | (cp & 63)); }
	}
	return text;
}

namespace
{
	std::string Attribute(const std::string &tag, const char *key)
	{
		// Parse attributes, rather than matching "id=" inside data-id or a
		// quoted value. HTML attribute names (but not ID values) ignore case.
		size_t at = tag.find_first_of(" \t\r\n");
		while( at != std::string::npos && at < tag.size() )
		{
			while( at < tag.size() && std::isspace((unsigned char)tag[at]) ) ++at;
			if( at >= tag.size() || tag[at] == '>' || tag[at] == '/' ) break;
			const size_t begin = at;
			while( at < tag.size() && !std::isspace((unsigned char)tag[at])
				&& tag[at] != '=' && tag[at] != '>' ) ++at;
			std::string name = tag.substr(begin,at-begin);
			for( char &ch : name ) ch = (char)std::tolower((unsigned char)ch);
			while( at < tag.size() && std::isspace((unsigned char)tag[at]) ) ++at;
			if( at >= tag.size() || tag[at] != '=' ) continue;
			++at;
			while( at < tag.size() && std::isspace((unsigned char)tag[at]) ) ++at;
			if( at >= tag.size() ) break;
			const char quote = tag[at];
			std::string value;
			if( quote == '\'' || quote == '"' )
			{
				const size_t end = tag.find(quote,++at);
				if( end == std::string::npos ) return "";
				value = tag.substr(at,end-at); at = end+1;
			}
			else
			{
				const size_t end = tag.find_first_of(" >\t\r\n",at);
				value = tag.substr(at,end-at); at = end;
			}
			if( name == key ) return value;
		}
		return "";
	}
	float Dimension(const std::string &tag, const char *key, float fallback)
	{
		const std::string value = Attribute(tag,key);
		return value.empty() ? fallback : std::max(0.0f,(float)atof(value.c_str()));
	}
}

std::string WiiHtmlElementMarkup(const std::string &html,const std::string &id)
{
	std::string lower = html;
	for( char &ch : lower ) ch = (char)std::tolower((unsigned char)ch);
	for( std::size_t at = 0; at < html.size(); )
	{
		const std::size_t begin = lower.find('<',at),end = lower.find('>',begin);
		if( begin == std::string::npos || end == std::string::npos ) break;
		at = end + 1;
		if( begin+1 >= end || lower[begin+1] == '/' || lower[begin+1] == '!' ) continue;
		std::size_t nameEnd = begin+1;
		while( nameEnd < end && !std::isspace((unsigned char)lower[nameEnd]) && lower[nameEnd] != '/' ) ++nameEnd;
		const std::string tagName = lower.substr(begin+1,nameEnd-begin-1);
		std::string tag = html.substr(begin,end-begin+1);
		// Attribute names are case-insensitive; ID values retain their spelling.
		if( Attribute(tag,"id") != id ) continue;
		if( tagName == "img" || tagName == "input" || tagName == "br" || lower[end-1] == '/' )
			return tag;
		int depth = 1;
		std::size_t cursor = end+1;
		while( depth && cursor < lower.size() )
		{
			const std::size_t open = lower.find('<',cursor),close = lower.find('>',open);
			if( open == std::string::npos || close == std::string::npos ) break;
			const bool closing = lower[open+1] == '/';
			const std::size_t start = open+1+(closing ? 1 : 0);
			std::size_t stop = start;
			while( stop < close && !std::isspace((unsigned char)lower[stop]) && lower[stop] != '/' ) ++stop;
			if( lower.substr(start,stop-start) == tagName )
			{
				if( closing ) --depth;
				else if( lower[close-1] != '/' ) ++depth;
			}
			cursor = close+1;
		}
		// Never borrow an unrelated element's image if this tag is malformed.
		return depth ? tag : html.substr(begin,cursor-begin);
	}
	return "";
}

std::size_t WiiHtmlButtonCell(const std::vector<WiiHtmlCell> &cells,
	float x,float y,float w,float h,const std::vector<bool> &used)
{
	float closest = 1e20f; std::size_t chosen = cells.size();
	for( std::size_t i = 0; i < cells.size(); ++i )
		if( cells[i].listButton && (i >= used.size() || !used[i]) )
		{
			const WiiHtmlCell &cell = cells[i];
			const float dx = cell.x + cell.w*.5f - x - w*.5f;
			const float dy = cell.y + cell.h*.5f - y - h*.5f;
			const float distance = dx*dx + dy*dy;
			if( distance < closest ) { closest = distance; chosen = i; }
		}
	return chosen;
}

std::vector<WiiHtmlCell> WiiHtmlTableCells(const std::string &html)
{
	std::vector<WiiHtmlCell> cells;
	std::string lower = html;
	for( char &ch : lower ) ch = (char)std::tolower((unsigned char)ch);
	size_t at = lower.find("<body");
	if( at == std::string::npos ) at = 0;
	float y = 0;
	while( cells.size() < 256 )
	{
		const size_t begin = lower.find("<table",at);
		if( begin == std::string::npos ) break;
		const size_t open = lower.find('>',begin), end = lower.find("</table>",open);
		if( open == std::string::npos || end == std::string::npos ) break;
		const std::string table = lower.substr(begin,open-begin+1);
		const float width = Dimension(table,"width",607);
		const float tableHeight = Dimension(table,"height",0);
		size_t rowAt = open + 1;
		const float tableTop = y;
		while( rowAt < end && cells.size() < 256 )
		{
			const size_t row = lower.find("<tr",rowAt);
			if( row == std::string::npos || row >= end ) break;
			const size_t rowOpen = lower.find('>',row), rowEnd = lower.find("</tr>",rowOpen);
			if( rowOpen == std::string::npos || rowEnd == std::string::npos || rowEnd > end ) break;
			float height = Dimension(lower.substr(row,rowOpen-row+1),"height",0);
			float x = 0;
			const size_t first = cells.size();
			size_t cellAt = rowOpen + 1;
			while( cellAt < rowEnd && cells.size() < 256 )
			{
				const size_t th = lower.find("<th",cellAt), td = lower.find("<td",cellAt);
				const size_t start = std::min(th,td);
				if( start == std::string::npos || start >= rowEnd ) break;
				const bool heading = start == th;
				const size_t cOpen = lower.find('>',start);
				const size_t cEnd = lower.find(heading ? "</th>" : "</td>",cOpen);
				if( cOpen == std::string::npos || cEnd == std::string::npos || cEnd > rowEnd ) break;
				const std::string tag = lower.substr(start,cOpen-start+1);
				WiiHtmlCell cell;
				cell.x = x; cell.y = y; cell.w = Dimension(tag,"width",width-x);
				cell.h = Dimension(tag,"height",height);
				cell.markup = html.substr(cOpen+1,cEnd-cOpen-1);
				// CSS names are case-sensitive: retain the authored spelling.
				cell.cssClass = Attribute(html.substr(start,cOpen-start+1),"class");
				cell.background = Attribute(html.substr(start,cOpen-start+1),"background");
				cell.listButton = Attribute(tag,"background").find("btn_list") != std::string::npos;
				cell.centered = heading || Attribute(tag,"align") == "center";
				if( Attribute(tag,"align") == "left" ) cell.centered = false;
				height = std::max(height,cell.h);
				cells.push_back(cell); x += cell.w; cellAt = cEnd + 5;
			}
			if( height == 0 ) height = tableHeight > 0 ? tableHeight : 32;
			for( size_t i = first; i < cells.size(); ++i ) cells[i].h = height;
			y += height; rowAt = rowEnd + 5;
		}
		y = std::max(y,tableTop+tableHeight);
		at = end + 8;
	}
	return cells;
}
