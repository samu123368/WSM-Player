#include "menufreecamera.h"
#include <algorithm>
#include <cmath>

MenuFreeCamera::MenuFreeCamera() : active(false),threeD(false),layers(true),zoom(1.f),panX(0),panY(0),yaw(25.f),pitch(-15.f) {}

void MenuFreeCamera::ResetView()
{
	zoom = 1.0f;
	panX = panY = 0;
	yaw=threeD ? 0.f : 25.f;pitch=threeD ? 0.f : -15.f;
	flyX=flyY=flyZ=0;flySpeed=0;
	dragging=dragMoved=false;
}

void MenuFreeCamera::SetActive( bool enabled )
{
	active = enabled;
	if(!enabled)threeD=false;
	ResetView();
}

void MenuFreeCamera::Orbit(int horizontal,int vertical,float seconds)
{
	if(!active || !threeD)return;
	seconds=std::max(0.f,std::min(seconds,.05f));
	yaw=std::max(-75.f,std::min(75.f,yaw+std::max(-1,std::min(1,horizontal))*60.f*seconds));
	pitch=std::max(-65.f,std::min(65.f,pitch+std::max(-1,std::min(1,vertical))*60.f*seconds));
}
void MenuFreeCamera::DragOrbit(bool held,bool valid,float x,float y)
{
	if(!active || !threeD || !valid || !std::isfinite(x) || !std::isfinite(y)) {dragging=false;return;}
	if(!held) {if(dragging && !dragMoved)ResetView();dragging=false;return;}
	if(!dragging) {dragging=true;dragMoved=false;dragX=x;dragY=y;return;}
	float dx=x-dragX,dy=y-dragY;dragX=x;dragY=y;
	if(std::fabs(dx)+std::fabs(dy)<1.f)return;
	dragMoved=true;dx=std::max(-40.f,std::min(40.f,dx));dy=std::max(-40.f,std::min(40.f,dy));
	yaw=std::max(-75.f,std::min(75.f,yaw+dx*.35f));pitch=std::max(-65.f,std::min(65.f,pitch+dy*.35f));
}

void MenuFreeCamera::Move( int horizontal, int vertical, int zoomDirection,
	float seconds, float screenWidth, float screenHeight )
{
	if( !active ) return;
	seconds = std::max( 0.0f, std::min( seconds, 0.05f ) );
	zoomDirection = std::max( -1, std::min( zoomDirection, 1 ) );
	zoom = std::max( 0.125f, std::min( 8.0f,
		zoom * std::pow( 2.0f, zoomDirection * seconds ) ) );
	const float distance = 240.0f * seconds / zoom;
	panX += std::max( -1, std::min( horizontal, 1 ) ) * distance;
	panY += std::max( -1, std::min( vertical, 1 ) ) * distance;
	panX = std::max( -4.0f * screenWidth, std::min( panX, 4.0f * screenWidth ) );
	panY = std::max( -4.0f * screenHeight, std::min( panY, 4.0f * screenHeight ) );
}

void MenuFreeCamera::ApplyProjection( float (&projection)[4][4] ) const
{
	if( !active ) return;
	// Spectator view/projection are applied at submission, not to authored UI.
	if(threeD)return;
	// Zoom about the original view centre, then move the orthographic camera.
	// Leave Z/W intact so layouts retain their authored depth and draw order.
	for( int column = 0; column < 4; ++column )
	{
		projection[0][column] *= zoom;
		projection[1][column] *= zoom;
	}
	projection[0][3] -= panX * projection[0][0];
	projection[1][3] -= panY * projection[1][1];
}

float MenuFreeCamera::FlySpeed() const { return std::pow(2.f,(float)flySpeed); }
void MenuFreeCamera::Fly(float strafe,float forward,float rise,float lookX,float lookY,
	int speedDirection,float seconds)
{
	if(!active || !threeD || !std::isfinite(strafe) || !std::isfinite(forward)
		|| !std::isfinite(rise) || !std::isfinite(lookX) || !std::isfinite(lookY)
		|| !std::isfinite(seconds))return;
	seconds=std::max(0.f,std::min(seconds,.05f));
	flySpeed=std::max(-3,std::min(3,flySpeed+std::max(-1,std::min(1,speedDirection))));
	yaw=std::fmod(yaw+std::max(-1.f,std::min(1.f,lookX))*70.f*seconds+540.f,360.f)-180.f;
	pitch=std::max(-85.f,std::min(85.f,pitch+std::max(-1.f,std::min(1.f,lookY))*60.f*seconds));
	strafe=std::max(-1.f,std::min(1.f,strafe));forward=std::max(-1.f,std::min(1.f,forward));
	rise=std::max(-1.f,std::min(1.f,rise));
	const float length=std::sqrt(strafe*strafe+forward*forward+rise*rise);
	if(length>1.f){strafe/=length;forward/=length;rise/=length;}
	const float rad=.01745329252f,cy=std::cos(yaw*rad),sy=std::sin(yaw*rad);
	const float cp=std::cos(pitch*rad),sp=std::sin(pitch*rad);
	const float distance=240.f*FlySpeed()*seconds;
	// Camera-relative forward follows pitch; strafe stays level, C/Z fly vertically.
	flyX+=distance*(strafe*cy+forward*sy*cp);
	flyY+=distance*(forward*sp+rise);
	flyZ+=distance*(strafe*sy-forward*cy*cp);
	flyX=std::max(-20000.f,std::min(20000.f,flyX));
	flyY=std::max(-20000.f,std::min(20000.f,flyY));
	flyZ=std::max(-20000.f,std::min(20000.f,flyZ));
}
