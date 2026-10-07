#include "channelpreviewconfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "tinyxml/tinyxml.h"

namespace
{
	ChannelPreview::Config config;
	bool configReady = false;
	volatile u32 configRevision = 1;
	const char *const everybodyVotesJokeText[ 3 ] = {
		"do you know what's up dog?",
		"what's up dog?",
		"not much what about you?"
	};

	bool ParseBool( const char *text, bool fallback )
	{
		if( !text )
			return fallback;
		if( !strcasecmp( text, "1" ) || !strcasecmp( text, "yes" )
			|| !strcasecmp( text, "true" ) || !strcasecmp( text, "on" ) )
			return true;
		if( !strcasecmp( text, "0" ) || !strcasecmp( text, "no" )
			|| !strcasecmp( text, "false" ) || !strcasecmp( text, "off" ) )
			return false;
		return fallback;
	}

	bool ReadBoundedText( TiXmlElement *parent, const char *name,
		std::string &destination, size_t capacity )
	{
		TiXmlElement *element = parent ? parent->FirstChildElement( name ) : NULL;
		if( !element )
			return false;
		const char *text = element->GetText();
		if( !text )
		{
			destination.clear();
			return true;
		}
		const size_t length = strlen( text );
		if( length >= capacity )
			return false;
		destination.assign( text, length );
		return true;
	}

	void AddText( TiXmlElement *parent, const char *name,
		const std::string &value )
	{
		TiXmlElement *element = new TiXmlElement( name );
		element->LinkEndChild( new TiXmlText( value.c_str() ) );
		parent->LinkEndChild( element );
	}

	std::string Bounded( const std::string &value, size_t capacity )
	{
		if( value.size() < capacity )
			return value;
		size_t length = capacity - 1;
		while( length > 0
			&& ( (unsigned char)value[ length ] & 0xc0 ) == 0x80 )
			--length;
		return value.substr( 0, length );
	}

	TiXmlElement *AddSection( TiXmlElement *parent, const char *name,
		bool enabled )
	{
		TiXmlElement *section = new TiXmlElement( name );
		section->SetAttribute( "enabled", enabled ? "true" : "false" );
		parent->LinkEndChild( section );
		return section;
	}
}

ChannelPreview::Config &ChannelPreview::Get()
{
	if( !configReady )
		ResetDefaults();
	return config;
}

u32 ChannelPreview::Revision()
{
	return configRevision;
}

std::string ChannelPreview::ForecastUnitLabel()
{
	const ForecastConfig &forecast = Get().forecast;
	return std::string("\xC2\xB0") + (forecast.customUnit.empty()
		? (forecast.unit == Fahrenheit ? "F" : "C") : forecast.customUnit);
}

void ChannelPreview::MarkChanged()
{
	__sync_add_and_fetch( &configRevision, 1 );
}

const u16 ChannelPreview::ForecastWeatherChoices[ ForecastWeatherChoiceCount ] = {
	ForecastWeatherSunny,
	ForecastWeatherPartlySunny,
	ForecastWeatherPartlyCloudy,
	ForecastWeatherMostlyCloudy,
	ForecastWeatherCloudy,
	ForecastWeatherFog,
	ForecastWeatherShowers,
	ForecastWeatherPartlySunnyRain,
	ForecastWeatherMostlyCloudyShowers,
	ForecastWeatherRain,
	ForecastWeatherPartlySunnyThunder,
	ForecastWeatherMostlyCloudyThunder,
	ForecastWeatherThunder,
	ForecastWeatherPartlySunnyFlurries,
	ForecastWeatherSnow,
	ForecastWeatherRainAndSnow,
	ForecastWeatherSleet
};

const char *ChannelPreview::EverybodyVotesJokeText( int message )
{
	return message >= 0 && message < 3 ? everybodyVotesJokeText[ message ] : "";
}

void ChannelPreview::ResetEverybodyVotesJoke()
{
	if( !configReady )
	{
		ResetDefaults();
		return;
	}
	config.everybodyVotes.style = EverybodyVotesJoke;
	config.everybodyVotes.firstBlue = EverybodyVotesJokeText( 0 );
	config.everybodyVotes.greenQuestion = EverybodyVotesJokeText( 1 );
	config.everybodyVotes.finalBlue = EverybodyVotesJokeText( 2 );
}


