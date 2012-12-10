#ifndef LIBTEN_LLQUEUE_HH
#define LIBTEN_LLQUEUE_HH

#include <atomic>
#include <type_traits>

namespace ten {
// low lock queue
// http://www.drdobbs.com/parallel/211601363 by herb sutter

// getconf LEVEL1_DCACHE_LINESIZE
#define CACHE_LINE_SIZE 64

template <typename T>
struct llqueue {
private:
    typedef typename std::remove_pointer<T>::type V;
    struct node {
        node(V *val) : value(val), next(nullptr) {}
        V *value;
        std::atomic<node *> next;
        char pad[CACHE_LINE_SIZE - sizeof(V*) - sizeof(std::atomic<node *>)];
    };
private:
    char pad0[CACHE_LINE_SIZE];
    // for one consumer at a time
    node *_first;

    char pad1[CACHE_LINE_SIZE - sizeof(node*)];
    // shared among consumers
    std::atomic_flag _consumer_lock = ATOMIC_FLAG_INIT;

    char pad2[CACHE_LINE_SIZE - sizeof(std::atomic<bool>)];
    // for one producer at a time
    node *_last; 

    char pad3[CACHE_LINE_SIZE - sizeof(node*)];
    // shared among producers
    std::atomic_flag _producer_lock = ATOMIC_FLAG_INIT;

    char pad4[CACHE_LINE_SIZE - sizeof(std::atomic<bool>)]; 
public:
    llqueue() {
        _first = _last = new node(nullptr);
    }

    ~llqueue() {
        while (_first != nullptr) {
            node *tmp = _first;
            _first = tmp->next;
            if (!std::is_pointer<T>::value) {
                // if T is a pointer already
                // we dont own pointer so don't delete
                delete tmp->value;
            }
            delete tmp;
        }
    }

    template <typename A, typename std::enable_if<std::is_pointer<A>::value, int>::type = 0>
        void push(const A &t)
        {
            node *tmp = new node(t);
            while (_producer_lock.test_and_set(std::memory_order_acquire))
            { } // acquire exclusivity
            _last->next = tmp;      // publish to consumers
            _last = tmp;            // swing last forward
            _producer_lock.clear(std::memory_order_release); // release exclusivity
        }

    template <typename A, typename std::enable_if<!std::is_pointer<A>::value, int>::type = 0>
        void push(const A &t)
        {
            node *tmp = new node(new T(t));
            while (_producer_lock.test_and_set(std::memory_order_acquire))
            { } // acquire exclusivity
            _last->next = tmp;      // publish to consumers
            _last = tmp;            // swing last forward
            _producer_lock.clear(std::memory_order_release); // release exclusivity
        }


    template <typename A, typename std::enable_if<std::is_pointer<A>::value, int>::type = 0>
        bool pop(A &result)
        {
            while (_consumer_lock.test_and_set(std::memory_order_acquire)) 
            { } // acquire exclusivity
            node *first = _first;
            node *next = _first->next;
            if (next != nullptr) { // if queue is nonempty
                // optimization when T is already a pointer
                result = next->value;
                next->value = nullptr;
                _first = next;
                _consumer_lock.clear(std::memory_order_release); // release exclusivity
                delete first;
                return true;            // and report success
            }
            _consumer_lock.clear(std::memory_order_release); // release exclusivity
            return false;           // report queue was empty
        }

    template <typename A, typename std::enable_if<!std::is_pointer<A>::value, int>::type = 0>
        bool pop(A &result)
        {
            while (_consumer_lock.test_and_set(std::memory_order_acquire)) 
            { } // acquire exclusivity
            node *first = _first;
            node *next = _first->next;
            if (next != nullptr) { // if queue is nonempty
                V *val = next->value;    // take it out
                next->value = nullptr;  // of the Node
                _first = next;          // swing first forward
                _consumer_lock.clear(std::memory_order_release); // release exclusivity
                result = *val;          // now copy it back
                delete val;             // clean up the value
                delete first;           // and the old dummy
                return true;            // and report success
            }
            _consumer_lock.clear(std::memory_order_release); // release exclusivity
            return false;           // report queue was empty
        }

    bool empty() {
        while (_consumer_lock.test_and_set(std::memory_order_acquire)) 
        { } // acquire exclusivity
        bool rvalue(_first->next == nullptr);
        _consumer_lock.clear(std::memory_order_release); // release exclusivity
        return rvalue;
    }
};

} // ten
#endif
