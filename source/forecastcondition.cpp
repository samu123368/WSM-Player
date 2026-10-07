#include "channelpreviewconfig.h"
#include <string>

namespace
{
	// Locale-independent UTF-8 folding: C's tolower(char) is undefined for
	// negative UTF-8 bytes and only handled English on the Wii's C locale.
	std::string Fold( const std::string &input )
	{
		std::string out;
		for( size_t i = 0; i < input.size(); )
		{
			u32 c = (u8)input[i++];
			int more = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1
				: (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
			if( more < 0 || i + more > input.size() ) continue;
			if( more ) c &= (1u << (6 - more)) - 1;
			bool valid = true;
			for( int j = 0; j < more; ++j )
			{
				const u8 next = (u8)input[i++];
				if( (next & 0xc0) != 0x80 ) valid = false;
				c = (c << 6) | (next & 0x3f);
			}
			if( !valid ) continue;
			if( (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)
				|| (c >= 0x410 && c <= 0x42f) || (c >= 0x391 && c <= 0x3ab && c != 0x3a2) ) c += 32;
			else if( c >= 0x400 && c <= 0x40f ) c += 80;
			else if( c >= 0x531 && c <= 0x556 ) c += 48;
			else if( c >= 0x1c90 && c <= 0x1cbf ) c -= 0xbc0;
			else if( ((c >= 0x100 && c <= 0x12f) || (c >= 0x132 && c <= 0x137)
				|| (c >= 0x14a && c <= 0x177)) && !(c & 1) ) ++c;
			else if( ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e)) && (c & 1) ) ++c;
			if( c == 0x130 ) c = 'i';
			if( c < 0x80 ) out += (char)c;
			else if( c < 0x800 ) { out += (char)(0xc0 | (c >> 6)); out += (char)(0x80 | (c & 63)); }
			else if( c < 0x10000 ) { out += (char)(0xe0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 63)); out += (char)(0x80 | (c & 63)); }
		}
		return out;
	}
	bool Has( const std::string &text, const char *words )
	{
		const std::string list(words);
		for( size_t start = 0; start < list.size(); )
		{
			const size_t end = list.find('|', start);
			if( text.find(list.substr(start, end == std::string::npos ? end : end-start)) != std::string::npos ) return true;
			if( end == std::string::npos ) break;
			start = end + 1;
		}
		return false;
	}
}

u16 ChannelPreview::ForecastWeatherCodeFromCondition( const std::string &condition )
{
	const std::string text = Fold(condition);
	const bool partly = Has(text, "partly|partial|partiel|teilweise|teils|parzial|parcial|gedeelt|delvis|osittain|części|částeč|čiastoč|részben|parțial|μερικ|переменн|мінлив|променлив|djelomi|delno|pjesërisht|osaliselt|daļēji|dalinai|晴れ時々|晴れ一時|晴時|晴时|구름 조금");
	const bool mostly = Has(text, "mostly|mainly|überwiegend|meist|prevalent|mayormente|grotendeels|mestadels|pääosin|przeważ|převáž|többnyire|преимущ");
	const bool cloud = Has(text, "cloud|overcast|wolk|bewölk|bedeckt|nuage|couvert|nuvem|nublado|nuvol|nube|nubos|bewolk|moln|skyet|skyer|pilvi|skýja|chmur|zachmur|oblač|zamračen|felh|noros|nori|συννεφ|νεφελ|облач|хмар|облак|vranët|pilv|mākoņ|debes|scamall|cymylog|sħab|wolle|núvol|hodei|bulud|bulut|ամպ|ღრუბ|曇|云|雲|흐림|구름");
	const bool sunny = Has(text, "sun|clear|sonn|heiter|soleil|dégagé|soleado|soalheiro|ensolar|solegg|sereno|zonn|solig|solskin|solrik|aurink|sól|słonecz|sluneč|slneč|napos|senin|ηλιο|αίθρι|солнеч|ясно|соняч|слънч|sunčan|sončno|diell|päik|saul|grian|heulog|xemx|sonneg|assolell|eguzki|güneş|günəş|արև|მზიანი|晴|맑음");
	const bool rain = Has(text, "rain|drizzle|regen|pluie|bruine|lluvia|lloviz|chuva|piogg|piov|regn|vesisade|satein|rigning|deszcz|déšť|dešť|dážď|dažď|eső|ploaie|βροχ|дожд|дощ|дъжд|kiš|dež|shi me|reshje|vihm|lietus|báiste|glaw|xita|reesch|pluja|euri|chuvia|yağmur|yağış|անձրև|წვიმ|雨|비");
	const bool snow = Has(text, "snow|flurr|schnee|neige|nieve|neve|sneeuw|snö|sne|snø|lumi|lumis|snjór|śnieg|sníh|sněž|sneh|havaz|ninsoare|zăpad|χιόν|снег|сніг|сняг|snijeg|borë|lumi|snieg|sneachta|eira|borra|schnéi|neu|elur|neve|kar yağ|qar|ձյուն|თოვლ|雪|눈");
	if( Has(text,"sleet|ice pellet|graupel|grésil|aguanieve|nevischio|ijzel|sludd|räntä|lapovi|суснеж|мокрый снег|みぞれ|雨夹雪|진눈깨비") ) return ForecastWeatherSleet;
	if( rain && snow ) return ForecastWeatherRainAndSnow;
	if( text == "shi" || text == "sade" ) return ForecastWeatherRain;
	if( Has(text,"thunder|storm|gewitter|orage|tormenta|trovoada|temporale|onweer|åska|torden|ukkos|þruma|burza|bouř|búrk|zivatar|furtun|καταιγ|гроза|гроз|гръмот|grmljav|neviht|bubull|äike|pērkon|perkūn|toirneach|taran|ragħad|donner|tempesta|trumoi|troada|gök gür|şimşək|ամպրոպ|ჭექა|雷|뇌우") )
		return partly || (sunny && cloud) ? ForecastWeatherPartlySunnyThunder
			: cloud ? ForecastWeatherMostlyCloudyThunder : ForecastWeatherThunder;
	if( snow ) return partly ? ForecastWeatherPartlySunnyFlurries : ForecastWeatherSnow;
	if( Has(text,"shower|schauer|averse|chubasco|rovesci|bui|skur|kuuro|przelot|přeháň|prehán|zápor|ливень|злив|にわか雨|소나기") )
		return partly || (sunny && cloud) ? ForecastWeatherPartlySunnyRain
			: cloud ? ForecastWeatherMostlyCloudyShowers : ForecastWeatherShowers;
	if( rain ) return partly ? ForecastWeatherPartlySunnyRain : cloud ? ForecastWeatherMostlyCloudyShowers : ForecastWeatherRain;
	if( Has(text,"fog|mist|haze|nebel|brouillard|brume|niebla|nevoeiro|nebbia|dimma|tåge|tåke|sumu|þoka|mgła|mlha|hmla|köd|ceață|ομίχλ|туман|мъгла|magla|megla|mjegull|udu|migla|rūkas|ceo|niwl|ċpar|niwwel|boira|laino|néboa|sis|duman|մառախուղ|ნისლ|霧|雾|안개") ) return ForecastWeatherFog;
	if( partly && cloud ) return ForecastWeatherPartlyCloudy;
	if( mostly && sunny ) return ForecastWeatherSunny;
	if( mostly && cloud ) return ForecastWeatherMostlyCloudy;
	if( cloud ) return ForecastWeatherCloudy;
	return partly ? ForecastWeatherPartlySunny : ForecastWeatherSunny;
}
