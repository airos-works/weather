#pragma once
#include "Weather.h"
#include <View.h>
#include <Bitmap.h>
#include <map>
#include <memory>
namespace weather {
struct ScreenState {
    Location location;
    Forecast forecast;
    bool fahrenheit=false;
    std::string status, panelStatus;
};
class Artwork {
public:
    explicit Artwork(const std::string& directory);
    BBitmap* Get(int code, bool day=true) const;
private:
    std::map<std::string,std::unique_ptr<BBitmap>> fImages;
};
struct Palette {
    bool dark;
    rgb_color background, card, edge, ink, muted, rain, temperature, shadow;
};
Palette PaletteFor(bool dark);
Palette SystemPalette();
struct WeatherLayout {
    bool stacked = false;
    BRect current, rain, hourly, week, outside;
    float height = 0;
};
WeatherLayout LayoutWeather(float width, float viewportHeight, bool forecast, bool status);
void DrawWeather(BView*, BRect bounds, const ScreenState&, const Artwork&, bool compact, time_t now);
void DrawWeather(BView*, BRect bounds, const ScreenState&, const Artwork&, bool compact, time_t now, const Palette&);
class WeatherView : public BView {
public:
    explicit WeatherView(const Artwork& art);
    void Draw(BRect) override;
    void AttachedToWindow() override;
    void FrameResized(float width, float height) override;
    void MessageReceived(BMessage*) override;
    void SetState(const ScreenState& state);
    WeatherLayout Layout() const;
private:
    void UpdateScrollbars();
    const Artwork& fArt;
    ScreenState fState;
};
}
