#include "Weather.h"
#include "Settings.h"
#include "Network.h"
#include "Render.h"
#include <Alert.h>
#include <Application.h>
#include <AuxDisplay.h>
#include <Button.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <Roster.h>
#include <ScrollView.h>
#include <ScrollBar.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using namespace weather;
namespace {
constexpr uint32 kLocate='loca',kLocations='locs',kSearch='srch',kUse='useL',kSelect='selL',kRemove='remL';
constexpr uint32 kRefresh='refr',kUnits='unit',kPanel='pnel',kForget='frgt',kTick='tick',kClosed='clos',kLocationClosed='lcls';
constexpr int32 ForecastRequest=1,SearchRequest=2,LocationRequest=3;
std::string ResourceDir() {
    app_info info; be_app->GetAppInfo(&info); BPath p(&info.ref);p.GetParent(&p);p.Append("resources"); return p.Path();
}
class MainWindow:public BWindow {
public:
    MainWindow(const Artwork& art):BWindow(BRect(80,70,1060,920),"Weather",B_TITLED_WINDOW,B_AUTO_UPDATE_SIZE_LIMITS) {
        auto menus=new BMenuBar("menus");
        auto file=new BMenu("Weather");
        file->AddItem(new BMenuItem("Refresh",new BMessage(kRefresh),'R'));
        file->AddItem(new BMenuItem("Locations…",new BMessage(kLocations),'L'));
        file->AddSeparatorItem();file->AddItem(new BMenuItem("About Weather…",new BMessage(B_ABOUT_REQUESTED)));
        file->AddItem(new BMenuItem("Quit",new BMessage(B_QUIT_REQUESTED),'Q'));
        file->SetTargetForItems(be_app);menus->AddItem(file);
        auto view=new BMenu("View");
        units=new BMenuItem("Use Fahrenheit / mph",new BMessage(kUnits));view->AddItem(units);
        panel=new BMenuItem("Show on auxiliary display",new BMessage(kPanel));view->AddItem(panel);
        view->SetTargetForItems(be_app);menus->AddItem(view);
        auto privacy=new BMenu("Location"); privacy->AddItem(new BMenuItem("Use my approximate location…",new BMessage(kLocate)));
        privacy->AddItem(new BMenuItem("Revoke location permission",new BMessage(kForget)));privacy->SetTargetForItems(be_app);menus->AddItem(privacy);
        auto places=new BButton("Locations…",new BMessage(kLocations));places->SetTarget(be_app);
        auto locate=new BButton("My location",new BMessage(kLocate));locate->SetTarget(be_app);
        auto refresh=new BButton("Refresh",new BMessage(kRefresh));refresh->SetTarget(be_app);
        panelLabel=new BStringView("panel status","");
        canvas=new WeatherView(art);
        scroll=new BScrollView("forecast scroll",canvas,0,false,true,B_NO_BORDER);
        scroll->SetExplicitMinSize(BSize(820,400));
        BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(menus)
            .AddGroup(B_HORIZONTAL,8).SetInsets(12,8,12,8).Add(places).Add(locate).AddGlue().Add(panelLabel).Add(refresh).End().Add(scroll);
    }
    bool QuitRequested() override { be_app->PostMessage(kClosed);return false; }
    void FrameResized(float,float) override { SizeCanvas(); }
    void SizeCanvas() {
        if (!scroll) return;
        float width=scroll->Bounds().Width()-B_V_SCROLL_BAR_WIDTH;
        float height=scroll->Bounds().Height();canvas->ResizeTo(width,std::max(840.0f,height));
        auto bar=scroll->ScrollBar(B_VERTICAL);bar->SetRange(0,std::max(0.0f,840-height));bar->SetProportion(std::min(1.0f,height/840));bar->SetSteps(36,height*.8f);
    }
    void Update(const ScreenState& state,bool enabled) {
        if (!Lock()) return;
        canvas->SetState(state);units->SetMarked(state.fahrenheit);panel->SetMarked(enabled);
        panelLabel->SetText(state.panelStatus.c_str());SizeCanvas();Unlock();
    }
    WeatherView* canvas=nullptr;BScrollView* scroll=nullptr;BMenuItem *units,*panel;BStringView* panelLabel;
};
class LocationsWindow:public BWindow {
public:
    LocationsWindow():BWindow(BRect(170,130,790,770),"Weather locations",B_TITLED_WINDOW,B_AUTO_UPDATE_SIZE_LIMITS) {
        query=new BTextControl("City or postal code:","",new BMessage(kSearch));query->SetTarget(this);
        auto search=new BButton("Search",new BMessage(kSearch));search->SetTarget(this);
        results=new BListView("search results"); results->SetInvocationMessage(new BMessage(kUse));results->SetTarget(this);
        saved=new BListView("saved places");saved->SetInvocationMessage(new BMessage(kSelect));saved->SetTarget(this);
        auto foundScroll=new BScrollView("results",results,0,false,true);foundScroll->SetExplicitMinSize(BSize(570,180));
        auto savedScroll=new BScrollView("saved",saved,0,false,true);savedScroll->SetExplicitMinSize(BSize(570,150));
        auto use=new BButton("Save and show",new BMessage(kUse));use->SetTarget(this);
        auto select=new BButton("Show selected",new BMessage(kSelect));select->SetTarget(this);
        auto remove=new BButton("Remove",new BMessage(kRemove));remove->SetTarget(this);
        status=new BStringView("status","Search for a city, then select the correct region.");
        BLayoutBuilder::Group<>(this,B_VERTICAL,10).SetInsets(16)
            .AddGroup(B_HORIZONTAL,8).Add(query).Add(search).End().Add(foundScroll).Add(use).Add(status)
            .Add(new BStringView("saved label","Saved locations")).Add(savedScroll)
            .AddGroup(B_HORIZONTAL,8).Add(select).Add(remove).AddGlue().End();
        SetDefaultButton(search);
    }
    bool QuitRequested() override {be_app->PostMessage(kLocationClosed);return false;}
    void MessageReceived(BMessage* m) override {
        if (m->what==kSearch) { BMessage send(kSearch);send.AddString("query",query->Text());be_app->PostMessage(&send); }
        else if (m->what==kUse || m->what==kSelect || m->what==kRemove) {
            BMessage send(m->what);send.AddInt32("index",m->what==kUse?results->CurrentSelection():saved->CurrentSelection());be_app->PostMessage(&send);
        } else BWindow::MessageReceived(m);
    }
    void Update(const std::vector<Location>& places,const std::vector<Location>& found,const std::string& message,int selected) {
        if(!Lock())return;
        auto fill=[](BListView* list,const std::vector<Location>& locations) {
            while(auto item=list->RemoveItem(int32(0)))delete item;
            for(auto& l:locations)list->AddItem(new BStringItem(l.name.c_str()));
        };
        fill(saved,places);fill(results,found);saved->Select(selected);status->SetText(message.c_str());Unlock();
    }
    BTextControl* query;BListView *results,*saved;BStringView* status;
};
class WeatherApp:public BApplication {
public:
    WeatherApp():BApplication("application/x-vnd.airos-Weather"){}
    ~WeatherApp() override { network.reset();timer.reset();StopPanel(); }
    bool QuitRequested() override {
        if(locations && locations->Lock()){locations->Quit();locations=nullptr;}
        if(window && window->Lock()){window->Quit();window=nullptr;}
        return true;
    }
    void ArgvReceived(int32 argc,char** argv) override {
        bool wantsPanel=false;
        for(int32 i=1;i<argc;++i) if(strcmp(argv[i],"--panel")==0) {panelOnly=true;wantsPanel=true;}
        else if(strcmp(argv[i],"--location")==0 && i+3<argc) {
            char *a,*b;double lat=strtod(argv[i+1],&a),lon=strtod(argv[i+2],&b);
            Location l{argv[i+3],lat,lon,false}; if(a!=argv[i+1] && b!=argv[i+2] && *a==0 && *b==0 && l.Valid()) commandLocation=l;
            else fprintf(stderr,"Invalid --location LAT LON NAME\n");
            i+=3;
        }
        if (ready && commandLocation.Valid()) {settings.follow=false;++locationGeneration;AddLocation(commandLocation);commandLocation={};}
        if (ready) {if(wantsPanel)StartPanel();else OpenWindow();}
    }
    void ReadyToRun() override {
        ready=true;std::string error;settings.Load(error);state.status=error;
        art=std::make_unique<Artwork>(ResourceDir()+"/icons");network=std::make_unique<Network>(BMessenger(this));
        if(commandLocation.Valid()) {settings.follow=false;AddLocation(commandLocation);commandLocation={};}
        else if(settings.selected>=0) {
            state.location=settings.locations[settings.selected];
            if(settings.cachedKey==state.location.Key() && ParseForecast(settings.cachedBody,state.forecast,error)) {
                state.forecast.fetched=settings.cachedAt;state.status="Saved forecast · refreshing…";
            }
            Refresh();
        }
        if(!panelOnly || !state.location.Valid())OpenWindow();
        if(panelOnly)StartPanel();
        if(settings.follow && settings.consent)Locate(false);
        else if(!settings.asked && !state.location.Valid())Locate(true);
        timer=std::make_unique<BMessageRunner>(BMessenger(this),new BMessage(kTick),60000000);
        Publish();
    }
    void RefsReceived(BMessage*) override {OpenWindow();}
    BHandler* ResolveSpecifier(BMessage* m,int32 index,BMessage* spec,int32 what,const char* property) override {
        if(property && strcmp(property,"WeatherState")==0)return this;
        return BApplication::ResolveSpecifier(m,index,spec,what,property);
    }
    void AppActivated(bool active) override {if(active && ready && !panelOnly && !window)OpenWindow();}
    void AboutRequested() override {
        (new BAlert("About Weather","Weather for air/OS\n\nForecasts: Open-Meteo, CC BY 4.0\nPlaces: GeoNames via Open-Meteo\nLocation estimate: ipapi.co, only with permission\nArtwork: Meteocons by Bas Milius, MIT\n\nFree API for personal/non-commercial use; no key.\n15-minute precipitation is forecast model guidance,\nnot minute-by-minute radar or severe-weather alerts.\n\nhttps://github.com/airos-works/weather","OK"))->Go();
    }
    void MessageReceived(BMessage* m) override {
        switch(m->what) {
            case kLocate:Locate(true);break;
            case kLocations:OpenLocations();break;
            case kLocationClosed:if(locations && locations->Lock()){locations->Quit();locations=nullptr;}break;
            case kSearch:{const char* q="";m->FindString("query",&q);if(strlen(q)<2){searchStatus="Enter at least two characters.";UpdateLocations();break;}
                found.clear();searchStatus="Searching…";UpdateLocations();network->Submit({SearchRequest,++searchGeneration,SearchURL(q)});break;}
            case kUse:{int32 i=-1;m->FindInt32("index",&i);if(i>=0 && size_t(i)<found.size()){settings.follow=false;++locationGeneration;AddLocation(found[i]);}break;}
            case kSelect:{int32 i=-1;m->FindInt32("index",&i);if(i>=0 && size_t(i)<settings.locations.size()){settings.follow=false;++locationGeneration;Select(i);}break;}
            case kRemove:{int32 i=-1;m->FindInt32("index",&i);if(i>=0 && size_t(i)<settings.locations.size()){
                bool active=i==settings.selected;settings.locations.erase(settings.locations.begin()+i);
                if(active){settings.follow=false;++locationGeneration;Select(settings.locations.empty()?-1:0);}
                else {if(i<settings.selected)--settings.selected;Save();UpdateLocations();}
            }break;}
            case kRefresh:Refresh();break;
            case kUnits:settings.fahrenheit=!settings.fahrenheit;Save();Publish();break;
            case kForget:settings.asked=true;settings.consent=false;settings.follow=false;++locationGeneration;Save();state.status="Location permission revoked. Saved cities remain available.";Publish();break;
            case kPanel:if(display.InitCheck()==B_OK){StopPanel();state.panelStatus="Auxiliary display off";Publish();}else StartPanel();break;
            case kTick:if(system_time()-lastRefresh>=600000000)Refresh();else Publish();break;
            case kClosed:if(window && window->Lock()){window->Quit();window=nullptr;}if(display.InitCheck()!=B_OK)PostMessage(B_QUIT_REQUESTED);break;
            case NetworkResult:Result(m);break;
            case B_GET_PROPERTY:{BMessage spec;int32 index,what;const char* property=nullptr;
                if(m->GetCurrentSpecifier(&index,&spec,&what,&property)==B_OK && property && strcmp(property,"WeatherState")==0){
                    BMessage reply(B_REPLY);reply.AddInt32("error",B_OK);reply.AddString("location",state.location.name.c_str());
                    reply.AddBool("valid",state.forecast.valid);reply.AddBool("consent",settings.consent);reply.AddBool("follow",settings.follow);
                    reply.AddBool("stale",Stale(state.forecast,time(nullptr)));reply.AddDouble("temperature",state.forecast.temperature);
                    reply.AddInt64("location_requests",locationRequests);reply.AddInt64("forecast_requests",forecastRequests);
                    reply.AddBool("fahrenheit",settings.fahrenheit);reply.AddInt32("search_results",found.size());
                    reply.AddBool("isolated_settings",getenv("WEATHER_SETTINGS_DIR")!=nullptr);
                    reply.AddInt32("saved_locations",settings.locations.size());reply.AddBool("panel",display.InitCheck()==B_OK);
                    reply.AddInt64("panel_frames",panelFrames);reply.AddString("panel_status",state.panelStatus.c_str());
                    reply.AddString("status",state.status.c_str());m->SendReply(&reply);
                }else BApplication::MessageReceived(m);break;}
            default:BApplication::MessageReceived(m);break;
        }
    }
private:
    void OpenWindow(){if(window){window->Activate();return;}window=new MainWindow(*art);window->Show();Publish();}
    void OpenLocations(){if(locations){locations->Activate();return;}locations=new LocationsWindow;locations->Show();UpdateLocations();}
    void UpdateLocations(){if(locations)locations->Update(settings.locations,found,searchStatus,settings.selected);}
    void Save(){std::string error;if(!settings.Save(error))state.status=error;}
    void AddLocation(const Location& location){
        for(size_t i=0;i<settings.locations.size();++i)if(settings.locations[i].Key()==location.Key()){settings.locations[i]=location;Select(i);return;}
        if(settings.locations.size()>=50){state.status="You have 50 saved locations. Remove one before adding another.";Publish();return;}
        settings.locations.push_back(location);Select(settings.locations.size()-1);
    }
    void Select(int index){
        settings.selected=index;state.forecast={};++forecastGeneration;
        settings.cachedBody.clear();settings.cachedKey.clear();settings.cachedAt=0;
        state.location=index<0?Location{}:settings.locations[index];state.status="";
        lastRequestKey.clear();lastRefresh=0;Save();UpdateLocations();Refresh();
    }
    void Refresh(){
        if(!state.location.Valid()){Publish();return;}
        // Button presses cannot overwhelm the free endpoint; retries remain available after 30 s.
        if(state.location.Key()==lastRequestKey && system_time()-lastRefresh<30000000){Publish();return;}
        lastRequestKey=state.location.Key();lastRefresh=system_time();state.status="Updating forecast…";
        ++forecastRequests;
        network->Submit({ForecastRequest,++forecastGeneration,ForecastURL(state.location)});Publish();
    }
    void Locate(bool ask){
        if(ask){
            int32 choice=(new BAlert("Location permission","Use your approximate location?\n\nWeather will contact ipapi.co, which sees your public IP address and estimates a city. Coordinates are then sent to Open-Meteo for forecasts. This is not GPS and may be inaccurate, especially with a VPN.\n\nAllow remembers permission and looks up your location when Weather starts. You can revoke it in the Location menu or choose a saved city.","Choose city","Allow",nullptr,B_WIDTH_AS_USUAL,B_INFO_ALERT))->Go();
            settings.asked=true;settings.consent=choice==1;settings.follow=choice==1;Save();
            if(choice!=1){OpenLocations();Publish();return;}
        }
        if(!settings.consent)return;
        ++locationRequests;
        state.status="Finding your approximate location…";network->Submit({LocationRequest,++locationGeneration,"https://ipapi.co/json/"});Publish();
    }
    void Result(BMessage* m){
        int32 kind=0;int64 gen=0;const char *body="",*err="";m->FindInt32("kind",&kind);m->FindInt64("generation",&gen);m->FindString("body",&body);m->FindString("error",&err);
        if((kind==ForecastRequest && gen!=forecastGeneration)||(kind==SearchRequest && gen!=searchGeneration)||(kind==LocationRequest && gen!=locationGeneration))return;
        std::string error=err;
        if(kind==ForecastRequest){
            Forecast f;if(error.empty() && ParseForecast(body,f,error)){
                state.forecast=std::move(f);state.status="";settings.cachedBody=body;settings.cachedKey=state.location.Key();settings.cachedAt=state.forecast.fetched;Save();
            }else state.status=error+(state.forecast.valid?" Showing saved forecast.":"");
        }else if(kind==SearchRequest){
            if(error.empty() && ParseLocations(body,found,error))searchStatus=found.empty()?"No matches. Try another city or postal code.":"Select the correct city and region, then Save and show.";
            else searchStatus=error;
            UpdateLocations();
        }else if(kind==LocationRequest){
            if(!settings.consent)return;
            Location l;if(error.empty() && ParseIPLocation(body,l,error)){settings.follow=true;AddLocation(l);}
            else {state.status=error+" Search for a city in Locations.";if(!state.location.Valid())OpenLocations();}
        }
        Publish();
    }
    void StopPanel(){
        if(display.InitCheck()==B_OK)display.SetPower(false);
        display.Unset();panelView=nullptr;
    }
    void StartPanel(){
        if(display.InitCheck()==B_OK)return;
        if(display.SetTo(0)!=B_OK){state.panelStatus="No available auxiliary display";if(!window)OpenWindow();Publish();return;}
        auto bitmap=display.Bitmap();
        if(!bitmap || display.SetPower(true)!=B_OK){display.Unset();state.panelStatus="Could not open auxiliary display";Publish();return;}
        panelView=new BView(bitmap->Bounds(),"Weather panel",B_FOLLOW_NONE,0);bitmap->AddChild(panelView);
        state.panelStatus=std::string("Display: ")+display.Name();Publish();
    }
    void Publish(){
        state.fahrenheit=settings.fahrenheit;if(window)window->Update(state,display.InitCheck()==B_OK);
        if(panelView && display.InitCheck()==B_OK){auto bitmap=display.Bitmap();if(bitmap && bitmap->Lock()){
            DrawWeather(panelView,bitmap->Bounds(),state,*art,true,time(nullptr));panelView->Sync();bitmap->Unlock();
            if(display.Present()==B_OK)++panelFrames;else state.panelStatus="Auxiliary display update failed";
        }}
    }
    Settings settings;ScreenState state;MainWindow* window=nullptr;LocationsWindow* locations=nullptr;
    std::unique_ptr<Artwork> art;std::unique_ptr<Network> network;std::unique_ptr<BMessageRunner> timer;
    BAuxDisplay display;BView* panelView=nullptr;int64 panelFrames=0;
    bool panelOnly=false,ready=false;Location commandLocation;bigtime_t lastRefresh=0;std::string lastRequestKey;
    int64 forecastGeneration=0,searchGeneration=0,locationGeneration=0;
    int64 locationRequests=0,forecastRequests=0;
    std::vector<Location> found;std::string searchStatus="Search for a city, then select the correct region.";
};
}
int main(int argc,char** argv){
    if(argc>1 && strcmp(argv[1],"--help")==0){puts("Weather [--panel] [--location LAT LON NAME]\n--panel runs on the local BAuxDisplay device using the selected location.\n--location explicitly selects and saves coordinates; it does not request IP location.");return 0;}
    WeatherApp app;app.Run();return 0;
}
