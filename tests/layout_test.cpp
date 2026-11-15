// Native regression test: real app_server resizing, scrolling and appearance.
// --theme briefly changes the Screen preference and restores it on exit.
#include "Render.h"
#include <Application.h>
#include <Path.h>
#include <BitmapStream.h>
#include <File.h>
#include <LayoutBuilder.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>
#include <Window.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace weather;
static void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Probe : public WeatherView {
public:
    using WeatherView::WeatherView;
    std::atomic<int> draws{0};
    std::atomic<int> colors{0};
    void Draw(BRect r) override { WeatherView::Draw(r); ++draws; }
    void MessageReceived(BMessage* m) override {
        WeatherView::MessageReceived(m);
        if (m->what == B_COLORS_UPDATED) ++colors;
    }
};
static void Save(BBitmap* bitmap, const char* path) {
    BBitmapStream stream(bitmap);
    BFile output(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    auto status = BTranslatorRoster::Default()->Translate(&stream, nullptr, nullptr, &output, B_PNG_FORMAT);
    BBitmap* detached = nullptr; stream.DetachBitmap(&detached);
    Require(status == B_OK, "PNG export");
}
class TestApp : public BApplication {
public:
    TestApp(bool theme) : BApplication("application/x-vnd.airos-WeatherLayoutTests"), theme(theme), art(BPath("resources/icons").Path()) {}
    ~TestApp() override { if (worker.joinable()) worker.join(); }
    void ReadyToRun() override {
        window = new BWindow(BRect(40,80,940,780), "Weather layout regression", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS);
        view = new Probe(art);
        auto scroll = new BScrollView("scroll", view, 0, false, true, B_NO_BORDER);
        BLayoutBuilder::Group<>(window, B_VERTICAL, 0).Add(scroll);
        std::ifstream fixture("tests/forecast.json");
        std::string body((std::istreambuf_iterator<char>(fixture)), {}), error;
        ScreenState state;
        Require(ParseForecast(body, state.forecast, error), "forecast fixture");
        // Keep samples away from the live clock's inclusion boundaries, so
        // advancing a second cannot shift the hourly/rain charts mid-comparison.
        // Stale status also avoids a minute-changing clock label in the pixels.
        auto delta = time(nullptr) + 120 - state.forecast.currentTime;
        state.forecast.currentTime += delta; state.forecast.fetched = time(nullptr) - 7200;
        for (auto& h : state.forecast.hours) h.time += delta;
        for (auto& d : state.forecast.days) d.time += delta;
        for (auto& r : state.forecast.rain) r.time += delta;
        state.location = {"Hobart, Tasmania", -42.88, 147.33, false};
        view->SetState(state);
        window->Show();
        worker = std::thread([this] { RunChecks(); });
    }
    int result = 1;
private:
    void SetSize(float width, float height) {
        Require(window->Lock(), "window lock");
        window->ResizeTo(width, height); window->Unlock();
        snooze(100000);
    }
    std::unique_ptr<BBitmap> Capture() {
        Require(window->Lock(), "capture lock");
        BRect frame = view->ConvertToScreen(view->Bounds());
        view->Sync(); window->Unlock();
        BBitmap* bitmap = nullptr;
        Require(BScreen(window).GetBitmap(&bitmap, false, &frame) == B_OK, "screen capture");
        return std::unique_ptr<BBitmap>(bitmap);
    }
    void CheckPaint() {
        auto before = Capture();
        Require(window->Lock(), "paint lock");
        view->Invalidate(); window->Unlock(); snooze(100000);
        auto after = Capture();
        Require(before->BitsLength() == after->BitsLength(), "capture dimensions");
        // Compare visible pixels, excluding undefined row padding and alpha.
        size_t different = 0;
        for (int y=0; y<=before->Bounds().IntegerHeight(); ++y) {
            auto a=static_cast<const uint8*>(before->Bits()) + y*before->BytesPerRow();
            auto b=static_cast<const uint8*>(after->Bits()) + y*after->BytesPerRow();
            for(int x=0; x<=before->Bounds().IntegerWidth(); ++x)
                if (memcmp(a+x*4,b+x*4,3)!=0) ++different;
        }
        if (different) {
            Save(before.get(), "/tmp/weather-resize-before.png");
            Save(after.get(), "/tmp/weather-resize-after.png");
            std::cerr << different << " pixels changed after forced repaint\n";
        }
        Require(different == 0, "resize left stale pixels");
    }
    void CheckScroll(bool expected) {
        Require(window->Lock(), "scroll lock");
        float low, high; view->ScrollBar(B_VERTICAL)->GetRange(&low,&high);
        bool valid = (high > 0) == expected && view->Bounds().top <= high + .01f;
        bool frameValid = view->Frame().Height() < window->Bounds().Height() + 1;
        window->Unlock();
        Require(valid, "scroll range/clamping"); Require(frameValid, "viewport owned by layout");
    }
    void CheckTheme() {
        using Get = bool (*)(); using Set = void (*)(bool);
        auto get = reinterpret_cast<Get>(dlsym(RTLD_DEFAULT,"_Z12ui_dark_modev"));
        auto set = reinterpret_cast<Set>(dlsym(RTLD_DEFAULT,"_Z16set_ui_dark_modeb"));
        Require(get && set, "Screen appearance API");
        struct Restore { Set set; bool value; ~Restore() { set(value); snooze(300000); } } restore{set,get()};
        for (bool dark : {!restore.value, restore.value}) {
            int colors = view->colors, draws = view->draws;
            set(dark);
            for (int i=0;i<100 && (view->colors==colors || view->draws==draws);++i) snooze(20000);
            Require(view->colors > colors && view->draws > draws, "live theme notification and repaint");
            Require(SystemPalette().dark == dark, "Screen flag honored");
            snooze(100000);
            auto bitmap = Capture();
            auto pixel = static_cast<const uint8*>(bitmap->Bits()) + 5*bitmap->BytesPerRow()+5*4;
            auto bg = PaletteFor(dark).background;
            Require(pixel[0]==bg.blue && pixel[1]==bg.green && pixel[2]==bg.red, "visible theme background");
            Save(bitmap.get(),dark?"/tmp/weather-live-dark.png":"/tmp/weather-live-light.png");
        }
    }
    void RunChecks() {
        try {
            snooze(300000);
            for (int width : {420,600,799,800,980,1200,1400})
                for (int height : {320,700,1100}) {
                    SetSize(width,height); CheckPaint();
                    Require(window->Lock(), "layout lock");
                    auto l=view->Layout(); auto b=view->Bounds(); window->Unlock();
                    Require(l.stacked == (b.Width()+1 < 800), "responsive breakpoint");
                    Require(l.current.right < b.Width()+1 && l.outside.bottom <= l.height, "content containment");
                }
            SetSize(420,400); CheckScroll(true);
            Require(window->Lock(), "scroll lock");
            float low,high; auto bar=view->ScrollBar(B_VERTICAL); bar->GetRange(&low,&high); bar->SetValue(high); window->Unlock();
            snooze(100000); CheckPaint();
            SetSize(1200,1000); CheckScroll(false); CheckPaint();
            if (theme) CheckTheme();
            std::cout << "PASS: 21 live resize/repaint comparisons, narrow reflow, scrolling and clamping"
                << (theme?", Screen light/dark notifications and pixels":"") << std::endl;
            result=0;
        } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << std::endl; }
        if(window->Lock()) { window->Quit(); window=nullptr; }
        PostMessage(B_QUIT_REQUESTED);
    }
    bool theme; Artwork art; BWindow* window=nullptr; Probe* view=nullptr; std::thread worker;
};
int main(int argc,char** argv) {
    TestApp app(argc>1 && std::string(argv[1])=="--theme"); app.Run(); return app.result;
}
