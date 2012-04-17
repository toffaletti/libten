#ifndef LIBTEN_SHARED_POOL_HH
#define LIBTEN_SHARED_POOL_HH

#include "task/rendez.hh"
#include "ten/logging.hh"

#include <boost/call_traits.hpp>
#include <set>
#include <deque>

namespace ten {

namespace detail {
    template <typename T> class scoped_resource;
}

//! thread and task safe pool of shared resources
//
//! useful for connection pools and other types of shared resources
template <typename ResourceT, typename ScopeT = detail::scoped_resource<ResourceT> >
class shared_pool {
public:
    // use this scoped_resource type to RAII resources from the pool
    typedef ScopeT scoped_resource;
    typedef std::deque<std::shared_ptr<ResourceT> > queue_type;
    typedef std::set<std::shared_ptr<ResourceT> > set_type;
    typedef std::function<std::shared_ptr<ResourceT> ()> alloc_func;
protected:
    template <typename TT> friend class detail::scoped_resource;

    qutex _mut;
    rendez _not_empty;
    queue_type _q;
    set_type _set;
    std::string _name;
    alloc_func _new_resource;
    ssize_t _max;

public:
    shared_pool(const std::string &name_,
            const alloc_func &alloc_,
            ssize_t max_ = -1)
        : _name(name_),
        _new_resource(alloc_),
        _max(max_)
    {}

    shared_pool(const shared_pool &) = delete;
    shared_pool &operator =(const shared_pool &) = delete;

    size_t size() {
        std::unique_lock<qutex> lk(_mut);
        return _set.size();
    }

    void clear() {
        std::unique_lock<qutex> lk(_mut);
        _q.clear();
        _set.clear();
    }

    const std::string &name() const { return _name; }

protected:
    bool is_not_empty() const { return !_q.empty(); }

    std::shared_ptr<ResourceT> acquire() {
        std::unique_lock<qutex> lk(_mut);
        return create_or_acquire_nolock(lk);
    }

    // internal, does not lock mutex
    std::shared_ptr<ResourceT> create_or_acquire_nolock(std::unique_lock<qutex> &lk) {
        std::shared_ptr<ResourceT> c;
        if (_q.empty()) {
            // need to create a new resource
            if (_max < 0 || _set.size() < (size_t)_max) {
                try {
                    c = _new_resource();
                    CHECK(c) << "new_resource failed for pool: " << _name;
                    DVLOG(5) << "inserting to shared_pool(" << _name << "): " << c;
                    _set.insert(c);
                } catch (std::exception &e) {
                    LOG(ERROR) << "exception creating new resource for pool: " <<_name << " " << e.what();
                    throw;
                }
            } else {
                // can't create anymore we're at max, try waiting
                while (_q.empty()) {
                    _not_empty.sleep(lk);
                }
                CHECK(!_q.empty());
                c = _q.front();
                _q.pop_front();
            }
        } else {
            // pop resource from front of queue
            c = _q.front();
            _q.pop_front();
        }
        CHECK(c) << "acquire shared resource failed in pool: " << _name;
        return c;
    }

    bool insert(std::shared_ptr<ResourceT> &c) {
        // add a resource to the pool. use with care
        std::unique_lock<qutex> lk(_mut);
        auto p = _set.insert(c);
        if (p.second) {
            _q.push_front(c);
            _not_empty.wakeup();
        }
        return p.second;
    }

    void release(std::shared_ptr<ResourceT> &c) {
        std::unique_lock<qutex> lk(_mut);
        // don't add resource to queue if it was removed from _set
        if (_set.count(c)) {
            _q.push_front(c);
            _not_empty.wakeup();
        }
    }

    // return a defective resource to the pool
    // get a new one back.
    std::shared_ptr<ResourceT> exchange(std::shared_ptr<ResourceT> &c) {
        std::unique_lock<qutex> lk(_mut);
        // remove bad resource
        if (c) {
            _set.erase(c);
            c.reset();
        }

        return create_or_acquire_nolock(lk);
    }

    void destroy(std::shared_ptr<ResourceT> &c) {
        std::unique_lock<qutex> lk(_mut);
        // remove bad resource
        DVLOG(5) << "shared_pool(" << _name
            << ") destroy in set? " << _set.count(c)
            << " : " << c << " rc: " << c.use_count();
        _set.erase(c);
        typename queue_type::iterator i = find(_q.begin(), _q.end(), c);
        if (i!=_q.end()) { _q.erase(i); }

        c.reset();

        // replace the destroyed resource
        std::shared_ptr<ResourceT> cc = _new_resource();
        _set.insert(cc);
        _q.push_front(cc);
        cc.reset();
        _not_empty.wakeup();
    }

};

namespace detail {

// do not use this class directly, instead use your shared_pool<>::scoped_resource
template <typename T> class scoped_resource {
public:
    typedef typename boost::call_traits<shared_pool<T> >::reference poolref;
protected:
    poolref _pool;
    std::shared_ptr<T> _c;
public:
    explicit scoped_resource(poolref p)
        : _pool(p) {
            _c = _pool.acquire();
        }

    ~scoped_resource() {
        if (!_c) return;
        _pool.release(_c);
        // XXX: use destroy once done() has been added everywhere
        //_pool.destroy(_c);
    }

    void exchange() {
        _c = _pool.exchange(_c);
    }

    void destroy() {
        _pool.destroy(_c);
    }

    void acquire() {
        if (_c) return;
        _c = _pool.acquire();
    }

    void done() {
        _pool.release(_c);
        _c.reset();
    }

    T *operator->() {
        if (!_c) throw std::runtime_error("null pointer");
        return _c.get();
    }

};

} // end detail namespace

} // end namespace ten

#endif // LIBTEN_SHARED_POOL_HH
