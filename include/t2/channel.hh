#ifndef T2_CHANNEL_HH
#define T2_CHANNEL_HH

#include "ten/optional.hh"
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>

#include "t2/double_lock_queue.hh"

namespace t2 {

#if 0

template <typename T>
class channel {
private:
    struct pimpl;
    std::shared_ptr<pimpl> _pimpl;
    bool _autoclose;
public:
    channel(bool autoclose=false);
    ~channel();
    void close() noexcept;

#if 0
    bool send(const T &t) {
        std::lock_guard<std::mutex> lock(_pimpl->mutex);
        if (_pimpl->closed) return false;
        _pimpl->queue.push(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }
#endif

    bool send(T &&t) {
        if (_pimpl->closed) return false;
        _pimpl->queue.push(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }

    ten::optional<T> recv() {
        T value;
        uint64_t spin_count = 0;
        while (!_pimpl->queue.pop(value)) {
            ++spin_count;
            if (_pimpl->closed) return {};
            if (spin_count > 4000) {
                std::unique_lock<std::mutex> lock(_pimpl->mutex);
                _pimpl->not_empty.wait(lock);
            }
        }
        return value;
    }

    std::vector<T> recv_all() {
        std::vector<T> all;
        T value;
        while (_pimpl->queue.pop(value)) {
            all.push_back(std::move(value));
        }
        return all;
    }
};

template <typename T>
struct channel<T>::pimpl {
    double_lock_queue<T> queue;
    std::condition_variable not_empty;
    std::mutex mutex;
    std::atomic<bool> closed;
};

template <typename T>
channel<T>::channel(bool autoclose)
    : _autoclose(autoclose)
{
    _pimpl = std::make_shared<pimpl>();
}

template <typename T>
channel<T>::~channel() {
    if (_autoclose) close();
}

template <typename T>
void channel<T>::close() noexcept {
    if (_pimpl->closed.exchange(true) == false) {
        _pimpl->not_empty.notify_all();
    }
}

#else

template <typename T>
class channel {
private:
    struct pimpl;
    std::shared_ptr<pimpl> _pimpl;
    bool _autoclose;
public:
    channel(bool autoclose=false);
    ~channel();
    void close() noexcept;

    bool send(const T &t) {
        std::lock_guard<std::mutex> lock(_pimpl->mutex);
        if (_pimpl->closed) return false;
        _pimpl->queue.push_back(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }

    bool send(T &&t) {
        std::lock_guard<std::mutex> lock(_pimpl->mutex);
        if (_pimpl->closed) return false;
        _pimpl->queue.push_back(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }

    template <class ...Args>
        bool emplace_send(Args&& ...args) {
            std::lock_guard<std::mutex> lock(_pimpl->mutex);
            if (_pimpl->closed) return false;
            _pimpl->queue.emplace_back(std::forward<Args>(args)...);
            _pimpl->not_empty.notify_one();
            return true;
        }

    ten::optional<T> recv() {
        std::unique_lock<std::mutex> lock(_pimpl->mutex);
        while (_pimpl->queue.empty() && !_pimpl->closed) {
            _pimpl->not_empty.wait(lock);
        }
        if (_pimpl->queue.empty()) return {};
        ten::optional<T> t(std::move(_pimpl->queue.front()));
        _pimpl->queue.pop_front();
        return t;
    }

    std::vector<T> recv_all() {
        std::unique_lock<std::mutex> lock(_pimpl->mutex);
        std::vector<T> all;
        all.reserve(_pimpl->queue.size());
        std::move(begin(_pimpl->queue), end(_pimpl->queue), std::back_inserter(all));
        _pimpl->queue.clear();
        return all;
    }
};

template <typename T>
struct channel<T>::pimpl {
    std::deque<T> queue;
    std::mutex mutex;
    std::condition_variable not_empty;
    bool closed = false;
};

template <typename T>
channel<T>::channel(bool autoclose)
    : _autoclose(autoclose)
{
    _pimpl = std::make_shared<pimpl>();
}

template <typename T>
channel<T>::~channel() {
    if (_autoclose) close();
}

template <typename T>
void channel<T>::close() noexcept {
    std::lock_guard<std::mutex> lock(_pimpl->mutex);
    if (!_pimpl->closed) {
        _pimpl->closed = true;
        _pimpl->not_empty.notify_all();
    }
}
#endif

} // t2

#endif
