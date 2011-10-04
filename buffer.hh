#ifndef BUFFER_HH
#define BUFFER_HH

#include <memory>
#include <cassert>

namespace fw {

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
    buffer(size_t bytes=0) : m(new impl(bytes)) {}

    //! slice this buffer at pos
    slice operator ()(size_t pos) const {
        return slice(m, pos, m->n-pos);
    }

    //! slice at pos with len bytes
    slice operator ()(size_t pos, size_t len) const {
        return slice(m, pos, len);
    }
};

} // end namespace fw

#endif // BUFFER_HH

