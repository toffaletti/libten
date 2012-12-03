#ifndef LIBTEN_SHARED_POOL_HH
#define LIBTEN_SHARED_POOL_HH

#include "task/rendez.hh"
#include "ten/logging.hh"

#include <boost/call_traits.hpp>
#include <set>
#include <deque>
#include <memory>
#include <atomic>

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

    struct pool_impl {
        qutex mut;
        rendez not_empty;
        queue_type q;
        set_type set;
        std::string name;
        alloc_func new_resource;
        ssize_t max;
    };

    std::mutex _mutex;
    std::shared_ptr<pool_impl> _m;

    std::shared_ptr<pool_impl> get_safe() {
#if 0
        return std::atomic_load(&_m);
#else
        std::lock_guard<std::mutex> lock(_mutex);
        return _m;
#endif
    }

    void set_safe(std::shared_ptr<pool_impl> &other) {
#if 0
        std::atomic_store(&_m, other);
#else

        std::lock_guard<std::mutex> lock(_mutex);
        _m = other;
#endif
    }

public:
    shared_pool(const std::string &name_,
            const alloc_func &alloc_,
            ssize_t max_ = -1)
    {
        std::shared_ptr<pool_impl> m = std::make_shared<pool_impl>();
        m->name = name_;
        m->new_resource = alloc_;
        m->max = max_;
        set_safe(m);
    }

    shared_pool(const shared_pool &) = delete;
    shared_pool &operator =(const shared_pool &) = delete;

    size_t size() {
        std::shared_ptr<pool_impl> m(get_safe());
        std::lock_guard<qutex> lk(m->mut);
        return m->set.size();
    }

    void clear() {
        std::shared_ptr<pool_impl> m(get_safe());
        std::lock_guard<qutex> lk(m->ut);
        m->q.clear();
        m->set.clear();
    }

    const std::string &name() {
        std::shared_ptr<pool_impl> m(get_safe());
        return m->name;
    }

protected:
    bool is_not_empty() {
        std::shared_ptr<pool_impl> m(get_safe());
        return !m->q.empty();
    }

    static std::shared_ptr<ResourceT> acquire(std::shared_ptr<pool_impl> &m) {
        std::unique_lock<qutex> lk(m->mut);
        return create_or_acquire_with_lock(m, lk);
    }

    // internal, does not lock mutex
    static std::shared_ptr<ResourceT> create_or_acquire_with_lock(std::shared_ptr<pool_impl> &m, std::unique_lock<qutex> &lk) {
        while (m->q.empty()) {
            if (m->max < 0 || m->set.size() < (size_t)m->max) {
                // need to create a new resource
                return add_new_resource(m, lk);
                break;
            } else {
                // can't create anymore we're at max, try waiting
                // we don't use a predicate here because
                // we might be woken up from destroy()
                // in which case we might not be at max anymore
                m->not_empty.sleep(lk);
            }
        }

        CHECK(!m->q.empty());
        // pop resource from front of queue
        std::shared_ptr<ResourceT> c = m->q.front();
        m->q.pop_front();
        CHECK(c) << "acquire shared resource failed in pool: " << m->name;
        return c;
    }

    static void release(std::shared_ptr<pool_impl> &m, std::shared_ptr<ResourceT> &c) {
        safe_lock<qutex> lk(m->mut);
        // don't add resource to queue if it was removed from _set
        if (m->set.count(c)) {
            m->q.push_front(c);
            m->not_empty.wakeup();
        }
    }

    static void destroy(std::shared_ptr<pool_impl> &m, std::shared_ptr<ResourceT> &c) {
        safe_lock<qutex> lk(m->mut);
        // remove bad resource
        DVLOG(4) << "shared_pool(" << m->name
            << ") destroy in set? " << m->set.count(c)
            << " : " << c << " rc: " << c.use_count();
        size_t count = m->set.erase(c);
        if (count) {
            LOG(WARNING) << "destroying shared resource from pool " << m->name;
        }
        typename queue_type::iterator i = std::find(m->q.begin(), m->q.end(), c);
        if (i!=m->q.end()) { m->q.erase(i); }

        c.reset();

        // give waiting threads a chance to allocate a new resource
        m->not_empty.wakeup();
    }

    static std::shared_ptr<ResourceT> add_new_resource(std::shared_ptr<pool_impl> &m, std::unique_lock<qutex> &lk) {
        std::shared_ptr<ResourceT> c;
        lk.unlock(); // unlock while newing resource
        try {
            c = m->new_resource();
            CHECK(c) << "new_resource failed for pool: " << m->name;
            DVLOG(4) << "inserting to shared_pool(" << m->name << "): " << c;
        } catch (std::exception &e) {
            LOG(ERROR) << "exception creating new resource for pool: " << m->name << " " << e.what();
            lk.lock();
            throw;
        }
        lk.lock(); // re-lock before inserting to set
        m->set.insert(c);
        return c;
    }

};

namespace detail {

// do not use this class directly, instead use your shared_pool<>::scoped_resource
template <typename T> class scoped_resource {
public:
    //typedef typename std::add_reference<shared_pool<T>>::type poolref;
    typedef typename boost::call_traits<shared_pool<T>>::reference poolref;
    typedef typename shared_pool<T>::pool_impl pool_impl;
protected:
    std::shared_ptr<pool_impl> _pool;
    std::shared_ptr<T> _c;
    bool _success;
public:
    explicit scoped_resource(poolref p)
        : _pool(p.get_safe()),
        _success(false)
    {
        _c = shared_pool<T>::acquire(_pool);
    }

    //! must call done() to return the resource to the pool
    //! otherwise we destroy it because a timeout or other exception
    //! could have occured causing the resource state to be in transition
    ~scoped_resource() {
        if (_success) {
            shared_pool<T>::release(_pool, _c);
        } else {
            shared_pool<T>::destroy(_pool, _c);
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
