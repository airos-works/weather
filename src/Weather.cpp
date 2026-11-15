#include "Weather.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <locale>

namespace weather {
namespace {
const JsonValue empty;
const JsonValue& Get(const JsonValue& v, const char* key) {
    auto p = v.Find(key); return p ? *p : empty;
}
double Num(const JsonValue& v) { return v.IsNumber() && std::isfinite(v.Number()) ? v.Number() : Missing; }
double Num(const JsonValue& v, const char* key) { return Num(Get(v, key)); }
int Code(const JsonValue& v) {
    double n = Num(v); return std::isfinite(n) && n >= 0 && n <= 99 && std::floor(n) == n ? int(n) : -1;
}
double At(const JsonValue& v, const char* key, size_t i) {
    const auto& a = Get(v, key).Array(); return i < a.size() ? Num(a[i]) : Missing;
}
time_t Stamp(double v) { return std::isfinite(v) && v > 0 && v < 4102444800.0 ? time_t(v) : 0; }
int AtCode(const JsonValue& v, size_t i) {
    const auto& a = Get(v, "weather_code").Array(); return i < a.size() ? Code(a[i]) : -1;
}
bool Parse(const std::string& body, JsonValue& j, std::string& error) {
    if (body.size() > 2*1024*1024 || !JsonValue::Parse(body, j, &error) || !j.IsObject()) {
        error = "The service returned an invalid response."; return false;
    }
    if (Get(j, "error").Bool()) { error = "The service could not complete this request."; return false; }
    return true;
}
std::string Encode(const std::string& s) {
    std::string out; char b[4];
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') out += c;
        else { snprintf(b, sizeof(b), "%%%02X", c); out += b; }
    }
    return out;
}
}
bool Location::Valid() const { return !name.empty() && name.size() < 512 && std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180; }
std::string Location::Key() const {
    std::ostringstream s; s.imbue(std::locale::classic()); s << std::fixed << std::setprecision(4) << latitude << ',' << longitude; return s.str();
}
std::string ForecastURL(const Location& l) {
    if (!l.Valid()) return "";
    auto key = l.Key(); auto comma = key.find(',');
    return "https://api.open-meteo.com/v1/forecast?latitude=" + key.substr(0, comma) + "&longitude=" + key.substr(comma+1)
        + "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,surface_pressure"
          "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code"
          "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
          "&minutely_15=precipitation&forecast_minutely_15=12&forecast_days=7&timezone=auto&timeformat=unixtime"
          "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm";
}
std::string SearchURL(const std::string& q) { return "https://geocoding-api.open-meteo.com/v1/search?count=10&language=en&format=json&name=" + Encode(q); }
bool ParseForecast(const std::string& body, Forecast& out, std::string& error) {
    JsonValue j; if (!Parse(body, j, error)) return false;
    Forecast f; const auto& c = Get(j, "current");
    f.temperature = Num(c,"temperature_2m"); f.feels = Num(c,"apparent_temperature");
    f.humidity = Num(c,"relative_humidity_2m"); f.wind = Num(c,"wind_speed_10m"); f.pressure = Num(c,"surface_pressure");
    f.code = Code(Get(c,"weather_code")); f.daylight = Num(c,"is_day") != 0;
    f.currentTime = Stamp(Num(c,"time"));
    double offset = Num(j,"utc_offset_seconds");
    if (!f.currentTime || !std::isfinite(f.temperature) || !std::isfinite(offset) || std::abs(offset)>86400) {
        error = "Current weather is unavailable for this location."; return false;
    }
    f.offset = int(offset); f.timezone = Get(j,"timezone").String();
    const auto& h = Get(j,"hourly");
    const auto& ht = Get(h,"time").Array();
    for (size_t i=0; i < std::min<size_t>(ht.size(), 384); ++i) {
        Hour a; a.time = Stamp(Num(ht[i])); a.temperature=At(h,"temperature_2m",i); a.rain=At(h,"precipitation",i);
        a.probability=At(h,"precipitation_probability",i); a.code=AtCode(h,i);
        if (a.time && (f.hours.empty() || a.time > f.hours.back().time)) f.hours.push_back(a);
    }
    const auto& d = Get(j,"daily"); const auto& dt = Get(d,"time").Array();
    for (size_t i=0; i<std::min<size_t>(dt.size(), 7); ++i) {
        Day a; a.time=Stamp(Num(dt[i])); a.low=At(d,"temperature_2m_min",i); a.high=At(d,"temperature_2m_max",i);
        a.probability=At(d,"precipitation_probability_max",i); a.code=AtCode(d,i);
        if (a.time && (f.days.empty() || a.time > f.days.back().time)) f.days.push_back(a);
    }
    const auto& r=Get(j,"minutely_15"); const auto& rt=Get(r,"time").Array();
    for (size_t i=0; i<std::min<size_t>(rt.size(), 96); ++i) {
        Rain a{Stamp(Num(rt[i])), At(r,"precipitation",i)};
        if (a.amount<0) a.amount=Missing;
        if (a.time && (f.rain.empty() || a.time > f.rain.back().time)) f.rain.push_back(a);
    }
    f.valid=true; f.fetched=time(nullptr); out=std::move(f); return true;
}
bool ParseLocations(const std::string& body, std::vector<Location>& out, std::string& error) {
    JsonValue j; if (!Parse(body,j,error)) return false;
    out.clear();
    for (const auto& r : Get(j,"results").Array()) {
        Location l; l.name=Get(r,"name").String();
        std::string region=Get(r,"admin1").String(), country=Get(r,"country").String();
        if (!region.empty() && region != l.name) l.name += ", " + region;
        if (!country.empty()) l.name += ", " + country;
        l.latitude=Num(r,"latitude"); l.longitude=Num(r,"longitude");
        if (l.Valid()) out.push_back(l);
        if (out.size()==10) break;
    }
    return true;
}
bool ParseIPLocation(const std::string& body, Location& out, std::string& error) {
    JsonValue j; if (!Parse(body,j,error)) return false;
    Location l; l.name=Get(j,"city").String();
    if (!Get(j,"region").String().empty()) l.name += ", " + Get(j,"region").String();
    l.latitude=Num(j,"latitude"); l.longitude=Num(j,"longitude"); l.approximate=true;
    if (!l.Valid()) { error="Could not estimate your location. Search for a city instead."; return false; }
    out=l; return true;
}
std::string Condition(int c) {
    switch(c) {
        case 0:return "Clear sky"; case 1:return "Mostly clear"; case 2:return "Partly cloudy"; case 3:return "Overcast";
        case 45: case 48:return "Fog"; case 51: case 53: case 55:return "Drizzle";
        case 56: case 57:return "Freezing drizzle"; case 61:return "Light rain"; case 63:return "Rain"; case 65:return "Heavy rain";
        case 66: case 67:return "Freezing rain"; case 71:return "Light snow"; case 73: case 75: case 77:return "Snow";
        case 80: case 81: case 82:return "Rain showers"; case 85: case 86:return "Snow showers";
        case 95: case 96: case 99:return "Thunderstorms"; default:return "Conditions unavailable";
    }
}
std::string Icon(int c, bool day) {
    if (c==0 || c==1) return day ? "clear-day" : "clear-night";
    if (c==2) return day ? "partly-cloudy-day" : "partly-cloudy-night";
    if (c==3) return "overcast";
    if (c==45 || c==48) return "fog";
    if (c>=51 && c<=57) return "drizzle";
    if ((c>=61 && c<=67) || (c>=80 && c<=82)) return "rain";
    if ((c>=71 && c<=77) || c==85 || c==86) return "snow";
    if (c>=95 && c<=99) return "thunderstorms";
    return "not-available";
}
std::string Number(double v, const std::string& suffix, int decimals) {
    if (!std::isfinite(v)) return "—";
    char b[80]; snprintf(b,sizeof(b),"%.*f",decimals,v==0 ? 0 : v); return std::string(b)+suffix;
}
std::string Temperature(double c, bool f) { return Number(f ? c*1.8+32 : c, "°"); }
std::string LocalTime(time_t t, int offset, const char* format) {
    t+=offset; tm result{}; gmtime_r(&t,&result); char b[80]; strftime(b,sizeof(b),format,&result); return b;
}
bool Stale(const Forecast& f, time_t now) { return !f.valid || now-f.fetched>1800 || now-f.currentTime>3600 || now < f.fetched-300 || now < f.currentTime-300; }
std::string RainSummary(const Forecast& f, time_t now) {
    if (Stale(f,now)) return "Rain forecast needs an update";
    int count=0; time_t first=0;
    for (const auto& r:f.rain) if (r.time>=now-900 && r.time<now+7200) {
        if (!std::isfinite(r.amount)) continue;
        ++count; if (!first && r.amount>=0.1) first=r.time;
    }
    if (count<8) return "Next 2 hours: forecast unavailable";
    if (!first) return "Little or no precipitation expected";
    if (first<=now+900) return "Precipitation expected around now";
    return "Precipitation around " + LocalTime(first,f.offset,"%H:%M");
}
}
