#pragma once
#include "Json.h"
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

namespace weather {
constexpr double Missing = __builtin_nan("");
struct Location {
    std::string name;
    double latitude = Missing, longitude = Missing;
    bool approximate = false;
    bool Valid() const;
    std::string Key() const;
};
struct Hour {
    time_t time = 0;
    double temperature = Missing, rain = Missing, probability = Missing;
    int code = -1;
};
struct Day {
    time_t time = 0;
    double low = Missing, high = Missing, probability = Missing;
    int code = -1;
};
struct Rain { time_t time = 0; double amount = Missing; };
struct Forecast {
    bool valid = false;
    time_t fetched = 0, currentTime = 0;
    int offset = 0, code = -1;
    bool daylight = true;
    std::string timezone;
    double temperature = Missing, feels = Missing, humidity = Missing;
    double wind = Missing, pressure = Missing;
    std::vector<Hour> hours;
    std::vector<Day> days;
    std::vector<Rain> rain;
};
std::string ForecastURL(const Location& location);
std::string SearchURL(const std::string& query);
bool ParseForecast(const std::string&, Forecast&, std::string& error);
bool ParseLocations(const std::string&, std::vector<Location>&, std::string& error);
bool ParseIPLocation(const std::string&, Location&, std::string& error);
std::string Condition(int code);
std::string Icon(int code, bool daylight = true);
std::string Number(double value, const std::string& suffix = "", int decimals = 0);
std::string Temperature(double celsius, bool fahrenheit);
std::string LocalTime(time_t time, int offset, const char* format);
std::string RainSummary(const Forecast&, time_t now);
bool Stale(const Forecast&, time_t now);
}
