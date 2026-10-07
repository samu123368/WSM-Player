/*
Copyright (c) 2012 - Dimok and giantpune

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <gctypes.h>
#include <gccore.h>

#include "gecko.h"
#include "settings.h"
#include "wsmversion.h"
#include "channelpreviewconfig.h"
#include "SoundOperations/gui_sound.h"

namespace Settings
{

bool useDumpedBanners = true;
bool useNandBanners = true;
bool useHomebrewForBanners = false;
bool directHomeExit = false;
bool launchOnStart = false;
int musicVolume = 100;
bool menuMusicEnabled = true;
bool usbInputEnabled = true;
int mouseSpeed = 200;
bool clock24Hour = false;
int uiLanguage = -1;
bool freeCameraEnabled = false;
bool mountDVD = true;
std::string sdBannerPath = "sd:/banners/";
std::string resourcePath;
std::string customThemePath;
std::string applicationPath = "sd:/apps/wsmplayer/";
std::string updateManifestUrl;
bool safeModeActive = false;

#define CURRENT_SETTINGS_VERSION	10
#define SAVE_FILE_NAME "smPlayerSettings.xml"
static std::string saveFilePath = "sd:/apps/wsmplayer/";
static GuiSound *activeMusicSound = NULL;

namespace
{
	// Settings are human-editable, but they should never be large enough to
	// exhaust the Wii while TinyXML builds its in-memory tree.
	const off_t MaxSettingsXmlBytes = 256 * 1024;

	bool LoadBoundedSettingsXml( TiXmlDocument &document,
		const std::string &fileName )
	{
		struct stat fileInfo;
		if( stat( fileName.c_str(), &fileInfo ) != 0
			|| fileInfo.st_size < 0 || fileInfo.st_size > MaxSettingsXmlBytes )
		{
			return false;
		}
		return document.LoadFile();
	}
}

void RegisterMusicSound( GuiSound *sound )
{
	activeMusicSound = sound;
	ApplyMusicVolume();
}

void UnregisterMusicSound( GuiSound *sound )
{
	if( activeMusicSound == sound )
		activeMusicSound = NULL;
}

void ApplyMusicVolume()
{
	if( activeMusicSound )
		activeMusicSound->SetVolume( musicVolume );
}

void ApplySafeModeOverrides()
{
	// Keep only installed NAND channels and Nintendo's original resources.
	// These are runtime overrides: Save() is not called, so a recovery boot
	// cannot destroy the user's normal theme or preview configuration.
	safeModeActive = true;
	useDumpedBanners = false;
	useNandBanners = true;
	useHomebrewForBanners = false;
	launchOnStart = false;
	menuMusicEnabled = false;
	usbInputEnabled = false;
	mountDVD = false;
	resourcePath.clear();
	ChannelPreview::ResetDefaults();
	gprintf( "WSM safe mode: NAND channels only; launch/theme/audio/DVD disabled\n" );
}


bool GetXMLBool( TiXmlElement *node, bool defaultValue )
{
	const char *val = NULL;

	if( !node || !node->FirstChild() || !( val = node->FirstChild()->Value() ) )
	{
		return defaultValue;
	}

	if( !strcasecmp( val, "1" )
			|| !strcasecmp( val, "yes" )
			|| !strcasecmp( val, "true" ) )
	{
		return true;
	}

	if( !strcasecmp( val, "0" )
			|| !strcasecmp( val, "no" )
			|| !strcasecmp( val, "false" ) )
	{
		return false;
	}

	return defaultValue;
}

const std::string &GetXMLString( TiXmlElement *node, const std::string &defaultValue )
{
	if( !node || !node->FirstChild() )
	{
		return defaultValue;
	}

	return node->FirstChild()->ValueStr();
}

int GetXMLInt( TiXmlElement *node, int defaultValue, int minimum, int maximum )
{
	if( !node || !node->FirstChild() )
	{
		return defaultValue;
	}

	char *end = NULL;
	const char *text = node->FirstChild()->Value();
	long value = text ? strtol( text, &end, 10 ) : defaultValue;
	if( !text || end == text || *end != '\0' || value < minimum || value > maximum )
	{
		return defaultValue;
	}
	return (int)value;
}



// save and load
void Load( int argc, char *argv[] )
{
	applicationPath = "sd:/apps/wsmplayer/";
	safeModeActive = false;
	saveFilePath = applicationPath;
	if( argc > 0 && argv[ 0 ] && !strncasecmp( argv[ 0 ], "sd:/", 4 ) )
	{
		const char* test = argv[ 0 ];
		u32 len = strlen( test );
		int slashes = 0;
		int lastSlash = 0;
		for( u32 i = 0; i < len; i++ )
		{
			if( test[ i ] == '/' )
			{
				lastSlash = i;

				// ignore double slash
				if( test[ i + 1 ] != '/' )
				{
					slashes++;
				}
			}
		}

		if( slashes > 1 && lastSlash >= 0 && lastSlash + 1 < 0x100 )
		{
			char derp[ 0x100 ];
			memcpy( derp, test, lastSlash + 1 );
			derp[ lastSlash + 1 ] = '\0';
			applicationPath = derp;
		}
	}
	// Always save beside the application. HBC does not guarantee argv[0], and
	// choosing sd:/config only on those launches made settings appear to save
	// and then seemingly revert on the next boot.
	saveFilePath = applicationPath;

	// set all default values
	useDumpedBanners = true;
	useNandBanners = true;
	useHomebrewForBanners = false;
	directHomeExit = false;
	launchOnStart = false;
	musicVolume = 100;
	menuMusicEnabled = true;
	usbInputEnabled = true;
	mouseSpeed = 200;
	clock24Hour = false;
	uiLanguage = -1;
	mountDVD = true;
	sdBannerPath = "sd:/banners/";
	resourcePath.clear();
	customThemePath.clear();
	updateManifestUrl = WSM_UPDATE_MANIFEST_URL;
	ChannelPreview::ResetDefaults();

	std::string fileName( saveFilePath + SAVE_FILE_NAME );

	// try to load xml file
	TiXmlDocument primaryDoc( fileName );
	const std::string backupName = fileName + ".bak";
	const std::string legacyName = "sd:/config/" SAVE_FILE_NAME;
	TiXmlDocument backupDoc( backupName );
	TiXmlDocument legacyDoc( legacyName );
	TiXmlDocument legacyBackupDoc( legacyName + ".bak" );
	TiXmlDocument *xmlDoc = &primaryDoc;
	int version = 0;
	if( !LoadBoundedSettingsXml( primaryDoc, fileName ) )
	{
		// Recover the last complete file if power was lost between the two
		// rename operations in Save(). Load the backup in place: FAT cannot
		// reliably rename it over a corrupt destination, and the next normal
		// Save() will replace that destination atomically.
		if( LoadBoundedSettingsXml( backupDoc, backupName ) )
			xmlDoc = &backupDoc;
		else if( LoadBoundedSettingsXml( legacyDoc, legacyName ) )
			xmlDoc = &legacyDoc;
		else if( LoadBoundedSettingsXml( legacyBackupDoc, legacyName + ".bak" ) )
			xmlDoc = &legacyBackupDoc;
		else return;
	}

	TiXmlElement *appNode = xmlDoc->FirstChildElement( "settings" );
	if( !appNode )
	{
		return;
	}

	if( !appNode->Attribute( "version", &version ) || version < 1 )
	{
		return;
	}

	// read values
	useDumpedBanners = GetXMLBool( appNode->FirstChildElement( "useDumpedBanners" ), true );
	useNandBanners = GetXMLBool( appNode->FirstChildElement( "useNandBanners" ), true );
	// This is the inverse of the UI's "Hide Homebrew apps" choice.  Missing
	// settings default to hidden, while explicit legacy values remain valid.
	useHomebrewForBanners = GetXMLBool(
		appNode->FirstChildElement( "useHomebrewForBanners" ), false );
	directHomeExit = GetXMLBool( appNode->FirstChildElement( "directHomeExit" ), false );
	launchOnStart = GetXMLBool( appNode->FirstChildElement( "launchOnStart" ), false );
	musicVolume = GetXMLInt( appNode->FirstChildElement( "musicVolume" ), 100, 0, 100 );
	menuMusicEnabled = GetXMLBool(
		appNode->FirstChildElement( "menuMusicEnabled" ), true );
	usbInputEnabled = GetXMLBool(
		appNode->FirstChildElement( "usbInputEnabled" ), true );
	mouseSpeed = GetXMLInt( appNode->FirstChildElement( "mouseSpeed" ),
		200, 50, 400 );
	clock24Hour = GetXMLBool(
		appNode->FirstChildElement( "clock24Hour" ), false );
	uiLanguage = GetXMLInt( appNode->FirstChildElement( "uiLanguage" ), -1, -1, 45 );
	// Removed extra-language preferences migrate to English, not a new ID.
	if(uiLanguage > 6) uiLanguage = 1;
	mountDVD = GetXMLBool( appNode->FirstChildElement( "mountDVD" ), true );
	sdBannerPath = GetXMLString( appNode->FirstChildElement( "sdBannerPath" ), "sd:/banners/" );
	resourcePath = GetXMLString( appNode->FirstChildElement( "resourcePath" ), std::string() );
	customThemePath = GetXMLString(
		appNode->FirstChildElement( "customThemePath" ), resourcePath );
	ChannelPreview::LoadXml( appNode );
	updateManifestUrl = GetXMLString(appNode->FirstChildElement("updateManifestUrl"),WSM_UPDATE_MANIFEST_URL);
	// Migrate the previously unconfigured updater without overriding a custom
	// server or SD source selected by the user.
	if(updateManifestUrl.empty() || updateManifestUrl.size()>512)
		updateManifestUrl = WSM_UPDATE_MANIFEST_URL;

}

static void AddBoolSetting( TiXmlElement *parent, const char *name, bool value )
{
	TiXmlElement *element = new TiXmlElement( name );
	element->LinkEndChild( new TiXmlText( value ? "true" : "false" ) );
	parent->LinkEndChild( element );
}

static void AddStringSetting( TiXmlElement *parent, const char *name,
	const std::string &value )
{
	TiXmlElement *element = new TiXmlElement( name );
	element->LinkEndChild( new TiXmlText( value.c_str() ) );
	parent->LinkEndChild( element );
}

static void AddIntSetting( TiXmlElement *parent, const char *name, int value )
{
	char text[ 16 ];
	snprintf( text, sizeof( text ), "%d", value );
	TiXmlElement *element = new TiXmlElement( name );
	element->LinkEndChild( new TiXmlText( text ) );
	parent->LinkEndChild( element );
}

bool Save()
{
	if( musicVolume < 0 ) musicVolume = 0;
	if( musicVolume > 100 ) musicVolume = 100;
	if( mouseSpeed < 50 ) mouseSpeed = 50;
	if( mouseSpeed > 400 ) mouseSpeed = 400;
	mkdir( saveFilePath.c_str(), 0777 );

	TiXmlDocument xmlDoc;
	xmlDoc.LinkEndChild( new TiXmlDeclaration( "1.0", "UTF-8", "yes" ) );
	TiXmlElement *appNode = new TiXmlElement( "settings" );
	appNode->SetAttribute( "version", CURRENT_SETTINGS_VERSION );
	xmlDoc.LinkEndChild( appNode );

	AddBoolSetting( appNode, "useDumpedBanners", useDumpedBanners );
	AddBoolSetting( appNode, "useNandBanners", useNandBanners );
	AddBoolSetting( appNode, "useHomebrewForBanners", useHomebrewForBanners );
	AddBoolSetting( appNode, "directHomeExit", directHomeExit );
	AddBoolSetting( appNode, "launchOnStart", launchOnStart );
	AddIntSetting( appNode, "musicVolume", musicVolume );
	AddBoolSetting( appNode, "menuMusicEnabled", menuMusicEnabled );
	AddBoolSetting( appNode, "usbInputEnabled", usbInputEnabled );
	AddIntSetting( appNode, "mouseSpeed", mouseSpeed );
	AddBoolSetting( appNode, "clock24Hour", clock24Hour );
	AddIntSetting( appNode, "uiLanguage", uiLanguage );
	AddBoolSetting( appNode, "mountDVD", mountDVD );
	AddStringSetting( appNode, "sdBannerPath", sdBannerPath );
	AddStringSetting( appNode, "resourcePath", resourcePath );
	AddStringSetting( appNode, "customThemePath", customThemePath );
	AddStringSetting( appNode, "updateManifestUrl", updateManifestUrl );
	ChannelPreview::SaveXml( appNode );

	const std::string finalPath = saveFilePath + SAVE_FILE_NAME;
	const std::string temporaryPath = finalPath + ".tmp";
	const std::string backupPath = finalPath + ".bak";
	remove( temporaryPath.c_str() );
	if( !xmlDoc.SaveFile( temporaryPath.c_str() ) )
	{
		return false;
	}

	// FAT does not guarantee rename-overwrite.  Keep the previous complete
	// file until the new complete file has reached its final name.
	remove( backupPath.c_str() );
	const bool hadPrevious = rename( finalPath.c_str(), backupPath.c_str() ) == 0;
	if( rename( temporaryPath.c_str(), finalPath.c_str() ) != 0 )
	{
		if( hadPrevious )
			rename( backupPath.c_str(), finalPath.c_str() );
		remove( temporaryPath.c_str() );
		return false;
	}
	remove( backupPath.c_str() );
	return true;
}

/*
  example xml file:

<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<settings version="1">
	<useDumpedBanners>true</useDumpedBanners>
	<useNandBanners>true</useNandBanners>
	<useHomebrewForBanners>true</useHomebrewForBanners>
	<mountDVD>true</mountDVD>
	<sdBannerPath>sd:/banners/</sdBannerPath>
	<resourcePath>sd:/darkwii_orange_No-Spin_4.1U.csm</resourcePath>
</settings>





  */

}
