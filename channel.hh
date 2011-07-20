#ifndef CHANNEL_HH
#define CHANNEL_HH

#include <boost/circular_buffer.hpp>
#include "thread.hh"

// based on bounded_buffer example
// http://www.boost.org/doc/libs/1_41_0/libs/circular_buffer/doc/circular_buffer.html#boundedbuffer
class channel : boost::noncopyable {
public:
    typedef boost::circular_buffer<void *> container_type;
    typedef container_type::size_type size_type;

    explicit channel(size_type capacity) : m_unread(0), m_container(capacity) {}

    template <typename T> size_type send(T *p) {
        mutex::scoped_lock l(m_mutex);
        size_type unread = m_unread;
        // TODO: maybe if capacity is 1 then we block
        // waiting for m_not_full after the send.
        while (is_full()) {
            m_not_full.wait(l);
        }
        m_container.push_front(p);
        ++m_unread;
        l.unlock();
        m_not_empty.signal();
        return unread;
    }

    size_type send(uintptr_t p) {
        return send((void *)p);
    }

    template <typename T> T recv() {
        T item;
        mutex::scoped_lock l(m_mutex);
        while (is_empty()) {
            m_not_empty.wait(l);
        }
        // we don't pop_back because the item will just get overwritten
        item = reinterpret_cast<T>(m_container[--m_unread]);
        l.unlock();
        m_not_full.signal();
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
    size_type m_unread;
    container_type m_container;
    mutex m_mutex;
    // using task-aware conditions
    task::condition m_not_empty;
    task::condition m_not_full;

    bool is_empty() const { return m_unread == 0; }
    bool is_full() const { return m_unread >= m_container.capacity(); }
};

#endif // CHANNEL_HH

