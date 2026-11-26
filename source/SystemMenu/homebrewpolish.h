#ifndef WSM_HOMEBREW_POLISH_H
#define WSM_HOMEBREW_POLISH_H
#include <vector>

// The native VC chime is IMD5-wrapped LZ10. Bound both the compressed reads
// and backwards copies; a truncated optional SD file must not crash the GUI.
inline bool DecodeHomebrewLz10(const unsigned char *data, unsigned size,
	std::vector<unsigned char> &out)
{
	out.clear();
	if(!data || size<8 || data[0]!='L' || data[1]!='Z'
		|| data[2]!='7' || data[3]!='7' || data[4]!=0x10) return false;
	const unsigned expected=data[5]|(unsigned(data[6])<<8)|(unsigned(data[7])<<16);
	if(expected<12 || expected>512*1024) return false;
	out.reserve(expected);
	unsigned pos=8;
	while(out.size()<expected)
	{
		if(pos>=size) {out.clear();return false;}
		const unsigned flags=data[pos++];
		for(unsigned mask=128;mask && out.size()<expected;mask>>=1)
		{
			if(flags&mask)
			{
				if(size-pos<2) {out.clear();return false;}
				const unsigned first=data[pos++],second=data[pos++];
				const unsigned count=(first>>4)+3,back=((first&15)<<8)+second+1;
				if(back>out.size() || count>expected-out.size()) {out.clear();return false;}
				for(unsigned n=0;n<count;++n) out.push_back(out[out.size()-back]);
			}
			else
			{
				if(pos>=size) {out.clear();return false;}
				out.push_back(data[pos++]);
			}
		}
	}
	return true;
}

// Center-crop texture coordinates, keeping the geometry inside its tile.
struct HomebrewIconCrop
{
	float left, top, right, bottom;
	HomebrewIconCrop(float width, float height)
		: left(0), top(0), right(1), bottom(1)
	{
		if(width <= 0 || height <= 0) return;
		const float source = width / height, target = 128.f / 96.f;
		if(source > target) { left=(1.f-target/source)*.5f; right=1.f-left; }
		else { top=(1.f-source/target)*.5f; bottom=1.f-top; }
	}
};

struct HomebrewIconFit
{
	float width,height;
	HomebrewIconFit(float sourceWidth,float sourceHeight) : width(0),height(0)
	{
		if(sourceWidth<=0 || sourceHeight<=0) return;
		const float sx=118.f/sourceWidth,sy=88.f/sourceHeight;
		const float scale=sx<sy?sx:sy;
		width=sourceWidth*scale;height=sourceHeight*scale;
	}
};

// Prefer retaining the normal pages beside the homebrew boundary. The same
// bounded cache budget still applies; this does not pin an unlimited region.
inline int HomebrewReturnCachePriority(int page, int current, int firstHomebrew)
{
	if(firstHomebrew >= 0 && current >= firstHomebrew
		&& page < firstHomebrew && page >= firstHomebrew-2)
		return firstHomebrew-page-3;
	return page > current ? page-current : current-page;
}
#endif
