// Convert the bundled Meteocons artwork to Haiku's classic indexed app icons.
// Run on Haiku: g++ tools/make-app-icons.cpp -lbe -ltranslation -o /tmp/weather-icons
//              /tmp/weather-icons
#include <Application.h>
#include <Bitmap.h>
#include <IconUtils.h>
#include <TranslationUtils.h>
#include <View.h>
#include <cstdio>
#include <cassert>
#include <memory>
#include <initializer_list>
#include <unistd.h>
#include <string>
int main(){
    BApplication app("application/x-vnd.airos-WeatherIconBuilder");
    char cwd[4096];assert(getcwd(cwd,sizeof(cwd)));
    std::unique_ptr<BBitmap> source(BTranslationUtils::GetBitmap((std::string(cwd)+"/resources/icons/partly-cloudy-day.png").c_str()));assert(source);
    for(int size:{16,32}){
        BBitmap rgba(BRect(0,0,size-1,size-1),B_BITMAP_ACCEPTS_VIEWS,B_RGBA32);
        auto view=new BView(rgba.Bounds(),"icon",B_FOLLOW_NONE,0);rgba.AddChild(view);assert(rgba.Lock());
        view->SetHighColor(B_TRANSPARENT_COLOR);view->FillRect(rgba.Bounds());view->SetDrawingMode(B_OP_ALPHA);
        view->SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_COMPOSITE);
        // Trim the artwork's outer whitespace to keep the small icon legible.
        view->DrawBitmap(source.get(),BRect(28,28,227,227),rgba.Bounds());view->Sync();rgba.Unlock();
        BBitmap indexed(rgba.Bounds(),B_CMAP8);assert(BIconUtils::ConvertToCMAP8(&rgba,&indexed)==B_OK);
        char path[100];snprintf(path,sizeof(path),"resources/Weather-icon%d.raw",size);FILE* file=fopen(path,"wb");assert(file);
        assert(fwrite(indexed.Bits(),1,indexed.BitsLength(),file)==size_t(size*size));fclose(file);
    }
}
