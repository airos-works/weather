#include "Weather.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace weather;
int main(){
    std::ifstream stream("tests/forecast.json");std::string body((std::istreambuf_iterator<char>(stream)),{});
    Forecast f;std::string error;assert(ParseForecast(body,f,error));
    assert(f.valid && f.hours.size()==168 && f.days.size()==7 && !f.rain.empty());
    assert(f.offset==39600);assert(f.hours[1].time-f.hours[0].time==3600);
    assert(!ParseForecast("{}",f,error));assert(!ParseForecast("{\"error\":true}",f,error));
    Forecast missing;assert(ParseForecast("{\"utc_offset_seconds\":0,\"current\":{\"time\":1794724000,\"temperature_2m\":12},\"hourly\":{\"time\":[1794724000,1794727600],\"temperature_2m\":[null,10]}}",missing,error));
    assert(std::isnan(missing.hours[0].temperature));assert(std::isnan(missing.hours[1].probability));assert(missing.code==-1);
    assert(Number(Missing)=="—");assert(Temperature(0,true)=="32°");assert(Temperature(10,false)=="10°");
    assert(LocalTime(1794724000,39600,"%H:%M")=="17:26");
    Location l{"Hobart",-42.88,147.33,false};assert(l.Valid());assert(ForecastURL(l).find("apikey")==std::string::npos);
    assert(ForecastURL(l).find("latitude=-42.8800&longitude=147.3300")!=std::string::npos);
    l.latitude=91;assert(!l.Valid());assert(ForecastURL(l).empty());
    assert(SearchURL("São & ?").find("S%C3%A3o%20%26%20%3F")!=std::string::npos);
    std::vector<Location> places;
    assert(ParseLocations("{\"results\":[{\"name\":\"Test\",\"latitude\":1,\"longitude\":2,\"admin1\":\"Region\",\"country\":\"Country\"},{\"name\":\"Bad\",\"latitude\":95,\"longitude\":2}]}",places,error));
    assert(places.size()==1 && places[0].name=="Test, Region, Country");
    assert(ParseLocations("{}",places,error)&&places.empty());
    assert(!ParseIPLocation("{\"city\":\"Test\"}",l,error));
    assert(ParseIPLocation("{\"city\":\"Test\",\"latitude\":0,\"longitude\":0}",l,error)&&l.approximate);
    time_t now=1794724000;Forecast rain;rain.valid=true;rain.fetched=rain.currentTime=now;
    for(int i=0;i<8;++i)rain.rain.push_back({now+i*900,0});
    assert(RainSummary(rain,now)=="Little or no precipitation expected");
    rain.rain[2].amount=0.2;assert(RainSummary(rain,now).find("Precipitation around")==0);
    rain.rain[0].amount=Missing;assert(RainSummary(rain,now)=="Next 2 hours: forecast unavailable");
    assert(Stale(rain,now+3601));assert(RainSummary(rain,now+3601)=="Rain forecast needs an update");
    assert(Condition(96)=="Thunderstorms");assert(Icon(0,false)=="clear-night");assert(Icon(-1)=="not-available");
    std::cout<<"Weather core: all checks passed\n";
}
