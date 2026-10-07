#include "channelarttranslations.h"
#include "localization.h"
#include "../Layout.h"
#include <algorithm>
#include <cstring>

namespace {
const GXColor white={255,255,255,255}, dark={65,65,65,255};
const GXColor green={34,89,50,255}, blue={48,71,130,255};

void ArtworkTree(Pane *pane,bool hide)
{
 if(!pane) return;
 pane->SetHide(hide);
 for(Pane *child:pane->panes) ArtworkTree(child,hide);
}

bool NativeArtwork(Layout *layout,const char *english,const char *matching)
{
 Pane *original=layout->FindPane(english), *replacement=layout->FindPane(matching);
 if(!original || !replacement) return false;
 ArtworkTree(original,true);
 ArtworkTree(replacement,false);
 return true;
}

// Keep the original pane as the animation/clipping/visibility parent. Only
// replace its drawn artwork, not the surrounding channel or its animation.
void PictureTitle(Layout *layout,const char *name,const char *key,GXColor color)
{
 Pane *pane=layout->FindPane(name);
 if(!pane || pane->GetMaterialIndex()<0) return;
 Textbox *label=layout->AddLocalizedLabel(pane,Localization::GetText(key),
  pane->GetWidth(),pane->GetHeight(),pane->GetHeight()*.8f,color);
 if(!label) return;
 label->SetOrigin(pane->GetOriginX()+3*(2-pane->GetOriginY()));
 pane->SetDrawSelf(false);
}

// Some titles are individual letter sprites. Derive the replacement's bounds
// from those sprites and keep decorative hands, borders and disc art intact.
void LetterTitle(Layout *layout,const char *name,const char *prefix,
 const char *key,GXColor color)
{
 Pane *group=layout->FindPane(name);if(!group) return;
 float left=1e9f,right=-1e9f,bottom=1e9f,top=-1e9f;
 PaneList letters;
 for(Pane *p:group->panes) {
  if(p->GetMaterialIndex()<0 || (prefix && strncmp(p->getName(),prefix,strlen(prefix)))) continue;
  letters.push_back(p);
  float x=p->GetPosX(),y=p->GetPosY(),w=p->GetWidth(),h=p->GetHeight();
  left=std::min(left,x-w*.5f);right=std::max(right,x+w*.5f);
  bottom=std::min(bottom,y-h*.5f);top=std::max(top,y+h*.5f);
 }
 if(letters.empty()) return;
 Textbox *label=layout->AddLocalizedLabel(group,Localization::GetText(key),
  right-left,top-bottom,(top-bottom)*.78f,color);
 if(!label) return;
 label->SetPosition((left+right)*.5f,(top+bottom)*.5f);
 for(Pane *p:letters) p->SetHide(true);
}
}

void ChannelArtTranslations::Apply(Layout *layout,bool icon,bool nintendoChannel)
{
 if(!layout) return;
 // Retail native-language artwork remains untouched. Extra languages select
 // the English authored group and translate its image-backed title in place.
 const int language=Localization::CurrentLanguage();
 const bool today=layout->FindPane("title_en") || layout->FindPane("logo_center_en");
 if(language<7 && !(today && language==0)) return;
 // These Portuguese names are identical to the Spanish authored titles.
 // Use the actual pictures/letter sprites (including reflection and BRLAN
 // letter motion), not an approximation using the settings/menu font.
 if(language==Localization::Portuguese) {
  if(layout->FindPane("cork") && NativeArtwork(layout,"logoENG","logoSPA")) return;
  if(layout->FindPane("US_00") && NativeArtwork(layout,"N_titleUS_00","N_titleSP_00")) return;
  if(nintendoChannel) {
   if(icon && layout->FindPane("P_logoSp_00") && layout->FindPane("P_logoSp_01")) {
    NativeArtwork(layout,"P_logoE_00","P_logoSp_00");
    NativeArtwork(layout,"P_logoE_01","P_logoSp_01");
    return;
   }
   if(!icon && NativeArtwork(layout,"N_logoE_00","N_logoS_00")) return;
  }
 }
 if(layout->FindPane("cork")) {
  PictureTitle(layout,"logoENG","Photo Channel",white);
 } else if(layout->FindPane("map_WW_EUR") || layout->FindPane("map_WW_EUR_out")) {
  PictureTitle(layout,"logoENG","News Channel",green);
  PictureTitle(layout,"logoENG_sdw","News Channel",dark);
  if(Pane *flash=layout->FindPane("logoENG_flash")) flash->SetHide(true);
 } else if(layout->FindPane("N_titleUS_00") && layout->FindPane("US_00")) {
  LetterTitle(layout,"N_titleUS_00","US_","Mii Channel",dark);
 } else if(layout->FindPane("font_e") || layout->FindPane("P_ShopLogo_00")) {
  if(icon) {
   PictureTitle(layout,"P_title_E_00","Wii Shop Channel",blue);
   PictureTitle(layout,"P_title_E_01","Wii Shop Channel",blue);
  } else LetterTitle(layout,"font_e","Picture_","Wii Shop Channel",blue);
 } else if(nintendoChannel && (layout->FindPane("P_logoE_00") || layout->FindPane("N_partsE_00"))) {
  if(icon) {
   PictureTitle(layout,"P_logoE_00","Nintendo Channel",dark);
   PictureTitle(layout,"P_logoE_01","Nintendo Channel",dark);
  } else {
   LetterTitle(layout,"N_partsE_00","P_E_","Nintendo Channel",dark);
   LetterTitle(layout,"N_logoES_00",NULL,"Nintendo Channel",{135,135,135,160});
  }
 } else if(layout->FindPane("P_titleUS_00") || layout->FindPane("P_titleUS_01")) {
  if(icon) {
   PictureTitle(layout,"P_titleUS_00","Everybody Votes Channel",white);
   PictureTitle(layout,"P_titleShadUS_00","Everybody Votes Channel",dark);
  } else {
   LetterTitle(layout,"N_titleUS_00","P_titleUS_","Everybody Votes Channel",white);
   LetterTitle(layout,"N_titleUS_01","P_titleShdUS_","Everybody Votes Channel",dark);
  }
 } else if(today) {
  if(icon) PictureTitle(layout,"title_en","Today & Tomorrow Channel",blue);
  else LetterTitle(layout,"moji_en",NULL,"Today & Tomorrow Channel",blue);
 }
}
