#pragma once
#include <gccore.h>
#ifdef __cplusplus
extern "C" {
#endif
// Process-lifetime tracing of the SDK's FIRST calls, including BTE startup.
// Only dequeue/format on the normal input thread; never write SD in callbacks.
bool WSM_UsbTraceDiagnostic(char *line,unsigned capacity);
#ifdef __cplusplus
}
#endif
