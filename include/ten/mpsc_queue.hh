/* Multi-producer/single-consumer queue
 * Copyright (c) 2010-2011 Dmitry Vyukov. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Dmitry Vyukov.
 */

#ifndef LIBTEN_MPSC_QUEUE_HH
#define LIBTEN_MPSC_QUEUE_HH

#include <atomic>
#include <memory>
#include <utility>

namespace ten {

template <typename T>
class mpsc_queue {
private:
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t [cacheline_size];

    struct node {
        std::atomic<node *> next;
        T value;

        node() : next{nullptr} {}
        node(T v) : next{nullptr}, value{v} {}
    };

    cacheline_pad_t pad0_;
    node _stub;
    std::atomic<node *> _head;
    cacheline_pad_t pad1_;
    node *_tail;
    cacheline_pad_t pad2_;

    void _push(node *n) {
        n->next.store(nullptr, std::memory_order_release);
        node *prev = _head.exchange(n, std::memory_order_relaxed);
        // XXX maybe use memory_order_acq_rel
        prev->next.store(n, std::memory_order_release);
        if (prev == &_stub) {
            // was empty
            // could signal to wake up consumer
        }
    }
public:
    mpsc_queue() : _head{&_stub}, _tail{&_stub} {}
    mpsc_queue(const mpsc_queue&) = delete;
    mpsc_queue(mpsc_queue &&) = delete;
    mpsc_queue &operator =(const mpsc_queue&) = delete;
    mpsc_queue &operator =(mpsc_queue &&) = delete;

    ~mpsc_queue() {
        while (pop().first) {}
    }

    void push(T value) {
        _push(new node{value});
    }

    std::pair<bool, T> pop() {
        node *tail = _tail;
        node *next = tail->next.load(std::memory_order_acquire);
        if (tail == &_stub) {
            if (nullptr == next) {
                return std::make_pair(false, T{});
            }
            _tail = next;
            tail = next;
            next = next->next.load(std::memory_order_acquire);
        }
        if (next) {
            std::unique_ptr<node> old{tail};
            _tail = next;
            return std::make_pair(true, tail->value);
        }
        node *head = _head.load(std::memory_order_relaxed);
        if (tail != head) {
            return std::make_pair(false, T{});
        }
        _push(&_stub);
        next = tail->next.load(std::memory_order_acquire);
        if (next) {
            std::unique_ptr<node> old{tail};
            _tail = next;
            return std::make_pair(true, tail->value);
        }
        return std::make_pair(false, T{});
    }
};

} // ten

#endif // LIBTEN_MPSC_QUEUE_HH
