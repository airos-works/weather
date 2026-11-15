#pragma once
#include "Weather.h"
#include <string>
#include <vector>
namespace weather {
struct Settings {
    bool asked=false, consent=false, follow=false, fahrenheit=false;
    std::vector<Location> locations;
    int32_t selected=-1;
    std::string cachedBody, cachedKey;
    time_t cachedAt=0;
    bool Load(std::string& error);
    bool Save(std::string& error) const;
    bool writable=true;
};
}
