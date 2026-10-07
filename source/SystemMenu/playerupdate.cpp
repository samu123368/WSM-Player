#include "playerupdate.h"
#include "utils/updateformat.h"
#include "monocypher-ed25519.h"
#include "updatekey.h"
#include "wsmversion.h"
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>

namespace PlayerUpdate {
namespace {
struct Service {
    mutex_t mutex;
    lwp_t thread;
    Snapshot status;
    UpdateFormat::Manifest manifest;
    std::string source, directory;
    bool cancel, install, restart;
    Service() : thread(LWP_THREAD_NULL),cancel(false),install(false),restart(false) {
        LWP_MutexInit(&mutex,false);
    }
};
Service &Manager() { static Service *s=new Service; return *s; }
struct Lock {
    Service &s;
    Lock() : s(Manager()) { LWP_MutexLock(s.mutex); }
    ~Lock() { LWP_MutexUnlock(s.mutex); }
};
bool Stopped() { Lock lock; return lock.s.cancel; }
void Progress(State state,uint32_t received=0,uint32_t total=0) {
    Lock lock; lock.s.status.state=state; lock.s.status.received=received; lock.s.status.total=total;
}
void Finish(State state,Error error=NoError) {
    Lock lock; lock.s.status.state=lock.s.cancel && state!=Installed ? Cancelled : state;
    lock.s.status.error=error; lock.s.status.busy=false;
}
bool Again(int result) {
    return result==-EAGAIN || result==-EWOULDBLOCK || result==-EINPROGRESS || result==-EALREADY;
}
void NetworkResult(NetworkStage stage,int result) {
    Lock lock; lock.s.status.networkStage=stage; lock.s.status.networkResult=result;
}
bool NetworkReady() {
    NetworkResult(InitializingNetwork,net_get_status());
    if(net_get_status()==0) return true;
    int result=net_init_async(NULL,NULL);
    if(result<0 && result!=-EBUSY) { NetworkResult(InitializingNetwork,result); return false; }
    uint64_t deadline=gettime()+secs_to_ticks(20);
    while(net_get_status()==-EBUSY && gettime()<deadline && !Stopped()) usleep(10000);
    // Join libogc's asynchronous init chain before allowing exit/relaunch. This
    // runs only in the worker, never freezes the GUI, and does not reload IOS.
    const int status=net_get_status();
    if(status==-EBUSY) net_deinit();
    NetworkResult(InitializingNetwork,status==-EBUSY ? -ETIMEDOUT : status);
    return !Stopped() && status==0;
}
struct Socket {
    int fd;
    Socket() : fd(-1) {}
    ~Socket() { if(fd>=0) net_close(fd); }
};
int ReadSocket(int fd,void *out,size_t bytes) {
    uint64_t deadline=gettime()+secs_to_ticks(15);
    while(!Stopped() && gettime()<deadline) {
        int n=net_read(fd,out,bytes);
        if(!Again(n)) return n;
        usleep(10000);
    }
    return -1;
}
// Exactly one destination: bounded manifest memory or streamed DOL file.
Error Fetch(const std::string &url,uint32_t limit,uint32_t expected,
    std::string *text,FILE *file,State stage) {
    UpdateFormat::Source src;
    if(!UpdateFormat::ParseSource(url,src)) return InvalidSource;
    Socket socket; FILE *local=NULL; uint32_t total=0;
    if(src.local) {
        local=fopen(src.path.c_str(),"rb");
        if(!local) return DiskError;
        if(fseek(local,0,SEEK_END)!=0) { fclose(local); return DiskError; }
        long length=ftell(local);
        if(length<=0 || (unsigned long)length>limit) { fclose(local); return TransferError; }
        total=(uint32_t)length; rewind(local);
    } else {
        if(!NetworkReady()) return NetworkError;
        if(Stopped()) return TransferError;
        errno=0;
        hostent *host=net_gethostbyname(src.host.c_str());
        if(!host || host->h_length!=4 || !host->h_addr_list || !host->h_addr_list[0]) {
            NetworkResult(ResolvingHost,errno ? errno : -EHOSTUNREACH); return NetworkError;
        }
        sockaddr_in address={}; address.sin_family=AF_INET; address.sin_port=htons(src.port);
        memcpy(&address.sin_addr,host->h_addr_list[0],4);
        socket.fd=net_socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
        if(socket.fd<0) { NetworkResult(CreatingSocket,socket.fd); return NetworkError; }
        // IOS uses a different flag bit from newlib O_NONBLOCK. libogc's
        // FIONBIO adapter translates it; raw net_fcntl does not.
        unsigned int nonblocking=1;
        const int mode=net_ioctl(socket.fd,FIONBIO,&nonblocking);
        if(mode<0) { NetworkResult(ConfiguringSocket,mode); return NetworkError; }
        uint64_t deadline=gettime()+secs_to_ticks(15);
        int result;
        do {
            result=net_connect(socket.fd,(sockaddr*)&address,sizeof(address));
            NetworkResult(ConnectingServer,result);
            if(result==0 || result==-EISCONN) break;
            if(!Again(result) || Stopped()) return NetworkError;
            if(gettime()>=deadline) { NetworkResult(ConnectingServer,-ETIMEDOUT); return NetworkError; }
            usleep(10000);
        } while(true);
        char port[16]; snprintf(port,sizeof(port),":%u",src.port);
        std::string request="GET "+src.path+" HTTP/1.0\r\nHost: "+src.host
            +(src.port==80 ? "" : port)+"\r\nUser-Agent: WSMPlayer/" WSM_VERSION
            "\r\nAccept-Encoding: identity\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
        size_t sent=0; deadline=gettime()+secs_to_ticks(15);
        while(sent<request.size()) {
            if(Stopped() || gettime()>=deadline) return TransferError;
            int n=net_write(socket.fd,request.data()+sent,request.size()-sent);
            if(n>0) sent+=n;
            else if(Again(n)) usleep(10000);
            else return TransferError;
        }
        std::string header;
        // Header limited to 8 KiB; reading bytewise avoids retaining a DOL body.
        deadline=gettime()+secs_to_ticks(20);
        while(header.size()<8192 && gettime()<deadline) {
            char c; if(ReadSocket(socket.fd,&c,1)!=1) return TransferError;
            header+=c;
            if(header.size()>=4 && header.compare(header.size()-4,4,"\r\n\r\n")==0) break;
        }
        if(!UpdateFormat::HttpLength(header,limit,total)) return TransferError;
    }
    if(expected && total!=expected) { if(local) fclose(local); return TransferError; }
    if(text) { text->clear(); text->reserve(total); }
    uint8_t buffer[8192]; uint32_t received=0;
    const uint64_t deadline=gettime()+secs_to_ticks(300);
    Error error=NoError;
    while(received<total) {
        if(Stopped() || gettime()>=deadline) { error=TransferError; break; }
        uint32_t want=total-received; if(want>sizeof(buffer)) want=sizeof(buffer);
        int n=local ? (int)fread(buffer,1,want,local) : ReadSocket(socket.fd,buffer,want);
        if(n<=0 || (uint32_t)n>want) { error=TransferError; break; }
        if(file && fwrite(buffer,1,n,file)!=(size_t)n) { error=DiskError; break; }
        if(text) text->append((const char*)buffer,n);
        received+=n; Progress(stage,received,total);
    }
    if(local) fclose(local);
    return error;
}
Error Verify(const std::string &path,const UpdateFormat::Manifest &manifest) {
    FILE *file=fopen(path.c_str(),"rb"); if(!file) return DiskError;
    crypto_sha512_ctx ctx; crypto_sha512_init(&ctx);
    uint8_t buffer[8192],header[256],hash[64]; uint32_t received=0;
    Error error=NoError;
    while(received<manifest.size) {
        if(Stopped()) { error=TransferError; break; }
        uint32_t want=manifest.size-received; if(want>sizeof(buffer)) want=sizeof(buffer);
        size_t n=fread(buffer,1,want,file);
        if(n!=want) { error=DiskError; break; }
        if(received==0) memcpy(header,buffer,256);
        crypto_sha512_update(&ctx,buffer,n); received+=n;
        Progress(Verifying,received,manifest.size);
    }
    if(error==NoError && (fgetc(file)!=EOF || ferror(file))) error=DiskError;
    fclose(file);
    crypto_sha512_final(&ctx,hash);
    if(error!=NoError) return error;
    if(crypto_verify64(hash,manifest.hash)!=0) return HashError;
    return UpdateFormat::ValidDol(header,manifest.size) ? NoError : DolError;
}
void *Worker(void*) {
    Service &s=Manager();
    if(!s.install) {
        std::string text;
        Error error=Fetch(s.source,UpdateFormat::MaxManifestBytes,0,&text,NULL,Checking);
        if(error!=NoError || Stopped()) { Finish(Failed,error); return NULL; }
        UpdateFormat::Manifest candidate;
        UpdateFormat::Result parsed=UpdateFormat::ParseManifest(text,WsmUpdatePublicKey,candidate);
        if(parsed!=UpdateFormat::Valid) {
            Finish(Failed,parsed==UpdateFormat::BadSignature ? SignatureError : ManifestError); return NULL;
        }
        { Lock lock; s.manifest=candidate; s.status.version=candidate.version; s.status.build=candidate.build; }
        Finish(candidate.build>WSM_UPDATE_BUILD ? Available : Current); return NULL;
    }
    const std::string temp=s.directory+"boot.dol.update";
    const std::string target=s.directory+"boot.dol";
    const std::string swap=s.directory+"boot.dol.replace";
    // Never overwrite a previous incomplete transaction. Manual inspection is
    // required if a power loss left the very short rename window unfinished.
    struct stat st;
    if(stat(swap.c_str(),&st)==0) { Finish(Failed,ReplaceError); return NULL; }
    FILE *file=fopen(temp.c_str(),"wb");
    if(!file) { Finish(Failed,DiskError); return NULL; }
    Error error=Fetch(s.manifest.url,UpdateFormat::MaxDolBytes,s.manifest.size,NULL,file,Downloading);
    if(fflush(file)!=0 || ferror(file)) error=DiskError;
    if(fclose(file)!=0) error=DiskError;
    if(error==NoError) error=Verify(temp,s.manifest);
    if(error!=NoError || Stopped()) { remove(temp.c_str()); Finish(Failed,error); return NULL; }
    // Cancellation is disabled during the two renames. No retained old builds.
    { Lock lock; if(s.cancel) { remove(temp.c_str()); s.status.state=Cancelled; s.status.busy=false; return NULL; }
      s.status.state=Installing; }
    if(rename(target.c_str(),swap.c_str())!=0) {
        remove(temp.c_str()); Finish(Failed,ReplaceError); return NULL;
    }
    if(rename(temp.c_str(),target.c_str())!=0) {
        // Keep the old executable if installing the already verified file fails.
        rename(swap.c_str(),target.c_str()); remove(temp.c_str()); Finish(Failed,ReplaceError); return NULL;
    }
    // Only this transaction's old DOL is removed; never touch themes/assets.
    remove(swap.c_str());
    Finish(Installed); return NULL;
}
bool Start(bool install,const std::string &source,const std::string &directory) {
    Service &s=Manager();
    { Lock lock; if(s.status.busy) return false; }
    if(s.thread!=LWP_THREAD_NULL) { LWP_JoinThread(s.thread,NULL); s.thread=LWP_THREAD_NULL; }
    { Lock lock; s.cancel=false; }
    UpdateFormat::Source parsed;
    if(!UpdateFormat::ParseSource(source,parsed) || directory.find("sd:/")!=0
        || directory.empty() || directory.back()!='/' || directory.find("..")!=std::string::npos) {
        Finish(Failed,InvalidSource); return false;
    }
    { Lock lock;
      if(install && (s.status.state!=Available || s.manifest.build<=WSM_UPDATE_BUILD)) return false;
      s.install=install; s.source=source; s.directory=directory; s.cancel=false;
      s.status.error=NoError; s.status.busy=true; s.status.received=0; s.status.total=0;
      s.status.networkStage=NoNetworkStage; s.status.networkResult=0;
      s.status.state=install ? Downloading : Checking;
    }
    // Independent from GUI lifetimes; no renderer/WPAD/USB/IOS reload in worker.
    if(LWP_CreateThread(&s.thread,Worker,NULL,NULL,65536,45)<0) {
        s.thread=LWP_THREAD_NULL; Finish(Failed,WorkerError); return false;
    }
    return true;
}
}
Snapshot Get() { Lock lock; return lock.s.status; }
bool Busy() { return Get().busy; }
bool Check(const std::string &source,const std::string &directory) { return Start(false,source,directory); }
bool Install() { Service &s=Manager(); return Start(true,s.source,s.directory); }
void Cancel() { Lock lock; if(lock.s.status.state!=Installing) lock.s.cancel=true; }
void Reset() {
    Service &s=Manager();
    { Lock lock; if(s.status.busy) return; }
    if(s.thread!=LWP_THREAD_NULL) { LWP_JoinThread(s.thread,NULL); s.thread=LWP_THREAD_NULL; }
    Lock lock; s.status=Snapshot(); s.manifest=UpdateFormat::Manifest();
    s.source.clear(); s.cancel=false; s.restart=false;
}
void RequestRestart() { Lock lock; if(!lock.s.status.busy && lock.s.status.state==Installed) lock.s.restart=true; }
void RestartFailed() { Lock lock; lock.s.status.state=Installed; lock.s.status.error=RestartError; }
bool TakeRestartRequest() {
    Service &s=Manager();
    { Lock lock; if(!s.restart || s.status.busy) return false; s.restart=false; }
    if(s.thread!=LWP_THREAD_NULL) { LWP_JoinThread(s.thread,NULL); s.thread=LWP_THREAD_NULL; }
    return true;
}
const char *NetworkStageText(NetworkStage stage) {
    switch(stage) {
    case InitializingNetwork: return "Network initialization";
    case ResolvingHost: return "DNS lookup";
    case CreatingSocket: return "Opening socket";
    case ConfiguringSocket: return "Socket configuration";
    case ConnectingServer: return "Server connection";
    default: return "";
    }
}
const char *StatusText(State state) {
    switch(state) {
    case Idle: return "Ready to check for updates";
    case Checking: return "Checking for updates...";
    case Available: return "Update available";
    case Current: return "WSM Player is up to date";
    case Downloading: return "Downloading update...";
    case Verifying: return "Verifying update...";
    case Installing: return "Installing. Do not turn off the Wii.";
    case Installed: return "Update installed. Restart WSM Player.";
    case Failed: return "Update failed";
    case Cancelled: return "Update cancelled";
    }
    return "";
}
const char *ErrorText(Error error) {
    switch(error) {
    case NoError: return "";
    case InvalidSource: return "Use a direct HTTP or SD update manifest";
    case NetworkError: return "Network connection failed";
    case TransferError: return "Download incomplete or unsupported HTTP response";
    case ManifestError: return "Invalid update manifest";
    case SignatureError: return "Update signature is not trusted";
    case DiskError: return "Could not read or write the SD card";
    case HashError: return "Update checksum does not match";
    case DolError: return "Update is not a valid Wii executable";
    case WorkerError: return "Could not start the update worker";
    case ReplaceError: return "Could not replace boot.dol. Check SD files.";
    case RestartError: return "Restart failed. Reopen WSM Player from HBC.";
    }
    return "";
}
}
