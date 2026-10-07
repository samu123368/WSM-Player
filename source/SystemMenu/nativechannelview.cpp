#include "nativechannelview.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <zlib.h>

#include "ModernWSM/TinyFont.h"
#include "utils/sc.h"
#include "video.h"

extern const u8 fake_news_bin[];
extern const u32 fake_news_bin_size;
extern const u8 fake_forecast_bin[];
extern const u32 fake_forecast_bin_size;
extern const u8 fake_short_forecast_bin[];
extern const u32 fake_short_forecast_bin_size;

namespace
{
    u16 ReadBE16( const u8 *data )
    {
        return ( (u16)data[ 0 ] << 8 ) | data[ 1 ];
    }

    u32 ReadBE32( const u8 *data )
    {
        return ( (u32)data[ 0 ] << 24 ) | ( (u32)data[ 1 ] << 16 )
            | ( (u32)data[ 2 ] << 8 ) | data[ 3 ];
    }

    bool InRange( u32 offset, u32 length, u32 size )
    {
        return offset <= size && length <= size - offset;
    }

    std::string ReadUtf16( const u8 *data, u32 size, u32 offset, u32 length,
                           bool terminated )
    {
        std::string result;
        if( !data || offset > size || ( !terminated && !InRange( offset, length, size ) ) )
            return result;

        const u32 end = terminated ? size : offset + length;
        for( u32 pos = offset; pos + 1 < end; pos += 2 )
        {
            const u16 ch = ReadBE16( data + pos );
            if( terminated && !ch )
                break;
            if( !ch )
                continue;
            result += ch >= 32 && ch <= 126 ? (char)ch : '?';
        }
        return result;
    }

    bool ValidateChannelFile( const u8 *data, u32 available, u32 version,
                              u32 minimum, std::string &error )
    {
        if( !data || available < minimum )
        {
            error = "DATA FILE IS TOO SMALL";
            return false;
        }
        if( ReadBE32( data ) != version )
        {
            error = "UNSUPPORTED DATA VERSION";
            return false;
        }
        const u32 declared = ReadBE32( data + 4 );
        if( declared < minimum || declared > available )
        {
            error = "INVALID DATA FILE SIZE";
            return false;
        }
        const u32 storedCrc = ReadBE32( data + 8 );
        uLong crc = crc32( 0L, Z_NULL, 0 );
        crc = crc32( crc, data + 12, declared - 12 );
        if( storedCrc != (u32)crc )
        {
            error = "DATA CRC CHECK FAILED";
            return false;
        }
        return true;
    }

    const char *WindName( int direction )
    {
        static const char *names[] = {
            "CALM", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S",
            "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "N"
        };
        return direction >= 0 && direction < 17 ? names[ direction ] : "?";
    }
}

NativeChannelView::NativeChannelView( Kind k )
    : kind( k ), page( 0 ), dataValid( false ), forecastCurrentTemperature( 0 ),
      forecastCurrentWindDirection( 0 ), forecastCurrentWindSpeed( 0 )
{
}

NativeChannelView::~NativeChannelView()
{
}

bool NativeChannelView::Load()
{
    page = 0;
    dataError.clear();
    forecastDays.clear();
    newsArticles.clear();
    forecastCurrentCondition.clear();
    forecastCurrentTemperature = 0;
    forecastCurrentWindDirection = 0;
    forecastCurrentWindSpeed = 0;

    if( kind == Forecast )
    {
        dataValid = LoadForecastData( fake_forecast_bin, fake_forecast_bin_size );
        if( dataValid )
            dataValid = LoadShortForecastData( fake_short_forecast_bin,
                                               fake_short_forecast_bin_size );
    }
    else
    {
        dataValid = LoadNewsData( fake_news_bin, fake_news_bin_size );
    }
    gprintf( "NativeChannelView: %s fake data %s (%s)\n",
        kind == Forecast ? "Forecast" : "News", dataValid ? "loaded" : "failed",
        dataValid ? "CRC OK" : dataError.c_str() );
    return true;
}

void NativeChannelView::Regenerate()
{
    page = 0;
}

void NativeChannelView::NextPage()
{
    const size_t count = kind == Forecast ? forecastDays.size() : newsArticles.size();
    if( count ) page = ( page + 1 ) % count;
}

