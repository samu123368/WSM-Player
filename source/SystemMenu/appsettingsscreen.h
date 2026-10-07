#ifndef APPSETTINGSSCREEN_H
#define APPSETTINGSSCREEN_H

#include <string>
#include <vector>

#include <gccore.h>
#include "wsm_usbmouse_status.h"

#include "utils/tools.h"
#include "utils/char16.h"

class Texture;
class AgreementViewer;
class DialogWindow;

// WSM Player settings plus a read-only renderer for the real Wii Settings HTML
// archive.  The HTML view obtains its labels, links, themed GIF/PNG artwork and
// language directly from iplsetting.ash; only JavaScript/SYSCONF writes are
// intentionally disabled.
class AppSettingsScreen
{
public:
	AppSettingsScreen();
	~AppSettingsScreen();

	void Open();
	void OpenSystemSettings();
	void LoadSystemSettingsArchive( const u8 *data, u32 size );
	void BeginClose();
	bool TakeSystemSettingsExitRequest() {
		bool pending = systemSettingsExitPending;
		systemSettingsExitPending = false;
		return pending;
	}
	void HandleBack();
	void HandleSystemSettingsHistoryBack();
	void Render( Mtx &modelview, const Vec2f &screen, bool widescreen, bool allowInput = true );
	void RenderLauncherButton( const Vec2f &screen, bool hovered ) const;
	bool LauncherButtonContains( float x, float y, const Vec2f &screen ) const;

	bool IsOpen() const;
	// Shared Unicode UI text renderer for HOME and WSM-owned overlays.
	static void DrawInterfaceText( float x, float y, float maxWidth,
		const char *text, float height, float minimumHeight, GXColor color );
	bool IsClosed() const;
	bool IsFakeOobeActive() const { return fakeOobeActive; }
	bool IsUsbDevicesOpen() const { return IsOpen() && page == PageUsbDevices; }
	bool TakeChannelRefreshRequest();
	bool IsApplyingTheme() const { return themeApplyFrames >= 0; }
	bool TakeThemeRestartRequest();
	void ThemeRestartFailed( s32 error );

private:
	bool systemSettingsExitPending;
	enum Phase
	{
		Closed,
		Opening,
		Active,
		Closing
	};

	enum Dropdown
	{
		DropdownNone,
		DropdownHomeAction,
		DropdownHomebrew,
		DropdownLaunch
	};

	enum Page
	{
		PageMain,
		PageUpdates,
		PageClock,
		PageSystemSettings,
		PageAudio,
		PageInput,
		PageUsbDevices,
		PageThemes,
		PageLanguages,
		PageAgreement,
		PageChannels,
		PageNews,
		PageForecast,
		PageNintendo,
		PagePhoto,
		PageEverybodyVotes,
		PageTodayTomorrow,
		PageMiiContest,
		PageWiiFit,
		PageMiiChannel,
		PageKeyboard,
		PageImagePicker
	};

	enum EditTarget
	{
		EditNone,
		EditNewsArticle,
		EditForecastCity,
		EditForecastTemperature,
		EditForecastUnit,
		EditForecastDifference,
		EditForecastCondition,
		EditForecastFooter,
		EditForecastAttribution,
		EditNintendoText,
		EditNintendoText2,
		EditNintendoText3,
		EditEverybodyVotesFirstBlue,
		EditEverybodyVotesGreenQuestion,
		EditEverybodyVotesFinalBlue,
		EditTodayAffinity,
		EditTodayCleaning,
		EditTodayPlay,
		EditTodayMeal,
		EditMiiContestComment,
		EditWiiFitProfile,
		EditWiiFitStatus,
		EditUpdateSource
	};

	enum ImageTarget
	{
		ImagePhoto,
		ImageForecast,
		ImageNintendo,
		ImageMiiContest,
		ImageWiiFit,
		ImageMiiChannel
	};

