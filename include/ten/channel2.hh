#ifndef CHANNEL2_HH
#define CHANNEL2_HH

#include "task.hh"
#include "task/qutex.hh"
#include "task/rendez.hh"
#include "ten/channel.hh"
#include "ten/tagged_ptr.hh"
#include <atomic>

#include <unistd.h>
#include <sys/syscall.h>

namespace ten {

template <typename T> class transfer_queue {
private:
    typedef tagged_ptr<T, atomic_count_tagger<T>> item_ptr;

    struct qnode {
        typedef tagged_ptr<qnode, atomic_count_tagger<qnode>> qnode_ptr;
        std::atomic<qnode *> _next;
        std::atomic<T *> _item;
        std::atomic<task *> _waiter;
        bool _is_data;

        qnode(item_ptr item, bool is_data) :
            _next(nullptr), _item(item.get()), 
            _waiter(nullptr), _is_data(is_data) {}

        void try_cancel(item_ptr cmp) {
            T *cp = cmp.get();
            _item.compare_exchange_strong(cp, (T *)this);
        }

        bool is_cancelled() const {
            return item_ptr(_item).ptr() == (T *)this;
        }

        bool is_off_list() const {
            return _next == this;
        }

        bool cas_next(qnode_ptr cmp, qnode_ptr val) {
            qnode *cp = cmp.get();
            return _next.compare_exchange_strong(cp, val.get());
        }

        bool cas_item(item_ptr cmp, item_ptr val) {
            T *cp = cmp.get();
            return _item.compare_exchange_strong(cp, val.get());
        }

        ~qnode() {
            if (!is_cancelled())
                delete _item;
        }
    };

    typedef typename qnode::qnode_ptr qnode_ptr;
private:
    std::atomic<qnode *> _head;
    std::atomic<qnode *> _tail;
    pid_t _tid;
    uint16_t _max_spins;

private:

    uint16_t max_spins() {
        static __thread pid_t local_tid(0);
        if (_max_spins == 0) {
            if (local_tid == 0) {
                local_tid = syscall(SYS_gettid);
            }
            // detect if we're dealing with
            // more than one thread
            if (_tid == 0) {
                _tid = local_tid;
            } else if (_tid != local_tid) {
                _max_spins = 32;
            }
        }
        return _max_spins;
    }

    void advance_head(qnode_ptr h, qnode_ptr nh) {
        qnode *hp = h.get();
        if (_head.compare_exchange_strong(hp, nh.get())) {
            h.free();
        }
    }

    void advance_tail(qnode_ptr t, qnode_ptr nt) {
        qnode *tp = t.get();
        _tail.compare_exchange_strong(tp, nt.get());
    }

    item_ptr await_fulfill(qnode_ptr s, item_ptr e) {
        task *w = taskself();
        int spins = (qnode_ptr(_head.load())->_next == s.get()) ? max_spins() : 0;
        for (;;) {
            item_ptr x(s->_item);
            if (x != e)
                return x;
            if (spins > 0)
                --spins;
            else if (s->_waiter == nullptr)
                s->_waiter = w;
            else
                taskpark(w);
        }
    }

#if 0
    bool cas_cleanme(qnode *cmp, qnode *val) {
        return _cleanme.compare_exchange_strong(cmp, val);
    }

    void clean(qnode *pred, qnode *s) {
        s->_waiter = nullptr;
        while (pred->_next == s) { // return early if already unlinked
            qnode *h = _head;
            qnode *hn = h->_next; // absorb cancelled first node as head
            if (hn != nullptr && hn->is_cancelled()) {
                advance_head(h, hn);
                continue;
            }

            qnode *t = _tail; // ensure consistent read for tail
            if (t == h)
                return;
            qnode *tn = t->_next;
            if (t != _tail)
                continue;
            if (tn != nullptr) {
                advance_tail(t, tn);
                continue;
            }
            if (s != t) { // if not tail, try to unsplice
                qnode *sn = s->_next;
                if (sn == s || pred->cas_next(s, sn))
                    return;
            }
            qnode *dp = _cleanme;
            if (dp != nullptr) { // try unlinking previous cancelled node
                qnode *d = dp->_next;
                qnode *dn;
                if (d == nullptr || // d is gone or
                        d == dp || // d is off list or
                        !d->is_cancelled() || // d not cancelled or
                        (d != t && // d not tail and
                         (dn = d->_next) != nullptr && // has successor
                         dp->cas_next(d, dn))) // that is on list
                {
                    cas_cleanme(dp, nullptr);
                }
                if (dp == pred)
                    return; // s is already saved node
            } else if (cas_cleanme(nullptr, pred))
                return; // post pone cleaning s
        }
    }
#endif

public:
    transfer_queue() : _tid(0), _max_spins(0) {
        _head = _tail = qnode_ptr(new qnode(item_ptr(nullptr), false)).get();
    }

    ~transfer_queue() {
        // TODO: clear()
    }

    bool empty() {
        return _head == _tail || qnode_ptr(_head)->_next == _tail;
    }

    void clear() {
        while (!empty()) {
            std::unique_ptr<T> e(transfer(nullptr, true));
            DVLOG(4) << "bleh e: " << e.get();
        }
    }

    T *transfer(T *e_, bool nowait=false) {
        item_ptr e(e_);
        qnode_ptr s(nullptr);
        bool is_data = (e != nullptr);

        for (;;) {
            qnode_ptr t(_tail);
            qnode_ptr h(_head);

            if (t == nullptr || h == nullptr) // saw uninitialized value
                continue; // spin

            if (h == t || t->_is_data == is_data) { // empty or same-mode
                qnode_ptr tn(t->_next);
                if (t != _tail) // inconsistent read
                    continue;
                if (tn != nullptr) { // lagging tail
                    advance_tail(t, tn);
                    continue;
                }

                if (nowait)
                    return e.ptr();

                if (s == nullptr)
                    s = qnode_ptr(new qnode(e, is_data));

                if (!t->cas_next(qnode_ptr(nullptr), s)) // failed to link in
                    continue;

                advance_tail(t, s);
                item_ptr x(nullptr);
                try {
                    x = await_fulfill(s, e);
                } catch (...) { // wait was cancelled
                    s->try_cancel(e);
                    //clean(t, s);
                    throw;
                }

                if (!s->is_off_list()) { // not already unlinked
                    advance_head(t, s);  // unlink if head
                    if (x != nullptr)    // and forget fields
                        s->_item = (T *)s.get();
                    s->_waiter = nullptr;
                }
                return (x != nullptr) ? x.ptr() : e.ptr();
            } else { // complementary-mode
                qnode_ptr m(h->_next); // node to fulfill
                if (t != _tail || m == nullptr || h != _head)
                    continue; // inconsistent read
                item_ptr x(m->_item);
                if (is_data == (x != nullptr) || // m already fulfilled
                        x == (T *)m.get() || // m cancelled
                        !m->cas_item(x, e)) // lost CAS
                {
                    advance_head(h, m); // dequeue and retry
                    continue;
                }

                advance_head(h, m); // successfully fulfilled
                taskunpark(m->_waiter);
                return (x != nullptr) ? x.ptr() : e.ptr();
            }
        }
    }

};

namespace exp {

template <typename T> class channel {
private:
    struct impl {
        transfer_queue<T> queue;
    };
private:
    std::shared_ptr<impl> _m;

public:
    explicit channel(bool autoclose = false)
        : _m(std::make_shared<impl>()), _autoclose(autoclose)
    {
    }

    channel(const channel &other) : _m(other._m) {}
    channel &operator = (const channel &other) {
        _m = other._m;
        return *this;
    }

    ~channel() {
    }

    //! send data
    void send(T &&p) {
        std::unique_ptr<T> e(new T(p));
        if (_m->queue.transfer(e.get()) != nullptr) {
            e.release();
        }
    }

    //! receive data
    T recv() {
        std::unique_ptr<T> e(_m->queue.transfer(nullptr));
        return *e;
    }

};

} // namespace exp

} // ten

#endif
