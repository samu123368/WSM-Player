/*
Copyright (c) 2010 - Wii Banner Player Project
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
#include <stdarg.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <ctime>
#include "SystemMenu/SystemFont.h"
#include "SystemMenu/localization.h"
#include "SystemMenu/miiprofiles.h"
#include "Banner.h"
#include "channelpreviewconfig.h"
#include "Picture.h"
#include "Textbox.h"
#include "utils/nandtitle.h"
#include "utils/localuiassets.h"
#include "utils/lz77.h"
#include "utils/sc.h"
#include "utils/tools.h"

extern const u8 fake_news_bin[];
extern const u32 fake_news_bin_size;
extern const u8 fake_forecast_bin[];
extern const u32 fake_forecast_bin_size;
extern const u8 fake_short_forecast_bin[];
extern const u32 fake_short_forecast_bin_size;

namespace
{
	enum
	{
		TitleNewsPrefix = 0x48414700,     // HAG?
		TitleForecastPrefix = 0x48414600, // HAF?
		TitleTodayTomorrowPrefix = 0x48415600, // HAV?
		TitleMiiContestPrefix = 0x48415000,    // HAP?
		TitleMiiChannelPrefix = 0x48414300,    // HAC?
		TitleWiiFitPrefix = 0x52464e00,        // RFN?
		TitleWiiFitPlusPrefix = 0x52465000     // RFP?
	};

	bool IsTitlePrefix( u64 titleId, u32 prefix )
	{
		return ( (u32)titleId & 0xffffff00 ) == prefix;
	}

	bool IsNewsTitle( u64 titleId )
	{
		return ( (u32)titleId & 0xffffff00 ) == TitleNewsPrefix;
	}

	bool IsForecastTitle( u64 titleId )
	{
		return ( (u32)titleId & 0xffffff00 ) == TitleForecastPrefix;
	}

	bool IsTodayTomorrowTitle( u64 titleId )
	{
		return IsTitlePrefix( titleId, TitleTodayTomorrowPrefix );
	}

	bool IsMiiContestTitle( u64 titleId )
	{
		return IsTitlePrefix( titleId, TitleMiiContestPrefix );
	}

	bool IsMiiChannelTitle( u64 titleId )
	{
		return IsTitlePrefix( titleId, TitleMiiChannelPrefix );
	}

	bool IsWiiFitTitle( u64 titleId )
	{
		return IsTitlePrefix( titleId, TitleWiiFitPrefix )
			|| IsTitlePrefix( titleId, TitleWiiFitPlusPrefix );
	}

	bool IsForecastLayout( Layout *layout )
	{
		return layout && layout->FindPane( "code" )
			&& layout->FindPane( "weather" )
			&& ( layout->FindPane( "city" ) || layout->FindPane( "day0" ) );
	}

	bool IsNewsLayout( Layout *layout )
	{
		return layout && layout->FindPane( "telop0" )
			&& ( layout->FindPane( "news" ) || layout->FindPane( "send_id" ) );
	}

	bool IsTodayTomorrowLayout( Layout *layout )
	{
		return layout && layout->FindPane( "TextBox_affinity" )
			&& layout->FindPane( "TextBox_beau" )
			&& layout->FindPane( "TextBox_meel" )
			&& layout->FindPane( "TextBox_play" );
	}

	bool IsMiiContestLayout( Layout *layout )
	{
		return layout && layout->FindPane( "N_commentText_00" )
			&& layout->FindPane( "N_photo_00" )
			&& layout->FindPane( "N_theme_00" );
	}

	bool IsWiiFitBannerLayout( Layout *layout )
	{
		return layout && layout->FindPane( "N_MiiAll_00" )
			&& layout->FindPane( "P_Mii_00" )
			&& layout->FindPane( "P_Mii_07" );
	}

	bool IsWiiFitIconLayout( Layout *layout )
	{
		return layout && layout->FindPane( "N_MiiMessages_00" )
			&& layout->FindPane( "T_message_00" )
			&& layout->FindPane( "T_message_07" );
	}

	u16 ReadBE16( const u8 *data )
	{
		return ( (u16)data[ 0 ] << 8 ) | data[ 1 ];
	}

	u32 ReadBE32( const u8 *data )
	{
		return ( (u32)data[ 0 ] << 24 ) | ( (u32)data[ 1 ] << 16 )
			| ( (u32)data[ 2 ] << 8 ) | data[ 3 ];
	}

	const u8 *FindImetHeader( const u8 *data, u32 length )
	{
		if( !data || length < sizeof( IMET ) )
			return NULL;

		// Prefer the two Nintendo offsets, then accept IMET v2+ headers made by
		// tools which prepend or extend metadata.  The IMET body (including its
		// localized names) remains the same across these versions.
		static const u32 commonOffsets[] = { 0x40, 0x80, 0 };
		for( u32 i = 0; i < sizeof( commonOffsets ) / sizeof( commonOffsets[ 0 ] ); ++i )
		{
			const u32 offset = commonOffsets[ i ];
			if( offset <= length && sizeof( IMET ) <= length - offset
				&& ReadBE32( data + offset ) == IMET_SIGNATURE )
				return data + offset;
		}

		const u32 searchEnd = length < 0x10000 ? length : 0x10000;
		for( u32 offset = 4; offset + sizeof( IMET ) <= searchEnd; offset += 4 )
			if( ReadBE32( data + offset ) == IMET_SIGNATURE )
				return data + offset;
		return NULL;
	}

	bool DecodeEmbeddedForecastArchive( const u8 *source, u32 sourceSize,
		u8 **decoded, u32 *decodedSize )
	{
		if( !decoded || !decodedSize )
			return false;
		*decoded = NULL;
		*decodedSize = 0;
		if( !source || sourceSize < 4 )
			return false;

		// Optional user-owned HAFJ files can carry an IMD5/LZ77 wrapper. NAND
		// acquisition already strips it; mirror that operation for local SD files.
		if( sourceSize >= 0x24 && ReadBE32( source ) == 0x494d4435 ) // IMD5
		{
			source += 0x20;
			sourceSize -= 0x20;
		}
		if( sourceSize >= 8 && ReadBE32( source ) == 0x4c5a3737 ) // LZ77
		{
			source += 4;
			sourceSize -= 4;
		}
		if( sourceSize < 4 || !isLZ77compressed( source ) )
			return false;

		u32 outputSize = sourceSize;
		u8 *output = NULL;
		if( decompressLZ77content( source, sourceSize, &output, &outputSize )
			|| !output || outputSize < 0x40
			|| ReadBE32( output ) != 0x55aa382d )
		{
			free( output );
			return false;
		}
		DCFlushRange( output, outputSize );
		*decoded = output;
		*decodedSize = outputSize;
		return true;
	}

	bool LoadLocalForecastArchive(bool icon, u8 **decoded, u32 *decodedSize)
	{
		u32 size=0;
		u8 *bytes=LocalUiAssets::ForecastArchive(icon,&size);
		if(!bytes) return false;
		if(size>=32 && ReadBE32(bytes)==0x55aa382d && U8Archive(bytes,size).IsValid())
		{ *decoded=bytes; *decodedSize=size; return true; }
		const bool ok=DecodeEmbeddedForecastArchive(bytes,size,decoded,decodedSize);
		free(bytes); return ok;
	}

	bool InRange( u32 offset, u32 length, u32 size )
	{
		return offset <= size && length <= size - offset;
	}

	bool CopyUtf16BE( const u8 *data, u32 size, u32 offset, u32 byteLength,
					 char16 *out, u32 capacity )
	{
		if( !data || !out || capacity < 2 || offset >= size )
			return false;
		if( byteLength && !InRange( offset, byteLength, size ) )
			return false;

		const u32 end = byteLength ? offset + byteLength : size;
		u32 count = 0;
		for( u32 pos = offset; pos + 1 < end && count + 1 < capacity; pos += 2 )
		{
			const u16 ch = ReadBE16( data + pos );
			if( !ch )
				break;
			out[ count++ ] = (char16)ch;
		}
		out[ count ] = 0;
		return count > 0;
	}

	void WrapAtSpace( char16 *text, u32 maxColumn )
	{
		if( !text || !maxColumn )
			return;
		u32 length = 0;
		int lastSpace = -1;
		while( text[ length ] )
		{
			if( text[ length ] == ' ' )
				lastSpace = (int)length;
			if( length >= maxColumn && lastSpace >= 0 )
			{
				text[ lastSpace ] = '\n';
				return;
			}
			++length;
		}
	}

	void Utf8ToChar16( char16 *out, u32 capacity, const char *input );

	u32 Utf16Length( const char16 *text )
	{
		if( !text )
			return 0;
		u32 length = 0;
		while( text[ length ] )
			++length;
		return length;
	}

	float NewsTickerScale( const char16 *text, bool arbitraryUserText )
	{
		// Preserve the original News icon size for short headlines and only
		// reduce long translations enough to keep the following bullet clear.
		const float normalScale = 0.85f;
		const float minimumScale = arbitraryUserText ? 0.16f : 0.68f;
		const u32 comfortableCharacters = 31;
		const u32 length = Utf16Length( text );
		if( length <= comfortableCharacters )
			return normalScale;

		const float scale = normalScale * (float)comfortableCharacters / (float)length;
		return scale < minimumScale ? minimumScale : scale;
	}

	struct NewsTranslation
	{
		const char *title;
		const char *headline1;
		const char *headline2;
		const char *headline3;
	};

	const NewsTranslation &GetNewsTranslation()
	{
		// CONF language order: JPN, ENG, GER, FRA, SPA, ITA, NED,
		// simplified Chinese, traditional Chinese, KOR.
		static const NewsTranslation translations[ 10 ] = {
			{ "ニュースチャンネル", "地元のMii、Aボタンを押して市長に当選", "Wiiポインター、初の単独周回飛行に成功", "ディスクチャンネル、ディスクは今も丸いと確認" },
			{ "News Channel", "Local Mii elected mayor after pressing A", "Wii pointer completes first solo orbit", "Disc Channel confirms discs are still round" },
			{ "Nachrichtenkanal", "Lokaler Mii wird nach Druck auf A zum Bürgermeister gewählt", "Wii-Zeiger vollendet ersten Alleinflug im Orbit", "Disc-Kanal bestätigt: Discs sind weiterhin rund" },
			{ "Chaîne infos", "Un Mii local élu maire après avoir appuyé sur A", "Le pointeur Wii réussit sa première orbite en solo", "La chaîne disques confirme que les disques sont toujours ronds" },
			{ "Canal Noticias", "Un Mii local es elegido alcalde tras pulsar A", "El puntero de Wii completa su primera órbita en solitario", "El Canal Disco confirma que los discos siguen siendo redondos" },
			{ "Canale Notizie", "Mii locale eletto sindaco dopo aver premuto A", "Il puntatore Wii completa la sua prima orbita in solitaria", "Il Canale Disco conferma che i dischi sono ancora rotondi" },
			{ "Nieuwskanaal", "Lokale Mii wordt burgemeester na een druk op A", "Wii-aanwijzer voltooit eerste solovlucht rond de aarde", "Disckanaal bevestigt dat discs nog steeds rond zijn" },
			{ "新闻频道", "本地Mii按下A键后当选市长", "Wii指针首次独自完成环绕飞行", "光盘频道确认光盘仍然是圆的" },
			{ "新聞頻道", "本地Mii按下A鍵後當選市長", "Wii指標首次獨自完成環繞飛行", "光碟頻道確認光碟仍然是圓的" },
			{ "뉴스 채널", "동네 Mii, A 버튼을 누른 뒤 시장에 당선", "Wii 포인터, 첫 단독 궤도 비행 완료", "디스크 채널, 디스크는 여전히 둥글다고 확인" }
		};

		int language = Localization::NativeLanguage();
		if( language < 0 || language > 9 )
			language = CONF_LANG_ENGLISH;
		return translations[ language ];
	}

	u32 LoadNewsGeneratedText( char16 text[ 13 ][ 256 ] )
	{
		for( int i = 0; i < 13; ++i )
			text[ i ][ 0 ] = 0;

		const NewsTranslation &translation = GetNewsTranslation();
		Utf8ToChar16( text[ 0 ], 256, Localization::GetUtf8( "News Channel" ) );

		const ChannelPreview::NewsConfig &custom = ChannelPreview::Get().news;
		if( custom.enabled )
		{
			u32 loaded = 0;
			const int requested = custom.count < 0 ? 0
				: custom.count > ChannelPreview::MaxNewsArticles
					? ChannelPreview::MaxNewsArticles : custom.count;
			for( int i = 0; i < requested && loaded < 12; ++i )
			{
				// Compact blank editor rows instead of leaving an empty belt in
				// the native News carrier. An all-blank custom list intentionally
				// produces an empty ticker rather than reviving fixture headlines.
				if( custom.articles[ i ].empty() )
					continue;
				Utf8ToChar16( text[ loaded + 1 ], 160,
					custom.articles[ i ].c_str() );
				if( text[ loaded + 1 ][ 0 ] )
					++loaded;
			}
			return loaded;
		}

		// fake_news.bin contains English-only strings. Keep those for English so
		// regenerated fixture data still appears automatically; every other Wii
		// language uses the translated local headlines below.
		u32 loaded = 0;
		if( Localization::NativeLanguage() == CONF_LANG_ENGLISH
			&& fake_news_bin_size >= 104 && ReadBE32( fake_news_bin ) == 512 )
		{
			const u32 declared = ReadBE32( fake_news_bin + 4 );
			const u32 count = ReadBE32( fake_news_bin + 96 );
			const u32 table = ReadBE32( fake_news_bin + 100 );
			if( declared <= fake_news_bin_size && InRange( table, count * 8, declared ) )
			{
				for( u32 i = 0; i < count && loaded < 12; ++i )
				{
					const u32 length = ReadBE32( fake_news_bin + table + i * 8 );
					const u32 offset = ReadBE32( fake_news_bin + table + i * 8 + 4 );
					if( CopyUtf16BE( fake_news_bin, declared, offset, length,
						text[ loaded + 1 ], 160 ) )
						++loaded;
				}
			}
		}

		if( !loaded )
		{
			Utf8ToChar16( text[ 1 ], 256, translation.headline1 );
			Utf8ToChar16( text[ 2 ], 256, translation.headline2 );
			Utf8ToChar16( text[ 3 ], 256, translation.headline3 );
			loaded = 3;
		}

		return loaded;
	}

	float NewsBannerLoopEnd( u32 headlineCount )
	{
		// banner_Rso1 moves the `line` carrier left two pixels per frame.
		// Three headlines share a vertical column; each additional 680-pixel
		// column therefore needs another 340 frames. Keep the proven fixture
		// timing exactly unchanged for its original three rows.
		const u32 columns = headlineCount ? ( headlineCount + 2 ) / 3 : 1;
		return 610.0f + (float)( columns - 1 ) * 340.0f;
	}

	float NewsIconLoopEnd( u32 headlineCount )
	{
		// icon_Rso1 advances one pixel per frame. Two rows share each
		// 327-pixel column.  End one column after the final populated one so
		// one/two custom headlines do not spend 327 frames showing empty belt.
		// The original three-item/two-column fixture still ends at frame 840.
		const u32 columns = headlineCount ? ( headlineCount + 1 ) / 2 : 1;
		return 513.0f + (float)( columns - 1 ) * 327.0f;
	}

	struct ForecastTranslation
	{
		const char *title;
		const char *city;
		const char *condition;
		const char *wind;
		const char *high;
		const char *low;
		const char *rain;
		const char *calm;
	};

	const ForecastTranslation &GetForecastTranslation( bool forceJapanese = false )
	{
		// CONF language order: JPN, ENG, GER, FRA, SPA, ITA, NED,
		// simplified Chinese, traditional Chinese, KOR.
		static const ForecastTranslation translations[ 10 ] = {
			{ "天気予報", "アモガスシティ", "晴れ\n自信ありげな雲", "風", "最高", "最低", "降水確率", "無風" },
			{ "Forecast", "Amogus City", "Sunny with suspiciously confident clouds", "Wind", "High", "Low", "Rain", "CALM" },
			{ "Wettervorhersage", "Amogus-Stadt", "Sonnig mit verdächtig selbstbewussten Wolken", "Wind", "Hoch", "Tief", "Regen", "Windstill" },
			{ "Prévisions", "Ville d'Amogus", "Ensoleillé avec des nuages étrangement sûrs d'eux", "Vent", "Max", "Min", "Pluie", "Calme" },
			{ "Pronóstico", "Ciudad Amogus", "Soleado con nubes sospechosamente seguras", "Viento", "Máx", "Mín", "Lluvia", "Calma" },
			{ "Previsioni", "Città di Amogus", "Soleggiato con nuvole sospettosamente sicure", "Vento", "Max", "Min", "Pioggia", "Calma" },
			{ "Weersverwachting", "Amogus-stad", "Zonnig met verdacht zelfverzekerde wolken", "Wind", "Max", "Min", "Regen", "Windstil" },
			{ "天气预报", "阿莫古斯市", "晴朗，云层显得异常自信", "风", "最高", "最低", "降雨", "无风" },
			{ "天氣預報", "阿莫古斯市", "晴朗，雲層顯得異常自信", "風", "最高", "最低", "降雨", "無風" },
			{ "일기예보", "아모거스 시티", "맑음, 구름이 수상할 정도로 자신만만함", "바람", "최고", "최저", "비", "고요" }
		};

		// Geometry/art selection is independent of the WSM text language.
		(void)forceJapanese;
		int language = Localization::NativeLanguage();
		if( language < 0 || language > 9 )
			language = CONF_LANG_ENGLISH;
		return translations[ language ];
	}

	void Utf8ToChar16( char16 *out, u32 capacity, const char *input )
	{
		if( !out || !capacity )
			return;
		if( !input )
		{
			out[ 0 ] = 0;
			return;
		}

		u32 written = 0;
		const unsigned char *p = (const unsigned char *)input;
		while( *p && written + 1 < capacity )
		{
			u32 codepoint;
			if( *p < 0x80 )
			{
				codepoint = *p++;
			}
			else if( ( p[ 0 ] & 0xe0 ) == 0xc0 && p[ 1 ] )
			{
				codepoint = ( ( p[ 0 ] & 0x1f ) << 6 ) | ( p[ 1 ] & 0x3f );
				p += 2;
			}
			else if( ( p[ 0 ] & 0xf0 ) == 0xe0 && p[ 1 ] && p[ 2 ] )
			{
				codepoint = ( ( p[ 0 ] & 0x0f ) << 12 )
					| ( ( p[ 1 ] & 0x3f ) << 6 ) | ( p[ 2 ] & 0x3f );
				p += 3;
			}
			else if( ( p[ 0 ] & 0xf8 ) == 0xf0 && p[ 1 ] && p[ 2 ] && p[ 3 ] )
			{
				codepoint = ( ( p[ 0 ] & 0x07 ) << 18 )
					| ( ( p[ 1 ] & 0x3f ) << 12 )
					| ( ( p[ 2 ] & 0x3f ) << 6 ) | ( p[ 3 ] & 0x3f );
				p += 4;
			}
			else
			{
				codepoint = '?';
				++p;
			}

			if( codepoint <= 0xffff )
			{
				out[ written++ ] = (char16)codepoint;
			}
			else if( written + 2 < capacity )
			{
				codepoint -= 0x10000;
				out[ written++ ] = (char16)( 0xd800 | ( codepoint >> 10 ) );
				out[ written++ ] = (char16)( 0xdc00 | ( codepoint & 0x3ff ) );
			}
		}
		out[ written ] = 0;
	}

	void FormatUtf8ToChar16( char16 *out, u32 capacity, const char *format, ... )
	{
		char buffer[ 512 ];
		va_list args;
		va_start( args, format );
		vsnprintf( buffer, sizeof( buffer ), format, args );
		va_end( args );
		buffer[ sizeof( buffer ) - 1 ] = 0;
		Utf8ToChar16( out, capacity, buffer );
	}

	bool ParseNativeTemperature( const std::string &text, int &temperature )
	{
		if( text.empty() )
			return false;

		const char *begin = text.c_str();
		char *end = NULL;
		const long parsed = strtol( begin, &end, 10 );
		if( end == begin )
			return false;
		while( *end == ' ' || *end == '\t' )
			++end;
		// The stock digit atlases have room for -99..999. Anything else is
		// still valid user content, but must use the native textbox fallback.
		if( *end || parsed < -99 || parsed > 999 )
			return false;

		temperature = (int)parsed;
		return true;
	}

	struct ForecastGeneratedData
	{
		u16 dailyWeatherCode;
		u16 currentWeatherCode;
		int currentCelsius;
		int highCelsius;
		int highDifferenceCelsius;
		int lowCelsius;
		u8 precipitation[ 4 ];
		u8 windDirection;
		u8 windSpeed;
		bool custom;
		bool nativeTemperature;
		bool fahrenheit;
	};

	ForecastGeneratedData LoadForecastGeneratedText( char16 text[ 13 ][ 256 ],
		bool japaneseLayout )
	{
		for( int i = 0; i < 13; ++i )
			text[ i ][ 0 ] = 0;

		const ForecastTranslation &translation = GetForecastTranslation( japaneseLayout );
		Utf8ToChar16( text[ 0 ], 256, Localization::GetUtf8( "Forecast Channel" ) );
		Utf8ToChar16( text[ 1 ], 256, translation.city );
		Utf8ToChar16( text[ 2 ], 256, translation.condition );
		Utf8ToChar16( text[ 9 ], 256, Localization::NativeLanguage() == CONF_LANG_JAPANESE
			? "supported by ウェザーニュース" : "supported by weathernews" );

		ForecastGeneratedData result;
		result.dailyWeatherCode = 0x0464;
		result.currentWeatherCode = 0x0464;
		result.currentCelsius = 18;
		result.highCelsius = 22;
		result.highDifferenceCelsius = 0;
		result.lowCelsius = 12;
		result.precipitation[ 0 ] = 5;
		result.precipitation[ 1 ] = 10;
		result.precipitation[ 2 ] = 25;
		result.precipitation[ 3 ] = 35;
		result.windDirection = 12;
		result.windSpeed = 14;
		result.custom = false;
		result.nativeTemperature = true;
		result.fahrenheit = false;

		if( fake_forecast_bin_size >= 88 && ReadBE32( fake_forecast_bin ) == 0 )
		{
			const u32 declared = ReadBE32( fake_forecast_bin + 4 );
			if( declared <= fake_forecast_bin_size )
			{
				const u32 longCount = ReadBE32( fake_forecast_bin + 32 );
				const u32 longOffset = ReadBE32( fake_forecast_bin + 36 );
				if( longCount && InRange( longOffset, 128, declared ) )
				{
					const u8 *record = fake_forecast_bin + longOffset;
					result.dailyWeatherCode = ReadBE16( record + 16 );
					result.highCelsius = (s8)record[ 26 ];
					result.highDifferenceCelsius = (s8)record[ 27 ];
					result.lowCelsius = (s8)record[ 28 ];
					for( u32 i = 0; i < 4; ++i )
						result.precipitation[ i ] = record[ 34 + i ];
					result.windDirection = record[ 38 ];
					result.windSpeed = record[ 39 ];
				}

				// The included fake forecast data is English. Keep it for English,
				// but retain the translated location for every other Wii language.
				const u32 locationCount = ReadBE32( fake_forecast_bin + 80 );
				const u32 locationOffset = ReadBE32( fake_forecast_bin + 84 );
				if( !japaneseLayout && Localization::NativeLanguage() == CONF_LANG_ENGLISH && locationCount
					&& InRange( locationOffset, 24, declared ) )
				{
					CopyUtf16BE( fake_forecast_bin, declared,
						ReadBE32( fake_forecast_bin + locationOffset + 4 ), 0,
						text[ 1 ], 160 );
				}
			}
		}

		if( fake_short_forecast_bin_size >= 36 && ReadBE32( fake_short_forecast_bin ) == 0 )
		{
			const u32 declared = ReadBE32( fake_short_forecast_bin + 4 );
			const u32 count = ReadBE32( fake_short_forecast_bin + 28 );
			const u32 offset = ReadBE32( fake_short_forecast_bin + 32 );
			if( declared <= fake_short_forecast_bin_size && count
				&& InRange( offset, 24, declared ) )
			{
				const u8 *record = fake_short_forecast_bin + offset;
				result.currentWeatherCode = ReadBE16( record + 12 );
				result.currentCelsius = (s8)record[ 15 ];
				result.windDirection = record[ 17 ];
				result.windSpeed = record[ 18 ];
			}
		}

		// The condition table embedded in fake_forecast.bin is English-only.
		// HAFJ uses the shorter translated default instead of this long fixture;
		// its 190-pixel description pane has separate fitting below.
		if( !japaneseLayout && Localization::NativeLanguage() == CONF_LANG_ENGLISH
			&& fake_forecast_bin_size >= 88 )
		{
			const u32 declared = ReadBE32( fake_forecast_bin + 4 );
			const u32 count = ReadBE32( fake_forecast_bin + 48 );
			const u32 table = ReadBE32( fake_forecast_bin + 52 );
			if( declared <= fake_forecast_bin_size && InRange( table, count * 8, declared ) )
			{
				for( u32 i = 0; i < count; ++i )
				{
					const u8 *entry = fake_forecast_bin + table + i * 8;
					if( ReadBE16( entry ) == result.currentWeatherCode
						|| ReadBE16( entry + 2 ) == result.currentWeatherCode )
					{
						CopyUtf16BE( fake_forecast_bin, declared, ReadBE32( entry + 4 ), 0,
							text[ 2 ], 160 );
						break;
					}
				}
			}
		}
		const ChannelPreview::ForecastConfig &custom = ChannelPreview::Get().forecast;
		if( custom.enabled )
		{
			result.custom = true;
			result.fahrenheit = custom.unit == ChannelPreview::Fahrenheit;
			result.dailyWeatherCode = custom.weatherCode;
			result.currentWeatherCode = custom.weatherCode;
			result.highDifferenceCelsius = custom.temperatureDifference;
			Utf8ToChar16( text[ 1 ], 256, custom.city.c_str() );

			int parsedTemperature = 0;
			result.nativeTemperature = ParseNativeTemperature(
				custom.temperature, parsedTemperature )
				&& (custom.customUnit.empty() || !japaneseLayout);
			if( result.nativeTemperature )
			{
				result.currentCelsius = parsedTemperature;
				result.highCelsius = parsedTemperature;
				Utf8ToChar16( text[ 2 ], 256, custom.condition.c_str() );
				// A custom unit does not change how numeric digits are drawn.
				// Keep the large authored atlas and replace only its C/F cell.
				Utf8ToChar16( text[ 7 ], 256, custom.customUnit.c_str() );
			}
			else
			{
				// Values outside the stock -99..999 digit atlas keep their exact
				// spelling. Store the large temperature and condition separately;
				// the PAL layout can then place them in their two authored regions.
				std::string temperature = custom.temperature;
				temperature += " " + ChannelPreview::ForecastUnitLabel();
				Utf8ToChar16( text[ 7 ], 256, temperature.c_str() );
				Utf8ToChar16( text[ 2 ], 256, custom.condition.c_str() );

				// Retain the old combined representation only as a compatibility
				// fallback for a regional BRLYT with no spare localized title pair.
				std::string description = temperature;
				if( !custom.condition.empty() )
				{
					description += "\n";
					description += custom.condition;
				}
				Utf8ToChar16( text[ 8 ], 256, description.c_str() );
			}
		}
		else if( !japaneseLayout )
		{
			WrapAtSpace( text[ 2 ], 22 );
		}

		if( Localization::NativeLanguage() == CONF_LANG_JAPANESE )
		{
			Utf8ToChar16( text[ 3 ], 256, "ローカル予報" );
			Utf8ToChar16( text[ 6 ], 256, "きょう" );
		}
		else
		{
			Utf8ToChar16( text[ 3 ], 256, Localization::GetUtf8( "Local forecast" ) );
			Utf8ToChar16( text[ 6 ], 256, Localization::GetUtf8( "Today" ) );
		}
		if( custom.enabled )
		{
			Utf8ToChar16( text[ 3 ], 256, custom.footer.c_str() );
			Utf8ToChar16( text[ 9 ], 256, custom.attribution.c_str() );
		}
		FormatUtf8ToChar16( text[ 4 ], 256, "%s %d / %s %d %c",
			translation.high, result.highCelsius, translation.low, result.lowCelsius,
			result.fahrenheit ? 'F' : 'C' );
		FormatUtf8ToChar16( text[ 5 ], 256, "%s %u%%", translation.rain,
			result.precipitation[ 0 ] );
		return result;
	}

	bool SetGeneratedTextPane( Layout *layout, const char *name, const char16 *text )
	{
		if( !layout || !name || !text )
			return false;

		Textbox *textbox = dynamic_cast< Textbox * >( layout->FindPane( name ) );
		if( !textbox )
			return false;

		textbox->SetText( text );
		return true;
	}

	u32 LongestTextLine( const char16 *text )
	{
		u32 longest = 0;
		u32 current = 0;
		while( text && *text )
		{
			if( *text == '\n' )
			{
				if( current > longest )
					longest = current;
				current = 0;
			}
			else
			{
				++current;
			}
			++text;
		}
		return current > longest ? current : longest;
	}

	bool SetGeneratedTextPaneFitted( Layout *layout, const char *name,
		const char16 *text, u32 comfortableCharacters, float minimumScale )
	{
		if( !SetGeneratedTextPane( layout, name, text ) )
			return false;
		Pane *pane = layout->FindPane( name );
		const u32 length = LongestTextLine( text );
		float fitted = 1.0f;
		if( comfortableCharacters && length > comfortableCharacters )
			fitted = (float)comfortableCharacters / (float)length;
		if( fitted < minimumScale )
			fitted = minimumScale;
		Vec2f scale = { fitted, fitted };
		pane->SetScale( scale );
		return true;
	}

	void ApplyWiiFitIconMessage( Layout *layout, const char16 *message )
	{
		if( !IsWiiFitIconLayout( layout ) || !message ) return;
		Pane *carrier = layout->FindPane( "N_MiiMessages_00" );
		carrier->SetVisible( true );
		carrier->SetHide( false );

		// The extracted RFNP icon has eight overlapping save-data slots. CHANS
		// normally spreads populated entries across the 9800-frame carrier. WSM
		// exposes one selected NAND Mii, so retain slot 00 and hide the seven empty
		// placeholders instead of drawing eight identical faces and `iiii...` rows.
		for( int slot = 0; slot < 8; ++slot )
		{
			char faceName[ 20 ];
			char windowName[ 20 ];
			char textName[ 20 ];
			snprintf( faceName, sizeof( faceName ), "P_MiiFace_%02d", slot );
			snprintf( windowName, sizeof( windowName ), "W_message_%02d", slot );
			snprintf( textName, sizeof( textName ), "T_message_%02d", slot );
			Pane *face = layout->FindPane( faceName );
			Pane *window = layout->FindPane( windowName );
			Pane *text = layout->FindPane( textName );
			const bool selected = slot == 0;
			if( face ) { face->SetVisible( selected ); face->SetHide( !selected ); }
			if( window ) { window->SetVisible( selected ); window->SetHide( !selected ); }
			if( text ) { text->SetVisible( selected ); text->SetHide( !selected ); }
			if( selected )
			{
				// Nintendo authors every save-data speech window at 1110 pixels so
				// CHANS can scroll long messages from as many as eight Miis. WSM shows
				// one short status, so size the real nine-slice window to its content
				// instead of dragging a giant empty tail across the channel icon.
				const u32 characters = std::max( 1u, LongestTextLine( message ) );
				const float bubbleWidth = std::max( 132.0f, std::min( 300.0f,
					72.0f + (float)characters * 10.5f ) );
				if( window ) window->SetSize( bubbleWidth, 44.0f );
				if( text ) text->SetSize( std::max( 72.0f, bubbleWidth - 60.0f ),
					24.0f );
				SetGeneratedTextPaneFitted( layout, textName, message, 20, 0.70f );
			}
		}
	}

	bool SetWorldwideForecastTextPane( Layout *layout, const char *name,
		const char16 *text, u32 comfortableCharacters, float minimumScale )
	{
		if( !layout || !name || !text )
			return false;
		Textbox *textbox = dynamic_cast< Textbox * >( layout->FindPane( name ) );
		if( !textbox )
			return false;

		// The PAL Forecast banner authors city/telop at 32 x 38.  Keep that
		// original size for normal strings and shrink only the font for genuinely
		// long custom text. Scaling the whole pane made ordinary Forecast labels
		// larger/differently anchored than the stock channel.
		const u32 length = LongestTextLine( text );
		float fitted = 1.0f;
		if( comfortableCharacters && length > comfortableCharacters )
			fitted = (float)comfortableCharacters / (float)length;
		if( fitted < minimumScale )
			fitted = minimumScale;

		textbox->SetText( text );
		textbox->SetFontSize( 32.0f * fitted, 38.0f * fitted );
		const Vec2f unitScale = { 1.0f, 1.0f };
		textbox->SetScale( unitScale );
		return true;
	}

	void SetNamedPaneVisible( Layout *layout, const char *name, bool visible )
	{
		if( !layout || !name )
			return;
		Pane *pane = layout->FindPane( name );
		if( !pane )
			return;
		pane->SetVisible( visible );
		pane->SetHide( !visible );
	}


	void LocalizeJapaneseForecastText( Layout *layout )
	{
		if( Localization::CurrentLanguage() == CONF_LANG_JAPANESE ) return;
		const char *titlePanes[] = { "titleJPN0", "titleJPN1", "titleJPN_sdw0",
			"titleJPN_sdw1", "titleJPN0sdw", "titleJPN1sdw" };
		for( u32 i = 0; i < sizeof(titlePanes) / sizeof(titlePanes[0]); ++i )
		{
			SetGeneratedTextPaneFitted( layout, titlePanes[i],
				Localization::GetText("Forecast Channel"), 18, 0.4f );
			Textbox *text = layout->FindTextbox( titlePanes[i] );
			// Icon and banner both declare a 608x456 layout, but their title
			// panes have different authored sizes. Preserve each pane's metrics.
			if( text ) text->SetUniformTextFit( true );
		}
		const char *panes[] = { "kionT0", "kionT1", "kion_high0", "kion_high1",
			"kion_low", "kion_zen", "rainT0", "rainT1", "rain_am6-12",
			"rain_am12-18", "rain_pm12-18", "rain_pm18-24" };
		const char *labels[] = { "Temperature", "Temperature", "High", "High",
			"Low", "Change", "Rain", "Rain", "6-12", "12-18", "12-18", "18-24" };
		for( u32 i = 0; i < sizeof(panes) / sizeof(panes[0]); ++i )
		{
			SetGeneratedTextPaneFitted( layout, panes[i],
				Localization::GetText(labels[i]), 10, 0.4f );
			Textbox *text = layout->FindTextbox( panes[i] );
			if( text ) text->SetUniformTextFit( true );
		}
		const char *fields[] = { "city", "city_sdw", "telop", "telop_sdw", "timeJP" };
		for( u32 i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i )
		{
			Textbox *text = layout->FindTextbox( fields[i] );
			if( text ) text->SetUniformTextFit( true );
		}
	}

	void CorrectInternetTitleSpacing( const PaneList &panes )
	{
		for( PaneList::const_iterator it = panes.begin(); it != panes.end(); ++it )
		{
			if( Textbox *text = dynamic_cast<Textbox *>(*it) )
				if( !strncmp(text->getName(), "T_title", 7) )
					text->SetAccurateGlyphAdvance(true);
			CorrectInternetTitleSpacing((*it)->panes);
		}
	}

	bool IsInternetTitleLayout( Layout *layout )
	{
		Textbox *title = layout->FindTextbox("T_title_ENG");
		if( !title ) title = layout->FindTextbox("T_title_00_ENG");
		if( !title || !title->GetText() ) return false;
		const char *expected = "InternetChannel";
		for( const char16 *ch = title->GetText(); *ch; ++ch )
		{
			if( *ch == ' ' || *ch == '\n' || *ch == '\r' ) continue;
			if( !*expected || *ch != (char16)*expected++ ) return false;
		}
		return !*expected;
	}

	void LocalizeAuthoredChannelPanes( const PaneList &panes )
	{
		for( PaneList::const_iterator it = panes.begin(); it != panes.end(); ++it )
		{
			Textbox *text = dynamic_cast<Textbox *>( *it );
			if( text )
			{
				const char16 *translated = Localization::TranslateChannelText( text->GetText() );
				size_t matched = 0;
				if( translated )
					while( translated[matched] && translated[matched] == text->GetText()[matched] ) ++matched;
				if( translated && (translated[matched] || text->GetText()[matched]) )
				{
					text->SetText( translated );
					text->SetUniformTextFit( true );
				}
			}
			LocalizeAuthoredChannelPanes( (*it)->panes );
		}
	}

	void LocalizeWorldwideForecastJapanese( Layout *layout )
	{
		if( Localization::CurrentLanguage() != CONF_LANG_JAPANESE ) return;
		Pane *root = layout->FindPane("RootPane");
		Pane *belt = layout->FindPane("belt");
		Pane *current = layout->FindPane("currentENG");
		if( !root || !belt || !current ) return; // International banner only.
		// The international archive has no JPN group and CURRENT is a picture.
		// Borrow the unused German title pair, retaining real textbox materials.
		const char *names[] = { "titleGER_sdw0", "titleGER0" };
		static const char16 currentJapanese[] = { 0x73fe, 0x5728, 0 }; // 現在
		for( int i = 0; i < 2; ++i )
		{
			Textbox *text = layout->FindTextbox(names[i]);
			if( !text ) return;
			PaneList::iterator old = std::find(root->panes.begin(),root->panes.end(),text);
			if( old != root->panes.end() )
			{
				root->panes.erase(old);
				belt->panes.push_back(text);
			}
			text->SetText(currentJapanese);
			text->SetOrigin(4);
			text->CenterText();
			text->SetPosition(current->GetPosX() + (i == 0 ? 1 : 0), current->GetPosY() - (i == 0 ? 1 : 0));
			text->SetSize(current->GetWidth(),current->GetHeight());
			text->SetFontSize(28,32);
			text->SetUniformTextFit(true);
			text->SetAlpha(255);
			text->SetHide(false);
			text->SetVisible(true);
			text->SetInfluencedAlpha(true);
		}
		SetNamedPaneVisible(layout,"currentENG",false);
		layout->BringPaneToFront("titleGER0");
	}

	void SelectNewsGlobe( Layout *layout )
	{
		// Both genuine orientations and their effect layers ship in the PAL
		// archive. Select only the artwork; keep the selected language's logo.
		const bool japanese = ChannelPreview::Get().news.japaneseGlobe
			|| Localization::CurrentLanguage() == CONF_LANG_JAPANESE;
		const char *suffixes[] = { "USA", "EUR", "USAefect", "EURefect",
			"USA_out", "EUR_out" }; // Icon uses separate *_out panes.
		for( u32 i = 0; i < sizeof(suffixes)/sizeof(suffixes[0]); ++i )
		{
			std::string jp = std::string("map_JP_") + suffixes[i];
			std::string ww = std::string("map_WW_") + suffixes[i];
			if( layout->FindPane(jp) && layout->FindPane(ww) )
			{
				SetNamedPaneVisible( layout, jp.c_str(), japanese );
				SetNamedPaneVisible( layout, ww.c_str(), !japanese );
			}
		}
	}

	void LocalizeJapaneseForecastDay( Layout *layout )
	{
		if( Localization::CurrentLanguage() == CONF_LANG_JAPANESE ) return;
		// 'today'/'tomorrow' are Japanese TPL lettering, not textboxes. Reuse
		// the duplicate support pair under the authored day belt so its slide and
		// fade are inherited; the other pair remains the editable attribution.
		SetNamedPaneVisible( layout, "today", false );
		SetNamedPaneVisible( layout, "tomorrow", false );
		SetNamedPaneVisible( layout, "date_hi", false ); // Japanese month suffix.
		Pane *weather = layout->FindPane( "weather" );
		Pane *belt = layout->FindPane( "belt_day" );
		if( !weather || !belt ) return;
		const char *names[] = { "sprt_JPN1", "sprt_JPN_sdw1" };
		for( int i = 0; i < 2; ++i )
		{
			Textbox *text = layout->FindTextbox( names[i] );
			if( !text ) continue;
			PaneList::iterator old = std::find( weather->panes.begin(), weather->panes.end(), text );
			if( old != weather->panes.end() )
			{
				weather->panes.erase( old );
				belt->panes.push_back( text );
			}
			text->SetText( Localization::GetText( "Today" ) );
			text->SetSize( 164.0f, 48.0f );
			text->SetPosition( 46.0f + i, -1.0f - i );
			text->SetFontSize( 31.0f, 36.0f );
			text->SetUniformTextFit( true );
			const Vec2f unit = { 1.0f, 1.0f };
			text->SetScale( unit );
			text->SetVisible( true );
			text->SetHide( false );
			text->SetAlpha( 255 );
			text->SetWidescreen( false );
			text->SetInfluencedAlpha( true );
		}
		// The shadow must be behind the white face. Appending it last painted
		// nearly the whole word dark, especially on enlarged translated glyphs.
		layout->BringPaneToFront( "sprt_JPN1" );
		layout->SetFitTextboxToPane( true );
	}

	bool SetArbitraryForecastTemperature( Layout *layout, const char16 *text,
		const char16 *supportText, bool visible )
	{
		if( !layout )
			return false;

		const char *faceName = "sprt_WW0";
		const char *shadowName = "sprt_WW_sdw0";
		Textbox *face = dynamic_cast< Textbox * >( layout->FindPane( faceName ) );
		Textbox *shadow = dynamic_cast< Textbox * >( layout->FindPane( shadowName ) );
		if( !face || !shadow )
			return false;

		if( !visible )
		{
			// Restore the authored first support pair. The untouched WW1 pair
			// remains in place even while WW0 is temporarily used for temperature.
			face->SetText( supportText );
			shadow->SetText( supportText );
			face->SetPosition( -276.0f, -91.0f );
			shadow->SetPosition( -275.0f, -92.0f );
			face->SetSize( 220.0f, 28.0f );
			shadow->SetSize( 220.0f, 28.0f );
			face->SetFontSize( 15.36f, 18.24f );
			shadow->SetFontSize( 15.36f, 18.24f );
			face->SetUniformTextFit( false );
			shadow->SetUniformTextFit( false );
			const Vec2f unitScale = { 1.0f, 1.0f };
			face->SetScale( unitScale );
			shadow->SetScale( unitScale );
			face->SetAlpha( 0xff );
			shadow->SetAlpha( 0xff );
			SetNamedPaneVisible( layout, faceName, true );
			SetNamedPaneVisible( layout, shadowName, true );
			return true;
		}

		const u32 length = LongestTextLine( text );
		float fontSize = 60.0f;
		if( length > 8 )
			fontSize *= 8.0f / (float)length;
		if( fontSize < 18.0f )
			fontSize = 18.0f;

		// The stock worldwide atlas occupies x=-235..-5, y=6..78. WW0 is
		// already inside the animated weather tree and uses left-middle alignment,
		// so it stays correctly layered and aspect-stable at this anchor.
		face->SetText( text );
		shadow->SetText( text );
		face->SetPosition( -235.0f, 42.0f );
		shadow->SetPosition( -234.0f, 41.0f );
		face->SetSize( 270.0f, 72.0f );
		shadow->SetSize( 270.0f, 72.0f );
		// Arbitrary letters must use the same proportions on both glyph axes.
		// Without uniform fitting a long custom unit compressed only the width;
		// the inherited footer spacing and 1.18 height made letters look distorted.
		face->SetFontSize( fontSize, fontSize );
		shadow->SetFontSize( fontSize, fontSize );
		face->SetCharacterSpacing( 0.0f );
		shadow->SetCharacterSpacing( 0.0f );
		face->SetUniformTextFit( true );
		shadow->SetUniformTextFit( true );
		const Vec2f unitScale = { 1.0f, 1.0f };
		face->SetScale( unitScale );
		shadow->SetScale( unitScale );
		face->SetAlpha( 0xff );
		shadow->SetAlpha( 0xff );
		SetNamedPaneVisible( layout, faceName, true );
		SetNamedPaneVisible( layout, shadowName, true );
		layout->BringPaneToFront( faceName );
		return true;
	}

	void SetForecastCustomUnit( Layout *layout, const char16 *unit,
		const char16 *supportText )
	{
		if( !SetArbitraryForecastTemperature(layout,unit,supportText,true) ) return;
		// Same animated weather parent as the native C/F cell, which is at
		// (-107 + 74, 42). Keep the degree sign and every numeric atlas pane.
		Textbox *face=layout->FindTextbox("sprt_WW0");
		Textbox *shadow=layout->FindTextbox("sprt_WW_sdw0");
		face->SetSize(82.0f,72.0f);
		shadow->SetSize(82.0f,72.0f);
		// Footer text is left-anchored, whereas the atlas unit is centered.
		// Shift the left edge by half the unit cell, without changing its
		// authored origin (the same panes are restored to the footer later).
		face->SetPosition(-61.0f,42.0f);
		shadow->SetPosition(-60.0f,41.0f);
	}

	bool ConfigurePhotoImageLayout( Layout *layout, bool fullBanner )
	{
		Picture *photo = layout
			? dynamic_cast< Picture * >( layout->FindPane( "pic_photo" ) ) : NULL;
		if( !photo )
			return false;

		if( fullBanner )
		{
			float contentWidth, contentHeight, contentCenterY;
			if( !ChannelImageOverride::PhotoBannerContentBounds( layout,
				contentWidth, contentHeight, contentCenterY ) )
				return false;
			// The photo is a full-bleed background, including behind the header.
			// Leaving out the region above belt_c produces an uncovered top strip.
			SetNamedPaneVisible( layout, "cork", false );
			SetNamedPaneVisible( layout, "photo", false );
			SetNamedPaneVisible( layout, "belt_a", false );
			SetNamedPaneVisible( layout, "belt_c", true );
			SetNamedPaneVisible( layout, "logo_nintendo", true );
			SetNamedPaneVisible( layout, "logo", true );
			// Some Photo Channel revisions place the downloaded photo after the
			// header siblings in the BRLYT.  Preserve the authored panes but move
			// them to the end of their own sibling lists so the full-bleed image
			// cannot paint over "Photo Channel" or the Nintendo mark.
			layout->BringPaneToFront( "belt_c" );
			layout->BringPaneToFront( "logo" );
			layout->BringPaneToFront( "logo_nintendo" );
			photo->SetPosition( 0.0f, contentCenterY );
			photo->SetSize( contentWidth, contentHeight );
			const Vec2f unitScale = { 1.0f, 1.0f };
			photo->SetScale( unitScale );
			photo->SetRotate( 0.0f );
			photo->SetOrigin( 4 );
			// Nintendo marks this injected-photo pane for horizontal correction.
			// A full-width replacement already matches the banner; applying that
			// 0.82 scale again leaves side gaps on a widescreen Wii.
			photo->SetWidescreen( false );
			photo->SetAlpha( 0xff );
			photo->SetVisible( true );
			photo->SetHide( false );
			return true;
		}

		// The compact icon already has the desired parent ordering.  Its cork is
		// the intended backing and is independent from the banner posterboard.
		Picture *cork = dynamic_cast< Picture * >( layout->FindPane( "cork" ) );
		if( !cork )
			return false;
		const GXColor black = { 0, 0, 0, 0xff };
		cork->SetVertexColor( black );
		cork->SetAlpha( 0xff );
		cork->SetVisible( true );
		cork->SetHide( false );
		return true;
	}

	bool IsPhotoInsertedImageLayout( Layout *layout, const U8Archive &archive )
	{
		// Photo Channel's ordinary cork-board slideshow is Rso0. Rso1 is a
		// separate presentation authored around an injected photo and its dark
		// top belt. Identify that pair from the resources instead of title IDs.
		return layout && layout->FindPane( "pic_photo" )
			&& layout->FindPane( "belt_c" )
			&& archive.GetFileExact( "/arc/anim/banner_Rso0.brlan" )
			&& archive.GetFileExact( "/arc/anim/banner_Rso1.brlan" );
	}

	bool IsPhotoInsertedImageIconLayout( Layout *layout,
		const U8Archive &archive )
	{
		return layout && layout->FindPane( "pic_photo" )
			&& layout->FindPane( "pic_center" )
			&& layout->FindPane( "belt" )
			&& archive.GetFileExact( "/arc/anim/icon_Rso0.brlan" )
			&& archive.GetFileExact( "/arc/anim/icon_Rso1.brlan" );
	}

	bool IsWiiShopIconLayout( Layout *layout, const U8Archive &archive )
	{
		// The Shop icon ships only numbered recommendation states.  Rso0 is
		// its local idle animation; its 5000-frame declaration contains an
		// unauthored white tail after the complete 0..649 sequence.
		return layout && layout->FindPane( "P_ShopLogo_00" )
			&& layout->FindPane( "bg_wiiplane_00" )
			&& layout->FindPane( "N_LogoTitles" )
			&& layout->FindPane( "iconBg" )
			&& archive.GetFileExact( "/arc/anim/icon_Rso0.brlan" );
	}

	bool IsWiiShopBannerLayout( Layout *layout, const U8Archive &archive )
	{
		// The retail Shop bag is split into independently rotated panels and
		// narrow seam strips below logo_base.  This signature avoids changing
		// transform composition for unrelated retail banners.
		Pane *shopLogo = layout ? layout->FindPane( "logo_base" ) : NULL;
		return shopLogo
			&& shopLogo->FindPane( "Null_00" )
			&& shopLogo->FindPane( "Null_01" )
			&& shopLogo->FindPane( "Null_02" )
			&& shopLogo->FindPane( "Null_03" )
			&& shopLogo->FindPane( "logo_12" )
			&& shopLogo->FindPane( "logo_14" )
			&& shopLogo->FindPane( "logo_15" )
			&& shopLogo->FindPane( "logo_16" )
			&& shopLogo->FindPane( "handle01" )
			&& shopLogo->FindPane( "handle02" )
			&& archive.GetFileExact( "/arc/anim/banner_Start.brlan" )
			&& archive.GetFileExact( "/arc/anim/banner_Loop.brlan" );
	}

	bool IsNintendoChannelIconLayout( Layout *layout, const U8Archive &archive )
	{
		// Nintendo Channel's real downloaded-story icon has three hidden text/
		// screenshot slots. icon_Rso15 is its self-contained first-story loop.
		// Use this exact structural signature instead of the asynchronously
		// assigned HAT? title ID, and never mutate the large banner layout.
		return layout && layout->FindPane( "N_base_00" )
			&& layout->FindPane( "N_scShot_00" )
			&& layout->FindPane( "P_scShot_00" )
			&& layout->FindTextbox( "T_scroll_00" )
			&& archive.GetFileExact( "/arc/anim/icon_Rso15.brlan" );
	}

	bool IsMarioKartBannerLayout( Layout *layout, const U8Archive &archive )
	{
		// Mario Kart Wii's disc/channel banner has a generic idle BRLAN whose
		// kart carriers are intentionally stationary.  The authored Rso4/Rso5
		// resources are the two moving vehicle carousels selected by the game.
		return layout && layout->FindPane( "N_kartAll_00" )
			&& layout->FindPane( "N_kartAnim_00" )
			&& layout->FindPane( "N_kartAnim_01" )
			&& layout->FindTextbox( "titleText_00" )
			&& archive.GetFileExact( "/arc/anim/banner_Rso4.brlan" )
			&& archive.GetFileExact( "/arc/anim/banner_Rso5.brlan" );
	}

	bool IsMarioKartBannerLayout( Layout *layout )
	{
		return layout && layout->FindPane( "N_kartAll_00" )
			&& layout->FindPane( "N_kartAnim_00" )
			&& layout->FindPane( "N_kartAnim_01" )
			&& layout->FindTextbox( "titleText_00" );
	}

	enum ForecastWeatherKind
	{
		ForecastSun,
		ForecastPartlySunny,
		ForecastPartlyCloudy,
		ForecastMostlyCloudy,
		ForecastCloud,
		ForecastFog,
		ForecastShowers,
		ForecastPartlySunnyRain,
		ForecastMostlyCloudyShowers,
		ForecastRain,
		ForecastPartlySunnyThunder,
		ForecastMostlyCloudyThunder,
		ForecastThunder,
		ForecastPartlySunnyFlurries,
		ForecastSnow,
		ForecastFlurries,
		ForecastRainAndSnow,
		ForecastSleet
	};

	enum ForecastJapaneseTexture
	{
		ForecastJapaneseSun,
		ForecastJapaneseCloud,
		ForecastJapaneseRain,
		ForecastJapaneseSnow,
		ForecastJapaneseThunder,
		ForecastJapaneseTextureCount
	};

	ForecastWeatherKind WeatherKindForCode( u16 weatherCode )
	{
		switch( weatherCode )
		{
		case ChannelPreview::ForecastWeatherPartlySunny:
			return ForecastPartlySunny;
		case 0x0462: // Mostly Sunny uses Nintendo's plain-sun composition.
			return ForecastSun;
		case ChannelPreview::ForecastWeatherPartlyCloudy:
		case 0x0066: case 0x0002:
			return ForecastPartlyCloudy;
		case ChannelPreview::ForecastWeatherMostlyCloudy:
			return ForecastMostlyCloudy;
		case ChannelPreview::ForecastWeatherShowers:
			return ForecastShowers;
		case ChannelPreview::ForecastWeatherPartlySunnyRain:
		case 0x0069: case 0x0003:
			return ForecastPartlySunnyRain;
		case ChannelPreview::ForecastWeatherMostlyCloudyShowers:
			return ForecastMostlyCloudyShowers;
		case ChannelPreview::ForecastWeatherPartlySunnyThunder:
			return ForecastPartlySunnyThunder;
		case ChannelPreview::ForecastWeatherMostlyCloudyThunder:
			return ForecastMostlyCloudyThunder;
		case ChannelPreview::ForecastWeatherPartlySunnyFlurries:
			return ForecastPartlySunnyFlurries;
		case ChannelPreview::ForecastWeatherRainAndSnow:
			return ForecastRainAndSnow;
		case ChannelPreview::ForecastWeatherSleet:
			return ForecastSleet;
		case ChannelPreview::ForecastWeatherCloudy:
		case 0x0469: case 0x006a: case 0x006b:
			return ForecastCloud;
		case ChannelPreview::ForecastWeatherFog:
		case 0x007c: case 0x00c7:
			return ForecastFog;
		case 0x0071: case 0x012c: case 0x0013:
		case ChannelPreview::ForecastWeatherRain:
		case 0x006f: case 0x012d:
			return ForecastRain;
		case ChannelPreview::ForecastWeatherThunder:
		case 0x007d: case 0x0384: case 0x0021:
			return ForecastThunder;
		case ChannelPreview::ForecastWeatherSnow:
		case 0x0074: case 0x001a: case 0x8191:
		case 0x04cc:
			return ForecastSnow;
		case 0x0592: case 0x0076: case 0x8190: case 0x801a:
		case 0x04cd:
			return ForecastFlurries;
		default:
			return ForecastSun;
		}
	}

	bool IsJapaneseForecastLayout( Layout *layout )
	{
		return layout && layout->FindPane( "J_sun_00" );
	}

	bool UseForecastNightArt()
	{
		const ChannelPreview::ForecastTimeMode mode =
			ChannelPreview::Get().forecast.timeMode;
		if( mode == ChannelPreview::ForecastTimeNight )
			return true;
		if( mode == ChannelPreview::ForecastTimeDay )
			return false;

		// The retail channel changes its presentation with the console clock.
		// Keep the automatic rule deliberately local and deterministic: night is
		// 18:00 through 05:59 according to the Wii's configured time zone.
		const time_t now = time( NULL );
		const struct tm *local = localtime( &now );
		return local && ( local->tm_hour < 6 || local->tm_hour >= 18 );
	}

	void ConfigureForecastWeatherPane( Layout *layout, const char *name,
		float x, float y, float scale )
	{
		if( !layout || !name )
			return;
		Pane *pane = layout->FindPane( name );
		if( !pane )
			return;

		pane->SetPosition( x, y );
		Vec2f paneScale = { scale, scale };
		pane->SetScale( paneScale );
		pane->SetVisible( true );
		pane->SetHide( false );
		Picture *picture = dynamic_cast< Picture * >( pane );
		if( picture )
		{
			GXColor white = { 0xff, 0xff, 0xff, 0xff };
			picture->SetVertexColor( white );
		}

		// The Forecast module removes selected panes from `code` and appends
		// them again in its mark-table order. That order is essential: rain is
		// drawn before its cloud while lightning is drawn last. Visibility alone
		// cannot reproduce the authored composite on PAL icon.brlyt, whose raw
		// child order differs from banner.brlyt.
		Pane *code = layout->FindPane( "code" );
		if( !code )
			return;
		PaneList::iterator child = std::find( code->panes.begin(),
			code->panes.end(), pane );
		if( child != code->panes.end() )
		{
			code->panes.erase( child );
			code->panes.push_back( pane );
		}
	}

	void SetForecastWeatherMaterialTexture( Layout *layout, const char *paneName,
		Texture *texture )
	{
		if( !layout || !paneName )
			return;
		Picture *picture = dynamic_cast< Picture * >( layout->FindPane( paneName ) );
		if( !picture )
			return;
		const int materialIndex = picture->GetMaterialIndex();
		const MaterialList &materials = layout->Materials();
		if( materialIndex < 0 || materialIndex >= (int)materials.size()
			|| !materials[ materialIndex ] )
			return;
		materials[ materialIndex ]->SetForcedTexture( texture );
	}

	void ConfigureForecastJapaneseCarrier( Layout *layout, const char *paneName,
		Texture *texture, float x, float y, float scale )
	{
		SetForecastWeatherMaterialTexture( layout, paneName, texture );
		ConfigureForecastWeatherPane( layout, paneName, x, y, scale );
	}

	void SetForecastWeatherPaneColor( Layout *layout, const char *paneName,
		const GXColor &color )
	{
		if( !layout || !paneName )
			return;
		Picture *picture = dynamic_cast< Picture * >( layout->FindPane( paneName ) );
		if( picture )
			picture->SetVertexColor( color );
	}

	void SetForecastWeatherIcon( Layout *layout, u16 weatherCode, bool night,
		bool japaneseLayout, bool japaneseStyle, bool iconLayout,
		Texture *japaneseStyleTextures, Texture *customImage = NULL )
	{
		static const char *worldwideIcons[] = {
			"W_sun_00", "W_cloud_00", "W_cloud_01", "W_cloud_02",
			"W_fog_00", "W_moon_00", "W_rain_00", "W_thunder_00",
			"W_wind_00", "W_hail_all", "W_sleet_all", "W_snow_all"
		};
		static const char *japaneseIcons[] = {
			"J_sun_00", "J_cloud_00", "J_rain_00", "J_snow_00",
			"J_thunder_00", "arrow"
		};

		const ForecastWeatherKind kind = WeatherKindForCode( weatherCode );
		for( u32 i = 0; i < sizeof( worldwideIcons ) / sizeof( worldwideIcons[ 0 ] ); ++i )
			SetNamedPaneVisible( layout, worldwideIcons[ i ], false );
		for( u32 i = 0; i < sizeof( japaneseIcons ) / sizeof( japaneseIcons[ 0 ] ); ++i )
			SetNamedPaneVisible( layout, japaneseIcons[ i ], false );
		const char *imageCarrier = japaneseLayout ? "J_sun_00" : "W_sun_00";
		SetForecastWeatherMaterialTexture( layout, imageCarrier, NULL );
		if( customImage )
		{
			// Keep the retail weather parent/animation. Only replace its glyph,
			// with transparent letterboxing for accepted 4:3 art in the square pane.
			ConfigureForecastJapaneseCarrier( layout, imageCarrier, customImage,
				0.0f, 0.0f, iconLayout && !japaneseLayout ? 44.0f / 75.0f : 1.0f );
			return;
		}
		// These two square PAL panes double as carriers for the genuine Japanese
		// art. Always release a previous override first so changing the setting on
		// a live preview can return to the authored worldwide textures safely.
		if( !japaneseLayout )
		{
			SetForecastWeatherMaterialTexture( layout, "W_sun_00", NULL );
			SetForecastWeatherMaterialTexture( layout, "W_moon_00", NULL );
		}

		const float positionScale = iconLayout ? 0.5f : 1.0f;
		const char *luminary = night && layout->FindPane( "W_moon_00" )
			? "W_moon_00" : "W_sun_00";

		if( japaneseLayout )
		{
			const char *japaneseLuminary = night
				&& layout->FindPane( "W_moon_00" )
				? "W_moon_00" : "J_sun_00";
			switch( kind )
			{
			case ForecastMostlyCloudyShowers:
				ConfigureForecastWeatherPane( layout, "J_cloud_00", -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_rain_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastPartlySunnyRain:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_rain_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastShowers:
			case ForecastRain:
				ConfigureForecastWeatherPane( layout, "J_rain_00", 0.0f, 0.0f, 1.0f );
				break;
			case ForecastPartlySunnyThunder:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_thunder_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastMostlyCloudyThunder:
				ConfigureForecastWeatherPane( layout, "J_cloud_00", -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_thunder_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastThunder:
				ConfigureForecastWeatherPane( layout, "J_thunder_00", 0.0f, 0.0f, 1.0f );
				break;
			case ForecastPartlySunnyFlurries:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_snow_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastRainAndSnow:
				ConfigureForecastWeatherPane( layout, "J_rain_00", -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_snow_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastSleet:
			case ForecastSnow:
			case ForecastFlurries:
				ConfigureForecastWeatherPane( layout, "J_snow_00", 0.0f, 0.0f, 1.0f );
				break;
			case ForecastPartlySunny:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, 0.0f, 0.0f, 1.0f );
				break;
			case ForecastPartlyCloudy:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, -28.0f * positionScale,
					12.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "J_cloud_00", 28.0f * positionScale,
					-12.0f * positionScale, 1.0f );
				break;
			case ForecastMostlyCloudy:
				if( night )
				{
					ConfigureForecastWeatherPane( layout, japaneseLuminary,
						-28.0f * positionScale, 12.0f * positionScale, 1.0f );
					ConfigureForecastWeatherPane( layout, "J_cloud_00",
						28.0f * positionScale, -12.0f * positionScale, 1.0f );
				}
				else
					ConfigureForecastWeatherPane( layout, "J_cloud_00", 0.0f, 0.0f, 1.0f );
				break;
			case ForecastCloud:
			case ForecastFog:
				ConfigureForecastWeatherPane( layout, "J_cloud_00", 0.0f, 0.0f, 1.0f );
				break;
			default:
				ConfigureForecastWeatherPane( layout, japaneseLuminary, 0.0f, 0.0f, 1.0f );
				break;
			}
		}
		else if( japaneseStyle && japaneseStyleTextures )
		{
			// PAL WADs contain no J_* panes or textures. Reuse two independent,
			// square PAL panes as carriers for the real 80x80 banner / 44x44 icon
			// symbols extracted from the user's Japanese Forecast WAD.
			const float primaryScale = iconLayout ? 44.0f / 75.0f : 80.0f / 150.0f;
			const float secondaryScale = iconLayout ? 44.0f / 38.0f : 80.0f / 75.0f;
			// Japanese WADs do not provide a separate extracted moon symbol. For
			// night mixtures, retain the authored PAL moon and use the other square
			// carrier for the genuine Japanese cloud/rain/thunder/snow symbol.
			if( night )
			{
				int nightSecondary = -2;
				switch( kind )
				{
				case ForecastSun:
				case ForecastPartlySunny:
					nightSecondary = -1;
					break;
				case ForecastPartlyCloudy:
				case ForecastMostlyCloudy:
					nightSecondary = ForecastJapaneseCloud;
					break;
				case ForecastPartlySunnyRain:
					nightSecondary = ForecastJapaneseRain;
					break;
				case ForecastPartlySunnyThunder:
					nightSecondary = ForecastJapaneseThunder;
					break;
				case ForecastPartlySunnyFlurries:
					nightSecondary = ForecastJapaneseSnow;
					break;
				default:
					break;
				}
				if( nightSecondary >= -1 )
				{
					ConfigureForecastWeatherPane( layout, "W_moon_00",
						nightSecondary >= 0 ? -28.0f * positionScale : 0.0f,
						nightSecondary >= 0 ? 12.0f * positionScale : 0.0f, 1.0f );
					if( nightSecondary >= 0 )
						ConfigureForecastJapaneseCarrier( layout, "W_sun_00",
							&japaneseStyleTextures[ nightSecondary ],
							28.0f * positionScale, -12.0f * positionScale,
							secondaryScale );
					return;
				}
			}
			int primary = ForecastJapaneseSun;
			int secondary = -1;
			switch( kind )
			{
			case ForecastPartlySunny:
				primary = ForecastJapaneseSun;
				break;
			case ForecastPartlyCloudy:
				primary = ForecastJapaneseSun;
				secondary = ForecastJapaneseCloud;
				break;
			case ForecastMostlyCloudy:
			case ForecastCloud:
			case ForecastFog:
				primary = ForecastJapaneseCloud;
				break;
			case ForecastShowers:
				primary = ForecastJapaneseRain;
				break;
			case ForecastPartlySunnyRain:
				primary = ForecastJapaneseSun;
				secondary = ForecastJapaneseRain;
				break;
			case ForecastMostlyCloudyShowers:
				primary = ForecastJapaneseCloud;
				secondary = ForecastJapaneseRain;
				break;
			case ForecastRain:
				primary = ForecastJapaneseRain;
				break;
			case ForecastPartlySunnyThunder:
				primary = ForecastJapaneseSun;
				secondary = ForecastJapaneseThunder;
				break;
			case ForecastMostlyCloudyThunder:
				primary = ForecastJapaneseCloud;
				secondary = ForecastJapaneseThunder;
				break;
			case ForecastThunder:
				primary = ForecastJapaneseThunder;
				break;
			case ForecastPartlySunnyFlurries:
				primary = ForecastJapaneseSun;
				secondary = ForecastJapaneseSnow;
				break;
			case ForecastRainAndSnow:
				primary = ForecastJapaneseRain;
				secondary = ForecastJapaneseSnow;
				break;
			case ForecastSleet:
			case ForecastSnow:
			case ForecastFlurries:
				primary = ForecastJapaneseSnow;
				break;
			default:
				primary = ForecastJapaneseSun;
				break;
			}
			if( secondary >= 0 )
			{
				ConfigureForecastJapaneseCarrier( layout, "W_sun_00",
					&japaneseStyleTextures[ primary ], -28.0f * positionScale,
					12.0f * positionScale, primaryScale );
				ConfigureForecastJapaneseCarrier( layout, "W_moon_00",
					&japaneseStyleTextures[ secondary ], 28.0f * positionScale,
					-12.0f * positionScale, secondaryScale );
			}
			else
			{
				ConfigureForecastJapaneseCarrier( layout, "W_sun_00",
					&japaneseStyleTextures[ primary ], 0.0f, 0.0f, primaryScale );
			}
		}
		else
		{
			// Exact records exported from the retail Forecast module's mark_tab.
			// icon.brlyt authors every worldwide texture at half the banner size,
			// so its mark-table translations must be halved as well. Applying the
			// banner translations directly is what shoved clouds into the corners.
			switch( kind )
			{
			case ForecastPartlySunny:
				ConfigureForecastWeatherPane( layout, luminary, 0.0f, -9.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 29.0f * positionScale,
					-27.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_02", -55.5f * positionScale,
					16.5f * positionScale, 1.0f );
				break;
			case ForecastPartlyCloudy:
				ConfigureForecastWeatherPane( layout, luminary, -24.0f * positionScale,
					-8.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 4.0f * positionScale,
					-28.0f * positionScale, 1.2f );
				break;
			case ForecastMostlyCloudy:
				ConfigureForecastWeatherPane( layout, luminary, 0.0f,
					-9.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 29.0f * positionScale,
					-27.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_02", -55.5f * positionScale,
					16.5f * positionScale, 1.0f );
				break;
			case ForecastCloud:
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 0.0f,
					-7.0f * positionScale, 1.2f );
				break;
			case ForecastFog:
				ConfigureForecastWeatherPane( layout, "W_fog_00", 0.0f, 0.0f, 1.0f );
				break;
			case ForecastShowers:
				ConfigureForecastWeatherPane( layout, "W_rain_00", -4.0f * positionScale,
					-37.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					20.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 47.0f * positionScale,
					-14.0f * positionScale, 1.0f );
				break;
			case ForecastPartlySunnyRain:
				ConfigureForecastWeatherPane( layout, luminary, -15.0f * positionScale,
					23.0f * positionScale, 0.8f );
				ConfigureForecastWeatherPane( layout, "W_rain_00", -4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				break;
			case ForecastMostlyCloudyShowers:
				ConfigureForecastWeatherPane( layout, "W_rain_00", -4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", -49.0f * positionScale,
					25.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				break;
			case ForecastRain:
				ConfigureForecastWeatherPane( layout, "W_rain_00", -4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				break;
			case ForecastPartlySunnyThunder:
				ConfigureForecastWeatherPane( layout, luminary, -15.0f * positionScale,
					23.0f * positionScale, 0.8f );
				ConfigureForecastWeatherPane( layout, "W_rain_00", 4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_thunder_00", 2.5f * positionScale,
					-54.5f * positionScale, 1.0f );
				break;
			case ForecastMostlyCloudyThunder:
				ConfigureForecastWeatherPane( layout, "W_rain_00", 4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", -49.0f * positionScale,
					25.0f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_thunder_00", 2.5f * positionScale,
					-54.5f * positionScale, 1.0f );
				break;
			case ForecastThunder:
				ConfigureForecastWeatherPane( layout, "W_rain_00", 4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_thunder_00", 2.5f * positionScale,
					-54.5f * positionScale, 1.0f );
				break;
			case ForecastPartlySunnyFlurries:
				ConfigureForecastWeatherPane( layout, luminary, -15.0f * positionScale,
					23.0f * positionScale, 0.8f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_snow_all", 13.5f * positionScale,
					-48.5f * positionScale, 1.0f );
				break;
			case ForecastSnow:
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_snow_all", 13.5f * positionScale,
					-48.5f * positionScale, 1.0f );
				break;
			case ForecastFlurries:
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					20.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_snow_all", -39.0f * positionScale,
					-35.0f * positionScale, 0.9f );
				ConfigureForecastWeatherPane( layout, "W_cloud_00", 47.0f * positionScale,
					-14.0f * positionScale, 1.0f );
				break;
			case ForecastRainAndSnow:
				ConfigureForecastWeatherPane( layout, "W_rain_00", -41.0f * positionScale,
					-50.5f * positionScale, 0.9f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_snow_all", 52.0f * positionScale,
					-48.5f * positionScale, 0.9f );
				break;
			case ForecastSleet:
			{
				ConfigureForecastWeatherPane( layout, "W_rain_00", -4.0f * positionScale,
					-50.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_cloud_01", 0.0f,
					7.5f * positionScale, 1.0f );
				ConfigureForecastWeatherPane( layout, "W_sleet_all", 13.5f * positionScale,
					-48.5f * positionScale, 1.0f );
				const GXColor sleetTint = { 0xd2, 0xff, 0xff, 0xff };
				SetForecastWeatherPaneColor( layout, "W_sleet_all", sleetTint );
				break;
			}
			default:
				ConfigureForecastWeatherPane( layout, luminary, 0.0f, 0.0f, 1.0f );
				break;
			}
		}
	}

	bool SetAtlasPane( Layout *layout, const char *name, int column, u32 columns )
	{
		if( !layout || !name )
			return false;
		Pane *pane = layout->FindPane( name );
		Picture *picture = dynamic_cast< Picture * >( pane );
		if( !picture )
			return false;

		const bool visible = column >= 0 && (u32)column < columns
			&& picture->SetTextureAtlasColumn( (u32)column, columns );
		pane->SetVisible( visible );
		pane->SetHide( !visible );
		return visible;
	}

	void SetMainTemperature( Layout *layout, const char *hundredsName,
		const char *tensName, const char *onesName, int temperature,
		u32 columns, int minusColumn )
	{
		if( temperature <= -100 || temperature >= 1000 )
		{
			SetAtlasPane( layout, hundredsName, -1, columns );
			SetAtlasPane( layout, tensName, minusColumn, columns );
			SetAtlasPane( layout, onesName, minusColumn, columns );
			return;
		}

		if( temperature < 0 )
		{
			const int absolute = -temperature;
			SetAtlasPane( layout, hundredsName, absolute >= 10 ? minusColumn : -1, columns );
			SetAtlasPane( layout, tensName, absolute >= 10 ? absolute / 10 : minusColumn, columns );
			SetAtlasPane( layout, onesName, absolute % 10, columns );
			return;
		}

		const int hundreds = temperature / 100;
		const int tens = ( temperature / 10 ) % 10;
		SetAtlasPane( layout, hundredsName, hundreds ? hundreds : -1, columns );
		SetAtlasPane( layout, tensName, ( hundreds || tens ) ? tens : -1, columns );
		SetAtlasPane( layout, onesName, temperature % 10, columns );
	}

	void SetTemperatureDifference( Layout *layout, const char *signName,
		const char *tensName, const char *onesName, int difference )
	{
		const u32 columns = 12;
		if( difference <= -100 || difference >= 100 )
		{
			SetAtlasPane( layout, signName, -1, columns );
			SetAtlasPane( layout, tensName, 11, columns );
			SetAtlasPane( layout, onesName, 11, columns );
			return;
		}

		const int absolute = difference < 0 ? -difference : difference;
		const int sign = difference < 0 ? 11 : 10;
		if( absolute >= 10 )
		{
			SetAtlasPane( layout, signName, sign, columns );
			SetAtlasPane( layout, tensName, absolute / 10, columns );
		}
		else
		{
			SetAtlasPane( layout, signName, -1, columns );
			SetAtlasPane( layout, tensName, sign, columns );
		}
		SetAtlasPane( layout, onesName, absolute % 10, columns );
	}

	void SetPrecipitation( Layout *layout, const char *hundredsName,
		const char *tensName, const char *onesName, u8 precipitation )
	{
		const u32 columns = 11;
		if( precipitation == 0xff || precipitation > 100 )
		{
			SetAtlasPane( layout, hundredsName, -1, columns );
			SetAtlasPane( layout, tensName, 10, columns );
			SetAtlasPane( layout, onesName, 10, columns );
			return;
		}

		const int hundreds = precipitation / 100;
		const int tens = ( precipitation / 10 ) % 10;
		SetAtlasPane( layout, hundredsName, hundreds ? hundreds : -1, columns );
		SetAtlasPane( layout, tensName, ( hundreds || tens ) ? tens : -1, columns );
		SetAtlasPane( layout, onesName, precipitation % 10, columns );
	}

	void ApplyWorldwideForecastNumbers( Layout *layout,
		const ForecastGeneratedData &data )
	{
		if( !data.nativeTemperature )
		{
			SetAtlasPane( layout, "kion_doF100", -1, 14 );
			SetAtlasPane( layout, "kion_doF10", -1, 14 );
			SetAtlasPane( layout, "kion_doF1", -1, 14 );
			SetAtlasPane( layout, "kion_doF_do", -1, 14 );
			SetAtlasPane( layout, "kion_doF_F", -1, 14 );
			return;
		}
		SetMainTemperature( layout, "kion_doF100", "kion_doF10", "kion_doF1",
			data.currentCelsius, 14, 11 );
		SetAtlasPane( layout, "kion_doF_do", 10, 14 );
		// The stock pane name says F; cells 12 and 13 are the C/F glyphs.
		const bool customUnit = data.custom && !ChannelPreview::Get().forecast.customUnit.empty();
		SetAtlasPane( layout, "kion_doF_F", customUnit ? -1 : data.fahrenheit ? 13 : 12, 14 );
	}

	void ApplyJapaneseForecastNumbers( Layout *layout,
		const ForecastGeneratedData &data )
	{
		SetNamedPaneVisible( layout, "today", true );
		SetNamedPaneVisible( layout, "tomorrow", false );
		SetNamedPaneVisible( layout, "belt_day", true );
		// HAFJ's authored Rso1 alternates the Japanese "today" belt with a month
		// belt. Populate its 11-cell 0..9/month atlas so the second half of the
		// loop is real Japanese UI rather than a blank or the raw "00 month" state.
		time_t now = time( NULL );
		struct tm *local = localtime( &now );
		const int month = local ? local->tm_mon + 1 : 1;
		SetNamedPaneVisible( layout, "belt_date", true );
		SetAtlasPane( layout, "date_10-10", month >= 10 ? month / 10 : -1, 11 );
		SetAtlasPane( layout, "date_1-10", month % 10, 11 );
		SetAtlasPane( layout, "date_hi", 10, 11 );

		SetNamedPaneVisible( layout, "kionH", data.nativeTemperature );
		SetNamedPaneVisible( layout, "kionL", false );
		SetNamedPaneVisible( layout, "kionZ", true );
		SetNamedPaneVisible( layout, "kion_low", false );
		SetNamedPaneVisible( layout, "kion_zen", true );
		SetNamedPaneVisible( layout, "ondo_C",
			data.nativeTemperature && !data.fahrenheit );
		SetNamedPaneVisible( layout, "ondo_F",
			data.nativeTemperature && data.fahrenheit );

		if( data.nativeTemperature )
		{
			SetMainTemperature( layout, "kionH_100", "kionH_10", "kionH_1",
				data.highCelsius, 12, 11 );
		}
		SetTemperatureDifference( layout, "kionZ_100", "kionZ_10", "kionZ_1",
			data.highDifferenceCelsius );

		SetPrecipitation( layout, "rainH_100", "rainH_10", "rainH_1",
			data.precipitation[ 2 ] );
		SetPrecipitation( layout, "rainL_100", "rainL_10", "rainL_1",
			data.precipitation[ 3 ] );
		SetNamedPaneVisible( layout, "rain_am6-12", false );
		SetNamedPaneVisible( layout, "rain_am12-18", false );
		SetNamedPaneVisible( layout, "rain_pm12-18", true );
		SetNamedPaneVisible( layout, "rain_pm18-24", true );
	}

	float EverybodyVotesFontSize( const char16 *text, float authoredSize,
		u32 comfortableCharacters )
	{
		const u32 length = LongestTextLine( text );
		if( !length || length <= comfortableCharacters )
			return authoredSize;
		// Arbitrary user text must stay inside its native bubble. The cap in
		// channelpreviewconfig bounds the worst case; proportional fitting keeps
		// the familiar authored size for normal-length messages.
		const float fitted = authoredSize * (float)comfortableCharacters
			/ (float)length;
		const float minimumReadable = authoredSize * 0.55f;
		return fitted < minimumReadable ? minimumReadable : fitted;
	}

	float EverybodyVotesBubbleWidth( const char16 *text )
	{
		// The retail downloaded panes are deliberately enormous conveyor
		// windows.  A preview message only needs its text width plus the native
		// left/right padding; keeping this bounded prevents the first blue bubble
		// from occupying most of the icon.
		const float width = 110.0f + 8.0f * LongestTextLine( text );
		return std::max( 180.0f, std::min( 330.0f, width ) );
	}

	bool ConfigureEverybodyVotesIcon( Layout *layout,
		char16 textStorage[ 3 ][ ChannelPreview::MaxEverybodyVotesTextBytes ],
		float &loopEnd )
	{
		// Original is a true pass-through mode: do not even probe/mutate the
		// network placeholder panes, and let LoadIcon use the retail BRLYT and
		// complete authored Rso0 range below.
		const ChannelPreview::EverybodyVotesConfig &config =
			ChannelPreview::Get().everybodyVotes;
		if( !layout || config.style == ChannelPreview::EverybodyVotesOriginal )
			return false;

		const bool ogSequence = config.style
			== ChannelPreview::EverybodyVotesOgSequence;
		const char *messages[ 3 ];
		if( config.style == ChannelPreview::EverybodyVotesCustom || ogSequence )
		{
			messages[ 0 ] = config.firstBlue.c_str();
			messages[ 1 ] = config.greenQuestion.c_str();
			messages[ 2 ] = config.finalBlue.c_str();
		}
		else
		{
			messages[ 0 ] = ChannelPreview::EverybodyVotesJokeText( 0 );
			messages[ 1 ] = ChannelPreview::EverybodyVotesJokeText( 1 );
			messages[ 2 ] = ChannelPreview::EverybodyVotesJokeText( 2 );
		}
		for( int message = 0; message < 3; ++message )
		{
			Utf8ToChar16( textStorage[ message ],
				ChannelPreview::MaxEverybodyVotesTextBytes, messages[ message ] );
			// These native panes are single-line conveyor labels. Treat manually
			// edited XML line breaks as spaces so they cannot escape the bubble.
			for( char16 *character = textStorage[ message ]; *character; ++character )
			{
				if( *character == '\r' || *character == '\n' )
					*character = ' ';
			}
		}
		Window *setup = dynamic_cast<Window *>( layout->FindPane( "W_vote_00" ) );
		Window *question = dynamic_cast<Window *>( layout->FindPane( "W_result_00" ) );
		Window *reply = dynamic_cast<Window *>( layout->FindPane( "W_result_01" ) );
		Textbox *setupFront = dynamic_cast<Textbox *>( layout->FindPane( "T_vote_00" ) );
		Textbox *setupShadow = dynamic_cast<Textbox *>( layout->FindPane( "T_vote_01" ) );
		Textbox *questionFront = dynamic_cast<Textbox *>( layout->FindPane( "T_result_00" ) );
		Textbox *questionShadow = dynamic_cast<Textbox *>( layout->FindPane( "T_result_02" ) );
		Textbox *replyFront = dynamic_cast<Textbox *>( layout->FindPane( "T_result_01" ) );
		Textbox *replyShadow = dynamic_cast<Textbox *>( layout->FindPane( "T_result_03" ) );
		if( !setup || !question || !reply || !setupFront || !setupShadow
			|| !questionFront || !questionShadow || !replyFront || !replyShadow )
			return false;

		// Custom and Joke retain the familiar blue -> green -> blue sequence.
		// OG sequence instead keeps the retail result colours and orders them as
		// blue -> purple -> green.  Copy both text materials with the final blue
		// window; copying only its shadow left the foreground in the old colour.
		if( !ogSequence
			&& ( !reply->CopyAppearanceFrom( *setup )
				|| !replyFront->CopyMaterialFrom( *setupFront )
				|| !replyShadow->CopyMaterialFrom( *setupShadow ) ) )
			return false;

		Window *middle = ogSequence ? reply : question;
		Window *last = ogSequence ? question : reply;
		Textbox *middleFront = ogSequence ? replyFront : questionFront;
		Textbox *middleShadow = ogSequence ? replyShadow : questionShadow;
		Textbox *lastFront = ogSequence ? questionFront : replyFront;
		Textbox *lastShadow = ogSequence ? questionShadow : replyShadow;

		// Only point native panes at generated storage after the regional layout
		// has passed every structural/material check. A failed alternate WAD then
		// remains completely authored instead of being left half-customized.
		setupFront->SetText( textStorage[ 0 ] );
		setupShadow->SetText( textStorage[ 0 ] );
		middleFront->SetText( textStorage[ 1 ] );
		middleShadow->SetText( textStorage[ 1 ] );
		lastFront->SetText( textStorage[ 2 ] );
		lastShadow->SetText( textStorage[ 2 ] );
		const float setupWidth = EverybodyVotesBubbleWidth( textStorage[ 0 ] );
		const float middleWidth = EverybodyVotesBubbleWidth( textStorage[ 1 ] );
		const float lastWidth = EverybodyVotesBubbleWidth( textStorage[ 2 ] );
		const u32 setupComfort = std::max( 12u,
			(u32)( ( setupWidth - 48.0f ) / 9.5f ) );
		const u32 middleComfort = std::max( 12u,
			(u32)( ( middleWidth - 48.0f ) / 9.5f ) );
		const u32 lastComfort = std::max( 12u,
			(u32)( ( lastWidth - 48.0f ) / 9.5f ) );
		const float setupFont = EverybodyVotesFontSize( textStorage[ 0 ],
			20.0f, setupComfort );
		const float middleFont = EverybodyVotesFontSize( textStorage[ 1 ],
			21.0f, middleComfort );
		const float lastFont = EverybodyVotesFontSize( textStorage[ 2 ],
			20.0f, lastComfort );

		// The real icon BRLYT is 608 pixels wide, not 128.  Use its authored
		// hand/bubble overlap (hand right edge ~=137, first bubble x=130) and its
		// native 25-pixel gaps instead of deriving an anchor from layout width.
		const float firstBubbleX = 130.0f;
		const float middleBubbleX = firstBubbleX + setupWidth + 25.0f;
		const float lastBubbleX = middleBubbleX + middleWidth + 25.0f;

		setup->SetHide( false );
		setup->SetVisible( true );
		setup->SetPosition( firstBubbleX, setup->GetPosY() );
		setup->SetSize( setupWidth, 64.0f );
		setupFront->SetPosition( 24.0f, setupFront->GetPosY() );
		setupShadow->SetPosition( 24.0f, setupShadow->GetPosY() );
		setupFront->SetSize( setupWidth - 48.0f, 31.0f );
		setupShadow->SetSize( setupWidth - 48.0f, 31.0f );
		setupFront->SetFontSize( setupFont, 28.0f );
		setupShadow->SetFontSize( setupFont, 28.0f );

		middle->SetHide( false );
		middle->SetVisible( true );
		middle->SetPosition( middleBubbleX, middle->GetPosY() );
		middle->SetSize( middleWidth, 64.0f );
		middleFront->SetPosition( 24.0f, middleFront->GetPosY() );
		middleShadow->SetPosition( 24.0f, middleShadow->GetPosY() );
		middleFront->SetSize( middleWidth - 48.0f, 31.0f );
		middleShadow->SetSize( middleWidth - 48.0f, 31.0f );
		middleFront->SetFontSize( middleFont, 28.0f );
		middleShadow->SetFontSize( middleFont, 28.0f );

		last->SetHide( false );
		last->SetVisible( true );
		last->SetPosition( lastBubbleX, last->GetPosY() );
		last->SetSize( lastWidth, 64.0f );
		lastFront->SetPosition( 24.0f, lastFront->GetPosY() );
		lastShadow->SetPosition( 24.0f, lastShadow->GetPosY() );
		lastFront->SetSize( lastWidth - 48.0f, 31.0f );
		lastShadow->SetSize( lastWidth - 48.0f, 31.0f );
		lastFront->SetFontSize( lastFont, 28.0f );
		lastShadow->SetFontSize( lastFont, 28.0f );

		// N_message_00 begins its one-pixel-per-frame travel at frame 350.
		// Reset only after the actual last pane has cleared the 608-pixel canvas.
		loopEnd = 350.0f + lastBubbleX + lastWidth
			+ layout->GetWidth() * 0.5f + 16.0f;
		return true;
	}
}

Banner::Banner( const u8 *data, u32 len )
	: arc( NULL ),
	  bannerObj( NULL ),
	  iconObj( NULL ),
	  marioKartCarouselDelay( 0.0f ),
	  marioKartCarouselLoopEnd( 0.0f ),
	  marioKartCarouselStarted( false ),
	  banner_bin(NULL)
	, icon_bin(NULL)
	, sound_bin(NULL)
	, sound_bin_size(0)
	, layout_banner(NULL)
	, layout_icon(NULL)
	, titleId(0)
	, hasInsertedPhotoImage(false)
	, bannerCustomImageAttempted(false)
	, iconCustomImageAttempted(false)
	, appliedWiiFitIconProfile(-1)
	, iconReady(false)
	, appliedPreviewRevision(0)
	, imetHeader( NULL )
{
	marioKartCarouselObj[ 0 ] = NULL;
	marioKartCarouselObj[ 1 ] = NULL;
	memset( generatedText, 0, sizeof( generatedText ) );
	memset( nintendoIconText, 0, sizeof( nintendoIconText ) );
	nintendoStoryCycle=false;
	nintendoStoryIndex=0;
	memset( customPreviewText, 0, sizeof( customPreviewText ) );
	memset( everybodyVotesText, 0, sizeof( everybodyVotesText ) );
	forecastLocalArtAttempted[0]=forecastLocalArtAttempted[1]=false;
	BeginBannerPresentation();
	Load(data, len);
}

void Banner::LoadLocalForecastTextures(bool icon)
{
	const int kind=icon?1:0;
	if(forecastLocalArtAttempted[kind]) return;
	forecastLocalArtAttempted[kind]=true;
	u8 *bytes=NULL; u32 size=0;
	if(!LoadLocalForecastArchive(icon,&bytes,&size)) return;
	U8Archive archive(bytes,size);
	const char *names[]={"sun","cloud","rain","snow","thunder"};
	for(int i=0;i<5;++i) {
		char path[80]; snprintf(path,sizeof(path),"/arc/timg/%sJ_%s_00.tpl",icon?"icon_":"",names[i]);
		u32 length=0; const u8 *tpl=archive.GetFile(path,&length);
		if(!tpl || length<56 || length>128*1024) continue;
		forecastLocalArt[kind][i].assign(tpl,tpl+length);
		(icon?forecastJapaneseIconTextures[i]:forecastJapaneseBannerTextures[i]).Load(&forecastLocalArt[kind][i][0]);
	}
	free(bytes);
}

Banner::~Banner()
{
	delete arc;
	delete bannerObj;
	delete iconObj;
	delete marioKartCarouselObj[ 0 ];
	delete marioKartCarouselObj[ 1 ];
	delete layout_banner;
	// The material which held the forced texture is gone now, so releasing the
	// owned backing store cannot leave a live Layout with a dangling pointer.
	bannerImageOverride.Reset();
	delete layout_icon;
	iconImageOverride.Reset();

	std::map< std::string, Animation *>:: iterator it = bannerBrlans.begin(), itE = bannerBrlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}

	it = iconBrlans.begin(), itE = iconBrlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}

	if(banner_bin)
		free(banner_bin);
	if(icon_bin)
		free(icon_bin);
	if(sound_bin)
		free(sound_bin);
}

bool Banner::Load( const u8 *data, u32 len )
{
	imetHeader = NULL;
	delete arc;
	arc = NULL;
	if( !data || len < 0x20 )
		return false;

	const u8 *imet = FindImetHeader( data, len );
	if( imet )
	{
		imetHeader = (u8 *)imet;
		gprintf( "IMET version %u at %08x\n", ReadBE32( imet + 8 ),
			(u32)( imet - data ) );
	}
	else
	{
		// Raw U8 banners have no localized IMET title but are still renderable.
		gprintf( "no IMET header found; trying raw/extended U8\n" );
	}

	arc = new U8Archive( data, len );
	if( !arc || !arc->IsValid() )
	{
		delete arc;
		arc = NULL;
		imetHeader = NULL;
		return false;
	}
	return true;
}

static void ConfigureEverybodyVotesBackground(Layout *layout, const U8Archive &archive)
{
	// The native banner has a separate, repeating 8x8 stripe texture. Slow
	// only its scroll; the hands, bubbles, logo and Start/Loop timing stay native.
	// Use the layout + texture signature for NAND, WAD and standalone banners.
	if (!layout || !layout->FindPane("N_hand_00")
		|| !layout->FindPane("P_hand_00")
		|| !archive.GetFileExact("/arc/timg/sk_CBgLine_00.tpl"))
		return;
	Material *background = layout->FindMaterial("Picture_00");
	Texture *stripes = layout->FindTexture("sk_CBgLine_00.tpl");
	if (!background || !stripes || background->GetTextureMapCount() != 1)
		return;
	const TextureList &textures = layout->Textures();
	const u16 index = background->GetTextureIndex();
	if (index >= textures.size() || textures[index] != stripes)
		return;
	// Both authored loops end on whole texture repeats after this correction.
	background->SetTextureTranslationScale(1.0f, 1.0f / 8.0f);
}

Object *Banner::LoadBanner()
{
	if(!arc)
		return NULL;

	if( bannerObj )
	{
		EnsureGeneratedChannelDataCurrent();
		return bannerObj;
	}
	u32 arcLen = 0;
	const bool japaneseForecastUi = IsForecastTitle( titleId )
		&& ChannelPreview::Get().forecast.japaneseIcons;
	if( japaneseForecastUi
		&& !LoadLocalForecastArchive(false, &banner_bin, &arcLen) )
	{
		gprintf( "Forecast Japanese banner archive invalid; using title archive\n" );
	}
	if( !banner_bin
		&& !( banner_bin = arc->GetFileAllocated( "/meta/banner.bin", &arcLen ) ) )
	{
		return NULL;
	}

	U8Archive theArc( banner_bin, arcLen );
	if(IsForecastTitle(titleId) && ChannelPreview::Get().forecast.japaneseWeatherIcons)
		LoadLocalForecastTextures(false);

	// create layout
	if( !( layout_banner = LoadLayout( theArc, "banner" ) ) )
	{
		return NULL;
	}
	ConfigureEverybodyVotesBackground(layout_banner, theArc);
	const bool forecastBannerLayout = IsForecastTitle( titleId )
		|| IsForecastLayout( layout_banner );
	const bool newsBannerLayout = IsNewsTitle( titleId )
		|| IsNewsLayout( layout_banner );
	const bool wiiFitBannerLayout = IsWiiFitTitle( titleId )
		|| IsWiiFitBannerLayout( layout_banner );
	if( IsWiiShopBannerLayout( layout_banner, theArc ) )
	{
		// Scope the correct NW4R matrix order to the Shop bag only.  Its seam
		// strips otherwise shear away from the panels near the end of Start.
		layout_banner->FindPane( "logo_base" )
			->SetNw4rTransformOrder( true, true );
	}

	const bool photoInsertedImageLayout = IsPhotoInsertedImageLayout(
		layout_banner, theArc );
	const bool advertisementBannerLayout =
		layout_banner->FindPane( "button" )
		&& layout_banner->FindPane( "txt1_eng" )
		&& layout_banner->FindPane( "txt1_2_eng" )
		&& layout_banner->FindPane( "txt2_eng" )
		&& theArc.GetFileExact( "/arc/anim/banner_Rso0.brlan" );
	const bool legacyInternetBannerLayout =
		layout_banner->FindPane( "N_messWindow_00" )
		&& layout_banner->FindPane( "N_title_00" )
		&& layout_banner->FindPane( "N_logoU_00" )
		&& layout_banner->FindPane( "N_logoD_00" )
		&& theArc.GetFileExact( "/arc/anim/banner_Rso0.brlan" )
		&& theArc.GetFileExact( "/arc/anim/banner_Rso1.brlan" );
	const ChannelPreview::Config &preview = ChannelPreview::Get();
	hasInsertedPhotoImage = false;
	bannerCustomImageAttempted = false;
	if( photoInsertedImageLayout && preview.photo.enabled )
	{
		bannerCustomImageAttempted = true;
		hasInsertedPhotoImage = bannerImageOverride.Apply( layout_banner,
			ChannelImageOverride::PhotoBanner, preview.photo.imagePath,
			ChannelPreview::Fill );
		if( hasInsertedPhotoImage
			&& !ConfigurePhotoImageLayout( layout_banner, true ) )
		{
			// A regional variant without the expected sibling hierarchy must stay
			// in its normal Rso0 state.  The override remains owned until unload,
			// while the unused inserted-image panes are hidden below.
			gprintf( "Photo banner image skipped: unsupported pane hierarchy\n" );
			hasInsertedPhotoImage = false;
		}
		if( !hasInsertedPhotoImage && !preview.photo.imagePath.empty() )
			gprintf( "Photo banner image skipped: %s\n",
				bannerImageOverride.LastError().c_str() );
	}
	// create object
	bannerObj = new Object;
	bannerObj->BindPane( layout_banner->FindPane( "RootPane" ) );
	bannerObj->BindMaterials( layout_banner->Materials() );

	// Animation names are case-sensitive inside U8 archives. Nintendo shipped
	// Banner_Start/Banner_Loop in Today & Tomorrow, plus lower-initial and fully
	// lowercase variants in other channels.
	static const char *startNames[] = {
		"banner_Start", "Banner_Start", "banner_start", "banner_In", "banner_in"
	};
	static const char *genericLoopNames[] = {
		"banner", "banner_Loop", "Banner_Loop", "banner_loop", "banner_Rso1", "banner_Rso0"
	};
	const bool marioKartBanner = IsMarioKartBannerLayout( layout_banner, theArc );
	const char *photoLoopNames[] = {
		hasInsertedPhotoImage ? "banner_Rso1" : "banner_Rso0"
	};
	const char *const *loopNames = photoInsertedImageLayout
		? photoLoopNames
		: genericLoopNames;
	const u32 loopNameCount = photoInsertedImageLayout
		? sizeof( photoLoopNames ) / sizeof( photoLoopNames[ 0 ] )
		: wiiFitBannerLayout ? 0
		: marioKartBanner ? 0
			: sizeof( genericLoopNames ) / sizeof( genericLoopNames[ 0 ] );
	std::string brlanName;
	Animation *anim = NULL;
	if( wiiFitBannerLayout )
	{
		// RFNP's authored sequence is Rso0 (631-frame entrance), followed by
		// Rso1 (181-frame Mii reveal). The generic fallback used to start at Rso1,
		// so the title/background entrance was skipped on every cold open.
		brlanName = "banner_Rso0";
		anim = LoadAnimation( theArc, brlanName );
	}
	for( u32 i = 0; !anim && i < sizeof( startNames ) / sizeof( startNames[ 0 ] ); ++i )
	{
		brlanName = startNames[ i ];
		anim = LoadAnimation( theArc, brlanName );
	}
	if( !anim && legacyInternetBannerLayout )
	{
		// Internet Channel v768 stores its complete message-window/title reveal
		// in Rso0 and only the subsequent logo idle motion in Rso1. Loading Rso1
		// by itself produces the empty blue/white banner seen on real hardware.
		brlanName = "banner_Rso0";
		anim = LoadAnimation( theArc, brlanName );
	}
	// we have a starting animation
	if( anim )
	{
		if( marioKartBanner )
			marioKartCarouselDelay = std::max( 0.0f,
				anim->FrameCount() - 1.0f );
		layout_banner->LoadBrlanTpls( anim, theArc );
		bannerBrlans[ brlanName ] = anim;
		bannerObj->AddAnimation( anim );
		bannerObj->SetAnimation( brlanName, 0, -1, -1, false );
		bannerObj->Start();
	}

	anim = NULL;
	for( u32 i = 0; !anim && i < loopNameCount; ++i )
	{
		brlanName = loopNames[ i ];
		anim = LoadAnimation( theArc, brlanName );
	}
	// we have a loop animation
	if( anim )
	{
		layout_banner->LoadBrlanTpls( anim, theArc );
		bannerBrlans[ brlanName ] = anim;
		bannerObj->AddAnimation( anim );
		if( photoInsertedImageLayout && hasInsertedPhotoImage
			&& brlanName == "banner_Rso1" )
		{
			// Rso1's frames 0..39 fade the inserted image from alpha zero. Begin
			// at its first fully opaque frame and loop only that stable range, so
			// neither the removed backing nor the old slideshow can flash through.
			const float end = anim->FrameCount() - 1.0f;
			bannerObj->ScheduleAnimation( brlanName, 40.0f, end, -1.0f, true );
		}
		else if( forecastBannerLayout && brlanName == "banner_Rso0" )
		{
			// Forecast's Rso0 is a 17-frame entrance fade, not an idle loop.
			// Looping it makes the weather blink once every 17 frames.
			const float end = anim->FrameCount() - 1.0f;
			bannerObj->ScheduleAnimation( brlanName, 0.0f, end, end, false );
		}
		else if( newsBannerLayout && brlanName == "banner_Rso1" )
		{
			// CHANS shortens the 36,000-frame carrier to the width of the
			// downloaded headlines. Size it from fixture or custom content.
			const u32 headlineCount = LoadNewsGeneratedText( generatedText );
			bannerObj->ScheduleAnimation( brlanName, 0.0f,
				NewsBannerLoopEnd( headlineCount ), -1.0f, true );
		}
		else if( advertisementBannerLayout && brlanName == "banner_Rso0" )
		{
			// Wii downloadable-channel advertisements author Rso0 as a one-shot
			// reveal. Repeating its 110 frames continuously hides the localized
			// message and advertised-channel details on every wrap, leaving only
			// the background visible. Play it once and retain its completed state.
			const float end = anim->FrameCount() - 1.0f;
			bannerObj->ScheduleAnimation( brlanName, 0.0f, end, end, false );
		}
		else
		{
			bannerObj->ScheduleAnimation( brlanName );
		}
		bannerObj->Start();
	}

	if( marioKartBanner )
	{
		// Rso4 moves N_kartAnim_00 and Rso5 moves N_kartAnim_01 in opposite
		// directions. Object deliberately supports only one AnimationLink, so
		// binding either file to RootPane makes the later link steal pane tracks
		// from the earlier one. Give each authored carrier its own controller and
		// leave all shared/static tracks with the intro object.
		static const char *carouselAnimations[ 2 ] = {
			"banner_Rso4", "banner_Rso5"
		};
		static const char *carouselPanes[ 2 ] = {
			"N_kartAnim_00", "N_kartAnim_01"
		};
		Animation *carousel[ 2 ] = { NULL, NULL };
		Pane *carrier[ 2 ] = { NULL, NULL };
		for( int i = 0; i < 2; ++i )
		{
			carousel[ i ] = LoadAnimation( theArc, carouselAnimations[ i ] );
			carrier[ i ] = layout_banner->FindPane( carouselPanes[ i ] );
		}
		if( carousel[ 0 ] && carousel[ 1 ] && carrier[ 0 ] && carrier[ 1 ] )
		{
			// Frames 0..3600 form the authored seamless lap: each carrier exits,
			// wraps to the opposite edge at 3100..3208, and returns to x=0.  The
			// remaining 3601..7500 tail is a one-way transition used by the channel
			// application; looping that tail leaves every kart off-screen for a long
			// stretch and looks like the banner stopped.
			marioKartCarouselLoopEnd = std::min( 3600.0f,
				std::max( 0.0f, std::min( carousel[ 0 ]->FrameCount(),
					carousel[ 1 ]->FrameCount() ) - 1.0f ) );
			for( int i = 0; i < 2; ++i )
			{
				layout_banner->LoadBrlanTpls( carousel[ i ], theArc );
				bannerBrlans[ carouselAnimations[ i ] ] = carousel[ i ];
				marioKartCarouselObj[ i ] = new Object;
				marioKartCarouselObj[ i ]->BindPane( carrier[ i ], false );
				marioKartCarouselObj[ i ]->AddAnimation( carousel[ i ] );
			}
			if( marioKartCarouselDelay <= 0.0f )
			{
				marioKartCarouselObj[ 0 ]->ScheduleAnimation( "banner_Rso4",
					0.0f, marioKartCarouselLoopEnd, -1.0f, true );
				marioKartCarouselObj[ 1 ]->ScheduleAnimation( "banner_Rso5",
					0.0f, marioKartCarouselLoopEnd, -1.0f, true );
				marioKartCarouselObj[ 0 ]->Start();
				marioKartCarouselObj[ 1 ]->Start();
				marioKartCarouselStarted = true;
			}
		}
		else
		{
			delete carousel[ 0 ];
			delete carousel[ 1 ];
		}
	}
	if( photoInsertedImageLayout && !hasInsertedPhotoImage )
	{
		// An animation visibility key must never reveal these panes without a
		// successfully prepared inserted-image texture.
		SetNamedPaneVisible( layout_banner, "pic_photo", false );
		SetNamedPaneVisible( layout_banner, "belt_c", false );
	}

	EnsureGeneratedChannelDataCurrent( true );
	return bannerObj;
}

void Banner::EnsureGeneratedChannelDataCurrent( bool force )
{
	const u32 revision = ChannelPreview::Revision();
	if( !force && appliedPreviewRevision == revision )
		return;
	if( layout_banner )
		ApplyGeneratedChannelText();
	// BannerAsync publishes iconReady only after all layout and animation state
	// is complete. Never patch the worker's partially constructed icon.
	if( iconReady && layout_icon )
		ApplyGeneratedChannelIcon();
	appliedPreviewRevision = revision;
}

void Banner::RefreshGeneratedChannelText()
{
	// Banner BRLANs rewrite visibility/alpha every frame, so the selected large
	// preview still needs its native data panes restored each render. Icons do
	// not: only refresh them when the shared settings/network revision changed.
	ApplyGeneratedChannelText();
	const u32 revision = ChannelPreview::Revision();
	if( appliedPreviewRevision != revision && iconReady && layout_icon )
		ApplyGeneratedChannelIcon();
	appliedPreviewRevision = revision;
}

void Banner::BeginBannerPresentation()
{
	bannerInfoFrames = 0;
	bannerInfoDelayFrames = (_CONF_GetVideo() == CONF_VIDEO_PAL
		&& _CONF_GetEuRGB60() < 1) ? 50 : 60;
	bannerFullyOpened = false;
	newsBannerExtent = 0.0f;
	newsBannerExtentRevision = ~0U;
}

void Banner::PrepareBannerFrame(bool fullyOpened)
{
	bannerFullyOpened = fullyOpened;
	RefreshGeneratedChannelText();
}

static u8 BannerInformationAlpha(u32 frames, u32 delay, u32 fadeFrames)
{
	if (frames <= delay) return 0;
	const u32 fade = frames - delay;
	return fade >= fadeFrames ? 255 : (u8)(fade * 255 / fadeFrames);
}

static float NewsTickerPosition(u32 frames, u32 delay, float lastPointRight)
{
	if (frames <= delay) return 0.0f;
	// Keep the native two-pixel scroll. As soon as the final point leaves the
	// 608-pixel banner, show the first column again -- no empty exit/intro tail.
	const u32 cycle = (u32)std::max(1.0f, ceilf((lastPointRight + 304.0f) / 2.0f));
	return -2.0f * (float)((frames - delay) % cycle);
}

void Banner::ApplyGeneratedChannelText()
{
	if( !layout_banner )
		return;
	if( hasInsertedPhotoImage && layout_banner->FindPane( "pic_photo" )
		&& layout_banner->FindPane( "belt_c" ) )
	{
		// Rso1 contains visibility/alpha keys for the original inserted-photo
		// presentation.  MenuHandler calls this after animation advancement and
		// before every render, so the full-bleed image and authored header remain
		// visible for the complete loop instead of disappearing on a later key.
		ConfigurePhotoImageLayout( layout_banner, true );
	}
	if( IsMarioKartBannerLayout( layout_banner ) )
	{
		// RMCP embeds a Japanese subtitle directly in titleText_00 instead of
		// providing language groups.  Replace only that exact structural pane.
		SetGeneratedTextPane( layout_banner, "titleText_00",
			Localization::Get( Localization::MarioKartChannel ) );
		return;
	}
	if( IsWiiFitTitle( titleId ) || IsWiiFitBannerLayout( layout_banner ) )
	{
		// Always retain Rso0's native no-save presentation. Custom Mii/status
		// data belongs to the icon only and must never turn the banner into a
		// simulated save-data graph.
		return;
	}
	const bool forecastLayout = IsForecastTitle( titleId )
		|| IsForecastLayout( layout_banner );
	const bool newsLayout = IsNewsTitle( titleId )
		|| IsNewsLayout( layout_banner );
	if( !forecastLayout && !newsLayout )
		return;

	if( forecastLayout )
	{
		const bool japaneseLayout = IsJapaneseForecastLayout( layout_banner );
		if( japaneseLayout ) LocalizeJapaneseForecastText( layout_banner );
		else LocalizeWorldwideForecastJapanese( layout_banner );
		const ChannelPreview::ForecastConfig &forecastConfig =
			ChannelPreview::Get().forecast;
		const bool customForecast = forecastConfig.enabled;
		const std::string imagePath = customForecast ? forecastConfig.imagePath : "";
		if( forecastBannerImagePath != imagePath )
		{
			forecastBannerImagePath = imagePath;
			if( !imagePath.empty() ) bannerImageOverride.Apply( layout_banner,
				ChannelImageOverride::ForecastImage, imagePath, ChannelPreview::Fit );
		}
		const bool japaneseStyle = japaneseLayout
			|| forecastConfig.japaneseWeatherIcons;
		const ForecastGeneratedData data = LoadForecastGeneratedText( generatedText,
			japaneseLayout );
		SetArbitraryForecastTemperature( layout_banner, generatedText[ 7 ],
			generatedText[ 9 ], false );

		// The downloaded-data module normally enables `all`.  It is authored
		// invisible, while banner_Rso0 only animates its alpha, so descendants
		// cannot render until visibility is selected here.
		SetNamedPaneVisible( layout_banner, "all", true );
		SetNamedPaneVisible( layout_banner, "weather", true );
		SetNamedPaneVisible( layout_banner, "belt", true );
		SetNamedPaneVisible( layout_banner, "code", true );
		SetNamedPaneVisible( layout_banner, "textB0", false );

		SetForecastWeatherIcon( layout_banner,
			japaneseLayout ? data.dailyWeatherCode : data.currentWeatherCode,
			UseForecastNightArt(),
			japaneseLayout, japaneseStyle, false,
			japaneseLayout || !forecastJapaneseBannerTextures[0].IsLoaded() ? NULL : forecastJapaneseBannerTextures,
			imagePath.empty() ? NULL : bannerImageOverride.GetTexture() );

		// These are the exact neutral data textboxes written by the Forecast
		// channel module.  Keep their authored fonts, alpha and language state.
		if( customForecast )
		{
			// User-entered fields are intentionally not bounded to Nintendo's
			// original feed lengths. Fit both face and shadow panes together.
			if( japaneseLayout )
			{
				SetGeneratedTextPaneFitted( layout_banner, "city", generatedText[ 1 ],
					24, 0.32f );
				SetGeneratedTextPaneFitted( layout_banner, "city_sdw", generatedText[ 1 ],
					24, 0.32f );
			}
			else
			{
				SetWorldwideForecastTextPane( layout_banner, "city", generatedText[ 1 ],
					24, 0.32f );
				SetWorldwideForecastTextPane( layout_banner, "city_sdw", generatedText[ 1 ],
					24, 0.32f );
			}
			const bool separateTemperature = !data.nativeTemperature
				&& SetArbitraryForecastTemperature( layout_banner,
					generatedText[ 7 ], generatedText[ 9 ], true );
			if( data.nativeTemperature && !japaneseLayout && !forecastConfig.customUnit.empty() )
				SetForecastCustomUnit(layout_banner,generatedText[7],generatedText[9]);
			const char16 *description = separateTemperature
				? generatedText[ 2 ] : data.nativeTemperature
					? generatedText[ 2 ] : generatedText[ 8 ];
			const u32 descriptionFit = separateTemperature || data.nativeTemperature
				? 24 : 15;
			if( japaneseLayout )
			{
				SetGeneratedTextPaneFitted( layout_banner, "telop", description,
					descriptionFit, 0.16f );
				SetGeneratedTextPaneFitted( layout_banner, "telop_sdw", description,
					descriptionFit, 0.16f );
			}
			else
			{
				SetWorldwideForecastTextPane( layout_banner, "telop", description,
					descriptionFit, 0.16f );
				SetWorldwideForecastTextPane( layout_banner, "telop_sdw", description,
					descriptionFit, 0.16f );
			}
		}
		else
		{
			if( japaneseLayout )
			{
				SetGeneratedTextPane( layout_banner, "city", generatedText[ 1 ] );
				SetGeneratedTextPane( layout_banner, "city_sdw", generatedText[ 1 ] );
				SetGeneratedTextPane( layout_banner, "telop", generatedText[ 2 ] );
				SetGeneratedTextPane( layout_banner, "telop_sdw", generatedText[ 2 ] );
			}
			else
			{
				SetWorldwideForecastTextPane( layout_banner, "city", generatedText[ 1 ],
					0, 1.0f );
				SetWorldwideForecastTextPane( layout_banner, "city_sdw", generatedText[ 1 ],
					0, 1.0f );
				SetWorldwideForecastTextPane( layout_banner, "telop", generatedText[ 2 ],
					0, 1.0f );
				SetWorldwideForecastTextPane( layout_banner, "telop_sdw", generatedText[ 2 ],
					0, 1.0f );
			}
		}
		if( japaneseLayout )
		{
			SetGeneratedTextPane( layout_banner, "timeJP", generatedText[ 3 ] );
			ApplyJapaneseForecastNumbers( layout_banner, data );
		}
		else
		{
			SetGeneratedTextPane( layout_banner, "timeWW", generatedText[ 3 ] );
			ApplyWorldwideForecastNumbers( layout_banner, data );
		}
		// WW0 is also the large-temperature carrier; WW1 is the genuine
		// footer copy and must remain separate from it.
		const char *supportPanes[] = { "sprt_WW1", "sprt_WW_sdw1",
			"sprt_JPN0", "sprt_JPN_sdw0", "sprt_JPN1", "sprt_JPN_sdw1" };
		for( u32 i = 0; i < sizeof(supportPanes) / sizeof(supportPanes[0]); ++i )
			SetGeneratedTextPaneFitted( layout_banner, supportPanes[i],
				generatedText[9], 28, 0.25f );
		if( japaneseLayout ) LocalizeJapaneseForecastDay( layout_banner );
		// Both layouts use `all` for the information layer, below the title.
		// Its native Rso0 fade lasts 16 frames; start it only after the menu zoom
		// is complete and the full one-second wait has elapsed.
		Pane *information = layout_banner->FindPane("all");
		if (information)
			information->SetAlpha(BannerInformationAlpha(bannerInfoFrames,
				bannerInfoDelayFrames, 16));
	}
	else
	{
		const u32 headlineCount = LoadNewsGeneratedText( generatedText );
		const u32 revision = ChannelPreview::Revision();
		const bool measureExtent = newsBannerExtent <= 0.0f
			|| newsBannerExtentRevision != revision;
		if (measureExtent) newsBannerExtent = 0.0f;
		SelectNewsGlobe( layout_banner );
		SetNamedPaneVisible( layout_banner, "text", false );
		SetNamedPaneVisible( layout_banner, "news", true );
		SetNamedPaneVisible( layout_banner, "line", true );

		// The channel module exposes one authored translucent belt per active
		// row.  Never recurse through `news`: that is what produced white bands.
		for( int i = 0; i < 3; ++i )
		{
			char beltName[ 16 ];
			snprintf( beltName, sizeof( beltName ), "belt%d", i );
			SetNamedPaneVisible( layout_banner, beltName, (u32)i < headlineCount );
		}

		for( int i = 0; i < 12; ++i )
		{
			char lineName[ 16 ];
			char telopName[ 16 ];
			snprintf( lineName, sizeof( lineName ), "line%d", i );
			snprintf( telopName, sizeof( telopName ), "telop%d", i );
			const bool active = (u32)i < headlineCount;
			SetNamedPaneVisible( layout_banner, lineName, active );
			SetNamedPaneVisible( layout_banner, telopName, active );
			if( active )
			{
				Pane *linePane = layout_banner->FindPane( lineName );
				if( linePane )
					linePane->SetPosition( (float)( i / 3 ) * 680.0f,
						linePane->GetPosY() );
				if( ChannelPreview::Get().news.enabled )
					SetGeneratedTextPaneFitted( layout_banner, telopName,
						generatedText[ i + 1 ], 46, 0.18f );
				else
					SetGeneratedTextPane( layout_banner, telopName,
						generatedText[ i + 1 ] );
				if (measureExtent)
				{
					float width = layout_banner->MeasureTextbox(telopName);
					if (ChannelPreview::Get().news.enabled)
					{
						const u32 length = LongestTextLine(generatedText[i + 1]);
						width *= length > 46 ? std::max(0.18f, 46.0f / length) : 1.0f;
					}
					if (width <= 0.0f) width = 638.0f;
					newsBannerExtent = std::max(newsBannerExtent,
						(float)(i / 3) * 680.0f + 42.0f + width + 12.0f);
				}
			}
		}
		newsBannerExtentRevision = revision;
		Pane *news = layout_banner->FindPane("news");
		Pane *line = layout_banner->FindPane("line");
		if (news)
			news->SetAlpha(BannerInformationAlpha(bannerInfoFrames,
				bannerInfoDelayFrames, 32));
		if (line)
		{
			line->SetAlpha(255);
			line->SetPosition(NewsTickerPosition(bannerInfoFrames,
				bannerInfoDelayFrames, newsBannerExtent), line->GetPosY());
		}
	}
}

void Banner::ApplyGeneratedChannelIcon()
{
	if( !layout_icon )
		return;
	// This state must also be applied when settings change after the grid has
	// already loaded the stock logo. Previously only LoadIcon selected stories.
	if(iconObj && layout_icon->FindPane("N_scShot_00")
		&& layout_icon->FindTextbox("T_scroll_00")
		&& iconBrlans.find("icon_Rso15")!=iconBrlans.end()) {
		const ChannelPreview::NintendoConfig &news=ChannelPreview::Get().nintendo;
		// This opaque stock-logo card is last in draw order. Rso15 does not
		// animate it away: the downloaded-data controller must hide it.
		SetNamedPaneVisible(layout_icon,"N_chIcon_00",!news.enabled);
		SetNamedPaneVisible(layout_icon,"N_base_00",news.enabled);
		SetNamedPaneVisible(layout_icon,"N_base_01",false);
		SetNamedPaneVisible(layout_icon,"N_base_02",false);
		if(news.enabled) {
			if(!news.imagePath.empty())
				iconImageOverride.Apply(layout_icon,ChannelImageOverride::NintendoIcon,news.imagePath,news.fitMode);
			nintendoStoryCycle=true;nintendoStoryIndex=0;StartNintendoStory();
		} else if(nintendoStoryCycle) {
			nintendoStoryCycle=false;
			iconObj->SetAnimation("icon_Rso0",0,-1,-1,true);iconObj->Start();
		}
		return;
	}
	const bool forecastLayout = IsForecastTitle( titleId )
		|| IsForecastLayout( layout_icon );
	const bool newsLayout = IsNewsTitle( titleId )
		|| IsNewsLayout( layout_icon );
	const bool todayTomorrowLayout = IsTodayTomorrowTitle( titleId )
		|| IsTodayTomorrowLayout( layout_icon );
	const bool miiContestLayout = IsMiiContestTitle( titleId )
		|| IsMiiContestLayout( layout_icon );
	const bool wiiFitLayout = IsWiiFitTitle( titleId )
		|| IsWiiFitIconLayout( layout_icon );
	if( !forecastLayout && !newsLayout && !todayTomorrowLayout
		&& !miiContestLayout && !wiiFitLayout )
		return;

	if( todayTomorrowLayout )
	{
		const ChannelPreview::TodayTomorrowConfig &today =
			ChannelPreview::Get().todayTomorrow;
		if( !today.enabled ) return;
		Utf8ToChar16( customPreviewText[ 0 ], 256, today.affinity.c_str() );
		Utf8ToChar16( customPreviewText[ 1 ], 256, today.cleaning.c_str() );
		Utf8ToChar16( customPreviewText[ 2 ], 256, today.play.c_str() );
		Utf8ToChar16( customPreviewText[ 3 ], 256, today.meal.c_str() );
		SetGeneratedTextPaneFitted( layout_icon, "TextBox_affinity",
			customPreviewText[ 0 ], 29, 0.28f );
		SetGeneratedTextPaneFitted( layout_icon, "TextBox_beau",
			customPreviewText[ 1 ], 29, 0.28f );
		SetGeneratedTextPaneFitted( layout_icon, "TextBox_play",
			customPreviewText[ 2 ], 29, 0.28f );
		SetGeneratedTextPaneFitted( layout_icon, "TextBox_meel",
			customPreviewText[ 3 ], 29, 0.28f );
		return;
	}

	if( miiContestLayout )
	{
		const ChannelPreview::MiiContestConfig &contest =
			ChannelPreview::Get().miiContest;
		if( !contest.enabled ) return;
		Utf8ToChar16( customPreviewText[ 4 ], 256, contest.comment.c_str() );
		SetNamedPaneVisible( layout_icon, "N_comment_00", true );
		SetNamedPaneVisible( layout_icon, "N_commentText_00", true );
		SetNamedPaneVisible( layout_icon, "N_photo_00", true );
		SetNamedPaneVisible( layout_icon, "N_theme_00", true );
		SetGeneratedTextPaneFitted( layout_icon, "N_commentText_00",
			customPreviewText[ 4 ], 28, 0.26f );
		return;
	}

	if( wiiFitLayout )
	{
		const ChannelPreview::WiiFitConfig &wiiFit = ChannelPreview::Get().wiiFit;
		if( !wiiFit.enabled )
		{
			SetNamedPaneVisible( layout_icon, "N_MiiMessages_00", false );
			SetNamedPaneVisible( layout_icon, "N_title_00", true );
			return;
		}
		// The profile is represented by its Mii portrait. The speech strip is the
		// status only; never prefix it with "Samu:" or another selected name.
		Utf8ToChar16( customPreviewText[ 6 ], 256, wiiFit.status.c_str() );
		ApplyWiiFitIconMessage( layout_icon, customPreviewText[ 6 ] );
		const int profileIndex = MiiProfiles::IndexOf( wiiFit.profile );
		if( appliedWiiFitIconProfile != profileIndex )
		{
			iconCustomImageAttempted = true;
			if( !iconImageOverride.ApplyWiiFitMiiCapture( layout_icon,
				ChannelImageOverride::WiiFitIcon, profileIndex ) )
				gprintf( "Wii Fit icon portrait skipped: %s\n",
					iconImageOverride.LastError().c_str() );
			else
				appliedWiiFitIconProfile = profileIndex;
		}
		return;
	}

	if( forecastLayout )
	{
		const bool japaneseLayout = IsJapaneseForecastLayout( layout_icon );
		const ChannelPreview::ForecastConfig &forecastConfig = ChannelPreview::Get().forecast;
		const std::string imagePath = forecastConfig.enabled ? forecastConfig.imagePath : "";
		if( forecastIconImagePath != imagePath )
		{
			forecastIconImagePath = imagePath;
			if( !imagePath.empty() ) iconImageOverride.Apply( layout_icon,
				ChannelImageOverride::ForecastImage, imagePath, ChannelPreview::Fit );
		}
		if( japaneseLayout ) LocalizeJapaneseForecastText( layout_icon );
		const bool japaneseStyle = japaneseLayout
			|| ChannelPreview::Get().forecast.japaneseWeatherIcons;
		const ForecastGeneratedData data = LoadForecastGeneratedText( generatedText,
			japaneseLayout );
		SetNamedPaneVisible( layout_icon, "code", true );
		SetForecastWeatherIcon( layout_icon,
			japaneseLayout ? data.dailyWeatherCode : data.currentWeatherCode,
			UseForecastNightArt(),
			japaneseLayout, japaneseStyle, true,
			japaneseLayout || !forecastJapaneseIconTextures[0].IsLoaded() ? NULL : forecastJapaneseIconTextures,
			imagePath.empty() ? NULL : iconImageOverride.GetTexture() );
		if( japaneseLayout )
		{
			SetNamedPaneVisible( layout_icon, "day", true );
			SetGeneratedTextPane( layout_icon, "day1sdw", generatedText[ 6 ] );
			SetGeneratedTextPane( layout_icon, "day0sdw", generatedText[ 6 ] );
			SetGeneratedTextPane( layout_icon, "day1", generatedText[ 6 ] );
			SetGeneratedTextPane( layout_icon, "day0", generatedText[ 6 ] );
		}
		// bg_logo_wn belongs to icon_Rso0 and deliberately alternates with the
		// selected glyph.  Do not touch its authored visibility or alpha.
		return;
	}

	const u32 headlineCount = LoadNewsGeneratedText( generatedText );
	SelectNewsGlobe( layout_icon );
	SetNamedPaneVisible( layout_icon, "news", true );
	SetNamedPaneVisible( layout_icon, "line", true );
	for( int row = 0; row < 2; ++row )
	{
		char beltName[ 16 ];
		snprintf( beltName, sizeof( beltName ), "belt%d", row );
		SetNamedPaneVisible( layout_icon, beltName, (u32)row < headlineCount );
	}

	for( int i = 0; i < 12; ++i )
	{
		char telopName[ 16 ];
		char sphereName[ 16 ];
		snprintf( telopName, sizeof( telopName ), "telop%d", i );
		snprintf( sphereName, sizeof( sphereName ), "sphere%d", i );
		const bool active = (u32)i < headlineCount;
		SetNamedPaneVisible( layout_icon, telopName, active );
		SetNamedPaneVisible( layout_icon, sphereName, active );
		if( !active )
			continue;

		SetGeneratedTextPane( layout_icon, telopName, generatedText[ i + 1 ] );
		// Exact wbf1 measurement of the first pair is about 284 pixels;
		// CHANS adds a 42-pixel gap before the next pair.
		const float column = (float)( i / 2 ) * 327.0f;
		Pane *telop = layout_icon->FindPane( telopName );
		Pane *sphere = layout_icon->FindPane( sphereName );
		if( telop )
		{
			telop->SetPosition( 27.0f + column, telop->GetPosY() );
			if( ChannelPreview::Get().news.enabled
				|| strcmp( CONF_GetLanguageString(), "JPN" ) )
			{
				const float fittedScale = NewsTickerScale( generatedText[ i + 1 ],
					ChannelPreview::Get().news.enabled );
				Vec2f scale = { fittedScale, fittedScale };
				telop->SetScale( scale );
			}
		}
		if( sphere )
			sphere->SetPosition( 5.0f + column, sphere->GetPosY() );
	}
}

Object *Banner::LoadIcon()
{
	if(!arc)
		return NULL;

	if( iconObj )
	{
		EnsureGeneratedChannelDataCurrent();
		return iconObj;
	}
	iconReady = false;

	//gprintf( "Banner::LoadIcon()\n" );
	u32 arcLen = 0;
	// Retail loop-only icons must begin at their authored frame zero. Starting
	// Rso0 at a random frame exposes half-initialized panes in Photo and Shop.
	bool random = false;
	const bool japaneseForecastUi = IsForecastTitle( titleId )
		&& ChannelPreview::Get().forecast.japaneseIcons;
	if( japaneseForecastUi
		&& !LoadLocalForecastArchive(true, &icon_bin, &arcLen) )
	{
		gprintf( "Forecast Japanese icon archive invalid; using title archive\n" );
	}
	if( !icon_bin
		&& !( icon_bin = arc->GetFileAllocated( "/meta/icon.bin", &arcLen ) ) )
	{
		return NULL;
	}

	U8Archive theArc( icon_bin, arcLen );
	if(IsForecastTitle(titleId) && ChannelPreview::Get().forecast.japaneseWeatherIcons)
		LoadLocalForecastTextures(true);

	// create layout
	if( !( layout_icon = LoadLayout( theArc, "icon" ) ) )
	{
		return NULL;
	}
	const bool photoInsertedImageIconLayout = IsPhotoInsertedImageIconLayout(
		layout_icon, theArc );
	const ChannelPreview::Config &preview = ChannelPreview::Get();
	const ChannelPreview::PhotoConfig &photoPreview = preview.photo;
	bool customPhotoIcon = false;
	if( photoInsertedImageIconLayout && photoPreview.enabled )
	{
		customPhotoIcon = iconImageOverride.Apply( layout_icon,
			ChannelImageOverride::PhotoIcon, photoPreview.imagePath,
			photoPreview.fitMode );
		if( customPhotoIcon )
			ConfigurePhotoImageLayout( layout_icon, false );
		if( !customPhotoIcon && !photoPreview.imagePath.empty() )
			gprintf( "Photo icon image skipped: %s\n",
				iconImageOverride.LastError().c_str() );
	}
	const bool nintendoArticleIcon = preview.nintendo.enabled
		&& IsNintendoChannelIconLayout( layout_icon, theArc );
	if( nintendoArticleIcon )
	{
		Utf8ToChar16( nintendoIconText,
			sizeof( nintendoIconText ) / sizeof( nintendoIconText[ 0 ] ),
			preview.nintendo.text.c_str() );
		// Rso15 is the first downloaded-story loop and only animates slot 00.
		// Do not fill the other two authored slots: their base/text-background
		// panes are visible in the raw BRLYT but have no Rso15 keys, so leaving
		// them live stacks three identical ticker strips over the first story.
		SetGeneratedTextPane( layout_icon, "T_scroll_00", nintendoIconText );
		SetNamedPaneVisible( layout_icon, "N_base_01", false );
		SetNamedPaneVisible( layout_icon, "N_base_02", false );
		if( !preview.nintendo.imagePath.empty()
			&& !iconImageOverride.Apply( layout_icon,
				ChannelImageOverride::NintendoIcon,
				preview.nintendo.imagePath, preview.nintendo.fitMode ) )
		{
			gprintf( "Nintendo icon image skipped: %s\n",
				iconImageOverride.LastError().c_str() );
		}
		gprintf( "Nintendo icon: native downloaded-story panes enabled\n" );
	}
	const bool miiContestIcon = IsMiiContestTitle( titleId )
		|| IsMiiContestLayout( layout_icon );
	const bool miiChannelIcon = IsMiiChannelTitle( titleId )
		|| ( layout_icon->FindPane( "Picture_00" )
			&& layout_icon->FindPane( "Picture_14" )
			&& theArc.GetFileExact( "/arc/timg/sk_face00.tpl" ) );
	const bool wiiFitIcon = IsWiiFitTitle( titleId )
		|| IsWiiFitIconLayout( layout_icon );
	if( miiContestIcon && preview.miiContest.enabled
		&& !preview.miiContest.imagePath.empty() )
	{
		iconCustomImageAttempted = true;
		if( !iconImageOverride.Apply( layout_icon,
			ChannelImageOverride::MiiContestIcon,
			preview.miiContest.imagePath, preview.miiContest.fitMode ) )
			gprintf( "Mii Contest icon image skipped: %s\n",
				iconImageOverride.LastError().c_str() );
	}
	else if( miiChannelIcon && preview.miiChannel.enabled
		&& !preview.miiChannel.imagePath.empty() )
	{
		iconCustomImageAttempted = true;
		if( !iconImageOverride.Apply( layout_icon,
			ChannelImageOverride::MiiChannelIcon,
			preview.miiChannel.imagePath, preview.miiChannel.fitMode ) )
			gprintf( "Mii Channel icon image skipped: %s\n",
				iconImageOverride.LastError().c_str() );
	}
	else if( wiiFitIcon && preview.wiiFit.enabled )
	{
		iconCustomImageAttempted = true;
		if( !iconImageOverride.ApplyWiiFitMiiCapture( layout_icon,
			ChannelImageOverride::WiiFitIcon,
			MiiProfiles::IndexOf( preview.wiiFit.profile ) ) )
			gprintf( "Wii Fit icon portrait skipped: %s\n",
				iconImageOverride.LastError().c_str() );
		else
			appliedWiiFitIconProfile = MiiProfiles::IndexOf(
				preview.wiiFit.profile );
	}
	float everybodyVotesLoopEnd = 1584.0f;
	const bool everybodyVotesIcon = ConfigureEverybodyVotesIcon( layout_icon,
		everybodyVotesText, everybodyVotesLoopEnd );
	const bool newsIconLayout = layout_icon->FindPane( "send_id" )
		&& layout_icon->FindPane( "telop0" );
	const bool wiiShopIcon = IsWiiShopIconLayout( layout_icon, theArc );

	// create object
	iconObj = new Object;

	iconObj->BindPane( layout_icon->FindPane( "RootPane" ) );
	iconObj->BindMaterials( layout_icon->Materials() );

	// Lowercase icon_start can be a tiny state initializer. Rso0 is considered
	// only as the fallback loop, never as an opener and never together with
	// Rso1+.
	static const char *startNames[] = { "icon_Start", "icon_In", "icon_in" };
	static const char *loopNames[] = {
		"icon", "Icon", "icon_Loop", "Icon_Loop", "icon_loop"
	};
	const bool haveRso0 = theArc.GetFileExact( "/arc/anim/icon_Rso0.brlan" ) != NULL;
	const bool haveRso1 = theArc.GetFileExact( "/arc/anim/icon_Rso1.brlan" ) != NULL;
	const bool haveRso2 = theArc.GetFileExact( "/arc/anim/icon_Rso2.brlan" ) != NULL;
	const bool haveRso15 = theArc.GetFileExact( "/arc/anim/icon_Rso15.brlan" ) != NULL;
	const bool advertisementIconLayout =
		layout_icon->FindPane( "txt_1" )
		&& layout_icon->FindPane( "txt_2" )
		&& layout_icon->FindPane( "txt_3" )
		&& layout_icon->FindPane( "txt_4_eng" )
		&& theArc.GetFileExact( "/arc/anim/icon_start.brlan" )
		&& haveRso0;
	std::string brlanName;
	Animation *anim = NULL;
	if( wiiFitIcon && preview.wiiFit.enabled && haveRso1 )
	{
		// The 31-frame Rso1 transition moves the Wii Fit title up before the
		// selected profile's message carrier begins scrolling.
		brlanName = "icon_Rso1";
		anim = LoadAnimation( theArc, brlanName );
	}
	// icon_Start is News's no-data idle. A successful CHANS load replaces it
	// immediately with Rso1, so do not add a six-second blank delay here.
	for( u32 i = 0; !newsIconLayout && !anim
		&& i < sizeof( startNames ) / sizeof( startNames[ 0 ] ); ++i )
	{
		brlanName = startNames[ i ];
		anim = LoadAnimation( theArc, brlanName );
	}
	if( !newsIconLayout && !anim )
	{
		brlanName = "icon_start";
		anim = LoadAnimation( theArc, brlanName );
	}

	// we have a starting animation
	if( anim )
	{
		layout_icon->LoadBrlanTpls( anim, theArc );
		iconBrlans[ brlanName ] = anim;
		iconObj->AddAnimation( anim );
		iconObj->SetAnimation( brlanName, 0, -1, -1, false );
		iconObj->Start();
		random = false;
	}

	anim = NULL;
	if( wiiFitIcon && haveRso2 )
	{
		// Rso2 is the stock 230-frame Wii Fit title loop. Custom data uses the
		// authored Rso0 message carrier after Rso1 instead.
		brlanName = preview.wiiFit.enabled && haveRso0
			? "icon_Rso0" : "icon_Rso2";
		anim = LoadAnimation( theArc, brlanName );
	}
	else if( customPhotoIcon && haveRso1 )
	{
		// Select the inserted-photo state before any generic icon loop.
		brlanName = "icon_Rso1";
		anim = LoadAnimation( theArc, brlanName );
	}
	else if( nintendoArticleIcon && haveRso15 )
	{
		// Rso15 is Nintendo's authored self-looping first downloaded story: it
		// reveals P_scShot_00 and scrolls T_scroll_00 under the logo reflection.
		brlanName = "icon_Rso15";
		anim = LoadAnimation( theArc, brlanName );
	}
	else if( wiiShopIcon && haveRso0 )
	{
		// Select the authored local Shop state before any regional generic loop.
		brlanName = "icon_Rso0";
		anim = LoadAnimation( theArc, brlanName );
	}
	for( u32 i = 0; !anim && i < sizeof( loopNames ) / sizeof( loopNames[ 0 ] ); ++i )
	{
		brlanName = loopNames[ i ];
		anim = LoadAnimation( theArc, brlanName );
	}
	if( !anim && !customPhotoIcon && haveRso0 )
	{
		brlanName = "icon_Rso0";
		anim = LoadAnimation( theArc, brlanName );
	}
	else if( !anim && newsIconLayout && haveRso1 )
	{
		// News has no generic icon loop or Rso0.  Rso1 is its live ticker;
		// Rso2 is an exit transition and must not be chained here.
		brlanName = "icon_Rso1";
		anim = LoadAnimation( theArc, brlanName );
	}

	// we have a loop animation
	if( anim )
	{
		layout_icon->LoadBrlanTpls( anim, theArc );
		iconBrlans[ brlanName ] = anim;
		iconObj->AddAnimation( anim );
		if( customPhotoIcon && brlanName == "icon_Rso1" )
		{
			// Like the banner, the icon's Rso1 fades pic_photo from zero during
			// frames 0..40. Keep its authored intro and later logo dimming, but
			// never revisit the invisible opening when the 800-frame loop wraps.
			const float end = anim->FrameCount() - 1.0f;
			iconObj->ScheduleAnimation( brlanName, 0.0f, end, -1.0f, false );
			iconObj->ScheduleAnimation( brlanName, 40.0f, end, -1.0f, true );
			random = false;
		}
		else if( wiiFitIcon && preview.wiiFit.enabled
			&& brlanName == "icon_Rso0" )
		{
			// RFNP reserves 9800 frames for as many as eight save-data rows. WSM's
			// content-sized window is at most 300 pixels wide and clears the icon by
			// roughly frame 440. Restart shortly afterward rather than leaving a long
			// blank wait based on the original 1110-pixel CHANS speech window.
			const float end = std::min( 460.0f,
				std::max( 0.0f, anim->FrameCount() - 1.0f ) );
			iconObj->ScheduleAnimation( brlanName, 0.0f, end, -1.0f, true );
			random = false;
		}
		else if( newsIconLayout && brlanName == "icon_Rso1" )
		{
			// Match the dynamic News module's content-sized carrier instead of
			// leaving a ten-minute blank tail from the authored 36,000 frames.
			const u32 headlineCount = LoadNewsGeneratedText( generatedText );
			iconObj->ScheduleAnimation( brlanName, 0.0f,
				NewsIconLoopEnd( headlineCount ), -1.0f, true );
			random = false;
		}
		else if( wiiShopIcon && brlanName == "icon_Rso0" )
		{
			// Rso0 declares 5000 frames, but authored content ends at 637.
			// Frames 638..4999 leave only iconBg and look like a white-out.
			const float authoredEnd = anim->FrameCount() - 1.0f;
			const float end = authoredEnd < 637.0f ? authoredEnd : 637.0f;
			iconObj->ScheduleAnimation( brlanName, 0.0f, end, -1.0f, true );
			random = false;
		}
		else if( everybodyVotesIcon )
		{
			// ConfigureEverybodyVotesIcon derives this from the last pane's real
			// position and width.  The repaired RLAN exposes the carrier keys through
			// frame 3050, so every bubble now clears before the loop wraps.
			iconObj->ScheduleAnimation( brlanName, 0.0f,
				everybodyVotesLoopEnd, -1.0f, true );
			random = false;
		}
		else if(layout_icon->FindPane("N_message_00") && layout_icon->FindPane("W_vote_00"))
		{
			// Original content also starts with the logo intro, never mid-bubble.
			iconObj->ScheduleAnimation(brlanName,0.0f,anim->FrameCount()-1.0f,-1.0f,true);
			random=false;
		}
		else if( advertisementIconLayout && brlanName == "icon_Rso0" )
		{
			// The header advertises only 48 frames, but this data-driven sequence
			// has real keys through frame 2084. Animation::Load exposes that full
			// range; loop it as one slow ~35-second presentation instead of
			// restarting its first 0.8 seconds or stopping on the blank last state.
			const float end = anim->FrameCount() - 1.0f;
			iconObj->ScheduleAnimation( brlanName, 0.0f, end, -1.0f, true );
			random = false;
		}
		else
		{
			iconObj->ScheduleAnimation( brlanName );
		}

		if( random )
		{
			iconObj->SetFrame( ((u32)rand()) % ((u32)anim->FrameCount()) );
		}

		iconObj->Start();
	}
	if(IsNintendoChannelIconLayout(layout_icon,theArc)) {
		const char *states[]={"icon_Rso0","icon_Rso15","icon_Rso2"};
		for(unsigned i=0;i<3;++i) if(iconBrlans.find(states[i])==iconBrlans.end()) {
			Animation *state=LoadAnimation(theArc,states[i]);
			if(!state)continue;
			layout_icon->LoadBrlanTpls(state,theArc);
			iconBrlans[states[i]]=state;iconObj->AddAnimation(state);
		}
	}
	// This is still the loader thread and iconReady is deliberately false, so
	// apply generated content directly before publishing the completed layout.
	ApplyGeneratedChannelIcon();
	appliedPreviewRevision = ChannelPreview::Revision();
	// Publish only after every pane, material, external image, animation and
	// generated Forecast/News field is complete. The PPC barrier pairs this
	// handoff with the polling grid thread.
	__sync_synchronize();
	iconReady = true;
	return iconObj;
}

#include "SystemMenu/channelarttranslations.h"
Layout *Banner::LoadLayout( const U8Archive &theArc, const std::string &lytName )
{
	// read layout data
	u8 *stuff = theArc.GetFile( "/arc/blyt/" + lytName + ".brlyt" );
	if( !stuff )
	{
		return NULL;
	}

	// load layout
	Layout *ret = new Layout;
	if( !ret->Load( stuff ) )
	{
		delete ret;
		return NULL;
	}
	ret->SetFitTextboxToPane( lytName == "icon" );
	ret->SetLanguage( CONF_GetLanguageString() );
	// Identify the original title, not just a pane name shared by other titles.
	// Keep the deliberate two-line icon title (Internet-\nKanal).
	if( IsInternetTitleLayout(ret) )
		CorrectInternetTitleSpacing(ret->Panes());
	// Regional Forecast archives without a JPN group retain the international
	// geometry; their real text panes can still display Japanese/localized text.
	LocalizeAuthoredChannelPanes( ret->Panes() );

	// load fonts and textures
	//ret->LoadFonts( U8Archive(SystemFont::GetFont(), SystemFont::GetFontSize()) );
	ret->LoadFonts( theArc );
	if( !ret->LoadTextures( theArc ) )
	{
		delete ret;
		return NULL;
	}

	// Shared P_logoE panes also occur in Wii U/instruction channel templates.
	// Read the stable English IMET identity here: titleId may not have been
	// assigned yet by BannerAsync's caller. An unknown raw APP is not Nintendo.
	bool nintendoChannel = false;
	if( imetHeader )
	{
		char16 name[17];
		memcpy( name, ((IMET*)imetHeader)->names[1].title, sizeof(name) );
		const char *expected = "Nintendo Channel";
		size_t i = 0;
		while( expected[i] && name[i] == (char16)expected[i] ) ++i;
		nintendoChannel = !expected[i] && !name[i];
	}
	ChannelArtTranslations::Apply(ret,lytName=="icon",nintendoChannel);
	return ret;
}

Animation *Banner::LoadAnimation( const U8Archive &theArc, const std::string &lanName )
{
	u8 *stuff = theArc.GetFileExact( "/arc/anim/" + lanName + ".brlan" );
	if( !stuff )
		return NULL;
	Animation *brlan = new Animation( lanName );
	brlan->Load( (const RLAN_Header*)stuff );
	return brlan;
}

void Banner::AdvanceBanner()
{
	// Count only frames displayed fully open, not loading/zooming or HOME.
	if (bannerFullyOpened && bannerInfoFrames < 0xffffffffU)
		++bannerInfoFrames;
	EnsureGeneratedChannelDataCurrent();
	if( bannerObj )
		bannerObj->Advance();

	if( !marioKartCarouselObj[ 0 ] || !marioKartCarouselObj[ 1 ] )
		return;
	if( !marioKartCarouselStarted )
	{
		if( marioKartCarouselDelay > 0.0f )
		{
			marioKartCarouselDelay -= 1.0f;
			return;
		}
		// Schedule only after the 401-frame intro. Scheduling constructs an
		// AnimationLink and immediately applies Rso frame zero; doing that during
		// LoadBanner() steals these carriers from banner_start and shifts both kart
		// rows by 228 pixels throughout the entrance.
		marioKartCarouselObj[ 0 ]->ScheduleAnimation( "banner_Rso4",
			0.0f, marioKartCarouselLoopEnd, -1.0f, true );
		marioKartCarouselObj[ 1 ]->ScheduleAnimation( "banner_Rso5",
			0.0f, marioKartCarouselLoopEnd, -1.0f, true );
		marioKartCarouselObj[ 0 ]->Start();
		marioKartCarouselObj[ 1 ]->Start();
		marioKartCarouselStarted = true;
	}
	marioKartCarouselObj[ 0 ]->Advance();
	marioKartCarouselObj[ 1 ]->Advance();
}

void Banner::AdvanceIcon()
{
	Object *object = LoadIcon();
	if( !object )
		return;
	object->Advance();
	if(nintendoStoryCycle && object->IsFinished()) {
		const ChannelPreview::NintendoConfig &news=ChannelPreview::Get().nintendo;
		for(int attempt=0;attempt<3;++attempt) {
			nintendoStoryIndex=(nintendoStoryIndex+1)%3;
			if(nintendoStoryIndex==0 || !news.extraText[nintendoStoryIndex-1].empty()) break;
		}
		StartNintendoStory();
	}
	if(nintendoStoryCycle)
		SetNamedPaneVisible(layout_icon,"N_chIcon_00",false);
	if(layout_icon->FindPane("N_message_00") && layout_icon->FindPane("W_vote_00"))
		layout_icon->FindPane("N_message_00")->SetHide(object->GetFrame()<350.0f);
	// News/Forecast animations contain the original no-data visibility keys.
	// Apply live content after the frame, immediately before ChannelGrid renders.
	if( IsForecastTitle( titleId ) || IsNewsTitle( titleId )
		|| IsTodayTomorrowTitle( titleId ) || IsMiiContestTitle( titleId )
		|| IsWiiFitTitle( titleId )
		|| IsForecastLayout( layout_icon ) || IsNewsLayout( layout_icon )
		|| IsTodayTomorrowLayout( layout_icon )
		|| IsMiiContestLayout( layout_icon )
		|| IsWiiFitIconLayout( layout_icon ) )
		ApplyGeneratedChannelIcon();
}

void Banner::UnloadBanner()
{
	BeginBannerPresentation();
	forecastBannerImagePath.clear();
	DELETE( marioKartCarouselObj[ 0 ] );
	DELETE( marioKartCarouselObj[ 1 ] );
	marioKartCarouselDelay = 0.0f;
	marioKartCarouselLoopEnd = 0.0f;
	marioKartCarouselStarted = false;
	DELETE( bannerObj );
	DELETE( layout_banner );
	bannerImageOverride.Reset();
	FREE( banner_bin );
	std::map< std::string, Animation *>:: iterator it = bannerBrlans.begin(), itE = bannerBrlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
	bannerBrlans.clear();
	// The inserted-image texture is owned outside this renderer. Never carry a
	// successful state into a later banner instance after that texture is gone.
	hasInsertedPhotoImage = false;
	// Icon textboxes can point into generatedText while the large banner is
	// unloaded.  Keep the stable backing store alive until this Banner dies.
}

void Banner::UnloadIcon()
{
	nintendoStoryCycle=false;
	forecastIconImagePath.clear();
	iconReady = false;
	__sync_synchronize();
	DELETE( iconObj );
	DELETE( layout_icon );
	iconImageOverride.Reset();
	appliedWiiFitIconProfile = -1;
	FREE( icon_bin );
	std::map< std::string, Animation *>:: iterator it = iconBrlans.begin(), itE = iconBrlans.end();
	while( it != itE )
	{
		delete it->second;
		++it;
	}
	iconBrlans.clear();
	iconCustomImageAttempted = false;
}

void Banner::StartNintendoStory()
{
	const ChannelPreview::NintendoConfig &news=ChannelPreview::Get().nintendo;
	const std::string &message=nintendoStoryIndex==0 ? news.text : news.extraText[nintendoStoryIndex-1];
	Utf8ToChar16(nintendoIconText,260,message.c_str());
	SetGeneratedTextPane(layout_icon,"T_scroll_00",nintendoIconText);
	// Exit Rso2 is intentionally stopped BEFORE its final visibility-reset keys.
	// At the invisible boundary restore only its two parent transforms, then
	// play Rso15 from frame 1 (frame 0 is a one-frame initialization pose).
	const char *parents[]={"N_scShot_00","N_textBG_00"};
	for(int i=0;i<2;++i) if(Pane *p=layout_icon->FindPane(parents[i])) {
		p->SetPosition(0,0);p->SetAlpha(255);p->SetHide(false);p->SetVisible(true);
	}
	// Use actual glyph advances, including wide letters and non-Latin text.
	const float textWidth=std::max(layout_icon->MeasureTextbox("T_scroll_00"),
		LongestTextLine(nintendoIconText)*12.0f);
	const float end=std::min(5750.0f,std::max(460.0f,
		235.0f+(156.0f+textWidth)/1.115f));
	iconObj->SetAnimation("icon_Rso15",1.0f,end,-1.0f,false);
	iconObj->SetFrame(1.0f);
	iconObj->ScheduleAnimation("icon_Rso2",0.0f,148.0f,-1.0f,false);
	iconObj->Start();
}

void Banner::UnloadSound()
{
	FREE( sound_bin );
	sound_bin_size = 0;
}

bool Banner::LoadSound()
{
	if(!arc)
		return false;

	if(sound_bin)
		free(sound_bin);

	sound_bin = arc->GetFileAllocated("/meta/sound.bin", &sound_bin_size);
	if(!sound_bin) {
		return false;
	}
	return true;
}

const char16 *Banner::GetTitle() const
{
	if( !imetHeader )
	{
		return NULL;
	}

	IMET *imet = (IMET*)imetHeader;
	int lang = Localization::NativeLanguage();

	if( lang < 0 || lang > 9 || !imet->names[ lang ].title[ 0 ] )
	{
		lang = 1;
	}
	return imet->names[ lang ].title;
}

const char16 *Banner::GetSubTitle() const
{
	if( !imetHeader )
	{
		return NULL;
	}

	IMET *imet = (IMET*)imetHeader;
	int lang = Localization::NativeLanguage();
	if( lang < 0 || lang > 9 || !imet->names[ lang ].subtitle[ 0 ] )
	{
		lang = 1;
	}
	return imet->names[ lang ].subtitle;
}
