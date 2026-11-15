#include "Render.h"
#include "Settings.h"
#include "Network.h"
#include <Application.h>
#include <BitmapStream.h>
#include <File.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
using namespace weather;
void Render(const char* path,int w,int h,const ScreenState& s,const Artwork& art,bool compact){
    auto b=new BBitmap(BRect(0,0,w-1,h-1),B_BITMAP_ACCEPTS_VIEWS,B_RGB32);
    assert(b->InitCheck()==B_OK);auto v=new BView(b->Bounds(),"test",B_FOLLOW_NONE,0);b->AddChild(v);
    assert(b->Lock());DrawWeather(v,b->Bounds(),s,art,compact,s.forecast.currentTime);v->Sync();b->Unlock();
    BBitmapStream input(b);BFile output(path,B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    assert(BTranslatorRoster::Default()->Translate(&input,nullptr,nullptr,&output,B_PNG_FORMAT)==B_OK);
}
int main(int argc,char** argv){
    BApplication app("application/x-vnd.airos-WeatherTests");
    std::string dir="/tmp/weather-test-"+std::to_string(getpid());mkdir(dir.c_str(),0700);setenv("WEATHER_SETTINGS_DIR",dir.c_str(),1);
    Settings s;std::string error;assert(s.Load(error));assert(!s.consent && !s.asked && s.selected==-1);
    s.locations.push_back({"Hobart",-42.88,147.33,false});s.selected=0;s.consent=true;s.asked=true;s.follow=true;s.fahrenheit=true;
    s.cachedBody="{\"cached\":true}";s.cachedKey=s.locations[0].Key();s.cachedAt=1794724000;
    assert(s.Save(error));Settings copy;assert(copy.Load(error));assert(copy.consent && copy.follow && copy.fahrenheit && copy.locations.size()==1);
    assert(copy.locations[0].name=="Hobart");
    assert(copy.cachedBody==s.cachedBody && copy.cachedKey==s.cachedKey && copy.cachedAt==s.cachedAt);
    {std::ofstream bad(dir+"/settings");bad<<"broken settings";}
    Settings broken;assert(!broken.Load(error));assert(!broken.Save(error));
    {std::ifstream check(dir+"/settings");std::string text;getline(check,text);assert(text=="broken settings");}
    unlink((dir+"/settings").c_str());rmdir(dir.c_str());
    std::ifstream fixture("tests/forecast.json");std::string body((std::istreambuf_iterator<char>(fixture)),{});
    ScreenState state;assert(ParseForecast(body,state.forecast,error));state.forecast.fetched=state.forecast.currentTime;
    state.location={"Hobart, Tasmania",-42.88,147.33,false};Artwork art("resources/icons");assert(art.Get(0)!=nullptr);
    Render("/tmp/weather-desktop.png",980,840,state,art,false);
    Render("/tmp/weather-panel.png",800,480,state,art,true);
    Render("/tmp/weather-panel-small.png",480,320,state,art,true);
    state.forecast.fetched-=7200;Render("/tmp/weather-panel-stale.png",800,480,state,art,true);
    if(argc>1 && std::string(argv[1])=="--live"){
        error.clear();body=Fetch(ForecastURL(state.location),error);if(!error.empty())std::cerr<<error<<'\n';assert(error.empty());
        assert(ParseForecast(body,state.forecast,error));std::cout<<"Live forecast: "<<state.forecast.temperature<<" C, "<<state.forecast.hours.size()<<" hours\n";
        error.clear();body=Fetch(SearchURL("Hobart"),error);assert(error.empty());std::vector<Location> locations;
        assert(ParseLocations(body,locations,error));assert(!locations.empty());std::cout<<"Live geocoding: "<<locations.size()<<" matches\n";
    }
    std::cout<<"Native settings and rendering: all checks passed\n";
}
