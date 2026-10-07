#include "wiredusbmouse.h"
#include "wsm_usbmouse_status.h"
#include "wsm_usbtrace.h"
#include <ogc/ios.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
#include <wiikeyboard/usbkeyboard.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>

namespace {
// Library workers have process lifetime, including WSM's in-process Apply.
// Do not deinitialize shared USB under WPAD/Bluetooth or free an outstanding
// library transfer on a menu reload. IOS handoff owns final process teardown.
bool mouseStarted=false, keyboardStarted=false;
char logLines[32][160];
unsigned logRead=0, logWrite=0;
void Log(const char *format, ...) {
    const unsigned next=(logWrite+1)%32;
    if(next==logRead) return;
    va_list ap; va_start(ap,format);
    vsnprintf(logLines[logWrite],sizeof(logLines[0]),format,ap);
    va_end(ap); logWrite=next;
}
void FlushLog() {
    if(logRead==logWrite) return;
    FILE *file=fopen("sd:/wsm-usb-mouse.log","a");
    if(!file) return;
    while(logRead!=logWrite) { fprintf(file,"%s\n",logLines[logRead]); logRead=(logRead+1)%32; }
    fclose(file);
}
u16 TranslateKey(u16 symbol) {
    switch(symbol) {
    case KS_KP_Enter: return WiredUsbMouse::KeyEnter;
    case KS_Home: case KS_F12: return WiredUsbMouse::KeyHome;
    case KS_Up: return WiredUsbMouse::KeyUp;
    case KS_Down: return WiredUsbMouse::KeyDown;
    case KS_Left: return WiredUsbMouse::KeyLeft;
    case KS_Right: return WiredUsbMouse::KeyRight;
    default: return symbol < 0xf000 ? symbol : WiredUsbMouse::KeyNone;
    }
}
}

