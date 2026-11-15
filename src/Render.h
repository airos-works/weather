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
void DrawWeather(BView*, BRect bounds, const ScreenState&, const Artwork&, bool compact, time_t now);
class WeatherView : public BView {
public:
    explicit WeatherView(const Artwork& art);
    void Draw(BRect) override;
    void SetState(const ScreenState& state) { fState=state; Invalidate(); }
private:
    const Artwork& fArt;
    ScreenState fState;
};
}
