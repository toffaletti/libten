#ifndef BUFFER_HH
#define BUFFER_HH

#include <memory>
#include <cassert>
#include "logging.hh"

namespace ten {

class buffer2 {
public:
    struct head {
        uint32_t capacity;
        uint32_t front;
        uint32_t back;
        uint8_t data[1];
    } __attribute__((packed));
private:
    head *h;

public:
    buffer2(uint32_t capacity) {
        h = (head *)malloc(offsetof(head, data) + capacity);
        memset(h, 0, sizeof(head));
        h->capacity = capacity;
    }

    void compact() {
        if (h->front != 0) {
            memmove(h->data, &h->data[h->front], size());
            h->back = size();
            h->front = 0;
        }
    }

    void reserve(uint32_t n) {
        if (n > h->capacity - size()) {
            compact();
            uint32_t need = n - (h->capacity - size());
            uint32_t newcapacity = h->capacity + need;
            head *tmp = (head *)realloc(h, offsetof(head, data) + newcapacity);
            if (tmp) {
                tmp->capacity = newcapacity;
                // TODO: DVLOG_IF is missing
                VLOG_IF(4, (h != tmp)) << "realloc returned new pointer: " << h << " " << tmp;
                h = tmp;
            } else {
                throw std::bad_alloc();
            }
        } else if (n > h->capacity - h->back) {
            compact();
        }
    }

    void commit(uint32_t n) {
        if (n > h->capacity - size()) {
            throw std::runtime_error("commit too big");
        }
        h->back += n;
    }

    void remove(uint32_t n) {
        if (n > size()) {
            throw std::runtime_error("remove > size");
        }
        h->front += n;
        if (h->front == h->back) {
            h->front = 0;
            h->back = 0;
        }
    }


    uint8_t *front() const { return &h->data[h->front]; }
    uint8_t *back() const { return &h->data[h->back]; }
    uint8_t *end(uint32_t n) const { return &h->data[h->back + n]; }
    uint8_t *end() const { return &h->data[h->capacity]; }

    uint32_t size() const {
        return h->back - h->front;
    }

    ~buffer2() {
        free(h);
    }
};

//! reference counted buffer, slices hold reference
class buffer {
private:
    struct impl {
        char *p;
        size_t n;

        impl(size_t bytes=0) : p(bytes ? new char[bytes] : 0), n(bytes) {}
        ~impl() { delete[] p; }
    };
    typedef std::shared_ptr<impl> shared_impl;

    shared_impl m;

public:
    //! a pointer and length to a buffer's memory
    class slice {
    private:
        friend class buffer;
        //! hold a reference to the buffer this slice belongs to
        shared_impl m;
        char *p;
        size_t len;

        slice(const shared_impl &m_, size_t pos, size_t len_)
            : m(m_), p(0), len(len_)
        {
            // bounds check
            assert(m->p);
            assert(pos+len <= m->n);
            p = &m->p[pos];
        }
    public:

        slice() : p(0), len(0) {}

        //! create a slice from this slice
        slice operator ()(size_t pos, size_t len) {
            return slice(m, (p-m->p)+pos, len);
        }

        char &operator [](size_t pos) {
            assert(p);
            assert(pos < len);
            return p[pos];
        }

        char &operator [](size_t pos) const {
            assert(p);
            assert(pos < len);
            return p[pos];
        }

        char *data(size_t pos=0) const {
            assert(p);
            return &p[pos];
        }

        size_t size() const { return len; }

        size_t maxsize() const { return m->n - (p-m->p); }

        void resize(size_t nlen) {
            assert((p-m->p)+nlen <= m->n);
            len=nlen;
        }

    };
public:
    //! allocate a block of memory
    buffer(size_t bytes=0) : m(std::make_shared<impl>(bytes)) {}

    //! slice this buffer at pos
    slice operator ()(size_t pos) const {
        return slice(m, pos, m->n-pos);
    }

    //! slice at pos with len bytes
    slice operator ()(size_t pos, size_t len) const {
        return slice(m, pos, len);
    }
};

} // end namespace ten

#endif // BUFFER_HH