void NativeChannelView::PreviousPage()
{
    const size_t count = kind == Forecast ? forecastDays.size() : newsArticles.size();
    if( count ) page = ( page + count - 1 ) % count;
}

bool NativeChannelView::LoadForecastData( const u8 *data, u32 size )
{
    if( !ValidateChannelFile( data, size, 0, 88, dataError ) )
        return false;
    size = ReadBE32( data + 4 );

    const u32 longCount = ReadBE32( data + 32 );
    const u32 longOffset = ReadBE32( data + 36 );
    const u32 weatherCount = ReadBE32( data + 48 );
    const u32 weatherOffset = ReadBE32( data + 52 );
    const u32 locationCount = ReadBE32( data + 80 );
    const u32 locationOffset = ReadBE32( data + 84 );
    if( !longCount || !locationCount || !InRange( longOffset, 128, size )
        || !InRange( locationOffset, 24, size )
        || !InRange( weatherOffset, weatherCount * 8, size ) )
    {
        dataError = "FORECAST TABLE IS MISSING";
        return false;
    }

    const u8 *location = data + locationOffset;
    forecastLocation = ReadUtf16( data, size, ReadBE32( location + 4 ), 0, true );
    if( forecastLocation.empty() ) forecastLocation = "UNKNOWN LOCATION";

    const u8 *record = data + longOffset;
    const auto conditionFor = [data, size, weatherOffset, weatherCount]( u16 code )
    {
        for( u32 i = 0; i < weatherCount; ++i )
        {
            const u8 *entry = data + weatherOffset + i * 8;
            if( ReadBE16( entry ) == code || ReadBE16( entry + 2 ) == code )
            {
                std::string value = ReadUtf16( data, size, ReadBE32( entry + 4 ), 0, true );
                if( !value.empty() ) return value;
            }
        }
        return std::string( "UNCLASSIFIED WEATHER" );
    };

    ForecastDay today;
    today.label = "TODAY";
    today.condition = conditionFor( ReadBE16( record + 16 ) );
    today.high = (s8)record[ 26 ];
    today.low = (s8)record[ 28 ];
    today.rain = record[ 34 ];
    today.windDirection = record[ 38 ];
    today.windSpeed = record[ 39 ];
    forecastDays.push_back( today );

    ForecastDay tomorrow;
    tomorrow.label = "TOMORROW";
    tomorrow.condition = conditionFor( ReadBE16( record + 44 ) );
    tomorrow.high = (s8)record[ 54 ];
    tomorrow.low = (s8)record[ 56 ];
    tomorrow.rain = record[ 62 ];
    tomorrow.windDirection = record[ 66 ];
    tomorrow.windSpeed = record[ 67 ];
    forecastDays.push_back( tomorrow );

    for( int i = 0; i < 7; ++i )
    {
        const u8 *daily = record + 72 + i * 8;
        ForecastDay day;
        char label[ 16 ];
        snprintf( label, sizeof( label ), "DAY %d", i + 3 );
        day.label = label;
        day.condition = conditionFor( ReadBE16( daily ) );
        day.high = (s8)daily[ 2 ];
        day.low = (s8)daily[ 3 ];
        day.rain = daily[ 6 ];
        day.windDirection = 0;
        day.windSpeed = 0;
        forecastDays.push_back( day );
    }
    return !forecastDays.empty();
}

bool NativeChannelView::LoadShortForecastData( const u8 *data, u32 size )
{
    if( !ValidateChannelFile( data, size, 0, 36, dataError ) )
        return false;
    size = ReadBE32( data + 4 );
    const u32 count = ReadBE32( data + 28 );
    const u32 offset = ReadBE32( data + 32 );
    if( !count || !InRange( offset, 24, size ) )
    {
        dataError = "CURRENT WEATHER TABLE IS MISSING";
        return false;
    }

    const u8 *record = data + offset;
    forecastCurrentTemperature = (s8)record[ 15 ];
    forecastCurrentWindDirection = record[ 17 ];
    forecastCurrentWindSpeed = record[ 18 ];
    // forecast.bin owns the condition-code text table; this fixture uses the
    // same code as its first daily entry, exactly as the retail pair does.
    forecastCurrentCondition = forecastDays.empty()
        ? "CURRENT CONDITIONS" : forecastDays[ 0 ].condition;
    return true;
}

