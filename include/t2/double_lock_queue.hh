#ifndef TEN_DOUBLE_LOCK_QUEUE_HH
#define TEN_DOUBLE_LOCK_QUEUE_HH

// http://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html

#include <mutex>
#include <memory>

template <class T>
class double_lock_queue {
private:
    struct node {
        node *next;
        T value;

        node() : next{nullptr}, value{} {}
        node(T &&value_) : next{nullptr}, value{std::move(value_)} {}
    };
private:
    char pad0[64];
    std::mutex _head_mutex;
    node *_head;
    static constexpr size_t padding_size = 64 - sizeof(node *) - sizeof(std::mutex);
    char pad1[padding_size];
    std::mutex _tail_mutex;
    node *_tail;
    char pad2[padding_size];
public:
    double_lock_queue() {
        _head = _tail = new node();
    }

    ~double_lock_queue() {
        for (;;) {
            std::unique_ptr<node> n{_head};
            node *new_head = n->next;
            if (new_head == nullptr) {
                break;
            }
            _head = new_head;
        }
    }

    void push(T value) {
        node *n = new node{std::move(value)};
        std::lock_guard<std::mutex> lock(_tail_mutex);
        _tail->next = n;
        _tail = n;
    }

    bool pop(T &value) {
        std::unique_ptr<node> n;
        std::lock_guard<std::mutex> lock(_head_mutex);
        n.reset(_head);
        node *new_head = n->next;
        if (new_head == nullptr) {
            n.release(); // queue was empty, don't free node
            return false;
        }
        value = std::move(new_head->value);
        _head = new_head;
        return true;
    }
};

#endif
