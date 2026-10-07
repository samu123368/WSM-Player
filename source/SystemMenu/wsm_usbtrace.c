#include <gccore.h>
#include <ogc/ipc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wsm_usbtrace.h"

// Link-time observers, not another USB owner. Pass every request/buffer through
// unchanged. No host opens, resets, retries, allocation or filesystem operations.
// 0=HID, 1=VEN; phase 1=open, 2=version, 3=submit, 4=complete, 5=close.
typedef struct {s32 host,phase,fd,command,result;u32 value;} TraceRecord;
typedef struct {
 bool owned;u32 generation;s32 host,fd,command;
 ipccallback callback;void *context;
} TraceCall;
static TraceRecord records[64];
static unsigned readRecord,writeRecord,dropped;
static s32 hostFd[2]={-1,-1};
static TraceCall calls[8];

extern s32 __real_IOS_Open(const char*,u32);
extern s32 __real_IOS_Close(s32);
extern s32 __real_IOS_Ioctl(s32,s32,void*,s32,void*,s32);
extern s32 __real_IOS_IoctlAsync(s32,s32,void*,s32,void*,s32,ipccallback,void*);

static int HostForFd(s32 fd) {
 const u32 level=IRQ_Disable();int host=-1;
 if(fd>=0)for(unsigned i=0;i<2;++i)if(hostFd[i]==fd)host=i;
 IRQ_Restore(level);return host;
}
static void Record(s32 host,s32 phase,s32 fd,s32 command,s32 result,u32 value) {
 const u32 level=IRQ_Disable();const unsigned next=(writeRecord+1)&63;
 if(next==readRecord)++dropped;
 else {records[writeRecord]=(TraceRecord){host,phase,fd,command,result,value};writeRecord=next;}
 IRQ_Restore(level);
}
s32 __wrap_IOS_Open(const char *path,u32 mode) {
 int host=-1;
 if(path){if(!strcmp(path,"/dev/usb/hid"))host=0;
          else if(!strcmp(path,"/dev/usb/ven"))host=1;}
 const s32 result=__real_IOS_Open(path,mode);
 if(host>=0){const u32 level=IRQ_Disable();hostFd[host]=result>=0 ? result : -1;
  IRQ_Restore(level);Record(host,1,result,0,result,mode);}
 return result;
}
s32 __wrap_IOS_Close(s32 fd) {
 const int host=HostForFd(fd);const s32 result=__real_IOS_Close(fd);
 if(host>=0){Record(host,5,fd,0,result,0);
  if(result>=0){const u32 level=IRQ_Disable();
   if(hostFd[host]==fd)hostFd[host]=-1;
   IRQ_Restore(level);}}
 return result;
}
s32 __wrap_IOS_Ioctl(s32 fd,s32 command,void *in,s32 inSize,void *out,s32 outSize) {
 const int host=HostForFd(fd);
 const s32 result=__real_IOS_Ioctl(fd,command,in,inSize,out,outSize);
 if(host>=0 && (command==0 || command==6)){
  u32 version=0;if(result>=0 && out && outSize>=4)memcpy(&version,out,4);
  Record(host,2,fd,command,result,version);
 }
 return result;
}
static s32 Complete(s32 result,void *context) {
 TraceCall *call=(TraceCall*)context;
 const u32 level=IRQ_Disable();
 const TraceCall saved=*call;call->owned=false;
 IRQ_Restore(level);
 Record(saved.host,4,saved.fd,saved.command,result,0);
 // Release the observer slot BEFORE the SDK callback chains ATTACHFINISH or
 // the next GETDEVICECHANGE. Never retain callback-stack or caller pointers.
 return saved.callback ? saved.callback(result,saved.context) : 0;
}
s32 __wrap_IOS_IoctlAsync(s32 fd,s32 command,void *in,s32 inSize,void *out,
                        s32 outSize,ipccallback callback,void *context) {
 const int host=HostForFd(fd);
 if(host<0 || (command!=1 && command!=6))
  return __real_IOS_IoctlAsync(fd,command,in,inSize,out,outSize,callback,context);
 TraceCall *call=NULL;u32 generation=0;
 const u32 level=IRQ_Disable();
 for(unsigned i=0;i<8;++i)if(!calls[i].owned){
  call=&calls[i];call->owned=true;generation=++call->generation;
  call->host=host;call->fd=fd;call->command=command;
  call->callback=callback;call->context=context;break;
 }
 IRQ_Restore(level);
 // Exhaustion must never prevent the original request or lose its callback.
 const s32 result=__real_IOS_IoctlAsync(fd,command,in,inSize,out,outSize,
     call ? Complete : callback,call ? (void*)call : context);
 Record(host,3,fd,command,result,call ? 1 : 0);
 if(call && result<0){const u32 release=IRQ_Disable();
  if(call->generation==generation)call->owned=false;
  IRQ_Restore(release);}
 return result;
}
bool WSM_UsbTraceDiagnostic(char *line,unsigned capacity) {
 if(!line || !capacity)return false;
 TraceRecord record;const u32 level=IRQ_Disable();
 if(dropped){const unsigned count=dropped;dropped=0;IRQ_Restore(level);
  snprintf(line,capacity,"USB startup trace dropped=%u",count);return true;}
 if(readRecord==writeRecord){IRQ_Restore(level);return false;}
 record=records[readRecord];readRecord=(readRecord+1)&63;IRQ_Restore(level);
 const char *phases[]={"?","open","version","submit","complete","close"};
 snprintf(line,capacity,"USB SDK %s %s fd=%ld cmd=%ld result=%ld value=%08lx",
  record.host==0 ? "HID" : "VEN",phases[record.phase],(long)record.fd,
  (long)record.command,(long)record.result,(unsigned long)record.value);
 return true;
}
