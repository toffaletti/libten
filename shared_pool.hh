#ifndef SHARED_POOL_HH
#define SHARED_POOL_HH

#include "task.hh"
#include "logging.hh"

#include <boost/call_traits.hpp>
#include <boost/utility.hpp>
#include <set>
#include <deque>

namespace fw {

namespace detail {
    template <typename T> class scoped_resource;
}

template <typename ResourceT, typename ScopeT = detail::scoped_resource<ResourceT> >
class shared_pool : boost::noncopyable {
public:
    // use this scoped_resource type to RAII resources from the pool
    typedef ScopeT scoped_resource;
    typedef std::deque<ResourceT *> queue_type;
    typedef std::set<ResourceT *> set_type;

    shared_pool(const std::string &name, ssize_t max_ = -1)
        : _name(name),
        _max(max_)
    {}

    size_t size() {
        std::unique_lock<qutex> lk(_mut);
        return _set.size();
    }

    void clear() {
        std::unique_lock<qutex> lk(_mut);
        _q.clear();
        for (typename set_type::const_iterator i = _set.begin(); i!= _set.end(); i++) {
            free_resource(*i);
        }
        _set.clear();
    }

    ~shared_pool() {
        clear();
    }

    const std::string &name() const { return _name; }

protected:
    template <typename TT> friend class detail::scoped_resource;

    qutex _mut;
    rendez _not_empty;
    queue_type _q;
    set_type _set;
    std::string _name;
    ssize_t _max;

    bool is_not_empty() const { return !_q.empty(); }

    virtual ResourceT *new_resource() = 0;
    virtual void free_resource(ResourceT *p) {}

    virtual ResourceT *acquire() {
        std::unique_lock<qutex> lk(_mut);
        return create_or_acquire_nolock(lk);
    }

    // internal, does not lock mutex
    inline ResourceT *create_or_acquire_nolock(std::unique_lock<qutex> &lk) {
        ResourceT *c;
        if (_q.empty()) {
            // need to create a new resource
            if (_max < 0 || _set.size() < (size_t)_max) {
                try {
                    c = new_resource();
                    DVLOG(5) << "inserting to shared_pool(" << _name << "): " << c;
                    _set.insert(c);
                } catch (std::exception &e) {
                    LOG(ERROR) << "exception creating new resource for pool: " << e.what();
                    throw;
                }
            } else {
                // can't create anymore we're at max, try waiting
                if (_not_empty.sleep_for(lk, 20)) {
                    c = _q.front();
                    _q.pop_front();
                } else {
                    throw std::runtime_error("timeout waiting for shared resource: " + _name);
                }
            }
        } else {
            // pop resource from front of queue
            c = _q.front();
            _q.pop_front();
        }
        return c;
    }

    virtual bool insert(ResourceT *c) {
        // add a resource to the pool. use with care
        std::unique_lock<qutex> lk(_mut);
        std::pair<typename set_type::iterator, bool> p = _set.insert(c);
        if (p.second) {
            _q.push_front(c);
            _not_empty.wakeup();
        }
        return p.second;
    }

    virtual void release(ResourceT *c) {
        std::unique_lock<qutex> lk(_mut);
        // don't add resource to queue if it was removed from _set
        if (_set.count(c)) {
            _q.push_front(c);
            _not_empty.wakeup();
        }
    }

    // return a defective resource to the pool
    // get a new one back.
    virtual ResourceT *exchange(ResourceT *c) {
        std::unique_lock<qutex> lk(_mut);
        // remove bad resource
        if (c) {
            free_resource(c);
            _set.erase(c);
        }

        return create_or_acquire_nolock(lk);
    }

    void destroy(ResourceT *c) {
        std::unique_lock<qutex> lk(_mut);
        // remove bad resource
        DVLOG(5) << "shared_pool(" << _name << ") destroy in set? " << _set.count(c) << " : " << c;
        free_resource(c);
        _set.erase(c);
        typename queue_type::iterator i = find(_q.begin(), _q.end(), c);
        if (i!=_q.end()) { _q.erase(i); }
    }

};

namespace detail {

// do not use this class directly, instead use your shared_pool<>::scoped_resource
template <typename T> class scoped_resource : boost::noncopyable {
public:
    typedef typename boost::call_traits<shared_pool<T> >::reference poolref;
    typedef T element_type;

    explicit scoped_resource(poolref p)
        : _pool(p) {
            _c = _pool.acquire();
        }

    virtual ~scoped_resource() {
        if (_c == NULL) return;
        _pool.release(_c);
    }

    void exchange() {
        _c = _pool.exchange(_c);
    }

    void destroy() {
        _pool.destroy(_c);
        _c = NULL;
    }

    element_type & operator*() const {
        if (_c == NULL) { throw std::runtime_error("null pointer"); }
        return *_c;
    }

    element_type * operator->() const {
        if (_c == NULL) { throw std::runtime_error("null pointer"); }
        return _c;
    }

    element_type * get() const { return _c; }

protected:
    T *_c;
    poolref _pool;
};

} // end detail namespace

} // end namespace fw

#endif // SHARED_POOL_HH
