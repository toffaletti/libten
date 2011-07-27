#ifndef CHANNEL_HH
#define CHANNEL_HH

#include <boost/shared_ptr.hpp>
#include <boost/circular_buffer.hpp>
#include "thread.hh"
#include "task.hh"

// based on bounded_buffer example
// http://www.boost.org/doc/libs/1_41_0/libs/circular_buffer/doc/circular_buffer.html#boundedbuffer

namespace detail {
template <typename T> struct impl : boost::noncopyable {
    typedef typename boost::circular_buffer<T> container_type;
    typedef typename container_type::size_type size_type;

    impl(size_type capacity_=0) : capacity(capacity_), unread(0),
        container(capacity ? capacity : 1) {}

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

    bool is_empty() const { return unread == 0; }
    bool is_full() const { return unread >= capacity; }
};

} // end namespace detail

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
public:
    //! create a new channel
    //! \param capacity number of items to buffer. the default is 0, unbuffered.
    channel(typename detail::impl<T>::size_type capacity=0) {
       m.reset(new detail::impl<T>(capacity));
    }

    //! send data
    typename detail::impl<T>::size_type send(T p) {
        mutex::scoped_lock l(m->mtx);
        typename detail::impl<T>::size_type unread = m->unread;
        while (m->is_full()) {
            m->not_full.wait(l);
        }
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
        while (m->is_empty()) {
            m->not_empty.wait(l);
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
private:
    boost::shared_ptr<detail::impl<T> > m;
};

#endif // CHANNEL_HH

