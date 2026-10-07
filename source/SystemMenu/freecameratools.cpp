#include "freecameratools.h"
#include "Inputs.h"
#include "appsettingsscreen.h"
#include "localization.h"
#include "video.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

void FreeCameraTools::Close()
{
	open=capturePending=picking=interactRequested=false;pickTarget=-1;
	RenderInspection::RequestBounds(false);preview.Clear();
}
void FreeCameraTools::CapturePreviews()
{
	if(!open || (!capturePending && !picking)) return;
	if(context!=RenderInspection::CurrentContext() || key!=RenderInspection::CurrentKey()) {Close();return;}
	// Read refreshed bounds/text after the world is drawn, before HUD and cursor.
	for(unsigned i=0;i<count;++i)
	{
		const auto current=RenderInspection::ElementAt(sourceIndices[i]);
		if(!std::strcmp(current.name,elements[i].name)) elements[i]=current;
	}
	if(capturePending) { capturePending=false;preview.Capture(); }
	RenderInspection::RequestBounds(picking);
}

void FreeCameraTools::Open()
{
	count=RenderInspection::Count();context=RenderInspection::CurrentContext();key=RenderInspection::CurrentKey();
	for(unsigned i=0;i<count;++i) elements[i]=RenderInspection::ElementAt(i);
	std::sort(elements,elements+count,[](const RenderInspection::Element &a,const RenderInspection::Element &b) {
		// Put useful artwork before the often numerous empty transform groups.
		const unsigned order[RenderInspection::KindCount]={6,2,3,1,4,0,5};
		return a.kind!=b.kind ? order[a.kind]<order[b.kind] : std::strcmp(a.name,b.name)<0;
	});
	for(unsigned i=0;i<count;++i) for(unsigned j=0;j<count;++j)
		if(!std::strcmp(elements[i].name,RenderInspection::ElementAt(j).name)) {sourceIndices[i]=j;break;}
	SavePointers();
	selected=first=0;hovered=-1;pointerMode=false;open=true;picking=false;pickTarget=-1;
	capturePending=true;RenderInspection::RequestBounds(true);
}
void FreeCameraTools::SavePointers()
{
	for(int i=0;i<4;++i)
	{
		const auto &ir=Pad(i).GetData().ir;
		pointers[i]={ir.valid!=0,ir.x,ir.y,ir.angle};
	}
}
void FreeCameraTools::Activate(int row)
{
	if(row==0) RenderInspection::ToggleMasks();
	else if(row==1) RenderInspection::RestoreAll();
	else if(row>=2 && (unsigned)(row-2)<count)
		RenderInspection::ToggleHidden(context,key,elements[row-2].name);
	else if(row==(int)count+2) {picking=true;pickTarget=-1;RenderInspection::RequestBounds(true);}
	else if(row==(int)count+3) {Close();interactRequested=true;}
}
void FreeCameraTools::Label(unsigned index,char (&label)[80]) const
{
	unsigned number=1;
	for(unsigned i=0;i<index;++i) if(elements[i].kind==elements[index].kind) ++number;
	std::snprintf(label,sizeof(label),"%s %u",Localization::GetUtf8(RenderInspection::KindLabel(elements[index].kind)),number);
}
int FreeCameraTools::HitTest(float x,float y,int current,int direction) const
{
	int hits[RenderInspection::Capacity];unsigned size=0;
	for(unsigned i=0;i<count;++i)
	{
		const auto &e=elements[i];
		if(e.previewVisible && e.kind!=RenderInspection::Container && x>=e.left && x<e.right
			&& y>=e.top && y<e.bottom && RenderInspection::GetPolicy(context,key,e.name)!=RenderInspection::Hidden)
			hits[size++]=i;
	}
	if(!size) return -1;
	std::sort(hits,hits+size,[this](int a,int b) {
		const auto &x=elements[a],&y=elements[b];
		const float areaX=(x.right-x.left)*(x.bottom-x.top),areaY=(y.right-y.left)*(y.bottom-y.top);
		return areaX!=areaY ? areaX<areaY : sourceIndices[a]>sourceIndices[b];
	});
	if(!direction) return hits[0];
	int position=0;
	for(unsigned i=0;i<size;++i) if(hits[i]==current) {position=i;break;}
	return hits[(position+(direction>0 ? 1 : -1)+(int)size)%size];
}
void FreeCameraTools::UpdatePicker(const Vec2f &screen)
{
	hovered=-1;
	for(int i=0;i<4;++i)
	{
		Controller &pad=Pad(i);const auto &ir=pad.GetData().ir;
		if(pad.pB() || pad.pTwo()) {picking=false;pickTarget=-1;capturePending=true;RenderInspection::RequestBounds(true);return;}
		if(pad.pOne()) RenderInspection::ToggleMasks();
		const float x=ir.x*640/screen.x,y=ir.y*480/screen.y;
		if(pickTarget>=0 && ir.valid && y>=418 && y<450)
			hovered=x>=284 && x<422 ? 0 : x>=434 && x<572 ? 1 : hovered;
		if(pad.pA() && ir.valid)
		{
			if(pickTarget>=0 && y>=418 && y<450)
			{
				if(x>=284 && x<422) {RenderInspection::SetHidden(context,key,elements[pickTarget].name,true);return;}
				if(x>=434 && x<572) {RenderInspection::SetHidden(context,key,elements[pickTarget].name,false);return;}
			}
			if(y>=0 && y<396) {pickX=x/640;pickY=y/480;pickTarget=HitTest(pickX,pickY,-1,0);return;}
		}
		const int wheel=pad.UsbMouseWheel();
		if(pad.pPlus() || pad.pRight() || pad.pMinus() || pad.pLeft() || wheel)
			pickTarget=HitTest(pickX,pickY,pickTarget,(pad.pMinus() || pad.pLeft() || wheel>0) ? -1 : 1);
		if(pad.pA() && !ir.valid && pickTarget>=0)
			RenderInspection::ToggleHidden(context,key,elements[pickTarget].name);
	}
}
void FreeCameraTools::Update(const Vec2f &screen)
{
	if(!open) return;
	SavePointers();
	if(picking) {UpdatePicker(screen);return;}
	hovered=-1;
	pointerMode=false;
	const int rows=(int)count+4;
	for(int i=0;i<4;++i)
	{
		const auto &ir=Pad(i).GetData().ir;
		pointers[i]={ir.valid!=0,ir.x,ir.y,ir.angle};
		pointerMode=pointerMode || ir.valid!=0;
	}
	for(int i=0;i<4;++i)
	{
		Controller &pad=Pad(i);
		if(pad.pB() || pad.pTwo()) {Close();return;}
		if(pad.pOne()) RenderInspection::ToggleMasks();
		const int wheel=pad.UsbMouseWheel();
		if(pad.pUp() || (pad.pMinus() && !wheel)) --selected;
		if(pad.pDown() || (pad.pPlus() && !wheel)) ++selected;
		if(wheel) { first=std::max(0,std::min(first-wheel,std::max(0,(int)count-5)));selected=first+2; }
		selected=std::max(0,std::min(selected,rows-1));
		const WPADData &data=pad.GetData();
		const float x=data.ir.x*640/screen.x,y=data.ir.y*480/screen.y;
		if(data.ir.valid && x>=28 && x<612 && y>=118 && y<154)
		{
			// A gap between toolbar actions is not an invisible button.
			const int row=x<372 ? 0 : x>=388 ? 1 : -1;
			if(row>=0) {hovered=row;if(pad.pA()){selected=row;Activate(row);return;}}
		}
		else if(data.ir.valid && x>=28 && x<578 && y>=166 && y<386)
		{
			const int local=(int)((y-166)/44),row=first+local+2;
			if(row<(int)count+2 && y-166-local*44<40)
				{hovered=row;if(pad.pA()){selected=row;Activate(row);return;}}
		}
		else if(data.ir.valid && x>=588 && x<612 && y>=166 && y<386 && count>5)
		{
			if(y<190 && pad.pA()) {first=std::max(0,first-1);selected=first+2;}
			else if(y>=362 && pad.pA()) {first=std::min((int)count-5,first+1);selected=first+2;}
			else if(y>=194 && y<358 && pad.hA())
			{
				const float height=std::max(24.f,164.f*5/count);
				first=std::max(0,std::min((int)((y-194-height/2)/(164-height)*((int)count-5)+.5f),(int)count-5));
				selected=first+2;
			}
		}
		else if(data.ir.valid && y>=416 && y<448 && x>=28 && x<612)
		{
			const int action=x<300 ? (int)count+2 : x>=316 ? (int)count+3 : -1;
			if(action>=0) {hovered=action;if(pad.pA()){Activate(action);return;}}
		}
		else if(pad.pA() && !data.ir.valid) {Activate(selected);return;}
	}
	if(selected>=2 && selected<(int)count+2 && selected-2<first) first=selected-2;
	if(selected>=first+7 && selected<(int)count+2) first=selected-6;
	first=std::max(0,std::min(first,std::max(0,(int)count-5)));
}
void FreeCameraTools::RestorePointers() const
{
	for(int i=0;i<4;++i) if(pointers[i].valid)
		Pad(i).RestoreInspectionPointer(pointers[i].x,pointers[i].y,pointers[i].angle);
}
void FreeCameraTools::Render(const Vec2f &screen) const
{
	if(!open) return;
	const float sx=screen.x/640,sy=screen.y/480;
	const GXColor white={245,248,252,255},cyan={140,225,255,255},muted={163,182,200,255};
	if(picking)
	{
		if(pickTarget>=0)
		{
			const auto &e=elements[pickTarget];
			const float x=std::max(0.f,std::min(screen.x,e.left*screen.x));
			const float y=std::max(0.f,std::min(screen.y,e.top*screen.y));
			const float w=std::max(0.f,std::min(screen.x,e.right*screen.x)-x);
			const float h=std::max(0.f,std::min(screen.y,e.bottom*screen.y)-y);
			if(w>4*sx && h>4*sy)
			{
				DrawSquare(x,y,w,2*sy,cyan);DrawSquare(x,y+h-2*sy,w,2*sy,cyan);
				DrawSquare(x,y,2*sx,h,cyan);DrawSquare(x+w-2*sx,y,2*sx,h,cyan);
			}
		}
		DrawSquare(16*sx,396*sy,608*sx,62*sy,(GXColor){8,15,24,235});
		char label[80];
		if(pickTarget>=0) Label(pickTarget,label);
		else std::snprintf(label,sizeof(label),"%s",Localization::GetUtf8("Click an element"));
		AppSettingsScreen::DrawInterfaceText(28*sx,410*sy,242*sx,label,18*sy,12*sy,white);
		AppSettingsScreen::DrawInterfaceText(28*sx,441*sy,242*sx,Localization::GetUtf8("B: Elements"),11*sy,10*sy,muted);
		if(pickTarget>=0) for(int action=0;action<2;++action)
		{
			DrawSquare((284+action*150)*sx,418*sy,138*sx,32*sy,
				hovered==action ? (GXColor){30,98,130,255} : (GXColor){30,75,100,245});
			AppSettingsScreen::DrawInterfaceText((294+action*150)*sx,427*sy,118*sx,
				Localization::GetUtf8(action ? "Show" : "Hide"),16*sy,12*sy,white);
		}
		return;
	}
	DrawSquare(16*sx,78*sy,608*sx,380*sy,(GXColor){8,15,24,235});
	AppSettingsScreen::DrawInterfaceText(28*sx,94*sy,560*sx,
		Localization::GetUtf8("Elements"),22*sy,16*sy,white);
	for(int action=0;action<2;++action)
	{
		const float x=action==0 ? 28 : 388,w=action==0 ? 344 : 224;
		const bool focus=pointerMode ? hovered==action : selected==action;
		DrawSquare(x*sx,118*sy,w*sx,36*sy,focus ? (GXColor){30,98,130,255} : (GXColor){42,55,70,255});
		AppSettingsScreen::DrawInterfaceText((x+10)*sx,128*sy,(w-20-(action==0 ? 48 : 0))*sx,
			Localization::GetUtf8(action==0 ? "Masks and clipping" : "Restore all elements"),16*sy,12*sy,white);
		if(action==0) AppSettingsScreen::DrawInterfaceText(322*sx,128*sy,42*sx,
			Localization::GetUtf8(RenderInspection::MasksEnabled() ? "On" : "Off"),16*sy,12*sy,cyan);
	}
	for(int local=0;local<5 && first+local<(int)count;++local)
	{
		const int row=first+local+2;
		const float y=(166+44*local)*sy;
		const bool focus=pointerMode ? row==hovered : row==selected;
		const bool hidden=RenderInspection::GetPolicy(context,key,elements[row-2].name)==RenderInspection::Hidden;
		DrawSquare(28*sx,y,550*sx,40*sy,focus ? (GXColor){30,75,100,245} : (GXColor){28,36,47,230});
		const auto &element=elements[row-2];
		DrawSquare(36*sx,y+3*sy,56*sx,34*sy,(GXColor){12,20,28,255});
		if(!preview.Draw(element,38*sx,y+5*sy,52*sx,30*sy))
		{
			if(element.kind==RenderInspection::Container)
			{
				DrawSquare(46*sx,y+9*sy,30*sx,22*sy,muted);
				DrawSquare(48*sx,y+11*sy,26*sx,18*sy,(GXColor){12,20,28,255});
				DrawSquare(51*sx,y+14*sy,9*sx,5*sy,cyan);
				DrawSquare(62*sx,y+14*sy,9*sx,5*sy,cyan);
			}
			else AppSettingsScreen::DrawInterfaceText(42*sx,y+10*sy,44*sx,
				element.kind==RenderInspection::Text ? "Aa" : "?",20*sy,12*sy,muted);
		}
		char label[80];Label(row-2,label);
		AppSettingsScreen::DrawInterfaceText(104*sx,y+4*sy,324*sx,label,18*sy,13*sy,hidden ? muted : white);
		char dimensions[48];std::snprintf(dimensions,sizeof(dimensions),"%.0f x %.0f",element.width,element.height);
		AppSettingsScreen::DrawInterfaceText(104*sx,y+25*sy,324*sx,
			element.caption[0] ? element.caption : dimensions,11*sy,10*sy,muted);
		AppSettingsScreen::DrawInterfaceText(434*sx,y+12*sy,94*sx,Localization::GetUtf8("Hide"),16*sy,12*sy,hidden ? cyan : muted);
		DrawSquare(540*sx,y+9*sy,22*sx,22*sy,hidden ? cyan : muted);
		DrawSquare(542*sx,y+11*sy,18*sx,18*sy,(GXColor){16,25,36,255});
		if(hidden) DrawSquare(546*sx,y+15*sy,10*sx,10*sy,cyan);
	}
	if(count>5)
	{
		DrawSquare(588*sx,166*sy,24*sx,24*sy,(GXColor){42,55,70,255});
		DrawSquare(588*sx,362*sy,24*sx,24*sy,(GXColor){42,55,70,255});
		AppSettingsScreen::DrawInterfaceText(592*sx,170*sy,16*sx,"-",16*sy,12*sy,white);
		AppSettingsScreen::DrawInterfaceText(592*sx,366*sy,16*sx,"+",16*sy,12*sy,white);
		DrawSquare(594*sx,194*sy,12*sx,164*sy,(GXColor){42,55,70,255});
		const float height=std::max(24.f,164.f*5/count);
		DrawSquare(594*sx,(194+(164-height)*first/(count-5))*sy,12*sx,height*sy,cyan);
	}
	char position[48];std::snprintf(position,sizeof(position),"%d - %d / %u",count ? first+1 : 0,std::min(first+5,(int)count),count);
	AppSettingsScreen::DrawInterfaceText(28*sx,393*sy,560*sx,position,15*sy,12*sy,cyan);
	for(int action=0;action<2;++action)
	{
		const int row=(int)count+2+action;const bool focus=pointerMode ? hovered==row : selected==row;
		DrawSquare((28+288*action)*sx,416*sy,(action ? 296 : 272)*sx,32*sy,
			focus ? (GXColor){30,98,130,255} : (GXColor){42,55,70,255});
		AppSettingsScreen::DrawInterfaceText((38+288*action)*sx,425*sy,(action ? 276 : 252)*sx,
			Localization::GetUtf8(action ? "Interact" : "Pick on screen"),16*sy,12*sy,white);
	}
}
