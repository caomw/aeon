#include <chrono>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "log.hpp"

using namespace std;

string                      nervana::logger::log_path;
deque<string>               nervana::logger::queue;
static mutex                queue_mutex;
condition_variable          queue_condition;
static unique_ptr<thread>   queue_thread;
static bool                 active = false;

class thread_starter {
public:
    thread_starter() {
        nervana::logger::start();
    }
    virtual ~thread_starter() {
        nervana::logger::stop();
    }
};

static thread_starter _starter;

void nervana::logger::set_log_path(const string& path) {
    log_path = path;
}

void nervana::logger::start() {
    active = true;
    queue_thread = unique_ptr<thread>(new thread(&thread_entry, nullptr));
}

void nervana::logger::stop() {
    {
        unique_lock<std::mutex> lk(queue_mutex);
        active = false;
        queue_condition.notify_one();
    }
    queue_thread->join();
}

void nervana::logger::process_event(const string& s) {
    cout << s << "\n";
}

void nervana::logger::thread_entry(void* param) {
    unique_lock<std::mutex> lk(queue_mutex);
    while(active) {
        queue_condition.wait(lk);
        while(!queue.empty()) {
            process_event(queue.front());
            queue.pop_front();
        }
    }
}

void nervana::logger::log_item(const string& s) {
    unique_lock<std::mutex> lk(queue_mutex);
    queue.push_back(s);
    queue_condition.notify_one();
}

nervana::log_helper::log_helper(LOG_TYPE type, const char* file, int line, const char* func) {
    switch(type){
    case LOG_TYPE::_LOG_TYPE_ERROR:
        _stream << "[ERR ] ";
        break;
    case LOG_TYPE::_LOG_TYPE_WARNING:
        _stream << "[WARN] ";
        break;
    case LOG_TYPE::_LOG_TYPE_INFO:
        _stream << "[INFO] ";
        break;
    }

    std::time_t tt = chrono::system_clock::to_time_t(chrono::system_clock::now());
    auto tm = std::gmtime(&tt);
    char buffer[256];
    strftime(buffer,sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", tm);
    _stream << buffer << " ";

    _stream << file;
    _stream << " " << line;
//    _stream << " " << func;
    _stream << "\t";
}

nervana::log_helper::~log_helper() {
    logger::log_item(_stream.str());
}
