/*-------------------------------------------------------------

usbmouse.c -- USB mouse support

Copyright (C) 2009
Daryl Borth (Tantric)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#if defined(HW_RVL)
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/usbmouse.h>
#include "wsm_usbmouse_status.h"
#include "wsm_receiver_mouse.h"

// WSM multi-mouse adaptation. Each slot owns a process-lifetime DMA buffer,
// queue, generation and outstanding transfer. No host reset, no WPAD changes.
#define MOUSE_MAX_DATA 32
#define DEVLIST_MAXSIZE 8
#define ALLLIST_MAXSIZE WSM_USB_INVENTORY_MAX
#define MOUSE_THREAD_STACKSIZE 4096
struct MouseSlot {
 volatile bool connected, pending;
 bool has_wheel, receiver_1ea7, receiver_reports;
 s32 fd, deviceId;
 u8 ep;
 u16 ep_size, interface;
 u16 vid,pid;
 volatile u32 generation;
 volatile s32 result;
 u32 submitted,completed,removed,retried;
 s8 *data;
 mouse_event events[64];
 unsigned read,write;
};
static struct MouseSlot mice[WSM_MAX_MICE];
static usb_device_entry nonMice[DEVLIST_MAXSIZE];
static unsigned nonMouseCount;
static s32 heap=-1;
static bool initialized=false;
static lwp_t worker=LWP_THREAD_NULL;
static u8 stack[MOUSE_THREAD_STACKSIZE] ATTRIBUTE_ALIGN(8);
static char diagnostics[128][144];
static unsigned diagRead,diagWrite,diagDropped;
static WSMUsbInventory inventory;
void WSM_GetUsbInventory(WSMUsbInventory *snapshot) {
 if(!snapshot)return;
 const u32 level=IRQ_Disable();
 memcpy(snapshot,&inventory,sizeof(*snapshot));snapshot->running=initialized;
 IRQ_Restore(level);
}
static void PublishInventory(s32 hidResult,const usb_device_entry *hid,u8 hidCount,
                             s32 allResult,const usb_device_entry *all,u8 count) {
 if(hidCount>32)hidCount=32;
 if(count>WSM_USB_INVENTORY_MAX)count=WSM_USB_INVENTORY_MAX;
 if(hidResult<0 || !hid)hidCount=0;
 if(allResult<0 || !all)count=0;
 WSMUsbEntry entries[WSM_USB_INVENTORY_MAX];
 memset(entries,0,sizeof(entries));
 for(unsigned d=0;d<count;++d) {
  entries[d].id=all[d].device_id;entries[d].vid=all[d].vid;
  entries[d].pid=all[d].pid;entries[d].token=all[d].token;
  entries[d].frontend=hidResult>=0 ? 2 : 0;
  for(unsigned h=0;h<hidCount;++h)
   if(hid[h].device_id==all[d].device_id && hid[h].token==all[d].token)
    entries[d].frontend=1;
 }
 // A failing combined list must not hide a successful HID list. The calls
 // can also straddle a hotplug event; include newly listed HID identities.
 unsigned published=count;
 for(unsigned h=0;h<hidCount && published<WSM_USB_INVENTORY_MAX;++h) {
  bool duplicate=false;
  for(unsigned d=0;d<published;++d)
   if(entries[d].id==hid[h].device_id && entries[d].token==hid[h].token)duplicate=true;
  if(duplicate)continue;
  WSMUsbEntry *entry=&entries[published++];
  entry->id=hid[h].device_id;entry->vid=hid[h].vid;entry->pid=hid[h].pid;
  entry->token=hid[h].token;entry->frontend=1;
 }
 const u32 level=IRQ_Disable();
 inventory.hidResult=hidResult;inventory.allResult=allResult;
 inventory.hidCount=hidCount;inventory.allCount=count;inventory.count=published;++inventory.scans;
 memcpy(inventory.entries,entries,sizeof(entries));
 IRQ_Restore(level);
}
static void Probe(const char *stage,s32 id,s32 a,s32 b,s32 c) {
 char line[144];
 snprintf(line,sizeof(line),"USB probe %s id=%ld a=%ld b=%ld c=%ld",
   stage,(long)id,(long)a,(long)b,(long)c);
 u32 level=IRQ_Disable();unsigned next=(diagWrite+1)&127;
 if(next==diagRead){++diagDropped;IRQ_Restore(level);return;}
 memcpy(diagnostics[diagWrite],line,sizeof(line));diagWrite=next;
 IRQ_Restore(level);
}
bool WSM_MouseDiagnostic(char *line,unsigned capacity) {
 if(!line || !capacity)return false;
 u32 level=IRQ_Disable();
 if(diagDropped){
  unsigned dropped=diagDropped;diagDropped=0;IRQ_Restore(level);
  snprintf(line,capacity,"USB probe dropped=%u diagnostic records",dropped);return true;
 }
 if(diagRead==diagWrite){IRQ_Restore(level);return false;}
 strncpy(line,diagnostics[diagRead],capacity-1);line[capacity-1]=0;
 diagRead=(diagRead+1)&127;IRQ_Restore(level);return true;
}

static void ClearQueue(struct MouseSlot *m) {
 u32 level=IRQ_Disable();m->read=m->write=0;IRQ_Restore(level);
}
static void Queue(struct MouseSlot *m,const mouse_event *e) {
 u32 level=IRQ_Disable();
 unsigned next=(m->write+1)&63;
 if(next==m->read)m->read=(m->read+1)&63;
 m->events[m->write]=*e;m->write=next;
 IRQ_Restore(level);
}
static s32 ReadComplete(s32 result,void *context) {
 struct MouseSlot *m=(struct MouseSlot*)context;
 ++m->completed;m->result=result;
 if(m->connected && result>=3 && result<=MOUSE_MAX_DATA) {
  mouse_event event;
  memset(&event,0,sizeof(event));
  bool decoded=false;
  if(m->receiver_1ea7)decoded=WSM_Decode1EA7Mouse((const u8*)m->data,result,
      &event.button,&event.rx,&event.ry,&event.rz);
  if(!decoded && !m->receiver_reports &&
     (!m->receiver_1ea7 || result==3 || result==4)) {
   event.button=m->data[0];event.rx=m->data[1];event.ry=m->data[2];
   // Boot reports have three bytes. An optional fourth byte is a signed
   // wheel delta, not necessarily -1/0/+1. Short reports never disable it.
   if(m->has_wheel && result>=4)event.rz=m->data[3];
   decoded=true;
  }
  if(decoded)Queue(m,&event);
 } else if(result<0) {
  Probe("read-error",m->deviceId,result,m->generation,0);
  m->connected=false;
 }
 // A successful empty/short report is not device removal. Ignore it without
 // changing buttons or player ownership; the worker will submit the next read.
 // Last operation: only completion may release ownership of the DMA buffer.
 m->pending=false;
 return 0;
}
static s32 Removed(s32 result,void *context) {
 const uintptr_t cookie=(uintptr_t)context;
 const unsigned slot=cookie&3;
 struct MouseSlot *m=&mice[slot];
 if((cookie>>2)==m->generation) {
  Probe("removed",m->deviceId,result,m->generation,m->pending);
  m->connected=false;++m->removed;ClearQueue(m);
 }
 return 1;
}
static void CloseSlot(struct MouseSlot *m) {
 if(m->fd!=-1)USB_CloseDevice(&m->fd);
 m->fd=-1;
 // A late interrupt completion still owns data. Slot cannot be reused yet.
}
static bool DeviceOwned(s32 id) {
 for(unsigned i=0;i<WSM_MAX_MICE;++i)
  if(mice[i].deviceId==id && (mice[i].fd!=-1 || mice[i].pending))return true;
 return false;
}
// -1 is a successfully inspected non-mouse; 0 is a transient open failure.
static int OpenSlot(struct MouseSlot *m,const usb_device_entry *entry) {
 s32 fd=-1;usb_devdesc desc;
 s32 result=USB_OpenDevice(entry->device_id,entry->vid,entry->pid,&fd);
 Probe("open(result,slot,fd)",entry->device_id,result,m-mice,fd);
 if(result<0)return false;
 result=USB_GetDescriptors(fd,&desc);
 Probe("descriptors(result,0,0)",entry->device_id,result,0,0);
 if(result<0){USB_CloseDevice(&fd);return false;}
 bool found=false, boot=false;
 const bool receiver=entry->vid==0x1ea7 && entry->pid==0x0064;
 for(unsigned c=0;c<desc.bNumConfigurations && !found;++c) {
  usb_configurationdesc *conf=&desc.configurations[c];
  for(unsigned i=0;i<conf->bNumInterfaces && !found;++i) {
   usb_interfacedesc *iface=&conf->interfaces[i];
   Probe("interface(number,class,subclass)",entry->device_id,
      iface->bInterfaceNumber,iface->bInterfaceClass,iface->bInterfaceSubClass);
   Probe("interface(protocol,endpoints,config)",entry->device_id,
      iface->bInterfaceProtocol,iface->bNumEndpoints,conf->bConfigurationValue);
   const bool bootMouse=iface->bInterfaceSubClass==USB_SUBCLASS_BOOT &&
      iface->bInterfaceProtocol==USB_PROTOCOL_MOUSE;
   /* Narrow exception for the descriptor-verified receiver, not arbitrary
    * keyboards/vendor devices. Other mice retain their original boot path. */
   const bool receiverMouse=receiver && iface->bInterfaceProtocol==0;
   if(iface->bInterfaceClass!=USB_CLASS_HID || (!bootMouse && !receiverMouse))continue;
   for(unsigned e=0;e<iface->bNumEndpoints;++e) {
    usb_endpointdesc *ep=&iface->endpoints[e];
    Probe("endpoint(address,attributes,packet)",entry->device_id,
       ep->bEndpointAddress,ep->bmAttributes,ep->wMaxPacketSize);
    if((ep->bmAttributes&3)!=USB_ENDPOINT_INTERRUPT ||
       !(ep->bEndpointAddress&USB_ENDPOINT_IN) ||
       ep->wMaxPacketSize<(bootMouse ? 3 : 7) || ep->wMaxPacketSize>MOUSE_MAX_DATA)continue;
    m->ep=ep->bEndpointAddress;m->ep_size=ep->wMaxPacketSize;
    m->interface=iface->bInterfaceNumber;boot=bootMouse;found=true;break;
   }
  }
 }
 USB_FreeDescriptors(&desc);
 if(!found){Probe("reject:no-boot-mouse",entry->device_id,0,0,0);USB_CloseDevice(&fd);return -1;}
 m->fd=fd;m->deviceId=entry->device_id;m->vid=entry->vid;m->pid=entry->pid;
 m->receiver_1ea7=receiver;m->receiver_reports=receiver && !boot;
 m->generation=(m->generation+1)&0x3fffffff;
 if(!m->generation)m->generation=1;
 m->has_wheel=true;ClearQueue(m);m->connected=true;
 const uintptr_t cookie=((uintptr_t)m->generation<<2)+(m-mice);
 result=USB_DeviceRemovalNotifyAsync(fd,Removed,(void*)cookie);
 Probe("removal-notify(result,slot,0)",entry->device_id,result,m-mice,0);
 if(result<0) {
  m->connected=false;CloseSlot(m);return false;
 }
 // Optional for fixed boot mice: a STALL is not fatal.
 if(boot) {
  result=USB_WriteCtrlMsg(fd,USB_REQTYPE_INTERFACE_SET,USB_REQ_SETPROTOCOL,0,m->interface,0,NULL);
  Probe("boot-protocol(result,slot,interface)",entry->device_id,result,m-mice,m->interface);
  if(receiver && result<0)m->receiver_reports=true;
 }
 if(receiver)Probe("receiver-1ea7(report-mode,slot,interface)",entry->device_id,
    m->receiver_reports,m-mice,m->interface);
 return m->connected;
}
static void ScanDevices(void) {
 static unsigned scans;
 static s32 previousResult=-999;
 static u8 previousCount=255;
 usb_device_entry *entries=iosAlloc(heap,(DEVLIST_MAXSIZE+ALLLIST_MAXSIZE)*sizeof(*entries));
 if(!entries){PublishInventory(-22,NULL,0,-22,NULL,0);
  Probe("list-allocation-failed",-1,0,0,0);return;}
 memset(entries,0,(DEVLIST_MAXSIZE+ALLLIST_MAXSIZE)*sizeof(*entries));
 u8 count=0;
 s32 listResult=USB_GetDeviceList(entries,DEVLIST_MAXSIZE,USB_CLASS_HID,&count);
 const bool report=listResult!=previousResult || count!=previousCount || (++scans%20)==0;
 if(report)Probe("list(result,count,limit)",-1,listResult,count,DEVLIST_MAXSIZE);
 previousResult=listResult;previousCount=count;
 if(listResult<0)count=0;
 if(count>DEVLIST_MAXSIZE)count=DEVLIST_MAXSIZE;
 // libogc's class=3 list excludes devices assigned to its other USB frontend.
 // Look for the verified receiver in its standard all-device list as well.
 // Never open unrelated storage, Bluetooth, keyboards or vendor devices.
 usb_device_entry *all=iosAlloc(heap,ALLLIST_MAXSIZE*sizeof(*all));
 if(all) {
  memset(all,0,ALLLIST_MAXSIZE*sizeof(*all));u8 total=0;
  const s32 allResult=USB_GetDeviceList(all,ALLLIST_MAXSIZE,0,&total);
  if(total>ALLLIST_MAXSIZE)total=ALLLIST_MAXSIZE;
  // Full HID inventory is display-only. Keep the existing eight-entry mouse
  // probing order and never open extra HID/vendor/storage devices for the UI.
  usb_device_entry *fullHid=iosAlloc(heap,32*sizeof(*fullHid));u8 hidTotal=0;
  s32 fullHidResult=-22;
  if(fullHid){memset(fullHid,0,32*sizeof(*fullHid));
   fullHidResult=USB_GetDeviceList(fullHid,32,USB_CLASS_HID,&hidTotal);}
  PublishInventory(fullHidResult,fullHid,hidTotal,allResult,all,total);
  if(fullHid)iosFree(heap,fullHid);
  unsigned added=0;
  if(allResult>=0)for(unsigned d=0;d<total;++d) {
   if(report)Probe("all-device(VID,PID,token)",all[d].device_id,all[d].vid,all[d].pid,all[d].token);
   if(all[d].vid!=0x1ea7 || all[d].pid!=0x0064)continue;
   bool duplicate=false;
   for(unsigned h=0;h<count;++h)if(entries[h].device_id==all[d].device_id && entries[h].token==all[d].token)duplicate=true;
   if(!duplicate && count<DEVLIST_MAXSIZE+ALLLIST_MAXSIZE){entries[count++]=all[d];++added;}
  }
  if(report || added)Probe("all-list(result,count,receiver-added)",-1,allResult,total,added);
  if(added && listResult<0)listResult=0;
  iosFree(heap,all);
 } else {
  PublishInventory(listResult,entries,count,-22,NULL,0);
  if(report)Probe("all-list-allocation-failed",-1,0,0,0);
 }
 if(listResult>=0) {
  // Do not repeatedly open/close a keyboard owned by libwiikeyboard. Forget
  // negative descriptor results only after that exact device disappears.
  unsigned kept=0;
  for(unsigned n=0;n<nonMouseCount;++n) {
   for(unsigned d=0;d<count;++d) {
    if(nonMice[n].device_id==entries[d].device_id && nonMice[n].token==entries[d].token &&
       nonMice[n].vid==entries[d].vid && nonMice[n].pid==entries[d].pid) {
     nonMice[kept++]=nonMice[n];break;
    }
   }
  }
  nonMouseCount=kept;
  for(unsigned d=0;d<count;++d) {
   if(report)Probe("device(VID,PID,token)",entries[d].device_id,entries[d].vid,entries[d].pid,entries[d].token);
   // IOS-listed devices are validated by their interface descriptors, not by
   // assuming nonzero VID/PID (some inexpensive mice report vendor zero).
   if(DeviceOwned(entries[d].device_id))continue;
   bool skip=false;
   for(unsigned n=0;n<nonMouseCount;++n)
    if(nonMice[n].device_id==entries[d].device_id && nonMice[n].token==entries[d].token)skip=true;
   if(skip)continue;
   for(unsigned i=0;i<WSM_MAX_MICE;++i) {
    struct MouseSlot *m=&mice[i];
    if(m->connected || m->pending || m->fd!=-1)continue;
    if(OpenSlot(m,&entries[d])<0 && nonMouseCount<DEVLIST_MAXSIZE)
     nonMice[nonMouseCount++]=entries[d];
    break;
   }
  }
 }
 if(report)for(unsigned i=0;i<WSM_MAX_MICE;++i) {
  struct MouseSlot *m=&mice[i];
  Probe("slot(number,connected,pending)",m->deviceId,i,m->connected,m->pending);
  Probe("reads(submitted,completed,last-result)",m->deviceId,m->submitted,m->completed,m->result);
 }
 iosFree(heap,entries);
}
static void ServiceSlot(struct MouseSlot *m) {
 if(!m->connected){CloseSlot(m);return;}
 if(m->pending)return;
 m->pending=true;++m->submitted;
 const s32 result=USB_ReadIntrMsgAsync(m->fd,m->ep,m->ep_size,
   m->data,ReadComplete,m);
 if(result<0) {
  // Rejected submissions have no callback; accepted ones always retain data.
  m->result=result;++m->retried;m->pending=false;m->connected=false;
 }
}
struct HostWatch {
 volatile bool pending;
 volatile u32 callbacks;
 volatile s32 completion;
 s32 registration;
 u32 nextAttempt,reportedCallbacks;
};
static struct HostWatch hostWatches[2] = {
 {false,0,0,-999,0,0}, {false,0,0,-999,0,0}
};
static s32 HostChanged(s32 result,void *context) {
 const unsigned index=(uintptr_t)context;
 if(index>=2)return 0;
 struct HostWatch *watch=&hostWatches[index];
 watch->completion=result;++watch->callbacks;
 // Do not submit IOS requests or write SD from the SDK callback thread.
 watch->pending=false;return 0;
}
static bool MouseTransfersOwned(void) {
 for(unsigned i=0;i<WSM_MAX_MICE;++i)
  if(mice[i].connected || mice[i].pending || mice[i].fd!=-1)return true;
 return false;
}
static void ServiceHostWatches(void) {
 static u32 polls,retries;
 ++polls;
 // USB_Initialize is idempotent, but its success alone does not establish that
 // both V5 frontends opened. Retry missing frontends only, at most three times
 // and never while a mouse owns a handle or an outstanding DMA transfer.
 if(polls%10==0 && retries<3 && !MouseTransfersOwned() &&
    (hostWatches[0].registration<0 || hostWatches[1].registration<0)) {
  ++retries;const s32 result=USB_Initialize();
  Probe("frontend-retry(result,attempt,0)",-1,result,retries,0);
 }
 for(unsigned i=0;i<2;++i) {
  struct HostWatch *watch=&hostWatches[i];
  const u32 callbacks=watch->callbacks;
  if(callbacks!=watch->reportedCallbacks) {
   Probe("host-change(frontend,result,count)",-1,i,watch->completion,callbacks);
   watch->reportedCallbacks=callbacks;
  }
  if(watch->pending || polls<watch->nextAttempt)continue;
  const s32 previous=watch->registration;
  watch->pending=true;
  const s32 result=USB_DeviceChangeNotifyAsync(i==0 ? USB_CLASS_HID : 0,
      HostChanged,(void*)(uintptr_t)i);
  watch->registration=result;
  if(result<0){watch->pending=false;watch->nextAttempt=polls+10;}
  if(previous!=result)Probe("host-watch(frontend,result,pending)",-1,i,result,watch->pending);
 }
 if(polls==1 || polls%10==0)for(unsigned i=0;i<2;++i)
  Probe("host-health(frontend,pending,callbacks)",-1,i,
     hostWatches[i].pending,hostWatches[i].callbacks);
 const u32 level=IRQ_Disable();
 for(unsigned i=0;i<2;++i){
  inventory.watchResult[i]=hostWatches[i].registration;
  inventory.watchPending[i]=hostWatches[i].pending;
  inventory.watchCallbacks[i]=hostWatches[i].callbacks;
 }
 IRQ_Restore(level);
}
static void *MouseWorker(void *unused) {
 (void)unused;unsigned ticks=500;
 for(;;) {
  for(unsigned i=0;i<WSM_MAX_MICE;++i)ServiceSlot(&mice[i]);
  if(++ticks>=500){ticks=0;ServiceHostWatches();ScanDevices();}
  usleep(1000);
 }
 return NULL;
}
s32 MOUSE_Init(void) {
 if(initialized)return 0;
 const s32 result=USB_Initialize();
 inventory.initAttempted=true;inventory.initResult=result;
 Probe("initialize(result,0,0)",-1,result,0,0);
 if(result!=IPC_OK)return -1;
 if(heap<0)heap=iosCreateHeap(4096);
 if(heap<0)return -2;
 for(unsigned i=0;i<WSM_MAX_MICE;++i) {
  struct MouseSlot *m=&mice[i];
  memset(m,0,sizeof(*m));m->fd=m->deviceId=-1;
  m->data=iosAlloc(heap,MOUSE_MAX_DATA);
  if(!m->data) {
   for(unsigned j=0;j<i;++j){iosFree(heap,mice[j].data);mice[j].data=NULL;}
   return -3;
  }
 }
 if(LWP_CreateThread(&worker,MouseWorker,NULL,stack,sizeof(stack),65)) {
  for(unsigned i=0;i<WSM_MAX_MICE;++i){iosFree(heap,mice[i].data);mice[i].data=NULL;}
  return -6;
 }
 initialized=true;return 0;
}
s32 MOUSE_Deinit(void) { return initialized ? -6 : 1; }
bool WSM_MouseConnected(unsigned slot) {
 return initialized && slot<WSM_MAX_MICE && mice[slot].connected;
}
u32 WSM_MouseSlotGeneration(unsigned slot) {
 return initialized && slot<WSM_MAX_MICE ? mice[slot].generation : 0;
}
s32 WSM_MouseGetEvent(unsigned slot,mouse_event *event) {
 if(!initialized || slot>=WSM_MAX_MICE)return 0;
 struct MouseSlot *m=&mice[slot];u32 level=IRQ_Disable();
 if(m->read==m->write){IRQ_Restore(level);return 0;}
 if(event)*event=m->events[m->read];
 m->read=(m->read+1)&63;IRQ_Restore(level);return 1;
}
bool MOUSE_IsConnected(void) {
 for(unsigned i=0;i<WSM_MAX_MICE;++i)if(WSM_MouseConnected(i))return true;
 return false;
}
// Legacy single-mouse entry points remain slot-zero only; WSM uses indexed API.
s32 MOUSE_GetEvent(mouse_event *event) {return WSM_MouseGetEvent(0,event);}
s32 MOUSE_FlushEvents(void) {
 s32 count=0;
 for(unsigned i=0;i<WSM_MAX_MICE;++i)while(WSM_MouseGetEvent(i,NULL))++count;
 return count;
}
u32 WSM_MouseGeneration(void) {return WSM_MouseSlotGeneration(0);}
void WSM_MouseStatus(WSMMouseStatus *s) {
 if(!s)return;
 u32 level=IRQ_Disable();memset(s,0,sizeof(*s));
 for(unsigned i=0;i<WSM_MAX_MICE;++i) {
  struct MouseSlot *m=&mice[i];
  if(m->connected){s->vid=m->vid;s->pid=m->pid;s->lastResult=m->result;}
  s->submitted+=m->submitted;s->completed+=m->completed;
  s->removed+=m->removed;s->retried+=m->retried;
 }
 IRQ_Restore(level);
}
#endif
