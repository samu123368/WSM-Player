#pragma once
#include <gccore.h>
#include <ogc/usbmouse.h>
#define WSM_MAX_MICE 4
#define WSM_USB_INVENTORY_MAX 64
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { u16 vid,pid; s32 lastResult; u32 submitted,completed,removed,retried; } WSMMouseStatus;
typedef struct { s32 id; u16 vid,pid; u32 token; u8 frontend; } WSMUsbEntry;
typedef struct {
 bool initAttempted,running;
 s32 initResult,hidResult,allResult;
 u32 scans;
 u8 hidCount,allCount,count;
 s32 watchResult[2];
 u32 watchCallbacks[2];
 bool watchPending[2];
 WSMUsbEntry entries[WSM_USB_INVENTORY_MAX];
} WSMUsbInventory;
// Copy a worker-owned snapshot; never issue USB requests from GUI rendering.
void WSM_GetUsbInventory(WSMUsbInventory *snapshot);
u32 WSM_MouseGeneration(void);
void WSM_MouseStatus(WSMMouseStatus *status);
bool WSM_MouseConnected(unsigned slot);
u32 WSM_MouseSlotGeneration(unsigned slot);
s32 WSM_MouseGetEvent(unsigned slot,mouse_event *event);
// Bounded worker-to-main diagnostic queue. Never writes SD from USB callbacks.
bool WSM_MouseDiagnostic(char *line,unsigned capacity);
#ifdef __cplusplus
}
#endif
