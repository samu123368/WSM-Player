#include "renderinspection.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace RenderInspection
{
namespace
{
	struct Override { Context context; unsigned key; Element element; Policy policy; };
	Element elements[Capacity];
	Override overrides[Capacity];
	unsigned count=0, overrideCount=0, currentKey=0;
	Context context=Grid;
	bool active=false, masks=true, hasContext=false, boundsRequested=false;
	bool threeD=false,separateLayers=true;
	bool spectator=false;
	float camera[3]={0,0,0};
	float rotation[3][3]={{1,0,0},{0,1,0},{0,0,1}},drawLayer=0;
	Element CopyName(const char *name)
	{
		Element element = {};
		if(name) for(unsigned i=0;i<16 && name[i];++i) element.name[i]=name[i];
		return element;
	}
	bool Equal(const char *a,const char *b) { return !std::strncmp(a,b,16); }
	void CopyCaption(char (&output)[97],const short *text)
	{
		unsigned out=0;
		if(text) for(unsigned i=0;i<32 && text[i];++i)
		{
			unsigned c=(unsigned short)text[i];
			if(c>=0xd800 && c<=0xdbff && i<31)
			{
				const unsigned low=(unsigned short)text[i+1];
				if(low>=0xdc00 && low<=0xdfff) {c=0x10000+((c-0xd800)<<10)+(low-0xdc00);++i;}
				else c=0xfffd;
			}
			else if(c>=0xd800 && c<=0xdfff) c=0xfffd;
			if(c<32) c=' ';
			const unsigned bytes=c<0x80 ? 1 : c<0x800 ? 2 : c<0x10000 ? 3 : 4;
			if(out+bytes>=sizeof(output)) break;
			if(bytes==1) output[out++]=c;
			else
			{
				output[out++]=(bytes==2 ? 0xc0 : bytes==3 ? 0xe0 : 0xf0) | (c>>(6*(bytes-1)));
				for(unsigned shift=bytes-1;shift>0;--shift) output[out++]=0x80|((c>>(6*(shift-1)))&63);
			}
		}
		output[out]=0;
	}
}
void BeginFrame(Context next,unsigned key)
{
	if(!hasContext || next!=context || key!=currentKey) count=0;
	context=next;currentKey=key;hasContext=true;active=true;
	drawLayer=0;
}
void EndFrame() { active=false; }
bool Active() { return active; }
bool Unmasked() { return active && !masks; }
bool MasksEnabled() { return masks; }
void ToggleMasks() { masks=!masks; }
bool IsMask(const char *name)
{
	Element element=CopyName(name);
	for(unsigned i=0;i<16 && element.name[i];++i)
		if(element.name[i]>='A' && element.name[i]<='Z') element.name[i]+='a'-'A';
	return std::strstr(element.name,"mask")!=0;
}
Policy GetPolicy(Context target,unsigned key,const char *name)
{
	for(unsigned i=0;i<overrideCount;++i)
		if(overrides[i].context==target && overrides[i].key==key
			&& Equal(overrides[i].element.name,name)) return overrides[i].policy;
	return Auto;
}
Kind Classify(unsigned magic,const char *name,bool channel)
{
	if(channel) return Channel;
	if(IsMask(name)) return Mask;
	Element id=CopyName(name);
	for(char &c:id.name) if(c>='A' && c<='Z') c+='a'-'A';
	if(std::strstr(id.name,"btn") || std::strstr(id.name,"button")) return Button;
	if(magic==0x74787431) return Text; // txt1
	if(magic==0x70696331) return Image; // pic1
	if(magic==0x776e6431) return Panel; // wnd1
	return Container;
}
const char *KindLabel(Kind kind)
{
	const char *const labels[]={"Container","Image","Text","Button","Panel","Channel","Mask"};
	return labels[(unsigned)kind<KindCount ? kind : Container];
}
void RequestBounds(bool enabled)
{
	boundsRequested=enabled;
	if(enabled) for(unsigned i=0;i<count;++i) elements[i].previewVisible=false;
}
bool WantsBounds() { return active && boundsRequested; }
void SetBounds(const char *name,float left,float top,float right,float bottom)
{
	if(!WantsBounds() || !name || !std::isfinite(left) || !std::isfinite(top)
		|| !std::isfinite(right) || !std::isfinite(bottom)) return;
	for(unsigned i=0;i<count;++i) if(Equal(elements[i].name,name))
	{
		Element &e=elements[i];
		e.left=std::max(0.f,std::min(1.f,left));e.top=std::max(0.f,std::min(1.f,top));
		e.right=std::max(0.f,std::min(1.f,right));e.bottom=std::max(0.f,std::min(1.f,bottom));
		e.previewVisible=e.right>e.left && e.bottom>e.top;
		return;
	}
}
Policy Observe(const char *name,Kind kind,float width,float height,const short *caption)
{
	if(!active || !name) return Auto;
	unsigned index=0;
	for(;index<count;++index) if(Equal(elements[index].name,name)) break;
	if(index==count && count<Capacity) elements[count++]=CopyName(name);
	if(index<count)
	{
		Element &element=elements[index];element.kind=kind;
		element.width=width;element.height=height;CopyCaption(element.caption,caption);
	}
	return GetPolicy(context,currentKey,name);
}
void CyclePolicy(Context target,unsigned key,const char *name)
{
	if(!name) return;
	for(unsigned i=0;i<overrideCount;++i)
		if(overrides[i].context==target && overrides[i].key==key
			&& Equal(overrides[i].element.name,name))
		{
			if(overrides[i].policy==Shown) overrides[i]=overrides[--overrideCount];
			else overrides[i].policy=Shown;
			return;
		}
	if(overrideCount<Capacity) overrides[overrideCount++]={target,key,CopyName(name),Hidden};
}
unsigned Count() { return count; }
void ToggleHidden(Context target,unsigned key,const char *name)
{
	SetHidden(target,key,name,GetPolicy(target,key,name)!=Hidden);
}
void SetHidden(Context target,unsigned key,const char *name,bool hidden)
{
	if(!name) return;
	for(unsigned i=0;i<overrideCount;++i)
		if(overrides[i].context==target && overrides[i].key==key
			&& Equal(overrides[i].element.name,name))
		{
			if(!hidden) overrides[i]=overrides[--overrideCount];
			else overrides[i].policy=Hidden;
			return;
		}
	if(hidden && overrideCount<Capacity) overrides[overrideCount++]={target,key,CopyName(name),Hidden};
}
Element ElementAt(unsigned index) { return index<count ? elements[index] : Element{}; }
Context CurrentContext() { return context; }
unsigned CurrentKey() { return currentKey; }
void RestoreAll() { overrideCount=0;masks=true; }
void Configure3D(bool enabled,float yaw,float pitch,bool layers)
{
	spectator=false;
	threeD=enabled && std::isfinite(yaw) && std::isfinite(pitch);separateLayers=layers;
	const float rad=.01745329252f;
	yaw=std::max(-75.f,std::min(75.f,yaw))*rad;
	pitch=std::max(-65.f,std::min(65.f,pitch))*rad;
	const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
	const float value[3][3]={{cy,0,sy},{sp*sy,cp,-sp*cy},{-cp*sy,sp,cp*cy}};
	std::memcpy(rotation,value,sizeof(rotation));
}
void ConfigureSpectator(bool enabled,float yaw,float pitch,bool layers,float x,float y,float z)
{
	threeD=enabled && std::isfinite(yaw) && std::isfinite(pitch)
		&& std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
	spectator=true;separateLayers=layers;camera[0]=x;camera[1]=y;camera[2]=z;
	if(!threeD)return;
	const float rad=.01745329252f,cy=std::cos(yaw*rad),sy=std::sin(yaw*rad);
	const float cp=std::cos(pitch*rad),sp=std::sin(pitch*rad);
	const float value[3][3]={{cy,0,sy},{-sy*sp,cp,cy*sp},{-sy*cp,-sp,cy*cp}};
	std::memcpy(rotation,value,sizeof(rotation));
}
bool PerspectiveProjection(float (&out)[4][4],const float authored[4][4],
	const float (&reference)[4][4])
{
	if(!ThreeDActive() || !spectator || std::fabs(reference[0][0])<.000001f
		|| std::fabs(reference[1][1])<.000001f)return false;
	const float distance=1.7320508f/std::fabs(reference[1][1]);
	const float cx=-reference[0][3]/reference[0][0],cy=-reference[1][3]/reference[1][1];
	const float near=1.f,far=60000.f;
	std::memset(out,0,sizeof(float)*16);
	out[0][0]=std::fabs(authored[0][0])*distance;
	out[1][1]=std::fabs(authored[1][1])*distance;
	out[0][2]=-(authored[0][0]*cx+authored[0][3]);
	out[1][2]=-(authored[1][1]*cy+authored[1][3]);
	// GX perspective depth maps the visible range to [-1,0], not OpenGL [-1,1].
	out[2][2]=-near/(far-near);out[2][3]=-far*near/(far-near);
	out[3][2]=-1.f;
	return true;
}
bool ThreeDActive() { return active && threeD; }
DrawLayerScope::DrawLayerScope(const char *name) : previous(drawLayer)
{
	drawLayer=0;
	if(ThreeDActive() && separateLayers && name)
		for(unsigned i=0;i<count;++i)if(Equal(elements[i].name,name)) {
			if(elements[i].kind!=Container)drawLayer=i*3.f;
			break;
		}
}
DrawLayerScope::~DrawLayerScope() { drawLayer=previous; }
void TransformView(float (&view)[3][4],const float (&projection)[4][4])
{
	if(!ThreeDActive() || std::fabs(projection[0][0])<.000001f || std::fabs(projection[1][1])<.000001f)return;
	const float cx=-projection[0][3]/projection[0][0],cy=-projection[1][3]/projection[1][1];
	if(!std::isfinite(cx) || !std::isfinite(cy))return;
	float original[3][4];std::memcpy(original,view,sizeof(original));original[2][3]+=drawLayer;
	if(spectator) {
		const float distance=1.7320508f/std::fabs(projection[1][1]);
		// Separation is a viewing aid, not a change to the authored UI layout.
		// Placing a full-size quad nearer the camera magnifies it and pulls its
		// lettering away from its background. Fit each layer about the common
		// view centre so the neutral perspective view matches the 2D projection.
		// Keep positive depths bounded smoothly: authored banner Z plus our
		// separation must not put a whole layer through the near plane.
		const float depth=original[2][3]>0.f
			? original[2][3]/(1.f+original[2][3]/(.75f*distance)) : original[2][3];
		const float fit=(distance-depth)/distance;
		const float sx=projection[0][0]<0 ? -1.f : 1.f,sy=projection[1][1]<0 ? -1.f : 1.f;
		for(unsigned c=0;c<4;++c){original[0][c]*=sx*fit;original[1][c]*=sy*fit;}
		original[0][3]-=sx*cx*fit+camera[0];original[1][3]-=sy*cy*fit+camera[1];
		original[2][3]=depth-distance-camera[2];
		for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
			view[r][c]=rotation[r][0]*original[0][c]+rotation[r][1]*original[1][c]+rotation[r][2]*original[2][c];
		return;
	}
	for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
		view[r][c]=rotation[r][0]*original[0][c]+rotation[r][1]*original[1][c]+rotation[r][2]*original[2][c];
	view[0][3]+=cx-rotation[0][0]*cx-rotation[0][1]*cy;
	view[1][3]+=cy-rotation[1][0]*cx-rotation[1][1]*cy;
	view[2][3]+=-rotation[2][0]*cx-rotation[2][1]*cy;
}
}
