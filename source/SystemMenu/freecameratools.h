#ifndef WSM_FREE_CAMERA_TOOLS_H
#define WSM_FREE_CAMERA_TOOLS_H
#include "renderinspection.h"
#include "freecamerapreview.h"
#include "utils/tools.h"

class FreeCameraTools
{
public:
	FreeCameraTools() : open(false),pointerMode(false),capturePending(false),picking(false),interactRequested(false),pickTarget(-1),pickX(0),pickY(0),count(0),selected(0),first(0),hovered(-1),key(0),context(RenderInspection::Grid) {}
	bool IsOpen() const { return open; }
	void Open();
	void Close();
	void CapturePreviews();
	void Update(const Vec2f &screen);
	void Render(const Vec2f &screen) const;
	void RestorePointers() const;
	void SavePointers();
	bool PointerOver(float x,float y,float width,float height) const {
		for(const auto &p:pointers)if(p.valid && p.x>=x && p.x<x+width && p.y>=y && p.y<y+height)return true;
		return false;
	}
	bool IsPicking() const { return open && picking; }
	bool TakeInteractionRequest() { const bool value=interactRequested;interactRequested=false;return value; }
private:
	bool open;
	bool pointerMode;
	bool capturePending;
	bool picking,interactRequested;
	int pickTarget;
	float pickX,pickY;
	FreeCameraPreview preview;
	unsigned count;
	int selected,first,hovered;
	unsigned key;
	RenderInspection::Context context;
	RenderInspection::Element elements[RenderInspection::Capacity];
	unsigned sourceIndices[RenderInspection::Capacity];
	struct Pointer { bool valid; float x,y,angle; };
	Pointer pointers[4] = {};
	void Activate(int row);
	void UpdatePicker(const Vec2f &screen);
	int HitTest(float x,float y,int current,int direction) const;
	void Label(unsigned index,char (&label)[80]) const;
};
#endif
