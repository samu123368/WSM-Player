#include "nandskinzip.h"
#include <cstring>
#include <zlib.h>

namespace {
unsigned U16(const unsigned char *p) { return p[0] | unsigned(p[1]) << 8; }
unsigned U32(const unsigned char *p) { return U16(p) | U16(p+2) << 16; }
bool Range(size_t offset,size_t length,size_t size) {
 return offset <= size && length <= size-offset;
}
}
bool ReadNandSkinZip(const unsigned char *zip,size_t size,const char *name,
                     std::vector<unsigned char> &out) {
 out.clear();
 if(!zip || !name || size<22 || size>2*1024*1024) return false;
 size_t end=size-22;
 const size_t first=size>65557 ? size-65557 : 0;
 for(;;) {
  if(U32(zip+end)==0x06054b50 && Range(end+22,U16(zip+end+20),size)
     && end+22+U16(zip+end+20)==size) break;
  if(end==first) return false;
  --end;
 }
 if(U16(zip+end+4) || U16(zip+end+6)
    || U16(zip+end+8)!=U16(zip+end+10)) return false;
 const unsigned count=U16(zip+end+10), directorySize=U32(zip+end+12);
 size_t at=U32(zip+end+16);
 if(count>1024 || !Range(at,directorySize,end) || at+directorySize!=end) return false;
 const size_t directoryStart=at,directoryEnd=at+directorySize, wanted=std::strlen(name);
 for(unsigned i=0;i<count;++i) {
  if(!Range(at,46,directoryEnd) || U32(zip+at)!=0x02014b50) return false;
  const unsigned n=U16(zip+at+28),extra=U16(zip+at+30),comment=U16(zip+at+32);
  if(!Range(at+46,n+extra+comment,directoryEnd)) return false;
  if(n==wanted && !std::memcmp(zip+at+46,name,n)) {
   const unsigned flags=U16(zip+at+8),method=U16(zip+at+10);
   const unsigned compressed=U32(zip+at+20),plain=U32(zip+at+24),crc=U32(zip+at+16);
   const size_t local=U32(zip+at+42);
   if(flags || U16(zip+at+34) || (method!=0 && method!=8)
      || !plain || plain>64*1024 || compressed>128*1024
      || !Range(local,30,directoryStart) || U32(zip+local)!=0x04034b50
      || U16(zip+local+6)!=flags || U16(zip+local+8)!=method
      || U32(zip+local+14)!=crc || U32(zip+local+18)!=compressed
      || U32(zip+local+22)!=plain || U16(zip+local+26)!=n) return false;
   const size_t payload=local+30+n+U16(zip+local+28);
   if(!Range(local+30,n,directoryStart) || std::memcmp(zip+local+30,name,n)
      || !Range(payload,compressed,directoryStart)) return false;
   out.resize(plain);
   bool ok=false;
   if(method==0) {ok=plain==compressed; if(ok)std::memcpy(out.data(),zip+payload,plain);}
   else {
    z_stream stream={}; stream.next_in=(Bytef*)(zip+payload);stream.avail_in=compressed;
    stream.next_out=out.data();stream.avail_out=plain;
    if(inflateInit2(&stream,-MAX_WBITS)==Z_OK) {
     ok=inflate(&stream,Z_FINISH)==Z_STREAM_END && stream.total_out==plain
        && stream.total_in==compressed;
     inflateEnd(&stream);
    }
   }
   if(ok && crc32(0,out.data(),plain)==crc) return true;
   out.clear();return false;
  }
  at+=46+n+extra+comment;
 }
 return false;
}
