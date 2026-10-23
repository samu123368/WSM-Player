#include "messageboardlive.h"
#include "localization.h"
#include "SystemMenuResources.h"
#include "object.h"
#include "video.h"
#include "messageboarddate.h"
#include "messageboardgeometry.h"
#include "utils/sc.h"
#include <ogc/conf.h>
#include "dateformat.h"
#include "settings.h"
#include "Textbox.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace
{
	void Pose(Layout *layout, const char *name, float x, float y)
	{
		Pane *pane = layout ? layout->FindPane(name) : NULL;
		if( pane ) { pane->SetPosition(x,y); pane->SetRotate(0); pane->SetScale((Vec2f){1,1}); }
	}
	void Hide(Layout *layout, const char *name, bool hidden = true)
	{
		Pane *pane = layout ? layout->FindPane(name) : NULL;
		if( pane ) pane->SetHide(hidden);
	}
	tm BoardDay(int offset)
	{
		time_t now = time(NULL);
		tm result = *localtime(&now);
		// Calendar-day arithmetic, not 86400 seconds: DST/year/month rollover
		// must not select the wrong day. Noon avoids local midnight transitions.
		result.tm_hour = 12; result.tm_min = result.tm_sec = 0;
		result.tm_mday += offset; result.tm_isdst = -1;
		mktime(&result);
		return result;
	}
}
MessageBoardLive::MessageBoardLive() : board(NULL), backdrop(NULL), chrome(NULL),
	calendar(NULL), cell(NULL), compose(NULL), memo(NULL), address(NULL),
	cellAnimation(NULL), dayStyle(NULL), selectedStyle(NULL), calendarPoseReady(false)
{
	dateText[0] = monthText[0] = cellText[0] = 0;
}
MessageBoardLive::~MessageBoardLive()
{
	delete dayStyle; delete selectedStyle; delete cellAnimation;
	for( Layout *layout : layouts ) delete layout;
	for( u8 *data : archives ) free(data);
}
Layout *MessageBoardLive::Read(const U8Archive &archive, const char *ash,
	const char *name, const char *settledAnimation)
{
	u8 *data = NULL;
	u32 size = 0;
	for( size_t i = 0; i < archiveNames.size(); ++i )
		if( archiveNames[i] == ash ) { data = archives[i]; size = archiveSizes[i]; break; }
	if( !data )
	{
		data = archive.GetFileAllocated(ash,&size);
		if( !data || !size ) { free(data); return NULL; }
		archives.push_back(data); archiveNames.push_back(ash);
		archiveSizes.push_back(size);
	}
	U8Archive arc(data,size);
	Layout *layout = LoadLayout(arc,name);
	if( !layout ) return NULL;
	layouts.push_back(layout);
	if( settledAnimation )
	{
		Animation *animation = LoadAnimation(arc,settledAnimation);
		if( animation )
		{
			{
			Object pose;
			pose.BindPane(layout->FindPane("RootPane"));
			pose.BindMaterials(layout->Materials());
			pose.AddAnimation(animation);
			pose.SetAnimation(settledAnimation);
			pose.SetFrame(animation->FrameCount());
			// Unbind before freeing animation tracks; final pose stays authored.
			pose.UnbindAllPanes();
			}
			delete animation;
		}
	}
	return layout;
}
bool MessageBoardLive::Load(const U8Archive &archive)
{
	board = Read(archive,"/layout/common/board.ash","my_IplTop_c");
	backdrop = Read(archive,"/layout/common/board.ash","my_Back_a","my_Back_a_Apear");
	chrome = Read(archive,"/layout/common/cmnBtn.ash","my_IplTop_e");
	calendar = Read(archive,"/layout/common/calendar.ash","my_IplTop_g");
	cell = Read(archive,"/layout/common/calendar.ash","my_IplTop_f");
	compose = Read(archive,"/layout/common/mlAdSel.ash","my_Mail_a","my_Mail_a_SelectIn");
	// Appear is the small board-card pose, not the opened memo page.
	memo = Read(archive,"/layout/common/letter.ash","my_Memo_a","my_Memo_a_SelectLetter");
	address = Read(archive,"/layout/common/board.ash","th_Adress_a");
	loaded = board && backdrop && chrome && calendar && cell && compose && memo && address;
	if( !loaded ) return false;
	for( size_t i = 0; i < archiveNames.size(); ++i )
		if( archiveNames[i] == "/layout/common/calendar.ash" )
		{
			cellAnimation = LoadAnimation(U8Archive(archives[i],archiveSizes[i]),"my_IplTop_f");
			if( cellAnimation )
			{
				dayStyle = new Object; dayStyle->BindMaterial(cell->FindMaterial("T_Cal"));
				dayStyle->AddAnimation(cellAnimation); dayStyle->SetAnimation("my_IplTop_f");
				selectedStyle = new Object; selectedStyle->BindMaterial(cell->FindMaterial("Cal_Ac"));
				selectedStyle->AddAnimation(cellAnimation); selectedStyle->SetAnimation("my_IplTop_f");
				selectedStyle->SetFrame(57);
			}
			break;
		}
	if( Pane *paper = backdrop->FindPane("Picture_00") ) paper->SetAlpha(255);
	Pose(board,"N_TopBack",0,0);
	Hide(board,"TopBack_a"); Hide(board,"TopBack_c");
	Pose(calendar,"N_CalAll",0,20);
	Pose(calendar,"N_Cal_b0",0,0);
	Pose(calendar,"N_Cal_b1",0,0);
	Hide(calendar,"N_Cal_a0"); Hide(calendar,"N_Cal_c0");
	Hide(cell,"Info_a"); Hide(cell,"Cal_Ac");
	if( Pane *highlight = cell->FindPane("Cal_Ac") ) highlight->SetAlpha(255);
	SetText(compose,"T_Mail",133); SetText(compose,"T_Adress",134);
	SetText(chrome,"T_CalExit",79);
	// This footer is rendered without its N_BtnL aspect-correction parent.
	// Keep its artwork and lettering corrected together, including long labels.
	if( Pane *back = chrome->FindPane("N_BtnL_a3_Cal") ) back->SetWidescreen(true);
	if( Textbox *back = chrome->FindTextbox("T_CalExit") ) back->SetUniformTextFit(true);
	SetText(memo,"T_Header",133,true);
	SetText(memo,"T_2l_TextBox",Localization::GetText("Memo"),true);
	// Retail placeholder portraits/demo messages are not real user data.
	Hide(memo,"Nigaoe"); Hide(memo,"N_TopBtn");
	Hide(memo,"N_Lost0"); Hide(memo,"T_TouchLetter"); Hide(memo,"T_Letter");
	Hide(memo,"T_Nigaoe");
	Hide(address,"N_note_move"); Hide(address,"N_note_a");
	Hide(address,"N_note_c"); Hide(address,"N_note_d"); Hide(address,"N_note_e");
	Hide(address,"N_crsr_all"); Hide(address,"N_mii_b_all");
	for( int n = 0; n < 5; ++n )
	{
		char name[32]; snprintf(name,sizeof(name),"T_name_b_%02d",n);
		SetText(address,name,Localization::GetText(""),true);
	}
	SetText(address,"T_nmbr_b",Localization::GetText("1"),true);
	Hide(chrome,"N_Dust"); Hide(chrome,"T_BbsMark1"); Hide(chrome,"Picture_00");
	Hide(chrome,"N_BtnR_a0_Bbs"); Hide(chrome,"N_BtnL_a0_Set");
	Hide(chrome,"BtnL_a1_Set"); Hide(chrome,"BtnR_a1_Bbs");
	Pose(chrome,"N_BtnL_a0",0,-156); Pose(chrome,"N_BtnL_a1",6,-163);
	Pose(chrome,"N_BtnR_a0_Ch",-61,0);
	Pose(chrome,"N_BtnL_a0_Cal",62,0); Pose(chrome,"N_BtnL_a0_Add",135,0);
	Hide(chrome,"BtnR_a0_Ch_Ac"); Hide(chrome,"BtnL_a0_Cal_Ac"); Hide(chrome,"BtnL_a0_Add_Ac");
	Hide(chrome,"ArwBtnL_Ac"); Hide(chrome,"ArwBtnR_Ac");
	return true;
}
bool MessageBoardLive::CalendarDateAt(float x, float y, const Vec2f &screen,
	int dayOffset, int monthOffset, int &selectedOffset) const
{
	if( screen.x <= 0 || screen.y <= 0 ) return false;
	Pane *frame = calendar ? calendar->FindPane("N_Cal_b1") : NULL;
	if(!frame || !calendarPoseReady) return false;
	Mtx inverse;
	if(!guMtxInverse(frame->GetView(),inverse)) return false;
	const bool pal50 = _CONF_GetVideo() == CONF_VIDEO_PAL && _CONF_GetEuRGB60() == 0;
	guVector point = {BoardGeometry::MenuX(x,screen.x),
		BoardGeometry::MenuY(y,screen.y,pal50),0}, local;
	guVecMultiply(inverse,&point,&local);
	const int n = BoardCalendarCell(local.x,local.y);
	if( n < 0 ) return false;
	tm first = BoardDay(dayOffset);
	first.tm_mday = 1; first.tm_mon += monthOffset; first.tm_isdst = -1;
	if( mktime(&first) == (time_t)-1 ) return false;
	first.tm_mday = 1 - (first.tm_wday + 6) % 7 + n; first.tm_isdst = -1;
	if( mktime(&first) == (time_t)-1 ) return false;
	selectedOffset = BoardCivilDay(first) - BoardCivilDay(BoardDay(0));
	return true;
}
void MessageBoardLive::DrawChrome(Mtx &view,
	bool landing, bool arrows, u8 alpha, bool widescreen)
{
	Hide(chrome,"N_BtnL_a0",!landing); Hide(chrome,"N_BtnL_a1",!landing);
	Hide(chrome,"N_BtnR_a0",!landing); Hide(chrome,"N_BtnR_a1",!landing);
	for( int n = 3; n <= 8; ++n )
	{
		char name[24]; snprintf(name,sizeof(name),"N_BtnL_a%d",n); Hide(chrome,name);
	}
	Hide(chrome,"N_ArwL",!arrows); Hide(chrome,"N_ArwR",!arrows);
	chrome->RenderPane("RootPane",view,alpha,widescreen);
	if( !landing )
	{
		// Isolated authored back button: no offscreen parent rotations or old
		// input objects to leak into the main menu after returning.
		Pose(chrome,"N_BtnL_a3_Cal",-210,-174);
		Hide(chrome,"N_BtnL_a3_Cal",false);
		Hide(chrome,"N_CalExitAc");
		chrome->RenderPane("N_BtnL_a3_Cal",view,alpha,widescreen);
	}
}
void MessageBoardLive::DrawCalendar(Mtx &view, const Vec2f &canvas,
	const tm &date, int monthOffset, u8 alpha, bool widescreen)
{
	tm first = date;
	first.tm_mday = 1; first.tm_mon += monthOffset; first.tm_isdst = -1;
	mktime(&first);
	const int monday = (first.tm_wday + 6) % 7;
	strlcpy16(monthText,Bmg::Instance()->GetMessage(118 + first.tm_mon),64);
	const int len = strlen16(monthText);
	snprintf16(monthText + len,64 - len," %d",first.tm_year + 1900);
	SetText(calendar,"T_CalMonth_b",monthText);
	if( Textbox *label = calendar->FindTextbox("T_CalMonth_b") ) label->SetUniformTextFit(true);
	const char *days[] = {"TextBox_08","TextBox_09","TextBox_10","TextBox_11","TextBox_12","TextBox_13","TextBox_07"};
	for( int day = 0; day < 7; ++day )
	{
		SetText(calendar,days[day],day == 6 ? 116 : 110 + day);
		if( Textbox *label = calendar->FindTextbox(days[day]) ) label->SetUniformTextFit(true);
	}
	calendar->RenderPane("RootPane",view,alpha,widescreen);
	Pane *frame = calendar->FindPane("N_Cal_b1");
	if( !frame ) return;
	calendarPoseReady = true;
	for( int n = 0; n < 42; ++n )
	{
		tm current = first; current.tm_mday = 1 - monday + n; current.tm_isdst = -1;
		mktime(&current);
		// Native frame 0=weekday, 1=Saturday, 2=Sunday, 3=other month.
		// The BRLYT's default C8/C8/C8 is the disabled-day style, not normal ink.
		if( dayStyle ) dayStyle->SetFrame(current.tm_mon != first.tm_mon ? 3
			: current.tm_wday == 6 ? 1 : current.tm_wday == 0 ? 2 : 0);
		snprintf16(cellText,8,"%d",current.tm_mday);
		SetText(cell,"T_Cal",cellText);
		if( Textbox *label = cell->FindTextbox("T_Cal") ) label->SetUniformTextFit(true);
		Hide(cell,"Cal_Ac", !(current.tm_year == date.tm_year
			&& current.tm_mon == date.tm_mon && current.tm_mday == date.tm_mday));
		Mtx translation, centered; guMtxIdentity(translation);
		guMtxTransApply(translation,translation,-210 + (n % 7) * 70,
			137 - (n / 7) * 50,0);
		// Reuse the straight frame's rendered matrix, including the native
		// widescreen correction. Do not apply that correction a second time.
		guMtxConcat(frame->GetView(),translation,centered);
		cell->RenderPane("N_CalDay_p",centered,
			current.tm_mon == first.tm_mon ? alpha : (u8)(alpha / 3));
		// Cal_Ac is an opaque highlight drawn after the number in the archive.
		// Repaint the selected number above it so today is never blank.
		if( current.tm_year == date.tm_year && current.tm_mon == date.tm_mon
			&& current.tm_mday == date.tm_mday ) cell->RenderPane("T_Cal",centered,alpha);
	}
}
void MessageBoardLive::Draw(int page, int dayOffset, int monthOffset,
	float x, float y, const Vec2f &screen, u8 alpha, bool widescreen)
{
	if( !loaded ) return;
	const bool landing = page <= 2;
	tm date = BoardDay(dayOffset);
	const int weekday = date.tm_wday == 0 ? 116 : date.tm_wday + 109;
	strlcpy16(dateText,Bmg::Instance()->GetMessage(weekday),64);
	const int len = strlen16(dateText);
	char numeric[24];
	DateFormat::Format(numeric, sizeof(numeric), Settings::dateOrder,
		date.tm_mon + 1, date.tm_mday, Localization::MonthFirstDate());
	snprintf16(dateText + len,64 - len," %s", numeric);
	SetText(board,"T_Day_b",dateText);
	if(Textbox *text = board->FindTextbox("T_Day_b")) text->SetUniformTextFit(true);
	Mtx view; guMtxIdentity(view);
	guMtxScaleApply(view,view,1,-1,1);
	guMtxTransApply(view,view,x + screen.x * .5f,y + screen.y * .5f,0);
	const Vec2f canvas = {608,456};
	if( landing ) board->RenderPane("RootPane",view,alpha,widescreen);
	else backdrop->RenderPane("RootPane",view,alpha,widescreen);
	if( page == 3 ) DrawCalendar(view,canvas,date,monthOffset,alpha,widescreen);
	if( page == 4 ) compose->RenderPane("RootPane",view,alpha,widescreen);
	if( page == 5 ) memo->RenderPane("RootPane",view,alpha,widescreen);
	if( page == 6 ) address->RenderPane("RootPane",view,alpha,widescreen);
	ThemeUi *skin = SystemMenuResources::Instance()->ThemeWidgets();
	// Overlay controls use the same centered native units as the board, not
	// a second independent stretch to the physical framebuffer dimensions.
	const float controlX = x + screen.x * .5f - 320;
	const float controlY = y + screen.y * .5f - 240;
	if( page == 7 || page == 8 )
	{
		// Remain read-only. Match the existing two visible button hitboxes,
		// without connecting to WC24 or changing the console's settings.
		if( skin )
		{
			skin->DialogText(canvas,x + (screen.x-canvas.x)*.5f,
				y + (screen.y-canvas.y)*.5f,alpha,Bmg::Instance()->GetMessage(382));
			skin->LabeledButton(controlX + 140,controlY + 340,160,72,alpha,Bmg::Instance()->GetMessage(79),widescreen);
			skin->LabeledButton(controlX + 340,controlY + 340,160,72,alpha,Bmg::Instance()->GetMessage(14),widescreen);
		}
		return;
	}
	DrawChrome(view,landing,landing || page == 3 || page == 6,alpha,widescreen);
	if( skin && (page == 5 || page == 6) )
		skin->LabeledButton(controlX + 455,controlY + 375,170,72,alpha,
			Bmg::Instance()->GetMessage(page == 5 ? 36 : 41),widescreen);
}
