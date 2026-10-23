#ifndef WSM_MESSAGE_BOARD_GEOMETRY_H
#define WSM_MESSAGE_BOARD_GEOMETRY_H

// The board shares the main Wii Menu projection and framebuffer. Native
// 608x456 BRLYTs are not a low-resolution surface to enlarge to the screen.
namespace BoardGeometry
{
const float ViewWidth = 620.0f;
inline float ViewHeight(bool pal50) { return pal50 ? 440.0f : 472.0f; }
inline float Left(float screenWidth) { return (screenWidth - ViewWidth) * .5f; }
inline float Top(float screenHeight, bool pal50) { return (screenHeight - ViewHeight(pal50)) * .5f; }
inline float MenuX(float pixelX, float screenWidth)
{
    return Left(screenWidth) + pixelX * ViewWidth / screenWidth;
}
inline float MenuY(float pixelY, float screenHeight, bool pal50)
{
    return Top(screenHeight,pal50) + pixelY * ViewHeight(pal50) / screenHeight;
}
inline float ControlX(float pixelX, float screenWidth)
{
    return 320.0f + MenuX(pixelX,screenWidth) - screenWidth * .5f;
}
inline float ControlY(float pixelY, float screenHeight, bool pal50)
{
    return 240.0f + MenuY(pixelY,screenHeight,pal50) - screenHeight * .5f;
}
}
#endif