bool NativeChannelView::LoadNewsData( const u8 *data, u32 size )
{
    if( !ValidateChannelFile( data, size, 512, 104, dataError ) )
        return false;
    size = ReadBE32( data + 4 );

    const u32 articleCount = ReadBE32( data + 60 );
    const u32 articleOffset = ReadBE32( data + 64 );
    const u32 sourceCount = ReadBE32( data + 68 );
    const u32 sourceOffset = ReadBE32( data + 72 );
    const u32 locationCount = ReadBE32( data + 76 );
    const u32 locationOffset = ReadBE32( data + 80 );
    if( !articleCount || !InRange( articleOffset, articleCount * 44, size ) )
    {
        dataError = "NEWS ARTICLE TABLE IS MISSING";
        return false;
    }

    std::string sourceName = "MII NEWS WIRE";
    if( sourceCount && InRange( sourceOffset, 28, size ) )
    {
        const u8 *source = data + sourceOffset;
        const u32 nameSize = ReadBE32( source + 12 );
        const u32 nameOffset = ReadBE32( source + 16 );
        std::string parsed = ReadUtf16( data, size, nameOffset, nameSize, false );
        if( !parsed.empty() ) sourceName = parsed;
    }

    std::string locationName = "THE GLOBE";
    if( locationCount && InRange( locationOffset, 16, size ) )
    {
        std::string parsed = ReadUtf16( data, size, ReadBE32( data + locationOffset ), 0, true );
        if( !parsed.empty() ) locationName = parsed;
    }

    for( u32 i = 0; i < articleCount; ++i )
    {
        const u8 *record = data + articleOffset + i * 44;
        const u32 headlineSize = ReadBE32( record + 28 );
        const u32 headlineOffset = ReadBE32( record + 32 );
        const u32 bodySize = ReadBE32( record + 36 );
        const u32 bodyOffset = ReadBE32( record + 40 );
        if( !InRange( headlineOffset, headlineSize, size )
            || !InRange( bodyOffset, bodySize, size ) )
            continue;

        NewsArticle article;
        article.headline = ReadUtf16( data, size, headlineOffset, headlineSize, false );
        article.body = ReadUtf16( data, size, bodyOffset, bodySize, false );
        article.source = sourceName;
        article.location = locationName;
        if( !article.headline.empty() ) newsArticles.push_back( article );
    }
    if( newsArticles.empty() )
    {
        dataError = "NO VALID NEWS ARTICLES";
        return false;
    }
    return true;
}

const char *NativeChannelView::LocalizedTitle() const
{
    const char *lang = CONF_GetLanguageString();

    if( !strcmp( lang, "GER" ) ) return kind == Forecast ? "WETTERKANAL" : "NACHRICHTENKANAL";
    if( !strcmp( lang, "FRA" ) ) return kind == Forecast ? "CHAINE METEO" : "CHAINE INFOS";
    if( !strcmp( lang, "SPA" ) ) return kind == Forecast ? "CANAL TIEMPO" : "CANAL NOTICIAS";
    if( !strcmp( lang, "ITA" ) ) return kind == Forecast ? "CANALE METEO" : "CANALE NOTIZIE";
    if( !strcmp( lang, "NED" ) ) return kind == Forecast ? "WEERKANAAL" : "NIEUWSKANAAL";
    if( !strcmp( lang, "JPN" ) ) return kind == Forecast ? "TENKI CHANNEL" : "NEWS CHANNEL";
    if( !strcmp( lang, "KOR" ) ) return kind == Forecast ? "NALSSI CHANNEL" : "NEWS CHANNEL";
    if( !strcmp( lang, "CHN" ) ) return kind == Forecast ? "TIANQI CHANNEL" : "XINWEN CHANNEL";

    return kind == Forecast ? "FORECAST CHANNEL" : "NEWS CHANNEL";
}

