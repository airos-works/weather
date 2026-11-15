#include "Render.h"
#include <TranslationUtils.h>
#include <String.h>
#include <algorithm>

namespace weather {
namespace {
const rgb_color bg={18,27,39,255}, card={28,40,55,255}, edge={53,70,88,255};
const rgb_color ink={242,245,247,255}, muted={166,185,200,255}, blue={102,197,236,255}, gold={248,196,109,255};
void Text(BView* v,float x,float y,std::string text,float size=14,rgb_color color=ink,bool bold=false,float width=0) {
    BFont font(bold?be_bold_font:be_plain_font); font.SetSize(size); v->SetFont(&font);
    if (width>0) { BString s(text.c_str()); font.TruncateString(&s,B_TRUNCATE_END,width); text=s.String(); }
    v->SetHighColor(color); v->DrawString(text.c_str(),BPoint(x,y));
}
void Card(BView* v,BRect r) {
    v->SetHighColor(9,17,27); v->FillRoundRect(r.OffsetByCopy(0,3),12,12);
    v->SetHighColor(card); v->FillRoundRect(r,12,12); v->SetHighColor(edge); v->StrokeRoundRect(r,12,12);
    v->SetLowColor(card);
}
void Image(BView* v,const Artwork& art,int code,bool day,BRect r) {
    if (auto bitmap=art.Get(code,day)) {
        v->SetDrawingMode(B_OP_ALPHA); v->SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
        v->DrawBitmap(bitmap,bitmap->Bounds(),r); v->SetDrawingMode(B_OP_COPY);
    }
}
void RainGraph(BView* v,const Forecast& f,BRect r,time_t now) {
    std::vector<Rain> samples;
    for (const auto& a:f.rain) if (a.time>=now-900 && a.time<now+7200) { samples.push_back(a); if (samples.size()==8) break; }
    double maximum=1; for (const auto& a:samples) if (std::isfinite(a.amount)) maximum=std::max(maximum,a.amount);
    v->SetHighColor(edge); v->StrokeLine(r.LeftBottom(),r.RightBottom());
    if (samples.empty()) { Text(v,r.left,r.top+25,"Precipitation data unavailable",13,muted); return; }
    float step=r.Width()/8;
    for (size_t i=0;i<samples.size();++i) {
        auto a=samples[i]; float x=r.left+i*step;
        if (!std::isfinite(a.amount)) Text(v,x+step/2-5,r.bottom-8,"—",14,muted);
        else {
            v->SetHighColor(blue); float h=std::max(2.0,(r.Height()-22)*a.amount/maximum);
            v->FillRoundRect(BRect(x+3,r.bottom-h,x+step-4,r.bottom),3,3);
        }
        if (i%2==0) Text(v,x+2,r.bottom+18,LocalTime(a.time,f.offset,"%H:%M"),11,muted);
    }
    Text(v,r.left,r.top,Number(maximum," mm / 15 min",1),11,muted);
}
}
Artwork::Artwork(const std::string& directory) {
    for (const char* name:{"clear-day","clear-night","partly-cloudy-day","partly-cloudy-night","overcast","fog","drizzle","rain","snow","thunderstorms","not-available"})
        fImages[name].reset(BTranslationUtils::GetBitmap((directory+"/"+name+".png").c_str()));
}
BBitmap* Artwork::Get(int code,bool day) const {
    auto i=fImages.find(Icon(code,day)); return i==fImages.end()?nullptr:i->second.get();
}
WeatherView::WeatherView(const Artwork& art): BView(BRect(0,0,900,780),"forecast",B_FOLLOW_NONE,B_WILL_DRAW),fArt(art) { SetViewColor(bg); }
void WeatherView::Draw(BRect) { DrawWeather(this,Bounds(),fState,fArt,false,time(nullptr)); }
void DrawWeather(BView* v,BRect bounds,const ScreenState& s,const Artwork& art,bool compact,time_t now) {
    v->SetDrawingMode(B_OP_COPY); v->SetHighColor(bg); v->FillRect(bounds); v->SetLowColor(bg);
    const auto& f=s.forecast;
    if (compact) {
        float scale=std::min((bounds.Width()+1)/640.0f,(bounds.Height()+1)/480.0f);
        v->SetOrigin((bounds.Width()+1-640*scale)/2,(bounds.Height()+1-480*scale)/2); v->SetScale(scale);
        Text(v,30,48,s.location.Valid()?s.location.name:"Weather",30,ink,true,580);
        Text(v,30,78,s.location.approximate?"Approximate location · IP estimate":"air/OS Weather",17,muted);
        if (!f.valid) {
            Text(v,30,220,"Choose a location",38,ink,true);
            Text(v,30,265,"Open Weather on the desktop to begin.",22,muted);
            Text(v,30,420,s.status,18,gold,false,580);
        } else {
            Text(v,25,244,Temperature(f.temperature,s.fahrenheit),130,ink,true);
            Image(v,art,f.code,f.daylight,BRect(365,92,610,310));
            Text(v,33,290,Condition(f.code),30,ink,true,560);
            if (!f.days.empty() && LocalTime(f.days[0].time,f.offset,"%F")==LocalTime(now,f.offset,"%F")) Text(v,33,330,"High "+Temperature(f.days[0].high,s.fahrenheit)+"   Low "+Temperature(f.days[0].low,s.fahrenheit)+"   Feels "+Temperature(f.feels,s.fahrenheit),24,muted);
            v->SetHighColor(edge);v->StrokeLine(BPoint(30,354),BPoint(610,354));
            Text(v,30,395,RainSummary(f,now),23,blue,true,580);
            Text(v,30,429,Stale(f,now)?"STALE · connect to update":"15-minute forecast · updated "+LocalTime(f.fetched,f.offset,"%H:%M"),18,Stale(f,now)?gold:muted);
            Text(v,30,462,"Open-Meteo · CC BY 4.0",14,muted);
        }
        v->SetScale(1);v->SetOrigin(0,0); return;
    }
    float width=bounds.Width(), left=22, right=width-22, mid=width*.54f;
    Text(v,left,36,s.location.Valid()?s.location.name:"Your weather, at a glance",24,ink,true,width-44);
    Text(v,left,59,s.location.approximate?"Approximate location from your public IP · choose a city to refine it":f.timezone,12,muted);
    if (!f.valid) {
        Card(v,BRect(left,90,right,390)); Image(v,art,2,true,BRect(width/2-90,110,width/2+90,270));
        Text(v,left+25,305,s.location.Valid()?"Getting your forecast…":"Start with your location",26,ink,true);
        Text(v,left+25,344,s.location.Valid()?s.status:"Use My location, or open Locations to search for a city.",15,muted,false,width-95);
        Text(v,left,430,s.status,14,gold,false,width-44); return;
    }
    Card(v,BRect(left,80,mid-8,258)); Card(v,BRect(mid+8,80,right,258));
    Text(v,left+20,108,"RIGHT NOW",11,muted,true);
    Text(v,left+16,194,Temperature(f.temperature,s.fahrenheit),76,ink,true);
    Image(v,art,f.code,f.daylight,BRect(mid-174,91,mid-20,226));
    Text(v,left+20,227,Condition(f.code),19,ink,true,mid-left-30);
    Text(v,left+20,248,"Feels like "+Temperature(f.feels,s.fahrenheit),12,muted);
    Text(v,mid+26,108,"NEXT 2 HOURS · 15-MIN FORECAST",11,muted,true);
    Text(v,mid+26,137,RainSummary(f,now),15,blue,true,right-mid-40);
    RainGraph(v,f,BRect(mid+26,159,right-18,226),now);
    Card(v,BRect(left,274,right,430)); Text(v,left+18,300,"NEXT 24 HOURS",11,muted,true);
    Text(v,right-225,300,"Temperature  /  chance of precipitation",11,muted);
    std::vector<Hour> hours; for (const auto& h:f.hours) if (h.time>=now-1800) {hours.push_back(h); if (hours.size()==24) break;}
    if (hours.empty()) Text(v,left+18,360,"Hourly forecast unavailable",16,muted);
    else {
        double lo=1000,hi=-1000; for (auto h:hours) if(std::isfinite(h.temperature)){lo=std::min(lo,h.temperature);hi=std::max(hi,h.temperature);}
        float step=(right-left-50)/23; bool previous=false; BPoint p;
        for (size_t i=0;i<hours.size();++i) {
            float x=left+25+i*step; const auto& h=hours[i];
            if(std::isfinite(h.temperature)) {
                BPoint q(x,360-35*(h.temperature-lo)/std::max(1.0,hi-lo));
                v->SetHighColor(gold);v->SetPenSize(2.5); if(previous)v->StrokeLine(p,q);v->FillEllipse(q,2.5,2.5);v->SetPenSize(1);p=q;previous=true;
                if(i%3==0)Text(v,x-10,q.y-10,Temperature(h.temperature,s.fahrenheit),12,ink,true);
            } else previous=false;
            if(i%3==0) { Text(v,x-12,394,Number(h.probability,"%"),12,blue);Text(v,x-15,415,LocalTime(h.time,f.offset,"%H:%M"),11,muted); }
        }
    }
    Card(v,BRect(left,446,width*.65f-8,757)); Card(v,BRect(width*.65f+8,446,right,757));
    Text(v,left+18,473,"THE WEEK AHEAD",11,muted,true);
    if (f.days.empty()) Text(v,left+18,520,"Daily forecast unavailable",15,muted);
    for (size_t i=0;i<f.days.size();++i) {
        const auto& d=f.days[i]; float y=509+i*36;
        Text(v,left+18,y,LocalTime(d.time,f.offset,"%F")==LocalTime(now,f.offset,"%F")?"Today":LocalTime(d.time,f.offset,"%a"),14,ink,true);
        Image(v,art,d.code,true,BRect(left+77,y-28,left+112,y+7));
        Text(v,left+120,y,Condition(d.code),12,muted,false,width*.65f-left-280);
        Text(v,width*.65f-155,y,Number(d.probability,"%"),12,blue);
        Text(v,width*.65f-100,y,Temperature(d.low,s.fahrenheit),14,muted);
        Text(v,width*.65f-54,y,Temperature(d.high,s.fahrenheit),14,ink,true);
    }
    float x=width*.65f+28;
    Text(v,x,473,"OUTSIDE",11,muted,true);
    Text(v,x,510,"Wind",13,muted); Text(v,x,542,Number(s.fahrenheit?f.wind/1.609344:f.wind,s.fahrenheit?" mph":" km/h"),25,ink,true);
    Text(v,x,578,"Humidity",13,muted); Text(v,x,608,Number(f.humidity,"%"),25,ink,true);
    Text(v,x,644,"Pressure",13,muted); Text(v,x,675,Number(f.pressure," hPa"),25,ink,true);
    Text(v,x,717,"Updated "+LocalTime(f.fetched,f.offset,"%H:%M"),12,muted);
    Text(v,x,740,Stale(f,now)?"STALE · refresh needed":"Local time "+LocalTime(now,f.offset,"%H:%M"),12,Stale(f,now)?gold:muted);
    Text(v,left,784,s.status,12,gold,false,width-44);
    Text(v,left,807,"Forecast: Open-Meteo (CC BY 4.0) · Places: GeoNames · Artwork: Meteocons (MIT)",11,muted);
    Text(v,left,826,"15-minute precipitation is model guidance, sometimes interpolated; it is not a radar nowcast.",11,muted);
}
}
