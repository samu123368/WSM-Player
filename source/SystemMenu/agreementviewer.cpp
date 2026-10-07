#include "agreementviewer.h"
#include "utils/localuiassets.h"
#include <cstdlib>
#include <algorithm>

AgreementViewer::~AgreementViewer() { Clear(); }
void AgreementViewer::Clear() {
 GX_DrawDone();
 delete layout; layout=NULL;
 archiveBytes.clear(); count=0;
}
bool AgreementViewer::Open(const char16 *no) {
 Clear();
 u32 size=0; u8 *local=LocalUiAssets::AgreementLayout(&size);
 if(!local)return false;
 archiveBytes.assign(local,local+size);free(local);
 U8Archive archive(&archiveBytes[0],archiveBytes.size());
 const u8 *data=archive.GetFile("/arc/blyt/EULA_Viewer_a.brlyt");
 layout=new Layout;
 if(!data||!layout->Load(data)||!layout->LoadTextures(archive)||!layout->LoadFonts(archive)) {Clear();return false;}
 // The archive stores N_All at the transparent start of EULA_In. The settings
 // screen supplies the transition alpha; use the native layout's visible pose.
 if(Pane *all=layout->FindPane("N_All"))all->SetAlpha(255);
 noText=no;
 if(Textbox *box=layout->FindTextbox("T_BtnA")){box->SetText(noText.c_str());box->SetUniformTextFit(true);}
 const char *hidden[]={"EULA_Pane","N_BtnA_Ac","N__BtnB_Ac","N_BtnB"};
 for(unsigned i=0;i<4;++i)if(Pane *p=layout->FindPane(hidden[i]))p->SetHide(true);
 count=1;
 return true;
}
void AgreementViewer::Render(Mtx &view,const Vec2f &screen,u8 alpha) {
 if(layout)layout->Render(view,screen,false,alpha);
}
