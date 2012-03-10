#ifndef BUFFER_HH
#define BUFFER_HH

#include <memory>
#include <cassert>

namespace ten {

class buffer : boost::noncopyable {
public:
    struct head {
        uint32_t capacity;
        uint32_t front;
        uint32_t back;
        char data[1];
    } __attribute__((packed));
private:
    head *h;

public:
    buffer(uint32_t capacity) {
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
        if (n > potential()) {
            compact();
            uint32_t need = n - potential();
            uint32_t newcapacity = h->capacity + need;
            head *tmp = (head *)realloc(h, offsetof(head, data) + newcapacity);
            if (tmp) {
                tmp->capacity = newcapacity;
                // TODO: DVLOG_IF is missing
                //VLOG_IF(4, (h != tmp)) << "realloc returned new pointer: " << h << " " << tmp;
                h = tmp;
            } else {
                throw std::bad_alloc();
            }
        } else if (n > available()) {
            compact();
        }
    }

    void commit(uint32_t n) {
        if (n > available()) {
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

    char *front() const { return &h->data[h->front]; }
    char *back() const { return &h->data[h->back]; }
    char *end(uint32_t n) const { return &h->data[h->back + n]; }
    char *end() const { return &h->data[h->capacity]; }

    //! size of commited space
    uint32_t size() const {
        return h->back - h->front;
    }

    //! space available to be commited
    uint32_t available() const {
        return (h->capacity - h->back);
    }

    //! available after compaction
    uint32_t potential() const {
        return h->capacity - size();
    }

    void clear() {
        remove(size());
    }

    ~buffer() {
        free(h);
    }
};

} // end namespace ten

#endif // BUFFER_HH

