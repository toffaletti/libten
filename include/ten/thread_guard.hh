#ifndef TEN_THREAD_GUARD_HH
#define TEN_THREAD_GUARD_HH

#include <thread>
#include <system_error>

namespace ten {

//! wrapper that calls join on joinable threads in the destructor
class thread_guard {
private:
    std::thread _thread;
public:
    template <class... Args>
    thread_guard(Args&&... args) : _thread{std::forward<Args>(args)...} {}

    thread_guard(std::thread t) : _thread{std::move(t)} {}
    thread_guard() {}
    thread_guard(const thread_guard &) = delete;
    thread_guard &operator =(const thread_guard &) = delete;
    thread_guard(thread_guard &&other) {
        if (this != &other) {
            std::swap(_thread, other._thread);
        }
    }
    thread_guard &operator=(thread_guard &&other) {
        if (this != &other) {
            std::swap(_thread, other._thread);
        }
        return *this;
    }

    ~thread_guard() {
        try {
            if (_thread.joinable()) {
                _thread.join();
            }
        } catch (std::system_error &e) {
        }
    }
};

} // ten

#endif // TEN_THREAD_GUARD_HH