void NativeChannelView::PrepareFlatGX()
{
    LoadMenuPositionMatrix(GXmodelView2D, GX_PNMTX0);
    GX_SetNumChans( 1 );
    GX_SetChanCtrl( GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
                    GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE );
    GX_SetNumTexGens( 0 );
    GX_SetNumTevStages( 1 );
    GX_SetNumIndStages( 0 );
    GX_SetTevOp( GX_TEVSTAGE0, GX_PASSCLR );
    GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0 );
    GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
    GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET );
    GX_ClearVtxDesc();
    GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
    GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
}

void NativeChannelView::DrawCentered( float x, float width, float y,
                                      const std::string &text, float scale,
                                      const GXColor &color )
{
    const float textWidth = ModernWSM::TinyFont::Width( text, scale );
    ModernWSM::TinyFont::Draw( x + std::max( 0.0f, ( width - textWidth ) * 0.5f ),
                               y, text, scale, color );
}

float NativeChannelView::DrawWrapped( float x, float y, float width,
                                      const std::string &text, float scale,
                                      const GXColor &color, size_t maxLines )
{
    std::string line;
    size_t lines = 0;
    size_t start = 0;
    while( start < text.size() && lines < maxLines )
    {
        while( start < text.size() && text[ start ] == ' ' ) ++start;
        size_t end = text.find( ' ', start );
        if( end == std::string::npos ) end = text.size();
        const std::string word = text.substr( start, end - start );
        const std::string candidate = line.empty() ? word : line + " " + word;
        if( !line.empty() && ModernWSM::TinyFont::Width( candidate, scale ) > width )
        {
            ModernWSM::TinyFont::Draw( x, y, line, scale, color );
            y += scale * 8.0f;
            ++lines;
            line = word;
        }
        else
        {
            line = candidate;
        }
        start = end + 1;
    }
    if( !line.empty() && lines < maxLines )
    {
        ModernWSM::TinyFont::Draw( x, y, line, scale, color );
        y += scale * 8.0f;
    }
    return y;
}

void NativeChannelView::RenderForecast( const Vec2f &screen ) const
{
    const ForecastDay &day = forecastDays[ page % forecastDays.size() ];
    const bool current = page == 0;
    const std::string &condition = current && !forecastCurrentCondition.empty()
        ? forecastCurrentCondition : day.condition;
    const GXColor navy = { 24, 70, 112, 255 };
    const GXColor blue = { 38, 161, 220, 255 };
    const GXColor pale = { 218, 246, 255, 255 };
    const GXColor white = { 255, 255, 255, 255 };
    const GXColor ink = { 35, 55, 72, 255 };
    const GXColor muted = { 85, 111, 128, 255 };

    DrawRect( 0, 0, screen.x, screen.y, pale );
    DrawRect( 0, 0, screen.x, 54, navy );
    ModernWSM::TinyFont::Draw( 22, 18, "FORECAST CHANNEL", 2.4f, white );
    DrawCentered( 320, 300, 19, forecastLocation, 1.7f, (GXColor){ 165, 231, 255, 255 } );

    DrawRect( 28, 72, 584, 244, white );
    DrawRect( 28, 72, 14, 244, blue );
    ModernWSM::TinyFont::Draw( 64, 94, day.label, 2.0f, blue );
    char temperature[ 32 ];
    snprintf( temperature, sizeof( temperature ), "%d C",
              current ? forecastCurrentTemperature : day.high );
    ModernWSM::TinyFont::Draw( 64, 132, temperature, 6.0f, navy );
    DrawWrapped( 264, 116, 316, condition, 2.0f, ink, 3 );

    char detail[ 96 ];
    snprintf( detail, sizeof( detail ), "HIGH %d C   LOW %d C", day.high, day.low );
    ModernWSM::TinyFont::Draw( 264, 202, detail, 1.8f, ink );
    snprintf( detail, sizeof( detail ), "RAIN %d PERCENT", day.rain );
    ModernWSM::TinyFont::Draw( 264, 232, detail, 1.6f, muted );
    const int windDirection = current ? forecastCurrentWindDirection : day.windDirection;
    const int windSpeed = current ? forecastCurrentWindSpeed : day.windSpeed;
    if( windSpeed )
    {
        snprintf( detail, sizeof( detail ), "WIND %s %d KM/H",
                  WindName( windDirection ), windSpeed );
        ModernWSM::TinyFont::Draw( 264, 258, detail, 1.6f, muted );
    }

    const size_t visibleDays = std::min( (size_t)5, forecastDays.size() );
    const float cardW = 112.0f;
    for( size_t i = 0; i < visibleDays; ++i )
    {
        const ForecastDay &preview = forecastDays[ i ];
        const float x = 28.0f + i * 119.0f;
        DrawRect( x, 332, cardW, 82, i == page ? (GXColor){ 38, 161, 220, 255 } : white );
        const GXColor cardInk = i == page ? white : ink;
        DrawCentered( x, cardW, 345, preview.label, 1.25f, cardInk );
        snprintf( detail, sizeof( detail ), "%d / %d C", preview.high, preview.low );
        DrawCentered( x, cardW, 375, detail, 1.45f, cardInk );
    }

    DrawCentered( 0, screen.x, 446, "LEFT RIGHT CHANGE DAY   B BACK", 1.35f, navy );
}

