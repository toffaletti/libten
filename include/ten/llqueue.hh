#ifndef LIBTEN_LLQUEUE_HH
#define LIBTEN_LLQUEUE_HH

#include <atomic>
#include <type_traits>
#include "ten/optional.hh"

namespace ten {
// low lock queue
// http://www.drdobbs.com/parallel/211601363 by herb sutter
// CACHE_LINE_SIZE = getconf LEVEL1_DCACHE_LINESIZE

template <typename T, size_t CACHE_LINE_SIZE=64>
struct llqueue {
private:
    //struct alignas(64) node {
    struct node {
        node() : next(nullptr) {}
        node(T val) : value(std::forward<T>(val)), next(nullptr) {}
        optional<T> value;
        std::atomic<node *> next;
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
        _first = _last = new node();
    }

    ~llqueue() {
        while (_first != nullptr) {
            node *tmp = _first;
            _first = tmp->next;
            delete tmp;
        }
    }

    void push(T t)
    {
        node *tmp = new node(std::forward<T>(t));
        while (_producer_lock.test_and_set(std::memory_order_acquire))
        { } // acquire exclusivity
        _last->next = tmp;      // publish to consumers
        _last = tmp;            // swing last forward
        _producer_lock.clear(std::memory_order_release); // release exclusivity
    }

    bool pop(T &result)
    {
        while (_consumer_lock.test_and_set(std::memory_order_acquire)) 
        { } // acquire exclusivity
        node *first = _first;
        node *next = _first->next;
        if (next != nullptr) { // if queue is nonempty
            if (next->value) {
                result = *(next->value);
                next->value = optional<T>();
            }
            _first = next;
            _consumer_lock.clear(std::memory_order_release); // release exclusivity
            delete first;
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
