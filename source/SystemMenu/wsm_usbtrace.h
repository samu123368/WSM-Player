#pragma once
#include <gccore.h>
#ifdef __cplusplus
extern "C" {
#endif
// Process-lifetime tracing of the SDK's FIRST calls, including BTE startup.
// Only dequeue/format on the normal input thread; never write SD in callbacks.
bool WSM_UsbTraceDiagnostic(char *line,unsigned capacity);
// Rearm only an observed failed SDK V5 device-list watcher, from the stable
// input owner. No USB reset, host/device close, or mouse decoder changes.
void WSM_UsbTraceRecoverDeviceWatch(void);
#ifdef __cplusplus
}
#endif
