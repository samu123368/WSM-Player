#ifndef WSM_MENU_FREE_CAMERA_H
#define WSM_MENU_FREE_CAMERA_H

// A transient, read-only developer view. No settings, input drivers or layouts
// are changed, and callers restore their projection after each drawn frame.
class MenuFreeCamera
{
public:
	MenuFreeCamera();
	bool IsActive() const { return active; }
	void SetActive( bool enabled );
	void ResetView();
	void Move( int horizontal, int vertical, int zoomDirection, float seconds,
		float screenWidth, float screenHeight );
	void ApplyProjection( float (&projection)[4][4] ) const;
	bool Is3D() const { return threeD; }
	bool LayersEnabled() const { return layers; }
	void Toggle3D() { threeD=!threeD;ResetView(); }
	void ToggleLayers() { layers=!layers; }
	void Orbit(int horizontal,int vertical,float seconds);
	void DragOrbit(bool held,bool valid,float x,float y);
	float Yaw() const { return yaw; }
	float Pitch() const { return pitch; }
	float Zoom() const { return zoom; }
	float PanX() const { return panX; }
	float PanY() const { return panY; }
	void Fly(float strafe,float forward,float rise,float lookX,float lookY,
		int speedDirection,float seconds);
	float FlyX() const { return flyX; }
	float FlyY() const { return flyY; }
	float FlyZ() const { return flyZ; }
	float FlySpeed() const;

private:
	bool active,threeD,layers;
	float zoom, panX, panY;
	float yaw,pitch;
	float flyX=0,flyY=0,flyZ=0;
	int flySpeed=0;
	bool dragging=false,dragMoved=false;
	float dragX=0,dragY=0;
};

#endif
