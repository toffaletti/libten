#ifndef LIBTEN_CHANNEL_HH
#define LIBTEN_CHANNEL_HH

#include "task.hh"
#include "task/qutex.hh"
#include "task/rendez.hh"
#include <memory>
#include <queue>
#include <deque>

namespace ten {

struct channel_closed_error : std::exception {
    virtual const char *what() const noexcept override {
        return "ten::channel_closed_error";
    }
};

//! send and receive data between tasks in FIFO order
//
//! channels can be buffered or unbuffered.
//! unbuffered channels block on every send until recv
//! buffered channels only block send when the buffer is full
//! data is copied, so if you need to send large data
//! its best to allocate them on the heap and send the pointer
//! share by communicating.
//! channels are thread and task safe.
template <typename T, typename ContainerT = std::deque<T> > class channel {
private:
    struct impl;
    std::shared_ptr<impl> _m;
    bool _autoclose;
public:
    //! create a new channel
    //! \param capacity number of items to buffer. the default is 0, unbuffered.
    explicit channel(typename impl::size_type capacity=0, bool autoclose=false)
        : _m(std::make_shared<impl>(capacity)), _autoclose(autoclose)
    {
    }

    channel(const channel &other) : _m(other._m), _autoclose(false) {}
    channel &operator = (const channel &other) {
        _m = other._m;
        _autoclose = false;
        return *this;
    }

    ~channel() {
        if (_autoclose) {
            close();
        }
    }

    //! send data
    typename impl::size_type send(T &&p) {
        return _m->send(std::move(p));
    }

    //! receive data
    T recv() {
       return _m->recv(); 
    }

    bool empty() {
        return unread() == 0;
    }

    //! \return number of unread items
    size_t unread() {
        std::lock_guard<qutex> lock(_m->qtx);
        return _m->unread;
    }

    bool is_closed() {
        std::lock_guard<qutex> l(_m->qtx);
        return _m->closed;
    }

    void close() {
        _m->close();
    }

    void clear() {
        _m->clear();
    }
private:
    struct impl {
        typedef typename ContainerT::size_type size_type;
        impl(size_type capacity_=0) : capacity(capacity_), unread(0),
            queue(ContainerT()), closed(false) {}
        impl(const impl &) = delete;
        impl &operator =(const impl &) = delete;

        // unbuffered channels use 0 capacity so send
        // will block waiting for recv on empty channel
        size_type capacity;
        size_type unread;
        std::queue<T, ContainerT> queue;
        qutex qtx;
        rendez not_empty;
        rendez not_full;
        bool closed;

        bool is_empty() const { return unread == 0; }
        bool is_full() const { return unread >= capacity; }

        size_type send(T &&p) {
            std::unique_lock<qutex> lk(qtx);
            size_type ret = unread;
            while (is_full() && !closed) {
                not_full.sleep(lk);
            }
            check_closed();
            queue.push(std::move(p));
            ++unread;
            not_empty.wakeup();
            return ret;
        }

        T recv() {
            std::unique_lock<qutex> lk(qtx);
            bool unbuffered = capacity == 0;
            if (unbuffered) {
                // grow the capacity for a single item
                capacity = 1;
                // unblock sender
                not_full.wakeup();
            }
            while (is_empty() && !closed) {
                not_empty.sleep(lk);
            }
            if (unread == 0) {
                check_closed();
            }

            --unread;
            T item(std::move(queue.front()));
            queue.pop();
            if (unbuffered) {
                // shrink capacity again so sends will block
                // waiting for recv
                capacity = 0;
            } else {
                not_full.wakeup();
            }
            return item;
        }

        void check_closed() {
            // i dont like throwing an exception for this
            // but i don't want to complicate the interface for send/recv
            if (closed) throw channel_closed_error();
        }

        void close() {
            std::lock_guard<qutex> l(qtx);
            closed = true;
            // wake up all users of channel
            not_empty.wakeupall();
            not_full.wakeupall();
        }

        void clear() {
            std::lock_guard<qutex> l(qtx);
            while (!queue.empty()) {
                queue.pop();
            }
            unread = 0;
            not_full.wakeupall();
        }
    };
};

} // end namespace ten

#endif // LIBTEN_CHANNEL_HH
