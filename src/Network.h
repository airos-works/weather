#pragma once
#include <Messenger.h>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <thread>
#include <string>

namespace weather {
constexpr uint32 NetworkResult = 'netR';
struct Request { int32 kind; int64 generation; std::string url; };
std::string Fetch(const std::string& url, std::string& error);
class Network {
public:
    explicit Network(BMessenger target);
    ~Network();
    void Submit(Request request);
private:
    void Run();
    BMessenger fTarget;
    std::mutex fMutex;
    std::condition_variable fWake;
    std::deque<Request> fQueue;
    bool fStop = false;
    std::thread fThread;
};
}
