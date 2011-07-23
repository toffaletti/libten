#ifndef CHANNEL_HH
#define CHANNEL_HH

#include <boost/circular_buffer.hpp>
#include "thread.hh"

// based on bounded_buffer example
// http://www.boost.org/doc/libs/1_41_0/libs/circular_buffer/doc/circular_buffer.html#boundedbuffer

//! send and receive pointers between tasks
template <typename T> class channel : boost::noncopyable {
public:
    typedef typename boost::circular_buffer<T> container_type;
    typedef typename container_type::size_type size_type;

    //! default is unbuffered (send will block until recv)
    channel(size_type capacity=0) : m_capacity(capacity), m_unread(0),
        m_container(capacity ? capacity : 1) {}

    size_type send(T p) {
        mutex::scoped_lock l(m_mutex);
        size_type unread = m_unread;
        while (is_full()) {
            m_not_full.wait(l);
        }
        m_container.push_front(p);
        ++m_unread;
        m_not_empty.signal();
        return unread;
    }

    T recv() {
        T item;
        mutex::scoped_lock l(m_mutex);
        bool unbuffered = m_capacity == 0;
        if (unbuffered) {
            // grow the capacity for a single item
            m_capacity = 1;
            // unblock sender
            m_not_full.signal();
        }
        while (is_empty()) {
            m_not_empty.wait(l);
        }
        // we don't pop_back because the item will just get overwritten
        // when the circular buffer wraps around
        item = m_container[--m_unread];
        if (unbuffered) {
            // shrink capacity again so sends will block
            // waiting for recv
            m_capacity = 0;
        } else {
            m_not_full.signal();
        }
        return item;
    }

    bool empty() {
        return unread() == 0;
    }

    size_t unread() {
        mutex::scoped_lock lock(m_mutex);
        return m_unread;
    }
private:
    // m_capacity is different than m_container.capacity()
    // to avoid needing to reallocate the container
    // on every send/recv for unbuffered channels
    size_type m_capacity;
    size_type m_unread;
    container_type m_container;
    mutex m_mutex;
    // using task-aware conditions
    task::condition m_not_empty;
    task::condition m_not_full;

    bool is_empty() const { return m_unread == 0; }
    bool is_full() const { return m_unread >= m_capacity; }
};

#endif // CHANNEL_HH

