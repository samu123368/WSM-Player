#include "updateformat.h"
#include "monocypher-ed25519.h"
#include <cstring>
#include <cctype>
#include <algorithm>
namespace UpdateFormat {
static bool Number(const std::string &s, uint32_t &out) {
    if(s.empty() || s.size()>10) return false;
    uint64_t n=0;
    for(size_t i=0;i<s.size();++i) {
        if(s[i]<'0'||s[i]>'9') return false;
        n=n*10+s[i]-'0'; if(n>0xffffffffULL) return false;
    }
    out=(uint32_t)n; return true;
}
static bool Hex(const std::string &s, uint8_t *out, size_t bytes) {
    if(s.size()!=bytes*2) return false;
    for(size_t i=0;i<bytes;++i) {
        unsigned v=0;
        for(size_t j=0;j<2;++j) {
            char c=s[i*2+j];
            if(c>='0'&&c<='9') v=v*16+c-'0';
            else if(c>='a'&&c<='f') v=v*16+c-'a'+10;
            else return false;
        }
        out[i]=(uint8_t)v;
    }
    return true;
}
bool ParseSource(const std::string &url, Source &s) {
    s=Source();
    if(url.empty() || url.size()>512) return false;
    for(size_t i=0;i<url.size();++i)
        if((unsigned char)url[i]<33 || (unsigned char)url[i]>126
           || url[i]=='\\' || url[i]=='#' || url[i]=='@') return false;
    if(url.find("sd:/")==0) {
        if(url.find("..")!=std::string::npos || url.size()<5) return false;
        s.local=true; s.path=url; return true;
    }
    // Signed public files use HTTP. No tokens, unverified TLS or silent downgrade.
    if(url.find("http://")!=0) return false;
    size_t slash=url.find('/',7);
    if(slash==std::string::npos) return false;
    std::string authority=url.substr(7,slash-7);
    size_t colon=authority.find(':');
    s.host=authority.substr(0,colon);
    if(s.host.empty() || s.host.size()>253) return false;
    for(size_t i=0;i<s.host.size();++i)
        if(!std::isalnum((unsigned char)s.host[i]) && s.host[i]!='.' && s.host[i]!='-') return false;
    if(colon!=std::string::npos) {
        uint32_t p;
        if(!Number(authority.substr(colon+1),p) || p<1 || p>65535) return false;
        s.port=(uint16_t)p;
    }
    s.path=url.substr(slash); return true;
}
Result ParseManifest(const std::string &text, const uint8_t key[32], Manifest &out) {
    if(text.empty() || text.size()>MaxManifestBytes || text.back()!='\n') return BadManifest;
    // Strict, ordered LF-only canonical format. Unknown/duplicate fields fail closed.
    const char *prefix[]={"WSM-UPDATE-1","version=","build=","size=","sha512=","url=","signature="};
    std::string value[7]; size_t pos=0, signedBytes=0;
    for(int i=0;i<7;++i) {
        size_t end=text.find('\n',pos);
        if(end==std::string::npos) return BadManifest;
        std::string line=text.substr(pos,end-pos);
        size_t n=strlen(prefix[i]);
        if(line.compare(0,n,prefix[i])!=0 || (i==0 && line.size()!=n)) return BadManifest;
        value[i]=line.substr(n); pos=end+1;
        if(i==5) signedBytes=pos;
    }
    if(pos!=text.size()) return BadManifest;
    Manifest m; m.version=value[1]; m.url=value[5]; Source source;
    if(m.version.empty() || m.version.size()>24 || !ParseSource(m.url,source)
       || !Number(value[2],m.build) || !m.build || !Number(value[3],m.size)
       || m.size<256 || m.size>MaxDolBytes || !Hex(value[4],m.hash,64)) return BadManifest;
    for(size_t i=0;i<m.version.size();++i)
        if(!std::isalnum((unsigned char)m.version[i]) && m.version[i]!='.' && m.version[i]!='-') return BadManifest;
    uint8_t sig[64]; if(!Hex(value[6],sig,64)) return BadManifest;
    if(crypto_ed25519_check(sig,key,(const uint8_t*)text.data(),signedBytes)!=0) return BadSignature;
    out=m; return Valid;
}
static uint32_t Be32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static bool Ram(uint32_t address, uint32_t size) {
    uint64_t end=(uint64_t)address+size;
    return size && ((address>=0x80004000 && end<=0x81800000)
        || (address>=0x90000000 && end<=0x93400000));
}
bool ValidDol(const uint8_t h[256], uint32_t size) {
    if(size<256 || size>MaxDolBytes) return false;
    const uint32_t entry=Be32(h+0xe0);
    bool executable=false;
    for(int i=0;i<18;++i) {
        uint32_t offset=Be32(h+i*4), address=Be32(h+0x48+i*4), bytes=Be32(h+0x90+i*4);
        if(!bytes) continue;
        if(offset<256 || (uint64_t)offset+bytes>size || !Ram(address,bytes) || (address&3)) return false;
        if(i<7 && entry>=address && (uint64_t)entry<(uint64_t)address+bytes && !(entry&3)) executable=true;
        for(int j=0;j<i;++j) {
            uint32_t otherSize=Be32(h+0x90+j*4), otherOffset=Be32(h+j*4), otherAddress=Be32(h+0x48+j*4);
            if(otherSize && (((uint64_t)offset<(uint64_t)otherOffset+otherSize && (uint64_t)otherOffset<(uint64_t)offset+bytes)
                || ((uint64_t)address<(uint64_t)otherAddress+otherSize && (uint64_t)otherAddress<(uint64_t)address+bytes))) return false;
        }
    }
    uint32_t bss=Be32(h+0xd8), bssSize=Be32(h+0xdc);
    return executable && (!bssSize || Ram(bss,bssSize));
}
bool HttpLength(const std::string &header, uint32_t limit, uint32_t &length) {
    size_t first=header.find("\r\n");
    if(first==std::string::npos || (header.compare(0,13,"HTTP/1.0 200 ") && header.compare(0,13,"HTTP/1.1 200 "))) return false;
    bool found=false; size_t pos=first+2;
    while(pos<header.size()) {
        size_t end=header.find("\r\n",pos); if(end==std::string::npos) return false;
        if(end==pos) return end+2==header.size() && found && length>0 && length<=limit;
        std::string line=header.substr(pos,end-pos);
        size_t colon=line.find(':'); if(colon==std::string::npos) return false;
        std::string name=line.substr(0,colon);
        std::transform(name.begin(),name.end(),name.begin(),::tolower);
        std::string value=line.substr(colon+1);
        size_t start=value.find_first_not_of(" \t"), stop=value.find_last_not_of(" \t");
        value=start==std::string::npos ? "" : value.substr(start,stop-start+1);
        if(name=="transfer-encoding" || name=="content-encoding") return false;
        if(name=="content-length") {
            if(found || !Number(value,length)) return false;
            found=true;
        }
        pos=end+2;
    }
    return false;
}
}
