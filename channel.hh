#ifndef CHANNEL_HH
#define CHANNEL_HH

#include "task.hh"
#include <memory>
#include <queue>
#include <deque>

namespace ten {

struct channel_closed_error : std::exception {
    virtual const char *what() const throw () { return "ten::channel_closed_error"; }
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
    struct impl : boost::noncopyable {
        typedef typename ContainerT::size_type size_type;

        impl(size_type capacity_=0) : capacity(capacity_), unread(0),
            queue(ContainerT()), closed(false) {}

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
    };

public:
    //! create a new channel
    //! \param capacity number of items to buffer. the default is 0, unbuffered.
    explicit channel(typename impl::size_type capacity=0, bool autoclose_=false)
        : m(std::make_shared<impl>(capacity)), autoclose(autoclose_)
    {
    }

    channel(const channel &other) : m(other.m), autoclose(false) {}
    channel &operator = (const channel &other) {
        m = other.m;
        autoclose = false;
        return *this;
    }

    ~channel() {
        if (autoclose) {
            close();
        }
    }

    //! send data
    typename impl::size_type send(T &&p) {
        std::unique_lock<qutex> l(m->qtx);
        typename impl::size_type ret = m->unread;
        while (m->is_full() && !m->closed) {
            m->not_full.sleep(l);
        }
        check_closed();
        m->queue.push(std::move(p));
        ++m->unread;
        m->not_empty.wakeup();
        return ret;
    }

    //! receive data
    T recv() {
        std::unique_lock<qutex> l(m->qtx);
        bool unbuffered = m->capacity == 0;
        if (unbuffered) {
            // grow the capacity for a single item
            m->capacity = 1;
            // unblock sender
            m->not_full.wakeup();
        }
        while (m->is_empty() && !m->closed) {
            m->not_empty.sleep(l);
        }
        if (m->unread == 0) {
            check_closed();
        }

        --m->unread;
        T item(std::move(m->queue.front()));
        m->queue.pop();
        if (unbuffered) {
            // shrink capacity again so sends will block
            // waiting for recv
            m->capacity = 0;
        } else {
            m->not_full.wakeup();
        }
        return item;
    }

    // TODO: rewrite this
#if 0
    //! timed receive data
    bool timed_recv(T &item, unsigned int ms) {
        std::unique_lock<qutex> l(m->qtx);
        bool unbuffered = m->capacity == 0;
        if (unbuffered) {
            // grow the capacity for a single item
            m->capacity = 1;
            // unblock sender
            m->not_full.wakeup();
        }
        while (m->is_empty() && !m->closed) {
            if (!m->not_empty.sleep_for(l, ms)) return false;
        }
        if (m->unread == 0) {
            check_closed();
        }

        // we don't pop_back because the item will just get overwritten
        // when the circular buffer wraps around
        item = m->container[--m->unread];
        if (unbuffered) {
            // shrink capacity again so sends will block
            // waiting for recv
            m->capacity = 0;
        } else {
            m->not_full.wakeup();
        }
        return true;
    }
#endif

    bool empty() {
        return unread() == 0;
    }

    //! \return number of unread items
    size_t unread() {
        std::unique_lock<qutex> lock(m->qtx);
        return m->unread;
    }

    void close() {
        std::unique_lock<qutex> l(m->qtx);
        m->closed = true;
        // wake up all users of channel
        m->not_empty.wakeupall();
        m->not_full.wakeupall();
    }

    void clear() {
        std::unique_lock<qutex> l(m->qtx);
        while (!m->queue.empty()) {
            m->queue.pop();
        }
        m->unread = 0;
        m->not_full.wakeupall();
    }
private:
    std::shared_ptr<impl> m;
    bool autoclose;

    void check_closed() {
        // i dont like throwing an exception for this
        // but i don't want to complicate the interface for send/recv
        if (m->closed) throw channel_closed_error();
    }
};

} // end namespace ten

#endif // CHANNEL_HH

