#ifndef CHANNEL_PREVIEW_CONFIG_H
#define CHANNEL_PREVIEW_CONFIG_H

#include <string>

#include <gctypes.h>

class TiXmlElement;

// User-authored data which is injected into supported channel previews.  This
// module deliberately owns only small strings and paths; decoded photos remain
// the renderer's responsibility so opening Settings cannot exhaust MEM2.
namespace ChannelPreview
{
	const int MaxNewsArticles = 12;
	const int MaxNewsArticleBytes = 160;
	const int MaxCityBytes = 64;
	const int MaxTemperatureBytes = 32;
	const int MaxConditionBytes = 160;
	const int MaxNintendoTextBytes = 256;
	const int MaxTodayTomorrowTextBytes = 160;
	const int MaxMiiContestTextBytes = 160;
	const int MaxWiiFitTextBytes = 160;
	// These are single-line native icon panes. Keep custom text within the
	// readable 28-character blue-bubble envelope instead of accepting a long
	// value that would have to be shrunk to only a few pixels on the grid.
	// The trailing byte is reserved for NUL by the settings keyboard/XML loader.
	const int MaxEverybodyVotesTextBytes = 29;
	const int MaxTextBytes = MaxNintendoTextBytes;
	const int MaxPathBytes = 256;

	enum Unit
	{
		Celsius,
		Fahrenheit
	};

	enum ForecastTimeMode
	{
		ForecastTimeDay,
		ForecastTimeNight,
		ForecastTimeAutomatic
	};

	// Retail Forecast Channel condition codes written to settings XML and
	// consumed by the banner renderer.  Keeping authored codes here lets the
	// normal renderer select its layered worldwide art while Japanese mode can
	// select the corresponding genuine single- or two-symbol composition.
	enum ForecastWeatherCode
	{
		ForecastWeatherSunny = 0x0464,
		ForecastWeatherPartlySunny = 0x0463,
		ForecastWeatherPartlyCloudy = 0x0465,
		ForecastWeatherMostlyCloudy = 0x04c9,
		ForecastWeatherCloudy = 0x04c8,
		ForecastWeatherFog = 0x0680,
		ForecastWeatherShowers = 0x052e,
		ForecastWeatherPartlySunnyRain = 0x0467,
		ForecastWeatherMostlyCloudyShowers = 0x04cb,
		ForecastWeatherRain = 0x052c,
		ForecastWeatherPartlySunnyThunder = 0x0466,
		ForecastWeatherMostlyCloudyThunder = 0x04ca,
		ForecastWeatherThunder = 0x0784,
		ForecastWeatherPartlySunnyFlurries = 0x0468,
		ForecastWeatherSnow = 0x05e0,
		ForecastWeatherRainAndSnow = 0x052f,
		ForecastWeatherSleet = 0x04cf
	};

	enum { ForecastWeatherChoiceCount = 17 };
	extern const u16 ForecastWeatherChoices[ ForecastWeatherChoiceCount ];

	enum FitMode
	{
		Fit,
		Fill
	};

	enum EverybodyVotesStyle
	{
		EverybodyVotesJoke,
		EverybodyVotesOriginal,
		EverybodyVotesCustom,
		// Keep the retail window colours/order while allowing all three
		// messages to be edited: blue, then purple, then green.
		EverybodyVotesOgSequence
	};

	struct NewsConfig
	{
		bool enabled;
		bool japaneseGlobe;
		int count;
		std::string articles[ MaxNewsArticles ];
	};

	struct ForecastConfig
	{
		bool enabled;
		std::string city;
		std::string temperature;
		int temperatureDifference; // Japanese daily layout only, -99..99.
		std::string condition;
		std::string footer;
		std::string attribution;
		std::string imagePath;
		bool automaticIcon;
		Unit unit;
		std::string customUnit; // Empty keeps C/F; display label only, no conversion.
		u16 weatherCode;
		ForecastTimeMode timeMode;
		bool japaneseIcons;
		bool japaneseWeatherIcons; // Independent from japaneseIcons (legacy layout key).
	};

	struct NintendoConfig
	{
		bool enabled;
		std::string text;
		std::string extraText[2]; // Optional stories 2/3; blank entries are skipped.
		std::string imagePath;
		FitMode fitMode;
	};

	struct PhotoConfig
	{
		bool enabled;
		std::string imagePath;
		FitMode fitMode;
	};

	struct EverybodyVotesConfig
	{
		EverybodyVotesStyle style;
		std::string firstBlue;
		std::string greenQuestion;
		std::string finalBlue;
	};

	struct TodayTomorrowConfig
	{
		bool enabled;
		std::string affinity;
		std::string cleaning;
		std::string play;
		std::string meal;
	};

	struct MiiContestConfig
	{
		bool enabled;
		std::string comment;
		std::string imagePath;
		FitMode fitMode;
	};

	struct WiiFitConfig
	{
		bool enabled;
		std::string profile;
		std::string status;
		std::string imagePath;
		FitMode fitMode;
	};

	struct MiiChannelConfig
	{
		bool enabled;
		std::string imagePath;
		FitMode fitMode;
	};

	struct Config
	{
		NewsConfig news;
		ForecastConfig forecast;
		NintendoConfig nintendo;
		PhotoConfig photo;
		EverybodyVotesConfig everybodyVotes;
		TodayTomorrowConfig todayTomorrow;
		MiiContestConfig miiContest;
		WiiFitConfig wiiFit;
		MiiChannelConfig miiChannel;
	};

	Config &Get();
	// Loaded BRLYT layouts retain textbox pointers and pane state.  This revision
	// lets every live Banner notice a settings/network update and reapply the
	// generated News/Forecast state without unloading the channel grid.
	u32 Revision();
	void MarkChanged();
	std::string ForecastUnitLabel();
	void ResetDefaults();
	const char *EverybodyVotesJokeText( int message );
	// Select the Up Dog preset and restore its exact three canonical messages.
	// Settings can call this without duplicating renderer literals.
	void ResetEverybodyVotesJoke();
	// Use the same condition-to-art mapping for keyboard edits and downloaded
	// weather so the label and layered icon cannot silently disagree.
	u16 ForecastWeatherCodeFromCondition( const std::string &condition );

	// Read/write a <channelPreviews> child below the application's <settings>
	// element.  Missing or malformed fields retain safe defaults.
	bool LoadXml( TiXmlElement *settingsElement );
	void SaveXml( TiXmlElement *settingsElement );
}

#endif // CHANNEL_PREVIEW_CONFIG_H
