#include "Settings.h"
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <sys/stat.h>
#include <unistd.h>

namespace weather {
namespace {
std::string Path() {
    const char* override=getenv("WEATHER_SETTINGS_DIR");
    BPath path;
    if (override && *override) path.SetTo(override);
    else { find_directory(B_USER_SETTINGS_DIRECTORY,&path); path.Append("Weather"); }
    create_directory(path.Path(),0700); path.Append("settings"); return path.Path();
}
}
bool Settings::Load(std::string& error) {
    BFile file(Path().c_str(),B_READ_ONLY);
    if (file.InitCheck()==B_ENTRY_NOT_FOUND) return true;
    BMessage m; off_t size=0;
    if (file.InitCheck()!=B_OK || file.GetSize(&size)!=B_OK || size>3*1024*1024 || m.Unflatten(&file)!=B_OK) {
        writable=false; error="Settings could not be read; the original file is preserved. Changes will not be saved."; return false;
    }
    m.FindBool("asked",&asked); m.FindBool("consent",&consent); m.FindBool("follow",&follow); m.FindBool("fahrenheit",&fahrenheit);
    follow=follow&&consent;
    m.FindInt32("selected",&selected); locations.clear();
    BMessage item;
    for (int32 i=0; i<50 && m.FindMessage("location",i,&item)==B_OK; ++i) {
        Location l; const char* s=nullptr; if (item.FindString("name",&s)==B_OK) l.name=s;
        item.FindDouble("lat",&l.latitude); item.FindDouble("lon",&l.longitude); item.FindBool("approximate",&l.approximate);
        if (l.Valid()) locations.push_back(l);
    }
    if (selected<0 || size_t(selected)>=locations.size()) selected=locations.empty() ? -1 : 0;
    const char* s=nullptr;
    if (m.FindString("cache",&s)==B_OK) cachedBody=s;
    if (m.FindString("cacheKey",&s)==B_OK) cachedKey=s;
    int64 fetched=0; m.FindInt64("cacheAt",&fetched); cachedAt=fetched;
    return true;
}
bool Settings::Save(std::string& error) const {
    if (!writable) { error="Settings are read-only because the existing file could not be read."; return false; }
    BMessage m('wset'); m.AddBool("asked",asked); m.AddBool("consent",consent); m.AddBool("follow",follow); m.AddBool("fahrenheit",fahrenheit);
    m.AddInt32("selected",selected);
    for (const auto& l:locations) {
        BMessage item; item.AddString("name",l.name.c_str()); item.AddDouble("lat",l.latitude); item.AddDouble("lon",l.longitude);
        item.AddBool("approximate",l.approximate); m.AddMessage("location",&item);
    }
    m.AddString("cache",cachedBody.c_str()); m.AddString("cacheKey",cachedKey.c_str()); m.AddInt64("cacheAt",cachedAt);
    auto path=Path(), temp=path+".tmp";
    BFile file(temp.c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    bool ok=file.InitCheck()==B_OK && chmod(temp.c_str(),0600)==0 && m.Flatten(&file)==B_OK && file.Sync()==B_OK;
    file.Unset();
    if (ok) ok=rename(temp.c_str(),path.c_str())==0;
    if (!ok) { unlink(temp.c_str()); error="Could not save Weather settings. Check free space and permissions."; }
    return ok;
}
}
