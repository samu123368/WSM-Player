#include "localuiassets.h"
#include "nandtitle.h"
#include "U8Archive.h"
#include "nandskinzip.h"
#include <cstdio>
#include <cstdlib>
#include <malloc.h>

u8 *LocalUiAssets::TitleFile(u64 title, const char *member, u32 *size)
{
 if(size) *size=0;
 if(!member || !size) return NULL;
 // Own the TMD instead of using NandTitles' shared scratch buffer: banner
 // workers and the main-thread agreement viewer can acquire assets together.
 u32 storedSize=0; u8 *stored=NULL;
 if(ES_GetStoredTMDSize(title,&storedSize)>=0 && storedSize>=sizeof(sig_header)
    && storedSize<=MAX_SIGNED_TMD_SIZE) {
  stored=(u8*)memalign(32,(storedSize+31)&~31);
  if(stored && ES_GetStoredTMD(title,(signed_blob*)stored,storedSize)<0) {free(stored);stored=NULL;}
 }
 if(!stored) {
  char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
  snprintf(path,sizeof(path),"/title/%08x/%08x/content/title.tmd",TITLE_UPPER(title),TITLE_LOWER(title));
  if(NandTitle::LoadFileFromNand(path,&stored,&storedSize)<0) return NULL;
 }
 if(!stored || storedSize<sizeof(sig_header) || storedSize>MAX_SIGNED_TMD_SIZE) {free(stored);return NULL;}
 const u32 signatureSize=SIGNATURE_SIZE((signed_blob*)stored);
 if(!signatureSize || signatureSize>storedSize || storedSize-signatureSize<sizeof(tmd)) {free(stored);return NULL;}
 const tmd *metadata=(const tmd*)(stored+signatureSize);
 if(metadata->title_id!=title || metadata->num_contents>MAX_NUM_TMD_CONTENTS
    || TMD_SIZE(metadata)>storedSize-signatureSize) {free(stored);return NULL;}
 std::vector<u32> ids;
 const tmd_content *contents=TMD_CONTENTS(metadata);
 for(u16 i=0;i<metadata->num_contents && i<512;++i) ids.push_back(contents[i].cid);
 free(stored);
 for(u32 id:ids) {
  char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
  snprintf(path,sizeof(path),"/title/%08x/%08x/content/%08x.app",TITLE_UPPER(title),TITLE_LOWER(title),id);
  U8NandArchive archive(path);
  if(!archive.FileDescriptor(member)) continue;
  u8 *bytes=archive.GetFileAllocated(member,size);
  if(bytes && *size>=32 && *size<=2*1024*1024) return bytes;
  free(bytes); *size=0;
 }
 return NULL;
}

u8 *LocalUiAssets::ForecastArchive(bool icon, u32 *size)
{
 return TitleFile(0x000100024841464aULL,icon?"/meta/icon.bin":"/meta/banner.bin",size);
}

u8 *LocalUiAssets::AgreementLayout(u32 *size)
{
 const u64 titles[]={0x0001000848414b50ULL,0x0001000848414b45ULL,
  0x0001000848414b4aULL,0x0001000848414b4bULL};
 for(u64 title:titles) if(u8 *bytes=TitleFile(title,"/layout.arc",size)) return bytes;
 return NULL;
}

bool LocalUiAssets::AgreementSkin(std::vector<std::vector<u8> > &images)
{
 images.clear();
 const u64 titles[]={0x0001000848414b50ULL,0x0001000848414b45ULL,
  0x0001000848414b4aULL,0x0001000848414b4bULL};
 const char *names[]={"scrollbar_v/arrow_up.png","scrollbar_v/arrow_down.png",
  "scrollbar_v/scroll_bar.png","push_button/center-center.png"};
 for(u64 title:titles) {
  u32 size=0;u8 *bytes=TitleFile(title,"/opera.arc",&size);
  if(!bytes) continue;
  U8Archive archive(bytes,size);u32 zipSize=0;
  const u8 *zip=archive.GetFile("/./eula_skin.zip",&zipSize);
  if(zip && zipSize) {
   images.resize(4);
   for(int i=0;i<4;++i) ReadNandSkinZip(zip,zipSize,names[i],images[i]);
  }
  free(bytes);
  if(images.size()==4) return true;
 }
 return false;
}