void ChannelPreview::ResetDefaults()
{
	configReady = true;
	config.news.enabled = false;
	config.news.japaneseGlobe = false;
	config.news.count = 3;
	config.news.articles[ 0 ] = "Local Mii elected mayor after pressing A";
	config.news.articles[ 1 ] = "Wii pointer completes first solo orbit";
	config.news.articles[ 2 ] = "Disc Channel confirms discs are still round";
	for( int i = config.news.count; i < MaxNewsArticles; ++i )
		config.news.articles[ i ].clear();

	config.forecast.enabled = false;
	config.forecast.city = "Amogus City";
	config.forecast.temperature = "18";
	config.forecast.temperatureDifference = 0;
	config.forecast.condition = "Sunny with suspiciously confident clouds";
	config.forecast.unit = Celsius;
	config.forecast.customUnit.clear();
	config.forecast.weatherCode = ForecastWeatherSunny;
	config.forecast.timeMode = ForecastTimeAutomatic;
	config.forecast.japaneseIcons = false;
	config.forecast.japaneseWeatherIcons = false;
	config.forecast.automaticIcon = false;
	config.forecast.footer = "Local forecast";
	config.forecast.attribution = "supported by weathernews";
	config.forecast.imagePath.clear();

	config.nintendo.enabled = false;
	config.nintendo.text = "We are scamming you, muahahahahaha!";
	config.nintendo.extraText[0].clear();
	config.nintendo.extraText[1].clear();
	config.nintendo.imagePath.clear();
	config.nintendo.fitMode = Fit;

	config.photo.enabled = false;
	config.photo.imagePath.clear();
	config.photo.fitMode = Fit;

	config.todayTomorrow.enabled = false;
	config.todayTomorrow.affinity = "What is my compatibility with the others today?";
	config.todayTomorrow.cleaning = "What should I clean today?";
	config.todayTomorrow.play = "What should I do for fun today?";
	config.todayTomorrow.meal = "What should I eat today?";

	config.miiContest.enabled = false;
	config.miiContest.comment = "A Mii worth voting for!";
	config.miiContest.imagePath.clear();
	config.miiContest.fitMode = Fit;

	config.wiiFit.enabled = false;
	config.wiiFit.profile = "Guest Mii";
	config.wiiFit.status = "Weighed today";
	config.wiiFit.imagePath.clear();
	config.wiiFit.fitMode = Fit;

	config.miiChannel.enabled = false;
	config.miiChannel.imagePath.clear();
	config.miiChannel.fitMode = Fill;

	// Missing/legacy XML keeps WSM Player's current three-bubble joke.
	ResetEverybodyVotesJoke();
}

