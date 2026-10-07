#ifndef WSM_FREE_CAMERA_PREVIEW_H
#define WSM_FREE_CAMERA_PREVIEW_H
#include <gccore.h>
#include "renderinspection.h"

// A single owned screenshot, not a collection of live layout pointers.
class FreeCameraPreview
{
public:
	FreeCameraPreview() : pixels(NULL),width(0),height(0) {}
	~FreeCameraPreview() { Clear(); }
	void Capture();
	void Clear();
	bool Draw(const RenderInspection::Element &element,float x,float y,float w,float h) const;
private:
	u8 *pixels;
	u16 width,height;
	GXTexObj texture;
	FreeCameraPreview(const FreeCameraPreview &) = delete;
	FreeCameraPreview &operator=(const FreeCameraPreview &) = delete;
};
#endif
