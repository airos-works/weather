// Run only against a disposable Weather instance in WEATHER_SETTINGS_DIR.
// Start that instance with --location -42.88 147.33 'Hobart, Tasmania'.
#include <Application.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
static BMessenger target;
BMessage State(){
    BMessage request(B_GET_PROPERTY),reply;request.AddSpecifier("WeatherState");
    assert(target.SendMessage(&request,&reply,1000000,1000000)==B_OK);return reply;
}
int64 Integer(const BMessage& m,const char* key){int64 n=-1;if(m.FindInt64(key,&n)==B_OK)return n;int32 i=-1;m.FindInt32(key,&i);return i;}
bool Boolean(const BMessage& m,const char* key){bool v=false;assert(m.FindBool(key,&v)==B_OK);return v;}
std::string String(const BMessage& m,const char* key){const char* s="";m.FindString(key,&s);return s;}
void Send(uint32 what,int index=-1,const char* query=nullptr){BMessage m(what);m.AddInt32("index",index);if(query)m.AddString("query",query);assert(target.SendMessage(&m)==B_OK);}
template<class F>BMessage Wait(F condition){for(int i=0;i<300;++i){auto s=State();if(condition(s))return s;snooze(100000);}assert(!"Weather workflow timed out");return BMessage();}
int main(){
    BApplication app("application/x-vnd.airos-WeatherWorkflowTests");
    target=BMessenger("application/x-vnd.airos-Weather");assert(target.IsValid());
    auto initial=State();assert(Boolean(initial,"isolated_settings"));assert(Integer(initial,"saved_locations")==1);assert(!Boolean(initial,"consent"));assert(Integer(initial,"location_requests")==0);
    std::string original=String(initial,"location");assert(original.find("Hobart")!=std::string::npos);
    Send('srch',-1,"London");Wait([](const BMessage& s){return Integer(s,"search_results")==10;});
    Send('useL',0);Wait([](const BMessage& s){return Integer(s,"saved_locations")==2 && Boolean(s,"valid") && String(s,"location").find("London")!=std::string::npos;});
    Send('unit');auto imperial=Wait([](const BMessage& s){return Boolean(s,"fahrenheit");});assert(Integer(imperial,"location_requests")==0);
    // Rapid selection invalidates any in-flight response for the old location.
    Send('selL',0);Send('selL',1);Send('selL',0);
    auto back=Wait([&](const BMessage& s){return String(s,"location")==original && Boolean(s,"valid");});
    assert(!Boolean(back,"follow"));assert(!Boolean(back,"consent"));
    int64 count=Integer(back,"forecast_requests");for(int i=0;i<10;++i)Send('refr');snooze(300000);
    assert(Integer(State(),"forecast_requests")==count);
    Send('remL',1);Wait([](const BMessage& s){return Integer(s,"saved_locations")==1;});
    Send('unit');Wait([](const BMessage& s){return !Boolean(s,"fahrenheit");});
    Send('frgt');assert(!Boolean(State(),"consent"));
    puts("Workflow passed: live city search, add/select/remove, units, rapid selection, refresh limiting, no IP lookup without consent.");
}