bool ChannelPreview::LoadXml( TiXmlElement *settingsElement )
{
	if( !configReady )
		ResetDefaults();
	TiXmlElement *root = settingsElement
		? settingsElement->FirstChildElement( "channelPreviews" ) : NULL;
	if( !root )
		return false;

	TiXmlElement *news = root->FirstChildElement( "news" );
	if( news )
	{
		config.news.japaneseGlobe = ParseBool( news->Attribute( "japaneseGlobe" ), false );
		config.news.enabled = ParseBool( news->Attribute( "enabled" ),
			config.news.enabled );
		int count = 0;
		for( TiXmlElement *article = news->FirstChildElement( "article" );
			article && count < MaxNewsArticles;
			article = article->NextSiblingElement( "article" ) )
		{
			const char *text = article->GetText();
			if( !text )
				text = "";
			if( strlen( text ) >= MaxNewsArticleBytes )
				continue;
			config.news.articles[ count++ ] = text;
		}
		if( count > 0 )
		{
			config.news.count = count;
			for( int i = count; i < MaxNewsArticles; ++i )
				config.news.articles[ i ].clear();
		}
	}

	TiXmlElement *forecast = root->FirstChildElement( "forecast" );
	if( forecast )
	{
		config.forecast.enabled = ParseBool( forecast->Attribute( "enabled" ),
			config.forecast.enabled );
		config.forecast.japaneseIcons = ParseBool(
			forecast->Attribute( "japaneseIcons" ),
			config.forecast.japaneseIcons );
		config.forecast.japaneseWeatherIcons = ParseBool(
			forecast->Attribute( "japaneseWeatherIcons" ), config.forecast.japaneseIcons );
		ReadBoundedText( forecast, "customUnit", config.forecast.customUnit, 17 );
		config.forecast.automaticIcon = ParseBool(
			forecast->Attribute( "automaticIcon" ), false );
		ReadBoundedText( forecast, "footer", config.forecast.footer, MaxConditionBytes );
		ReadBoundedText( forecast, "attribution", config.forecast.attribution, MaxConditionBytes );
		ReadBoundedText( forecast, "imagePath", config.forecast.imagePath, MaxPathBytes );
		ReadBoundedText( forecast, "city", config.forecast.city, MaxCityBytes );
		ReadBoundedText( forecast, "temperature", config.forecast.temperature,
			MaxTemperatureBytes );
		int difference = 0;
		if( forecast->QueryIntAttribute( "temperatureDifference", &difference ) == TIXML_SUCCESS
			&& difference >= -99 && difference <= 99 )
			config.forecast.temperatureDifference = difference;
		ReadBoundedText( forecast, "condition", config.forecast.condition,
			MaxConditionBytes );
		const char *unit = forecast->Attribute( "unit" );
		if( unit )
			config.forecast.unit = !strcasecmp( unit, "F" ) ? Fahrenheit : Celsius;
		const char *timeMode = forecast->Attribute( "timeMode" );
		if( timeMode && !strcasecmp( timeMode, "night" ) )
			config.forecast.timeMode = ForecastTimeNight;
		else if( timeMode && !strcasecmp( timeMode, "automatic" ) )
			config.forecast.timeMode = ForecastTimeAutomatic;
		else if( timeMode )
			config.forecast.timeMode = ForecastTimeDay;
		int weatherCode = config.forecast.weatherCode;
		if( forecast->QueryIntAttribute( "weatherCode", &weatherCode ) == TIXML_SUCCESS
			&& weatherCode >= 0 && weatherCode <= 0xffff )
			config.forecast.weatherCode = (u16)weatherCode;
	}

	TiXmlElement *nintendo = root->FirstChildElement( "nintendo" );
	if( nintendo )
	{
		config.nintendo.enabled = ParseBool( nintendo->Attribute( "enabled" ),
			config.nintendo.enabled );
		ReadBoundedText( nintendo, "text", config.nintendo.text,
			MaxNintendoTextBytes );
		ReadBoundedText( nintendo, "text2", config.nintendo.extraText[0], MaxNintendoTextBytes );
		ReadBoundedText( nintendo, "text3", config.nintendo.extraText[1], MaxNintendoTextBytes );
		ReadBoundedText( nintendo, "imagePath", config.nintendo.imagePath,
			MaxPathBytes );
		const char *fit = nintendo->Attribute( "fitMode" );
		if( fit )
			config.nintendo.fitMode = !strcasecmp( fit, "fill" ) ? Fill : Fit;
	}

	TiXmlElement *photo = root->FirstChildElement( "photo" );
	if( photo )
	{
		config.photo.enabled = ParseBool( photo->Attribute( "enabled" ),
			config.photo.enabled );
		ReadBoundedText( photo, "imagePath", config.photo.imagePath, MaxPathBytes );
		const char *fit = photo->Attribute( "fitMode" );
		if( fit )
			config.photo.fitMode = !strcasecmp( fit, "fill" ) ? Fill : Fit;
	}

	TiXmlElement *everybodyVotes = root->FirstChildElement( "everybodyVotes" );
	if( everybodyVotes )
	{
		const char *style = everybodyVotes->Attribute( "style" );
		if( style && !strcasecmp( style, "original" ) )
			config.everybodyVotes.style = EverybodyVotesOriginal;
		else if( style && !strcasecmp( style, "og-sequence" ) )
			config.everybodyVotes.style = EverybodyVotesOgSequence;
		else if( style && !strcasecmp( style, "custom" ) )
			config.everybodyVotes.style = EverybodyVotesCustom;
		else
			config.everybodyVotes.style = EverybodyVotesJoke;
		// Missing and unknown values are bounded back to the current Joke style.
		// Missing text children deliberately retain the exact Up Dog defaults so
		// version-5 settings files migrate without changing their conversation.
		ReadBoundedText( everybodyVotes, "firstBlue",
			config.everybodyVotes.firstBlue, MaxEverybodyVotesTextBytes );
		ReadBoundedText( everybodyVotes, "greenQuestion",
			config.everybodyVotes.greenQuestion, MaxEverybodyVotesTextBytes );
		ReadBoundedText( everybodyVotes, "finalBlue",
			config.everybodyVotes.finalBlue, MaxEverybodyVotesTextBytes );
	}

	// Retired customization sections are deliberately ignored when migrating
	// older settings files. The channels retain their original presentations.
	config.todayTomorrow.enabled = false;
	config.miiContest.enabled = false;
	config.wiiFit.enabled = false;
	config.miiChannel.enabled = false;

	return true;
}

