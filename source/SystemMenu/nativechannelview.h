#ifndef NATIVECHANNELVIEW_H
#define NATIVECHANNELVIEW_H

#include <gccore.h>
#include <string>
#include <vector>
#include "utils/tools.h"

class NativeChannelView
{
public:
    enum Kind { Forecast, News };
    explicit NativeChannelView( Kind kind );
    ~NativeChannelView();

    bool Load();
    void Render( Mtx &modelview, const Vec2f &screen, bool widescreen );
    void Regenerate();
    void NextPage();
    void PreviousPage();

private:
    struct ForecastDay
    {
        std::string label;
        std::string condition;
        int high;
        int low;
        int rain;
        int windDirection;
        int windSpeed;
    };

    struct NewsArticle
    {
        std::string headline;
        std::string body;
        std::string source;
        std::string location;
    };

    Kind kind;
    size_t page;
    bool dataValid;
    std::string dataError;
    std::string forecastLocation;
    std::string forecastCurrentCondition;
    int forecastCurrentTemperature;
    int forecastCurrentWindDirection;
    int forecastCurrentWindSpeed;
    std::vector<ForecastDay> forecastDays;
    std::vector<NewsArticle> newsArticles;

    const char *LocalizedTitle() const;
    bool LoadForecastData( const u8 *data, u32 size );
    bool LoadShortForecastData( const u8 *data, u32 size );
    bool LoadNewsData( const u8 *data, u32 size );
    void RenderForecast( const Vec2f &screen ) const;
    void RenderNews( const Vec2f &screen ) const;
    void RenderError( const Vec2f &screen ) const;

    static void PrepareFlatGX();
    static void DrawRect( float x, float y, float w, float h, const GXColor &color );
    static void DrawCentered( float x, float width, float y, const std::string &text,
                              float scale, const GXColor &color );
    static float DrawWrapped( float x, float y, float width, const std::string &text,
                              float scale, const GXColor &color, size_t maxLines );
};

#endif