void NativeChannelView::RenderNews( const Vec2f &screen ) const
{
    const NewsArticle &article = newsArticles[ page % newsArticles.size() ];
    const GXColor green = { 28, 95, 61, 255 };
    const GXColor pale = { 238, 242, 235, 255 };
    const GXColor white = { 255, 255, 255, 255 };
    const GXColor ink = { 31, 39, 35, 255 };
    const GXColor muted = { 93, 105, 98, 255 };

    DrawRect( 0, 0, screen.x, screen.y, pale );
    DrawRect( 0, 0, screen.x, 54, green );
    ModernWSM::TinyFont::Draw( 22, 18, "NEWS CHANNEL", 2.4f, white );
    char counter[ 32 ];
    snprintf( counter, sizeof( counter ), "ARTICLE %u OF %u",
              (unsigned)( page + 1 ), (unsigned)newsArticles.size() );
    ModernWSM::TinyFont::Draw( 440, 21, counter, 1.3f, (GXColor){ 194, 235, 210, 255 } );

    DrawRect( 28, 72, 584, 344, white );
    DrawRect( 28, 72, 584, 8, (GXColor){ 62, 143, 91, 255 } );
    DrawRect( 48, 98, 146, 28, green );
    DrawCentered( 48, 146, 107, "BREAKING NEWS", 1.35f, white );
    float y = DrawWrapped( 48, 150, 544, article.headline, 2.4f, ink, 3 );
    DrawRect( 48, y + 4, 544, 2, (GXColor){ 202, 211, 205, 255 } );
    y = DrawWrapped( 48, y + 28, 544, article.body, 1.65f, ink, 9 );

    std::string source = "SOURCE " + article.source + "   LOCATION " + article.location;
    ModernWSM::TinyFont::Draw( 48, 388, source, 1.25f, muted, 105 );
    DrawCentered( 0, screen.x, 446, "LEFT RIGHT CHANGE ARTICLE   B BACK", 1.25f, green );
}

void NativeChannelView::RenderError( const Vec2f &screen ) const
{
    const GXColor ink = { 45, 45, 45, 255 };
    DrawRect( 0, 0, screen.x, screen.y, (GXColor){ 226, 226, 226, 255 } );
    DrawRect( 70, 145, screen.x - 140, 160, (GXColor){ 255, 255, 255, 255 } );
    DrawCentered( 70, screen.x - 140, 178, LocalizedTitle(), 2.0f, ink );
    DrawCentered( 70, screen.x - 140, 232,
                  dataError.empty() ? "CANNOT LOAD FAKE DATA" : dataError, 1.5f, ink );
}

void NativeChannelView::DrawRect( float x, float y, float w, float h, const GXColor &color )
{
    GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
    GX_Position3f32( x, y, 0.0f );
    GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x + w, y, 0.0f );
    GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x + w, y + h, 0.0f );
    GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_Position3f32( x, y + h, 0.0f );
    GX_Color4u8( color.r, color.g, color.b, color.a );
    GX_End();
}

void NativeChannelView::Render( Mtx &modelview, const Vec2f &screen, bool widescreen )
{
    (void)modelview;
    (void)widescreen;

    PrepareFlatGX();
    if( !dataValid )
        RenderError( screen );
    else if( kind == Forecast )
        RenderForecast( screen );
    else
        RenderNews( screen );
}
