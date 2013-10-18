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
        _array.store(new array{(size_t)std::log(size)});
    }

    ~work_deque() {
        delete _array.load();
    }

    // push_back
    void push(T x) {
        size_t b = _bottom.load(std::memory_order_relaxed);
        size_t t = _top.load(std::memory_order_acquire);
        array *a = _array.load(std::memory_order_relaxed);
        if (b - t > a->size() - 1) {
            a = a->grow(t, b).release();
            std::unique_ptr<array> old{_array.exchange(a)};
        }
        a->put(b, x);
        std::atomic_thread_fence(std::memory_order_release);
        _bottom.store(b + 1, std::memory_order_relaxed);
    }

    // pop_back 
    optional<T> take() {
        size_t b = _bottom.load(std::memory_order_relaxed) - 1;
        array *a = _array.load(std::memory_order_relaxed);
        _bottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t t = _top.load(std::memory_order_relaxed); 
        optional<T> x;
        if (t <= b) {
            // non-empty queue
            x = a->get(b);
            if (t == b) {
                // last element in queue
                if (!_top.compare_exchange_strong(t, t + 1,
                            std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    // failed race
                    x = nullopt;
                }
                _bottom.store(b + 1, std::memory_order_relaxed);
            }
        } else {
            // empty queue
            //x = nullptr;
            _bottom.store(b + 1, std::memory_order_relaxed);
        }
        return x;
    }

    // pop_front
    // pair of
    //  bool success? - false means try again
    //  optional<T> item
    std::pair<bool, optional<T> > steal() {
        size_t t = _top.load(std::memory_order_acquire); 
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t b = _bottom.load(std::memory_order_acquire);
        optional<T> x;
        if (t < b) {
            // non empty
            array *a = _array.load(std::memory_order_relaxed);
            x = a->get(t);
            if (!_top.compare_exchange_strong(t, t + 1,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                // failed race
                return std::make_pair(false, nullopt);
            }
        }
        return std::make_pair(true, x);
    }

private:
    struct array {
        std::atomic<size_t> _size;
        std::atomic<std::atomic<T> *> _buffer;

        array(size_t size) {
            _size.store(size, std::memory_order_relaxed);
            _buffer.store(new std::atomic<T>[this->size()], std::memory_order_relaxed);
        }

        ~array() {
            delete[] _buffer;
        }

        size_t size() const {
            return 1<<_size.load(std::memory_order_seq_cst);
        }

        void put(size_t i, T v) {
            _buffer[i % size()].store(v, std::memory_order_relaxed);
        }

        T get(size_t i) const {
            return _buffer[i % size()].load(std::memory_order_relaxed);
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
