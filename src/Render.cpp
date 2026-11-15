#include "Render.h"
#include <InterfaceDefs.h>
#include <Message.h>
#include <ScrollBar.h>
#include <TranslationUtils.h>
#include <String.h>
#include <algorithm>
#include <dlfcn.h>

namespace weather {
Palette PaletteFor(bool dark)
{
    if (dark) return {true, {18,27,39,255}, {28,40,55,255}, {53,70,88,255},
        {242,245,247,255}, {166,185,200,255}, {102,197,236,255},
        {248,196,109,255}, {9,17,27,255}};
    return {false, {239,243,247,255}, {255,255,255,255}, {200,211,222,255},
        {25,40,55,255}, {80,99,117,255}, {0,105,151,255},
        {151,85,0,255}, {218,225,233,255}};
}

Palette SystemPalette()
{
    // The air/OS Screen preference is authoritative. Resolve at runtime because
    // older Haiku SDKs do not declare the API (including our ARM64 sysroot).
    using DarkMode = bool (*)();
    static auto darkMode = reinterpret_cast<DarkMode>(dlsym(RTLD_DEFAULT, "_Z12ui_dark_modev"));
    return PaletteFor(darkMode ? darkMode() : ui_color(B_PANEL_BACKGROUND_COLOR).IsDark());
}

WeatherLayout LayoutWeather(float width, float viewportHeight, bool forecast, bool status)
{
    WeatherLayout l;
    const float margin = width < 600 ? 14 : 22;
    const float gap = 16, left = margin, right = width - margin - 1;
    float top = status ? 94 : 72;
    l.stacked = width < 800;
    if (!forecast) {
        l.current = BRect(left, top, right, top + 300);
        l.height = std::max(viewportHeight, l.current.bottom + margin + 1);
        return l;
    }
    const float nowHeight = 176, rainHeight = 166;
    const float hourHeight = l.stacked ? 180 : 160, weekHeight = 304;
    const float outsideHeight = l.stacked ? 156 : weekHeight;
    float minimum = top + nowHeight + gap + hourHeight + gap + weekHeight + margin;
    if (l.stacked) minimum += rainHeight + gap + outsideHeight + gap;
    const float extra = std::max(0.0f, viewportHeight - minimum);
    const float nowExtra = std::min(60.0f, extra * .2f);
    const float hourExtra = (extra - nowExtra) * .55f;
    const float weekExtra = extra - nowExtra - hourExtra;

    float split = left + (right - left - gap) * .52f;
    l.current = BRect(left, top, l.stacked ? right : split, top + nowHeight + nowExtra);
    if (l.stacked) {
        top = l.current.bottom + gap;
        l.rain = BRect(left, top, right, top + rainHeight);
    } else l.rain = BRect(split + gap, top, right, l.current.bottom);
    top = l.rain.bottom + gap;
    l.hourly = BRect(left, top, right, top + hourHeight + hourExtra);
    top = l.hourly.bottom + gap;
    split = left + (right - left - gap) * .65f;
    l.week = BRect(left, top, l.stacked ? right : split, top + weekHeight + weekExtra);
    if (l.stacked) {
        top = l.week.bottom + gap;
        l.outside = BRect(left, top, right, top + outsideHeight);
    } else l.outside = BRect(split + gap, top, right, l.week.bottom);
    l.height = l.outside.bottom + margin;
    return l;
}

namespace {
void Text(BView* v, float x, float y, std::string text, float size,
    rgb_color color, bool bold = false, float width = 0)
{
    BFont font(bold ? be_bold_font : be_plain_font);
    font.SetSize(size);
    v->SetFont(&font);
    if (width > 0) {
        BString s(text.c_str());
        font.TruncateString(&s, B_TRUNCATE_END, width);
        text = s.String();
    }
    v->SetHighColor(color);
    v->DrawString(text.c_str(), BPoint(x, y));
}
void Right(BView* v, float x, float y, const std::string& text, float size,
    rgb_color color, bool bold = false)
{
    BFont font(bold ? be_bold_font : be_plain_font);
    font.SetSize(size);
    Text(v, x - font.StringWidth(text.c_str()), y, text, size, color, bold);
}
void Card(BView* v, BRect r, const Palette& p)
{
    v->SetHighColor(p.shadow);
    v->FillRoundRect(r.OffsetByCopy(0, 3), 12, 12);
    v->SetHighColor(p.card);
    v->FillRoundRect(r, 12, 12);
    v->SetHighColor(p.edge);
    v->StrokeRoundRect(r, 12, 12);
    v->SetLowColor(p.card);
}
void Image(BView* v, const Artwork& art, int code, bool day, BRect r)
{
    if (auto bitmap = art.Get(code, day)) {
        v->SetDrawingMode(B_OP_ALPHA);
        v->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
        v->DrawBitmap(bitmap, bitmap->Bounds(), r);
        v->SetDrawingMode(B_OP_COPY);
    }
}
void RainGraph(BView* v, const Forecast& f, BRect r, time_t now, const Palette& p)
{
    std::vector<Rain> samples;
    for (const auto& a : f.rain) if (a.time >= now - 900 && a.time < now + 7200) {
        samples.push_back(a);
        if (samples.size() == 8) break;
    }
    double maximum = 1;
    for (const auto& a : samples) if (std::isfinite(a.amount)) maximum = std::max(maximum, a.amount);
    v->SetHighColor(p.edge);
    v->StrokeLine(r.LeftBottom(), r.RightBottom());
    if (samples.empty()) {
        Text(v, r.left, r.top + 25, "Precipitation data unavailable", 13, p.muted);
        return;
    }
    float step = r.Width() / 8;
    for (size_t i = 0; i < samples.size(); ++i) {
        auto a = samples[i];
        float x = r.left + i * step;
        if (!std::isfinite(a.amount)) Text(v, x + step / 2 - 5, r.bottom - 8, "—", 14, p.muted);
        else {
            v->SetHighColor(p.rain);
            float h = std::max(2.0, (r.Height() - 22) * a.amount / maximum);
            v->FillRoundRect(BRect(x + 3, r.bottom - h, x + step - 4, r.bottom), 3, 3);
        }
        if (i % 2 == 0) Text(v, x + 2, r.bottom + 18, LocalTime(a.time, f.offset, "%H:%M"), 11, p.muted);
    }
    Text(v, r.left, r.top, Number(maximum, " mm / 15 min", 1), 11, p.muted);
}
void Hourly(BView* v, BRect r, const ScreenState& s, const Palette& p, time_t now)
{
    const auto& f = s.forecast;
    Text(v, r.left + 18, r.top + 27, "NEXT 24 HOURS", 11, p.muted, true);
    const bool narrow = r.Width() < 580;
    if (narrow) Text(v, r.left + 18, r.top + 47, "Temperature / chance of precipitation", 11, p.muted);
    else Right(v, r.right - 18, r.top + 27, "Temperature / chance of precipitation", 11, p.muted);
    std::vector<Hour> hours;
    for (const auto& h : f.hours) if (h.time >= now - 1800) {
        hours.push_back(h);
        if (hours.size() == 24) break;
    }
    if (hours.empty()) {
        Text(v, r.left + 18, r.top + 90, "Hourly forecast unavailable", 16, p.muted);
        return;
    }
    double lo = 1000, hi = -1000;
    for (const auto& h : hours) if (std::isfinite(h.temperature)) {
        lo = std::min(lo, h.temperature);
        hi = std::max(hi, h.temperature);
    }
    const BRect plot(r.left + 25, r.top + (narrow ? 75 : 58), r.right - 25, r.bottom - 66);
    float step = plot.Width() / std::max<size_t>(1, hours.size() - 1);
    size_t labelsEvery = std::max<size_t>(1, std::ceil(62.0f / step));
    bool previous = false;
    BPoint previousPoint;
    for (size_t i = 0; i < hours.size(); ++i) {
        float x = plot.left + i * step;
        const auto& h = hours[i];
        if (std::isfinite(h.temperature)) {
            BPoint q(x, plot.bottom - plot.Height() * (h.temperature - lo) / std::max(1.0, hi - lo));
            v->SetHighColor(p.temperature);
            v->SetPenSize(2.5);
            if (previous) v->StrokeLine(previousPoint, q);
            v->FillEllipse(q, 2.5, 2.5);
            v->SetPenSize(1);
            previousPoint = q;
            previous = true;
            if (i % labelsEvery == 0) Text(v, x - 10, q.y - 10, Temperature(h.temperature, s.fahrenheit), 12, p.ink, true);
        } else previous = false;
        if (i % labelsEvery == 0) {
            Text(v, x - 12, r.bottom - 36, Number(h.probability, "%"), 12, p.rain);
            Text(v, x - 15, r.bottom - 15, LocalTime(h.time, f.offset, "%H:%M"), 11, p.muted);
        }
    }
}
void Week(BView* v, BRect r, const ScreenState& s, const Artwork& art, const Palette& p, time_t now)
{
    const auto& f = s.forecast;
    Text(v, r.left + 18, r.top + 27, "THE WEEK AHEAD", 11, p.muted, true);
    Right(v, r.right - 110, r.top + 27, "RAIN", 10, p.muted);
    Right(v, r.right - 62, r.top + 27, "LOW", 10, p.muted);
    Right(v, r.right - 18, r.top + 27, "HIGH", 10, p.muted);
    if (f.days.empty()) Text(v, r.left + 18, r.top + 74, "Daily forecast unavailable", 15, p.muted);
    float step = (r.Height() - 52) / 7;
    for (size_t i = 0; i < f.days.size(); ++i) {
        const auto& d = f.days[i];
        float y = r.top + 44 + step * (i + .65f);
        Text(v, r.left + 18, y, LocalTime(d.time, f.offset, "%F") == LocalTime(now, f.offset, "%F")
            ? "Today" : LocalTime(d.time, f.offset, "%a"), 14, p.ink, true);
        Image(v, art, d.code, true, BRect(r.left + 67, y - 28, r.left + 102, y + 7));
        Text(v, r.left + 110, y, Condition(d.code), 12, p.muted, false, r.Width() - 260);
        Right(v, r.right - 110, y, Number(d.probability, "%"), 12, p.rain);
        Right(v, r.right - 62, y, Temperature(d.low, s.fahrenheit), 14, p.muted);
        Right(v, r.right - 18, y, Temperature(d.high, s.fahrenheit), 14, p.ink, true);
    }
}
void Outside(BView* v, BRect r, const ScreenState& s, const Palette& p, bool horizontal, time_t now)
{
    const auto& f = s.forecast;
    Text(v, r.left + 18, r.top + 27, "OUTSIDE", 11, p.muted, true);
    const std::string labels[] = {"Wind", "Humidity", "Pressure"};
    const std::string values[] = {
        Number(s.fahrenheit ? f.wind / 1.609344 : f.wind, s.fahrenheit ? " mph" : " km/h"),
        Number(f.humidity, "%"), Number(f.pressure, " hPa")};
    for (int i = 0; i < 3; ++i) {
        float x = r.left + 18, y = r.top + 62;
        if (horizontal) x += i * (r.Width() - 36) / 3;
        else y += i * (r.Height() - 102) / 3;
        Text(v, x, y, labels[i], 13, p.muted);
        Text(v, x, y + 30, values[i], horizontal && r.Width() < 600 ? 20 : 25, p.ink, true);
    }
    Text(v, r.left + 18, r.bottom - (horizontal ? 18 : 39), "Updated " + LocalTime(f.fetched, f.offset, "%H:%M"), 12, p.muted);
    std::string status = Stale(f, now) ? "STALE · refresh needed" : "Local time " + LocalTime(now, f.offset, "%H:%M");
    if (horizontal) Right(v, r.right - 18, r.bottom - 18, status, 12, Stale(f, now) ? p.temperature : p.muted);
    else Text(v, r.left + 18, r.bottom - 16, status, 12, Stale(f, now) ? p.temperature : p.muted);
}
}

Artwork::Artwork(const std::string& directory)
{
    for (const char* name : {"clear-day", "clear-night", "partly-cloudy-day", "partly-cloudy-night", "overcast", "fog", "drizzle", "rain", "snow", "thunderstorms", "not-available"})
        fImages[name].reset(BTranslationUtils::GetBitmap((directory + "/" + name + ".png").c_str()));
}
BBitmap* Artwork::Get(int code, bool day) const
{
    auto i = fImages.find(Icon(code, day));
    return i == fImages.end() ? nullptr : i->second.get();
}
WeatherView::WeatherView(const Artwork& art)
    : BView("forecast", B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE), fArt(art)
{
    SetViewColor(SystemPalette().background);
    SetExplicitMinSize(BSize(400, 300));
}
WeatherLayout WeatherView::Layout() const
{
    return LayoutWeather(Bounds().Width() + 1, Bounds().Height() + 1,
        fState.forecast.valid, !fState.status.empty());
}
void WeatherView::UpdateScrollbars()
{
    float viewportHeight = Bounds().Height() + 1;
    float contentHeight = Layout().height;
    float maximum = std::max(0.0f, contentHeight - viewportHeight);
    if (auto bar = ScrollBar(B_VERTICAL)) {
        bar->SetRange(0, maximum);
        bar->SetProportion(std::min(1.0f, viewportHeight / contentHeight));
        bar->SetSteps(36, viewportHeight * .8f);
    }
    if (Bounds().top > maximum) ScrollTo(0, maximum);
}
void WeatherView::AttachedToWindow() { UpdateScrollbars(); }
void WeatherView::FrameResized(float, float)
{
    UpdateScrollbars();
    Invalidate(); // Reflow moves existing content, not just newly exposed edges.
}
void WeatherView::SetState(const ScreenState& state)
{
    fState = state;
    UpdateScrollbars();
    Invalidate();
}
void WeatherView::MessageReceived(BMessage* m)
{
    if (m->what == B_COLORS_UPDATED) {
        SetViewColor(SystemPalette().background);
        Invalidate();
    } else BView::MessageReceived(m);
}
void WeatherView::Draw(BRect) { DrawWeather(this, Bounds(), fState, fArt, false, time(nullptr)); }
void DrawWeather(BView* v, BRect bounds, const ScreenState& s, const Artwork& art, bool compact, time_t now)
{
    DrawWeather(v, bounds, s, art, compact, now, SystemPalette());
}
void DrawWeather(BView* v, BRect bounds, const ScreenState& s, const Artwork& art, bool compact, time_t now, const Palette& p)
{
    v->SetDrawingMode(B_OP_COPY);
    v->SetHighColor(p.background);
    v->FillRect(bounds);
    v->SetLowColor(p.background);
    const auto& f = s.forecast;
    if (compact) {
        float scale = std::min((bounds.Width() + 1) / 640.0f, (bounds.Height() + 1) / 480.0f);
        v->SetOrigin((bounds.Width() + 1 - 640 * scale) / 2, (bounds.Height() + 1 - 480 * scale) / 2);
        v->SetScale(scale);
        Text(v, 30, 48, s.location.Valid() ? s.location.name : "Weather", 30, p.ink, true, 580);
        Text(v, 30, 78, s.location.approximate ? "Approximate location · IP estimate" : "air/OS Weather", 17, p.muted);
        if (!f.valid) {
            Text(v, 30, 220, "Choose a location", 38, p.ink, true);
            Text(v, 30, 265, "Open Weather on the desktop to begin.", 22, p.muted);
            Text(v, 30, 420, s.status, 18, p.temperature, false, 580);
        } else {
            Text(v, 25, 244, Temperature(f.temperature, s.fahrenheit), 130, p.ink, true);
            Image(v, art, f.code, f.daylight, BRect(365, 92, 610, 310));
            Text(v, 33, 290, Condition(f.code), 30, p.ink, true, 560);
            if (!f.days.empty() && LocalTime(f.days[0].time, f.offset, "%F") == LocalTime(now, f.offset, "%F"))
                Text(v, 33, 330, "High " + Temperature(f.days[0].high, s.fahrenheit) + "   Low " + Temperature(f.days[0].low, s.fahrenheit)
                    + "   Feels " + Temperature(f.feels, s.fahrenheit), 24, p.muted);
            v->SetHighColor(p.edge);
            v->StrokeLine(BPoint(30, 354), BPoint(610, 354));
            Text(v, 30, 395, RainSummary(f, now), 23, p.rain, true, 580);
            Text(v, 30, 429, Stale(f, now) ? "STALE · connect to update" : "15-minute forecast · updated " + LocalTime(f.fetched, f.offset, "%H:%M"),
                18, Stale(f, now) ? p.temperature : p.muted);
        }
        v->SetScale(1);
        v->SetOrigin(0, 0);
        return;
    }

    const float width = bounds.Width() + 1;
    const auto layout = LayoutWeather(width, bounds.Height() + 1, f.valid, !s.status.empty());
    const float left = layout.current.left;
    Text(v, left, 34, s.location.Valid() ? s.location.name : "Your weather, at a glance", 24, p.ink, true, width - 2 * left);
    Text(v, left, 57, s.location.approximate ? "Approximate location from your public IP" : f.timezone, 12, p.muted, false, width - 2 * left);
    if (!s.status.empty()) Text(v, left, 79, s.status, 12, p.temperature, false, width - 2 * left);
    auto r = layout.current;
    Card(v, r, p);
    if (!f.valid) {
        Image(v, art, 2, true, BRect(width / 2 - 80, r.top + 5, width / 2 + 80, r.top + 165));
        Text(v, left + 20, r.top + 210, s.location.Valid() ? "Getting your forecast…" : "Start with your location", 24, p.ink, true, r.Width() - 40);
        Text(v, left + 20, r.top + 245, "Use My location, or search in Locations.", 14, p.muted, false, r.Width() - 40);
        return;
    }
    Text(v, r.left + 20, r.top + 28, "RIGHT NOW", 11, p.muted, true);
    float size = std::min(100.0f, 76 + (r.Height() - 176) * .35f);
    Text(v, r.left + 16, r.bottom - 62, Temperature(f.temperature, s.fahrenheit), size, p.ink, true);
    float iconSize = std::min(r.Height() - 25, r.Width() * .42f);
    Image(v, art, f.code, f.daylight, BRect(r.right - iconSize - 14, r.top + 8, r.right - 14, r.top + 8 + iconSize));
    Text(v, r.left + 20, r.bottom - 31, Condition(f.code), 19, p.ink, true, r.Width() - 40);
    Text(v, r.left + 20, r.bottom - 10, "Feels like " + Temperature(f.feels, s.fahrenheit), 12, p.muted);

    r = layout.rain;
    Card(v, r, p);
    Text(v, r.left + 18, r.top + 28, "NEXT 2 HOURS · 15-MIN FORECAST", 11, p.muted, true);
    Text(v, r.left + 18, r.top + 57, RainSummary(f, now), 15, p.rain, true, r.Width() - 36);
    RainGraph(v, f, BRect(r.left + 18, r.top + 79, r.right - 18, r.bottom - 32), now, p);
    Card(v, layout.hourly, p);
    Hourly(v, layout.hourly, s, p, now);
    Card(v, layout.week, p);
    Week(v, layout.week, s, art, p, now);
    Card(v, layout.outside, p);
    Outside(v, layout.outside, s, p, layout.stacked, now);
}
}