void ChannelPreview::SaveXml( TiXmlElement *settingsElement )
{
	if( !settingsElement )
		return;
	if( !configReady )
		ResetDefaults();
	TiXmlElement *root = new TiXmlElement( "channelPreviews" );
	settingsElement->LinkEndChild( root );

	TiXmlElement *news = AddSection( root, "news", config.news.enabled );
	news->SetAttribute( "japaneseGlobe", config.news.japaneseGlobe ? "true" : "false" );
	for( int i = 0; i < config.news.count && i < MaxNewsArticles; ++i )
		AddText( news, "article", Bounded( config.news.articles[ i ],
			MaxNewsArticleBytes ) );

	TiXmlElement *forecast = AddSection( root, "forecast", config.forecast.enabled );
	forecast->SetAttribute( "unit", config.forecast.unit == Fahrenheit ? "F" : "C" );
	AddText( forecast, "customUnit", Bounded( config.forecast.customUnit, 17 ) );
	forecast->SetAttribute( "japaneseWeatherIcons", config.forecast.japaneseWeatherIcons ? "true" : "false" );
	forecast->SetAttribute( "weatherCode", (int)config.forecast.weatherCode );
	forecast->SetAttribute( "timeMode",
		config.forecast.timeMode == ForecastTimeNight ? "night"
			: config.forecast.timeMode == ForecastTimeAutomatic
				? "automatic" : "day" );
	forecast->SetAttribute( "japaneseIcons",
		config.forecast.japaneseIcons ? "true" : "false" );
	forecast->SetAttribute( "automaticIcon", config.forecast.automaticIcon ? "true" : "false" );
	AddText( forecast, "footer", Bounded( config.forecast.footer, MaxConditionBytes ) );
	AddText( forecast, "attribution", Bounded( config.forecast.attribution, MaxConditionBytes ) );
	AddText( forecast, "imagePath", Bounded( config.forecast.imagePath, MaxPathBytes ) );
	AddText( forecast, "city", Bounded( config.forecast.city, MaxCityBytes ) );
	AddText( forecast, "temperature", Bounded( config.forecast.temperature,
		MaxTemperatureBytes ) );
	forecast->SetAttribute( "temperatureDifference", config.forecast.temperatureDifference );
	AddText( forecast, "condition", Bounded( config.forecast.condition,
		MaxConditionBytes ) );

	TiXmlElement *nintendo = AddSection( root, "nintendo", config.nintendo.enabled );
	nintendo->SetAttribute( "fitMode", config.nintendo.fitMode == Fill ? "fill" : "fit" );
	AddText( nintendo, "text", Bounded( config.nintendo.text,
		MaxNintendoTextBytes ) );
	AddText( nintendo, "text2", Bounded( config.nintendo.extraText[0], MaxNintendoTextBytes ) );
	AddText( nintendo, "text3", Bounded( config.nintendo.extraText[1], MaxNintendoTextBytes ) );
	AddText( nintendo, "imagePath", Bounded( config.nintendo.imagePath,
		MaxPathBytes ) );

	TiXmlElement *photo = AddSection( root, "photo", config.photo.enabled );
	photo->SetAttribute( "fitMode", config.photo.fitMode == Fill ? "fill" : "fit" );
	AddText( photo, "imagePath", Bounded( config.photo.imagePath,
		MaxPathBytes ) );

	TiXmlElement *everybodyVotes = new TiXmlElement( "everybodyVotes" );
	const char *everybodyVotesStyle = config.everybodyVotes.style
		== EverybodyVotesOriginal ? "original"
		: config.everybodyVotes.style == EverybodyVotesOgSequence ? "og-sequence"
		: config.everybodyVotes.style == EverybodyVotesCustom ? "custom" : "joke";
	everybodyVotes->SetAttribute( "style", everybodyVotesStyle );
	AddText( everybodyVotes, "firstBlue", Bounded(
		config.everybodyVotes.firstBlue, MaxEverybodyVotesTextBytes ) );
	AddText( everybodyVotes, "greenQuestion", Bounded(
		config.everybodyVotes.greenQuestion, MaxEverybodyVotesTextBytes ) );
	AddText( everybodyVotes, "finalBlue", Bounded(
		config.everybodyVotes.finalBlue, MaxEverybodyVotesTextBytes ) );
	root->LinkEndChild( everybodyVotes );


}
