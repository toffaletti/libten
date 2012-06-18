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
        return create_or_acquire_with_lock(lk);
    }

    // internal, does not lock mutex
    std::shared_ptr<ResourceT> create_or_acquire_with_lock(std::unique_lock<qutex> &lk) {
        while (_q.empty()) {
            if (_max < 0 || _set.size() < (size_t)_max) {
                // need to create a new resource
                return add_new_resource(lk);
                break;
            } else {
                // can't create anymore we're at max, try waiting
                // we don't use a predicate here because
                // we might be woken up from destroy()
                // in which case we might not be at max anymore
                _not_empty.sleep(lk);
            }
        }

        CHECK(!_q.empty());
        // pop resource from front of queue
        std::shared_ptr<ResourceT> c = _q.front();
        _q.pop_front();
        CHECK(c) << "acquire shared resource failed in pool: " << _name;
        return c;
    }

    void release(std::shared_ptr<ResourceT> &c) {
        std::unique_lock<qutex> lk(_mut);
        // don't add resource to queue if it was removed from _set
        if (_set.count(c)) {
            _q.push_front(c);
            _not_empty.wakeup();
        }
    }

    void destroy(std::shared_ptr<ResourceT> &c) {
        std::unique_lock<qutex> lk(_mut);
        // remove bad resource
        DVLOG(4) << "shared_pool(" << _name
            << ") destroy in set? " << _set.count(c)
            << " : " << c << " rc: " << c.use_count();
        size_t count = _set.erase(c);
        if (count) {
            LOG(WARNING) << "destroying shared resource from pool " << _name;
        }
        typename queue_type::iterator i = std::find(_q.begin(), _q.end(), c);
        if (i!=_q.end()) { _q.erase(i); }

        c.reset();

        // give waiting threads a chance to allocate a new resource
        _not_empty.wakeup();
    }

    std::shared_ptr<ResourceT> add_new_resource(std::unique_lock<qutex> &lk) {
        std::shared_ptr<ResourceT> c;
        lk.unlock(); // unlock while newing resource
        try {
            c = _new_resource();
            CHECK(c) << "new_resource failed for pool: " << _name;
            DVLOG(4) << "inserting to shared_pool(" << _name << "): " << c;
        } catch (std::exception &e) {
            LOG(ERROR) << "exception creating new resource for pool: " <<_name << " " << e.what();
            lk.lock();
            throw;
        }
        lk.lock(); // re-lock before inserting to set
        _set.insert(c);
        return c;
    }

};

namespace detail {

// do not use this class directly, instead use your shared_pool<>::scoped_resource
template <typename T> class scoped_resource {
public:
    //typedef typename std::add_reference<shared_pool<T>>::type poolref;
    typedef typename boost::call_traits<shared_pool<T>>::reference poolref;
protected:
    poolref _pool;
    std::shared_ptr<T> _c;
    bool _success;
public:
    explicit scoped_resource(poolref p)
        : _pool(p), _success(false) {
            _c = _pool.acquire();
        }

    //! must call done() to return the resource to the pool
    //! otherwise we destroy it because a timeout or other exception
    //! could have occured causing the resource state to be in transition
    ~scoped_resource() {
        if (_success) {
            _pool.release(_c);
        } else {
            _pool.destroy(_c);
        }
    }

    void done() {
        DCHECK(!_success);
        _success = true;
    }

    T *operator->() {
        if (!_c) throw std::runtime_error("null pointer");
        return _c.get();
    }

};

} // end detail namespace

} // end namespace ten

#endif // LIBTEN_SHARED_POOL_HH
