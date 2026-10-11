#ifndef WSM_STMHOOK_H
#define WSM_STMHOOK_H
#include <ogc/ipc.h>
#ifdef __cplusplus
extern "C" {
#endif
// Observe only the SDK's STM listener. Never own/reload IOS or USB.
void WSM_StmHookOpened(const char *path, s32 result);
void WSM_StmHookClosed(s32 fd, s32 result);
bool WSM_StmHookHandles(s32 fd, s32 command);
s32 WSM_StmHookSubmit(s32 fd, s32 command, void *in, s32 inSize,
                     void *out, s32 outSize, ipccallback callback, void *context);
void WSM_StmHookService(void);
void WSM_StmHookDiagnostic(char *line, unsigned capacity);
#ifdef __cplusplus
}
#endif
#endif
