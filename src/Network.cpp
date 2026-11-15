#include "Network.h"
#include <DataIO.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <UrlProtocolRoster.h>
#include <Url.h>
#include <Message.h>
#include <OS.h>
#include <memory>
#include <algorithm>

namespace weather {
namespace {
// Bound memory while receiving, not merely after the response has arrived.
class LimitedIO : public BMallocIO {
public:
    ssize_t WriteAt(off_t pos, const void* data, size_t size) override {
        if (pos<0 || pos>2*1024*1024 || size>2*1024*1024-size_t(pos)) return B_FILE_TOO_LARGE;
        return BMallocIO::WriteAt(pos,data,size);
    }
};
}
std::string Fetch(const std::string& url, std::string& error) {
    using namespace BPrivate::Network;
    LimitedIO output;
    std::unique_ptr<BUrlRequest> base(BUrlProtocolRoster::MakeRequest(BUrl(url.c_str(), false), &output));
    auto request=dynamic_cast<BHttpRequest*>(base.get());
    if (!request) { error="Could not start an HTTPS connection."; return ""; }
    request->SetUserAgent("airOS-Weather/1.0 (https://github.com/airos-works/weather)");
    request->SetFollowLocation(false); request->SetTimeout(20000000);
    thread_id thread=request->Run();
    if (thread<0) { error="Could not start the network worker."; return ""; }
    bigtime_t deadline=system_time()+20000000; bool timeout=false;
    while (wait_for_thread_etc(thread,B_RELATIVE_TIMEOUT,100000,nullptr)==B_TIMED_OUT) {
        if (!timeout && system_time()>deadline) { timeout=true; request->Stop(); }
    }
    auto result=dynamic_cast<const BHttpResult*>(&request->Result());
    if (timeout) error="The weather service timed out. Try again shortly.";
    else if (request->Status()!=B_OK) error="Unable to connect securely. Check your connection and system clock.";
    else if (!result || result->StatusCode()!=200) {
        if (result && result->StatusCode()==429) error="The free service is busy. Please try again in a few minutes.";
        else error="The weather service is unavailable. Please try again later.";
    }
    if (!error.empty()) return "";
    if (output.BufferLength()==0) { error="The service returned an empty response."; return ""; }
    return std::string(static_cast<const char*>(output.Buffer()),output.BufferLength());
}
Network::Network(BMessenger target): fTarget(target), fThread([this]{Run();}) {}
Network::~Network() {
    { std::lock_guard<std::mutex> lock(fMutex); fStop=true; fQueue.clear(); }
    fWake.notify_one(); fThread.join();
}
void Network::Submit(Request request) {
    { std::lock_guard<std::mutex> lock(fMutex);
      fQueue.erase(std::remove_if(fQueue.begin(),fQueue.end(),[&](const Request& r){return r.kind==request.kind;}),fQueue.end());
      fQueue.push_back(std::move(request)); }
    fWake.notify_one();
}
void Network::Run() {
    for (;;) {
        Request r;
        { std::unique_lock<std::mutex> lock(fMutex); fWake.wait(lock,[this]{return fStop || !fQueue.empty();});
          if (fStop) return;
          r=fQueue.front(); fQueue.pop_front(); }
        std::string error,body=Fetch(r.url,error);
        BMessage m(NetworkResult); m.AddInt32("kind",r.kind); m.AddInt64("generation",r.generation);
        m.AddString("body",body.c_str()); m.AddString("error",error.c_str());
        fTarget.SendMessage(&m,static_cast<BHandler*>(nullptr),1000000);
    }
}
}
