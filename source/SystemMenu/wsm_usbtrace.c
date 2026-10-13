#include <gccore.h>
#include <ogc/ipc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wsm_usbtrace.h"
#include "wsm_stmhook.h"

// libogc remains the USB owner. Ordinary requests pass through unchanged. A
// failed V5 GETDEVICECHANGE can be rearmed explicitly from the stable UI loop;
// no host opens/closes/resets, device transfers, allocation or SD in callbacks.
// 0=HID, 1=VEN; phase 1=open, 2=version, 3=submit, 4=complete, 5=close.
typedef struct {s32 host,phase,fd,command,result;u32 value;} TraceRecord;
typedef struct {
 bool owned;u32 generation;s32 host,fd,command;
 ipccallback callback;void *context;
 void *deviceList;u32 hostGeneration;
} TraceCall;
typedef struct {
 bool queued;u32 generation;unsigned attempts,delay;
 s32 fd;void *deviceList;ipccallback callback;void *context;
} WatchRepair;
static TraceRecord records[64];
static unsigned readRecord,writeRecord,dropped;
static s32 hostFd[2]={-1,-1};
static TraceCall calls[8];
static u32 hostGeneration[2],hostVersion[2];
static WatchRepair repairs[2];

extern s32 __real_IOS_Open(const char*,u32);
extern s32 __real_IOS_Close(s32);
extern s32 __real_IOS_Ioctl(s32,s32,void*,s32,void*,s32);
extern s32 __real_IOS_IoctlAsync(s32,s32,void*,s32,void*,s32,ipccallback,void*);
s32 __wrap_IOS_IoctlAsync(s32,s32,void*,s32,void*,s32,ipccallback,void*);

static void WatchCompleted(const TraceCall *call,s32 result) {
 if(!call->deviceList)return;
 const u32 level=IRQ_Disable();const unsigned h=call->host;
 if(hostFd[h]==call->fd && hostGeneration[h]==call->hostGeneration && hostVersion[h]==0x50001) {
  WatchRepair *r=&repairs[h];
  if(result>=0){r->queued=false;r->attempts=0;}
  else {
   r->queued=true;r->generation=call->hostGeneration;r->fd=call->fd;
   r->deviceList=call->deviceList;r->callback=call->callback;r->context=call->context;
   r->delay=30u << (r->attempts<3 ? r->attempts : 3);
  }
 }
 IRQ_Restore(level);
}

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
 WSM_StmHookOpened(path,result);
 if(host>=0){const u32 level=IRQ_Disable();hostFd[host]=result>=0 ? result : -1;
  ++hostGeneration[host];hostVersion[host]=0;memset(&repairs[host],0,sizeof(WatchRepair));
  IRQ_Restore(level);Record(host,1,result,0,result,mode);}
 return result;
}
s32 __wrap_IOS_Close(s32 fd) {
 const int host=HostForFd(fd);
 // SDK teardown releases its persistent host/list even if IOS_Close fails.
 if(host>=0){const u32 level=IRQ_Disable();++hostGeneration[host];hostVersion[host]=0;
  memset(&repairs[host],0,sizeof(WatchRepair));IRQ_Restore(level);}
 const s32 result=__real_IOS_Close(fd);
 WSM_StmHookClosed(fd,result);
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
  if(command==0 && result>=0){const u32 level=IRQ_Disable();hostVersion[host]=version;IRQ_Restore(level);}
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
 WatchCompleted(&saved,result);
 // Release the observer slot BEFORE the SDK callback chains ATTACHFINISH or
 // the next GETDEVICECHANGE. Only an exact failed V5 SDK list request retains
 // its persistent host/list pointers, invalidated on frontend close/reopen.
 return saved.callback ? saved.callback(result,saved.context) : 0;
}
s32 __wrap_IOS_IoctlAsync(s32 fd,s32 command,void *in,s32 inSize,void *out,
                        s32 outSize,ipccallback callback,void *context) {
 if(WSM_StmHookHandles(fd,command))
  return WSM_StmHookSubmit(fd,command,in,inSize,out,outSize,callback,context);
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
 if(call){call->deviceList=NULL;call->hostGeneration=hostGeneration[host];
  // SDK V5 uses a persistent 32-device list (0x180 bytes). Do not retry
  // arbitrary caller requests, null callbacks, or a V4/unknown frontend.
  if(command==1 && hostVersion[host]==0x50001 && !in && !inSize &&
     out && outSize==0x180 && callback && context){
   call->deviceList=out;repairs[host].queued=false;
  }
 }
 IRQ_Restore(level);
 // Exhaustion must never prevent the original request or lose its callback.
 const s32 result=__real_IOS_IoctlAsync(fd,command,in,inSize,out,outSize,
     call ? Complete : callback,call ? (void*)call : context);
 Record(host,3,fd,command,result,call ? 1 : 0);
 if(call && result<0){const u32 release=IRQ_Disable();
  TraceCall saved=*call;bool rejected=call->generation==generation;
  if(rejected)call->owned=false;
  IRQ_Restore(release);if(rejected)WatchCompleted(&saved,result);}
 return result;
}
void WSM_UsbTraceRecoverDeviceWatch(void) {
 // Called only by the normal stable-grid input owner. Never timeout a healthy
 // pending watch, reinitialize shared USB, or duplicate a pipeline request.
 for(unsigned h=0;h<2;++h){
  const u32 level=IRQ_Disable();WatchRepair *r=&repairs[h];
  bool busy=false;
  for(unsigned i=0;i<8;++i)if(calls[i].owned && calls[i].host==(s32)h)busy=true;
  if(!r->queued || busy){IRQ_Restore(level);continue;}
  if(r->delay){--r->delay;IRQ_Restore(level);continue;}
  if(r->attempts>=5 || hostFd[h]!=r->fd || hostGeneration[h]!=r->generation){
   r->queued=false;IRQ_Restore(level);continue;
  }
  ++r->attempts;r->queued=false;const WatchRepair request=*r;IRQ_Restore(level);
  const s32 result=__wrap_IOS_IoctlAsync(request.fd,1,NULL,0,request.deviceList,0x180,
       request.callback,request.context);
  Record(h,6,request.fd,1,result,request.attempts);
 }
}
bool WSM_UsbTraceDiagnostic(char *line,unsigned capacity) {
 if(!line || !capacity)return false;
 TraceRecord record;const u32 level=IRQ_Disable();
 if(dropped){const unsigned count=dropped;dropped=0;IRQ_Restore(level);
  snprintf(line,capacity,"USB startup trace dropped=%u",count);return true;}
 if(readRecord==writeRecord){IRQ_Restore(level);return false;}
 record=records[readRecord];readRecord=(readRecord+1)&63;IRQ_Restore(level);
 const char *phases[]={"?","open","version","submit","complete","close","watch-rearm"};
 snprintf(line,capacity,"USB SDK %s %s fd=%ld cmd=%ld result=%ld value=%08lx",
  record.host==0 ? "HID" : "VEN",phases[record.phase],(long)record.fd,
  (long)record.command,(long)record.result,(unsigned long)record.value);
 return true;
}
