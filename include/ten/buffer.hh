#ifndef LIBTEN_BUFFER_HH
#define LIBTEN_BUFFER_HH

#include <stdexcept>
#include <memory>
#include <cassert>
#include <cstddef>
#include <string.h>

namespace ten {

//! wrapper around a malloced buffer used for io
//
//! buffer keeps track of its capacity and how much is used.
//! before reading, call reserve() to make sure there is enough
//! space at back to read into. after reading call commit()
//! when writing use front() and size(). after writing use
//! remove().
class buffer {
public:
    struct head {
        uint32_t capacity;
        uint32_t front;
        uint32_t back;
        char data[1];
    } __attribute__((packed));
private:
    head *_h;

public:
    buffer(uint32_t capacity) {
        _h = (head *)malloc(offsetof(head, data) + capacity);
        memset(_h, 0, sizeof(head));
        _h->capacity = capacity;
    }

    buffer(const buffer &) = delete;
    buffer &operator =(const buffer &) = delete;

    //! force commited memory to be moved to the front of the buffer
    //! making the most space available at the back
    void compact() {
        if (_h->front != 0) {
            memmove(_h->data, &_h->data[_h->front], size());
            _h->back = size();
            _h->front = 0;
        }
    }

    //! reserve n bytes past back
    //! this can either compact() or realloc() to make room
    void reserve(uint32_t n) {
        if (n > potential()) {
            compact();
            uint32_t need = n - potential();
            uint32_t newcapacity = _h->capacity + need;
            head *tmp = (head *)realloc(_h, offsetof(head, data) + newcapacity);
            if (tmp) {
                tmp->capacity = newcapacity;
                // TODO: DVLOG_IF is missing
                //VLOG_IF(4, (h != tmp)) << "realloc returned new pointer: " << h << " " << tmp;
                _h = tmp;
            } else {
                throw std::bad_alloc();
            }
        } else if (n > available()) {
            compact();
        }
    }

    //! mark n bytes after back as used
    void commit(uint32_t n) {
        if (n > available()) {
            throw std::runtime_error("commit too big");
        }
        _h->back += n;
    }

    //! move front towards back by n bytes
    void remove(uint32_t n) {
        if (n > size()) {
            throw std::runtime_error("remove > size");
        }
        _h->front += n;
        if (_h->front == _h->back) {
            _h->front = 0;
            _h->back = 0;
        }
    }

    //! read from here
    char *front() const { return &_h->data[_h->front]; }
    //! write to here
    char *back() const { return &_h->data[_h->back]; }
    char *end(uint32_t n) const { return &_h->data[_h->back + n]; }
    char *end() const { return &_h->data[_h->capacity]; }

    //! size of commited space
    uint32_t size() const {
        return _h->back - _h->front;
    }

    //! space available to be commited
    uint32_t available() const {
        return (_h->capacity - _h->back);
    }

    //! available after compaction
    uint32_t potential() const {
        return _h->capacity - size();
    }

    void clear() {
        remove(size());
    }

    ~buffer() {
        free(_h);
    }
};

} // end namespace ten

#endif // LIBTEN_BUFFER_HH
