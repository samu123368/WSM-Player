#ifndef WIRED_USB_MOUSE_H_
#define WIRED_USB_MOUSE_H_
#include <gccore.h>
#include "wsm_usbmouse_status.h"

// WSM input adapter only. Standard libraries own enumeration and transfers.
class WiredUsbMouse
{
public:
    enum Key { KeyNone=0, KeyBackspace=8, KeyTab=9, KeyEnter=13,
        KeyEscape=27, KeySpace=32, KeyDelete=127, KeyUp=0x100,
        KeyDown, KeyLeft, KeyRight, KeyHome };
    WiredUsbMouse();
    ~WiredUsbMouse();
    static void PrepareFrontend();
    bool Init();
    void Shutdown();
    void Update(bool allowBackgroundWork=true, bool acceptInput=true);
    bool ReadState(int &dx,int &dy,int &wheel,u8 &buttons,unsigned slot=0);
    bool ReadKey(u16 &key);
    bool IsConnected() const { return connected; }
    bool HasMouseStateReport(unsigned slot=0) const { return slot<WSM_MAX_MICE && mice[slot].seen; }
    bool IsKeyboardConnected() const { return keyboardConnected; }
    void LogRouting(int mousePlayer,int keyboardPlayer,u32 occupiedMask);
private:
    bool initialized, connected, keyboardConnected;
    struct MouseState { bool connected,seen; int x,y,wheel; u8 buttons; u32 generation; };
    MouseState mice[WSM_MAX_MICE];
    u8 keyRead, keyWrite;
    u16 keys[32];
    u32 diagnosticFrames;
};
#endif
