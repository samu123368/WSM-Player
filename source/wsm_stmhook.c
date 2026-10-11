#include <gccore.h>
#include <ogc/stm.h>
#include <stdio.h>
#include <string.h>
#include "wsm_stmhook.h"

extern s32 __real_IOS_IoctlAsync(s32,s32,void*,s32,void*,s32,ipccallback,void*);
extern s32 __STM_SetEventHook(void);
static s32 eventFd=-1, immediateFd=-1, hookResult=STM_ENOTINIT;
static bool pending, completing;
static unsigned generation, retries, cooldown;
static ipccallback sdkCallback;
static void *sdkContext;

void WSM_StmHookOpened(const char *path,s32 result) {
 if(!path)return;
 if(!strcmp(path,"/dev/stm/eventhook"))eventFd=result;
 else if(!strcmp(path,"/dev/stm/immediate"))immediateFd=result;
}
void WSM_StmHookClosed(s32 fd,s32 result) {
 if(result<0)return;
 if(fd==eventFd){eventFd=-1;pending=false;}
 if(fd==immediateFd)immediateFd=-1;
}
bool WSM_StmHookHandles(s32 fd,s32 command) {
 return fd>=0 && fd==eventFd && command==0x1000;
}
static s32 Complete(s32 result,void *unused) {
 (void)unused;
 // The SDK rearms inside its callback. Copy and relinquish the old request
 // first; never overwrite the newly chained callback when this one returns.
 const ipccallback callback=sdkCallback;void *context=sdkContext;
 pending=false;completing=true;hookResult=result;
 const s32 answer=callback ? callback(result,context) : 0;
 completing=false;
 return answer;
}
s32 WSM_StmHookSubmit(s32 fd,s32 command,void *in,s32 inSize,
                     void *out,s32 outSize,ipccallback callback,void *context) {
 // A healthy SDK request is left untouched by Service: exactly one listener.
 sdkCallback=callback;sdkContext=context;pending=true;
 const unsigned submitted=++generation;
 const s32 result=__real_IOS_IoctlAsync(fd,command,in,inSize,out,outSize,Complete,NULL);
 if(generation==submitted){hookResult=result;if(result<0)pending=false;}
 return result;
}
void WSM_StmHookService(void) {
 const u32 level=IRQ_Disable();const bool active=pending||completing;IRQ_Restore(level);
 if(active)return;
 if(cooldown){--cooldown;return;}
 if(retries>=3)return;
 ++retries;cooldown=300;
 // libogc 2.10's __STM_Init reports success even if submitting the listener
 // failed. Retry initialization only when its descriptors never opened.
 if(eventFd<0 || immediateFd<0){__STM_Init();return;}
 // A forwarder's abandoned IOS hook can reject the new listener. Cancel it
 // ONLY after our request has failed/completed, never while ours is pending.
 // The SDK release helper refuses when its registered flag is already zero.
 u32 in[8] ATTRIBUTE_ALIGN(32)={0},out[8] ATTRIBUTE_ALIGN(32)={0};
 IOS_Ioctl(immediateFd,0x3002,in,sizeof(in),out,sizeof(out));
 __STM_SetEventHook();
}
void WSM_StmHookDiagnostic(char *line,unsigned capacity) {
 if(!line || !capacity)return;
 const u32 level=IRQ_Disable();
 const s32 ef=eventFd,im=immediateFd,result=hookResult;
 const bool armed=pending;const unsigned attempts=retries;
 IRQ_Restore(level);
 snprintf(line,capacity,"console-stm event-fd=%ld immediate-fd=%ld armed=%u result=%ld retries=%u",
  (long)ef,(long)im,(unsigned)armed,(long)result,attempts);
}
