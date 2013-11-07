#ifndef TEN_WORK_DEQUE
#define TEN_WORK_DEQUE
#include "ten/optional.hh"
#include <memory>
#include <atomic>
#include <cmath>

// Chase/Lev work-stealing deque
// from Dynamic Circular Work-Stealing Deque
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
// and
// Correct and Efﬁcient Work-Stealing for Weak Memory Models
// http://www.di.ens.fr/~zappa/readings/ppopp13.pdf

namespace ten {

template <typename T>
class work_deque {
private:
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t [cacheline_size];
    struct array;

    cacheline_pad_t pad0_;
    std::atomic<size_t> _top;
    std::atomic<size_t> _bottom;
    std::atomic<array *> _array;
    cacheline_pad_t pad1_;
public:
    work_deque(size_t size) : _top{0}, _bottom{0} {
        _array.store(new array{(size_t)std::log(size)}, std::memory_order_seq_cst);
    }

    ~work_deque() {
        delete _array.load(std::memory_order_seq_cst);
    }

    bool empty() const {
        size_t b = _bottom.load(std::memory_order_acquire);
        size_t t = _top.load(std::memory_order_acquire);
        return t >= b;
    }

    // push_back
    void push(T x) {
        size_t b = _bottom.load(std::memory_order_seq_cst);
        size_t t = _top.load(std::memory_order_seq_cst);
        array *a = _array.load(std::memory_order_seq_cst);
        if (b - t > a->size() - 1) {
            a = a->grow(t, b).release();
            size_t ss = a->size();
            std::unique_ptr<array> old{_array.exchange(a, std::memory_order_seq_cst)};
            _bottom.store(b + ss, std::memory_order_seq_cst);
            t = _top.load(std::memory_order_seq_cst);
            if (!_top.compare_exchange_strong(t, t + ss,
                        std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                _bottom.store(b, std::memory_order_seq_cst);
            }
            b = _bottom.load(std::memory_order_seq_cst);

        }
        a->put(b, x);
        _bottom.store(b + 1, std::memory_order_seq_cst);
    }

    // pop_back 
    optional<T> take() {
        size_t b = _bottom.load(std::memory_order_seq_cst) - 1;
        array *a = _array.load(std::memory_order_seq_cst);
        _bottom.store(b, std::memory_order_release);
        size_t t = _top.load(std::memory_order_seq_cst); 
        optional<T> x;
        ssize_t size = b - t;
        if (size >= 0) {
            // non-empty queue
            x = a->get(b);
            if (t == b) {
                // last element in queue
                if (!_top.compare_exchange_strong(t, t + 1,
                            std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    // failed race
                    x = nullopt;
                }
                _bottom.store(b + 1, std::memory_order_seq_cst);
            }
        } else {
            // empty queue
            //x = nullptr;
            _bottom.store(b + 1, std::memory_order_seq_cst);
        }
        return x;
    }

    // pop_front
    // pair of
    //  bool success? - false means try again
    //  optional<T> item
    std::pair<bool, optional<T> > steal() {
        optional<T> x;
        size_t t = _top.load(std::memory_order_seq_cst); 
        array *old_a = _array.load(std::memory_order_seq_cst);
        size_t b = _bottom.load(std::memory_order_seq_cst);
        array *a = _array.load(std::memory_order_seq_cst);
        ssize_t size = b - t;
        if (size <= 0) {
            // empty
            return std::make_pair(true, x);
        }
        if ((size % a->size()) == 0) {
            if (a == old_a && t == _top.load(std::memory_order_seq_cst)) {
                // empty
                return std::make_pair(true, x);
            } else {
                // abort, failed race
                return std::make_pair(false, nullopt);
            }

        }
        // non empty
        x = a->get(t);
        if (!_top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_seq_cst)) {
            // failed race
            return std::make_pair(false, nullopt);
        }
        return std::make_pair(true, x);
    }

private:
    struct array {
        size_t _size;
        std::atomic<T> * _buffer;

        array(size_t size) :
            _size{size},
            _buffer{new std::atomic<T>[this->size()]}
        {
        }

        ~array() {
            delete[] _buffer;
        }

        size_t size() const {
            return 1<<_size;
        }

        void put(size_t i, T v) {
            _buffer[i % size()].store(v, std::memory_order_seq_cst);
        }

        T get(size_t i) const {
            return _buffer[i % size()].load(std::memory_order_seq_cst);
        }

        std::unique_ptr<array> grow(size_t top, size_t bottom) {
            std::unique_ptr<array> a{new array{_size+1}};
            for (size_t i=top; i<bottom; ++i) {
                a->put(i, get(i));
            }
            return a;
        }
    };
};

} // namespace ten

#endif // TEN_WORK_DEQUE
