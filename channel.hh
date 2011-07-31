#ifndef CHANNEL_HH
#define CHANNEL_HH

#include <boost/shared_ptr.hpp>
#include <boost/circular_buffer.hpp>
#include "thread.hh"
#include "task.hh"

// based on bounded_buffer example
// http://www.boost.org/doc/libs/1_41_0/libs/circular_buffer/doc/circular_buffer.html#boundedbuffer

// TODO: use http://www.boost.org/doc/libs/1_47_0/libs/smart_ptr/intrusive_ptr.html
// to close the channel if the reference count reaches 1 while in a blocking send/recv

struct channel_closed_error : std::exception {};
//! tried to block on a channel with no other references
struct channel_unique_error : std::exception {};

//! send and receive data between tasks in FIFO order
//
//! channels can be buffered or unbuffered.
//! unbuffered channels block on every send until recv
//! buffered channels only block send when the buffer is full
//! data is copied, so if you need to send large data
//! its best to allocate them on the heap and send the pointer
//! share by communicating!
//! channels are thread and task safe.
template <typename T> class channel {
private:
    struct impl : boost::noncopyable {
        typedef typename boost::circular_buffer<T> container_type;
        typedef typename container_type::size_type size_type;

        impl(size_type capacity_=0) : capacity(capacity_), unread(0),
            container(capacity ? capacity : 1), closed(false) {}

        // capacity is different than container.capacity()
        // to avoid needing to reallocate the container
        // on every send/recv for unbuffered channels
        size_type capacity;
        size_type unread;
        container_type container;
        mutex mtx;
        // using task-aware conditions
        task::condition not_empty;
        task::condition not_full;
        bool closed;

        bool is_empty() const { return unread == 0; }
        bool is_full() const { return unread >= capacity; }
    };

public:
    //! create a new channel
    //! \param capacity number of items to buffer. the default is 0, unbuffered.
    channel(typename impl::size_type capacity=0) {
       m.reset(new impl(capacity));
    }

    //! send data
    typename impl::size_type send(T p) {
        mutex::scoped_lock l(m->mtx);
        typename impl::size_type unread = m->unread;
        while (m->is_full() && !m.unique() && !m->closed) {
            m->not_full.wait(l);
        }
        check_closed();
        m->container.push_front(p);
        ++m->unread;
        m->not_empty.signal();
        return unread;
    }

    //! receive data
    T recv() {
        T item;
        mutex::scoped_lock l(m->mtx);
        bool unbuffered = m->capacity == 0;
        if (unbuffered) {
            // grow the capacity for a single item
            m->capacity = 1;
            // unblock sender
            m->not_full.signal();
        }
        while (m->is_empty() && !m.unique() && !m->closed) {
            m->not_empty.wait(l);
        }
        if (m->unread == 0) {
            check_closed();
            if (m.unique()) {
                throw channel_unique_error();
            }
        }

        // we don't pop_back because the item will just get overwritten
        // when the circular buffer wraps around
        item = m->container[--m->unread];
        if (unbuffered) {
            // shrink capacity again so sends will block
            // waiting for recv
            m->capacity = 0;
        } else {
            m->not_full.signal();
        }
        return item;
    }

    bool empty() {
        return unread() == 0;
    }

    //! \return number of unread items
    size_t unread() {
        mutex::scoped_lock lock(m->mtx);
        return m->unread;
    }

    void close() {
        mutex::scoped_lock l(m->mtx);
        m->closed = true;
        // wake up all users of channel
        m->not_empty.broadcast();
        m->not_full.broadcast();
    }
private:
    boost::shared_ptr<impl> m;

    void check_closed() {
        // i dont like throwing an exception for this
        // but i don't want to complicate the interface for send/recv
        if (m->closed) throw channel_closed_error();
    }
};

#endif // CHANNEL_HH