WiredUsbMouse::WiredUsbMouse(): initialized(false), connected(false),
    keyboardConnected(false), keyRead(0), keyWrite(0), diagnosticFrames(0)
{ memset(keys,0,sizeof(keys)); memset(mice,0,sizeof(mice)); }
WiredUsbMouse::~WiredUsbMouse() { Shutdown(); }
void WiredUsbMouse::PrepareFrontend() {
    // libogc is the sole USB host owner. No pre-WPAD HID/VEN reservation.
}
bool WiredUsbMouse::Init() {
    if(initialized) return true;
    Log("=== USB diagnostics v1.2 R10 mouse-only session %lu ===",(unsigned long)time(NULL));
    Log("Probe values are decimal; interface class=3 HID, subclass=1 boot, protocol=2 mouse.");
    Log("Host watch: class 0=HID, 1=other USB. Subscription success is not device enumeration success.");
    Log("WSM USB: mouse-only; keyboard scanning disabled; IOS %d revision %d",
        IOS_GetVersion(),IOS_GetRevision());
    char startup[144];
    for(unsigned i=0;i<16 && WSM_UsbTraceDiagnostic(startup,sizeof(startup));++i)
        Log("%s",startup);
    if(!mouseStarted) {
        const s32 result=MOUSE_Init(); mouseStarted=result>=0;
        Log("MOUSE_Init: %d",result);
    }
    // Mouse-only mode: libwiikeyboard probes and closes non-keyboard handles,
    // including mice already owned by our worker. Keep it entirely unstarted.
    // Text entry remains available through the pointer-operated keyboard.
    if(mouseStarted) MOUSE_FlushEvents();
    if(keyboardStarted) KEYBOARD_FlushEvents();
    initialized=mouseStarted || keyboardStarted;
    FlushLog();
    return initialized;
}
void WiredUsbMouse::Shutdown() {
    if(initialized) FlushLog();
    initialized=connected=keyboardConnected=false;
    memset(mice,0,sizeof(mice));
    keyRead=keyWrite=0;
    // See process-lifetime note above. No USB_Deinitialize / raw IOS close.
}
void WiredUsbMouse::Update(bool allowBackgroundWork, bool acceptInput) {
    if(!initialized) return;
    if(allowBackgroundWork) {
        char line[144];
        for(unsigned i=0;i<8 && WSM_UsbTraceDiagnostic(line,sizeof(line));++i)Log("%s",line);
        for(unsigned i=0;i<16 && WSM_MouseDiagnostic(line,sizeof(line));++i)Log("%s",line);
        FlushLog();
    }
    connected=false;
    for(unsigned slot=0;slot<WSM_MAX_MICE;++slot) {
        MouseState &m=mice[slot];
        bool online=mouseStarted && WSM_MouseConnected(slot);
        const u32 generation=WSM_MouseSlotGeneration(slot);
        if(generation!=m.generation) {
            m.generation=generation; online=false;
            m.x=m.y=m.wheel=0;m.buttons=0;m.seen=false;
            Log("Mouse %u generation %u",slot+1,(unsigned)generation);
        }
        if(online!=m.connected) {
            Log("Mouse %u: %s",slot+1,online?"connected":"disconnected");
            m.x=m.y=m.wheel=0;m.buttons=0;m.seen=false;
        }
        m.connected=online;connected|=online;
        mouse_event event;
        for(int count=0;mouseStarted && count<64 && WSM_MouseGetEvent(slot,&event)>0;++count) {
            if(!online)continue;
            const bool changed=event.button!=m.buttons;
            m.x+=event.rx;m.y+=event.ry;m.wheel+=event.rz;
            m.buttons=event.button;m.seen=true;
            // Present every button transition in its own input frame. Draining
            // through a press AND release used to erase the entire click.
            // Disabled input keeps draining, so old clicks cannot replay later.
            if(changed && acceptInput)break;
        }
        if(!acceptInput){m.x=m.y=m.wheel=0;m.seen=false;}
    }
    const bool nowKeyboard=keyboardStarted && USBKeyboard_IsConnected();
    if(nowKeyboard!=keyboardConnected) {
        Log("libwiikeyboard: %s",nowKeyboard?"connected":"disconnected");
        keyRead=keyWrite=0;
    }
    keyboardConnected=nowKeyboard;
    keyboard_event key;
    for(int n=0;keyboardStarted && n<64 && KEYBOARD_GetEvent(&key)>0;++n) {
        if(key.type!=KEYBOARD_PRESSED || !keyboardConnected) continue;
        const u16 symbol=TranslateKey(key.symbol);
        const u8 next=(keyWrite+1)&31;
        if(symbol && next!=keyRead) { keys[keyWrite]=symbol; keyWrite=next; }
    }
    if(++diagnosticFrames>=600 && allowBackgroundWork) {
        diagnosticFrames=0;
        WSMMouseStatus status;WSM_MouseStatus(&status);
        Log("USB read: submit=%u done=%u remove=%u retry=%u last=%d connected=%d",
            (unsigned)status.submitted,(unsigned)status.completed,
            (unsigned)status.removed,(unsigned)status.retried,status.lastResult,connected);
    }
    if(allowBackgroundWork) FlushLog();
}
bool WiredUsbMouse::ReadState(int &dx,int &dy,int &wheel,u8 &buttons,unsigned slot) {
    dx=dy=wheel=0;buttons=0;
    if(slot>=WSM_MAX_MICE)return false;
    MouseState &m=mice[slot];
    dx=m.x;dy=m.y;wheel=m.wheel;buttons=m.buttons;
    m.x=m.y=m.wheel=0;
    return initialized && m.connected;
}
bool WiredUsbMouse::ReadKey(u16 &key) {
    key=KeyNone;
    if(!initialized || keyRead==keyWrite) return false;
    key=keys[keyRead]; keyRead=(keyRead+1)&31; return true;
}
void WiredUsbMouse::LogRouting(int mousePlayer,int keyboardPlayer,u32 occupiedMask) {
    Log("Routing mouse=%d keyboard=%d occupied=%08x",mousePlayer,keyboardPlayer,(unsigned)occupiedMask);
}
