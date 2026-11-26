#ifndef WSM_TEXTURE_OUTLINE_H
#define WSM_TEXTURE_OUTLINE_H
#include <vector>

// Outline the existing logo alpha, preserving its actual lettering. Input is
// GX's tiled RGB5A3, output tiled RGBA8; no replacement artwork is bundled.
inline bool OutlineLogoRgb5a3(const unsigned char *src,unsigned w,unsigned h,
	std::vector<unsigned char> &out)
{
	out.clear();
	if(!src || !w || !h || w>512 || h>512) return false;
	const unsigned tiles=(w+3)/4;
	std::vector<unsigned char> rgba(w*h*4);
	for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
	{
		const unsigned p=(((y/4)*tiles+x/4)*16+(y%4)*4+x%4)*2;
		const unsigned v=(unsigned(src[p])<<8)|src[p+1],i=(y*w+x)*4;
		if(v&0x8000)
		{
			rgba[i]=((v>>10)&31)*255/31;rgba[i+1]=((v>>5)&31)*255/31;
			rgba[i+2]=(v&31)*255/31;rgba[i+3]=255;
		}
		else
		{
			rgba[i]=((v>>8)&15)*17;rgba[i+1]=((v>>4)&15)*17;
			rgba[i+2]=(v&15)*17;rgba[i+3]=((v>>12)&7)*255/7;
		}
	}
	out.assign(tiles*((h+3)/4)*64,0);
	for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
	{
		unsigned edge=0;
		for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
		{
			const int nx=int(x)+dx,ny=int(y)+dy;
			if(nx>=0 && ny>=0 && nx<int(w) && ny<int(h))
			{
				const unsigned a=rgba[(ny*w+nx)*4+3];
				if(a>edge) edge=a;
			}
		}
		const unsigned i=(y*w+x)*4,a=rgba[i+3];
		const unsigned alpha=a+(edge*(255-a)+127)/255;
		const unsigned p=((y/4)*tiles+x/4)*64+((y%4)*4+x%4)*2;
		out[p]=alpha;
		out[p+1]=alpha?(rgba[i]*a+alpha/2)/alpha:0;
		out[p+32]=alpha?(rgba[i+1]*a+alpha/2)/alpha:0;
		out[p+33]=alpha?(rgba[i+2]*a+alpha/2)/alpha:0;
	}
	return true;
}
#endif