	Phase phase;
	WSMUsbInventory usbInventory = {};
	Dropdown dropdown;
	Page page;
	Page returnPage;
	EditTarget editTarget;
	int transitionFrame;
	int selectedRow;
	int firstVisibleRow;
	AgreementViewer *agreementViewer = NULL;
	DialogWindow *systemUpdateDialog = NULL;
	std::basic_string<char16> systemUpdateText;
	std::basic_string<char16> systemUpdateButtonText;
	void SetSystemUpdateDialog(bool complete);
	bool agreementAccepted = false;
	void OpenAgreement();
	void RenderAgreement(const Vec2f &screen, float slide, float scale, u8 alpha);
	int hoveredRow;
	int dropdownFocus;
	int draggingChannel;
	int draggingScrollbar;
	float scrollbarGrabOffset;
	bool closeHovered;
	bool navigationMode;
	bool pointerWasValid;
	bool pointerHoverActive = false;
	int hoveredScrollButton = -1;
	float lastPointerX;
	float lastPointerY;
	bool settingsDirty;
	bool saveFailed;
	bool channelRefreshRequested;
	int themeApplyFrames;
	s32 themeRestartError;
	void ApplyThemeAndRestart();
	bool openedFromSystemSettings;
	bool systemSettingsAvailable;
	int systemSettingsPage;
	int systemSettingsColumns;
	std::string systemSettingsRoot;
	const u8 *systemSettingsArchiveData;
	u32 systemSettingsArchiveSize;
	std::string systemSettingsPath;
	std::string systemSettingsTitle;
	std::vector<std::string> systemSettingsLabels;
	std::vector<std::string> systemSettingsLinks;
	std::vector<std::string> systemSettingsHistory;
	std::string systemSettingsBackLink;
	std::string systemSettingsBackLabel;
	std::string systemSettingsConfirmLink;
	std::string systemSettingsMiddleLink;
	std::string systemSettingsLeftLink;
	std::string systemSettingsRightLink;
	std::string systemSettingsUpLink;
	std::string systemSettingsDownLink;
	struct HtmlRect
	{
		float x;
		float y;
		float w;
		float h;
	};
	struct HtmlSprite
	{
		std::string id;
		std::string image;
		std::string hoverImage;
		std::string backgroundImage;
		HtmlRect rect;
	};
	struct HtmlTexture
	{
		std::string path;
		Texture *texture;
		u8 *pixels;
		int width;
		int height;
	};
	struct HtmlControl
	{
		HtmlSprite sprite;
		std::string link;
		std::string label;
	};
	struct HtmlContentText
	{
		HtmlRect rect;
		std::string text;
		float fontSize;
		bool centered;
		bool bright;
		bool inputField;
	};
	bool SystemSettingsHover(const HtmlSprite &sprite, const Vec2f &screen) const;
	std::vector<HtmlContentText> systemSettingsContent;
	std::string systemSettingsBackgroundImage;
	std::vector<HtmlSprite> systemSettingsDecorations;
	std::vector<HtmlSprite> systemSettingsRowSprites;
	std::vector<HtmlControl> systemSettingsControls;
	HtmlSprite systemSettingsBackSprite;
	HtmlSprite systemSettingsConfirmSprite;
	HtmlSprite systemSettingsMiddleSprite;
	HtmlSprite systemSettingsLeftSprite;
	HtmlSprite systemSettingsRightSprite;
	HtmlSprite systemSettingsUpSprite;
	HtmlSprite systemSettingsDownSprite;
	HtmlRect systemSettingsTitleRect;
	std::string systemSettingsConfirmLabel;
	std::vector<HtmlTexture> systemSettingsTextures;
	int systemSettingsPageFrames;
	int fakeUpdatePercent;
	bool fakeOobeActive;
	int newsArticleIndex;
	int keyboardRow;
	int keyboardColumn;
	int keyboardHover;
	bool keyboardShift;
	bool keyboardAddingNewsArticle;
	int keyboardPreviousNewsIndex;
	std::string keyboardBuffer;
	std::vector<std::string> imagePaths;
	int imageIndex;
	int imageHover;
	ImageTarget imageTarget;
	bool forecastImageRejected;
	bool imageScanHadRoot;
	std::vector<std::string> themePaths;
	std::vector<bool> themeCompiled;

	void UpdateTransition();
	void UpdateInput( const Vec2f &screen );
	void Commit();
	void SelectDropdownOption( int option );
	void AdjustMusic( int delta, bool saveNow );
	void SetMusicFromPointer( float pointerX, const Vec2f &screen );
	int RowsForPage() const;
	void SetPage( Page next );
	void ActivateCurrent();
	void AdjustCurrent( int direction );
	void OpenKeyboard( EditTarget target, const std::string &value );
	void FinishKeyboard( bool accept );
	void ActivateKeyboardKey( int key );
	size_t KeyboardCapacity() const;
	void DeleteKeyboardCharacter();
	void OpenImagePicker( ImageTarget target );
	void ScanImages();
	void SelectImage();
	void ScanThemes();
	void SelectTheme();
	bool LoadSystemHtmlPage( const std::string &path, bool pushHistory );
	void ParseSystemHtmlArtwork( const std::string &html );
	void ClearSystemHtmlTextures();
	bool LoadSystemHtmlTexture( const std::string &path );
	void ResetSystemSettingsPage();
	void ActivateSystemSettingsRow();
	void NavigateSystemSettings( const std::string &link, bool pushHistory );
	void UpdateSystemSettingsSimulation();
	void RenderSystemSettings( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderDropdown( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderMain( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderUpdates( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderAudio( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderClock( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderInput( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderUsbDevices( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderLanguages( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderThemes( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderChannels( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderNews( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderForecast( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderNintendo( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderPhoto( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderEverybodyVotes( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderTodayTomorrow( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderMiiContest( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderWiiFit( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderMiiChannel( const Vec2f &screen, float slide,
		float scale, u8 alpha ) const;
	void RenderKeyboard( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
	void RenderImagePicker( const Vec2f &screen, float slide, float scale, u8 alpha ) const;
};

#endif // APPSETTINGSSCREEN_H
