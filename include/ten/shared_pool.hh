#ifndef LIBTEN_SHARED_POOL_HH
#define LIBTEN_SHARED_POOL_HH

#include "task/rendez.hh"
#include "ten/logging.hh"
#include "ten/optional.hh"

#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <type_traits>

namespace ten {

namespace detail {
template <typename T> class scoped_resource;
}

//! thread and task safe pool of shared resources
//! useful for connection pools and other types of shared resources
template <typename ResourceT, typename ScopeT = detail::scoped_resource<ResourceT>>
class shared_pool {
    friend class detail::scoped_resource<ResourceT>;

public:
    using resource_type = ResourceT;
    using scoped_resource = ScopeT;  // use this type to RAII resources from the pool

protected:
    using res_ptr = std::shared_ptr<resource_type>;
    using creator_type = std::function<res_ptr ()>;

    struct pool_impl {
        using queue_type = std::deque<res_ptr>;
        using set_type = std::unordered_set<res_ptr>;

        qutex mut;
        rendez not_empty;
        queue_type avail;
        set_type set;
        std::string name;
        creator_type new_resource;
        optional<size_t> max_resources;

        size_t size() const {
            std::lock_guard<qutex> lk(mut);
            return set.size();
        }

        bool has_avail() const {
            std::lock_guard<qutex> lk(mut);
            return !avail.empty();
        }

        void clear() {
            std::lock_guard<qutex> lk(mut);
            avail.clear();
            set.clear();
        }

        res_ptr acquire() {
            std::unique_lock<qutex> lk(mut);
            while (avail.empty()) {
                if (!max_resources || set.size() < *max_resources) {
                    // need to create a new resource
                    return new_with_lock(lk);
                }
                // can't create anymore we're at max, try waiting
                // we don't use a predicate here because
                // we might be woken up from destroy()
                // in which case we might not be at max anymore
                not_empty.sleep(lk);
            }

            CHECK(!avail.empty());
            // pop resource from front of queue
            res_ptr c{std::move(avail.front())};
            avail.pop_front();
            CHECK(c) << "acquire shared resource failed in pool: " << name;
            return c;
        }

        res_ptr new_with_lock(std::unique_lock<qutex> &lk) {
            if (lk)
                lk.unlock(); // unlock while newing resource
            res_ptr c;
            try {
                c = new_resource();
            } catch (std::exception &e) {
                LOG(ERROR) << "exception creating new resource for pool: " << name << " " << e.what();
                throw;
            }
            CHECK(c) << "new_resource failed for pool: " << name;
            DVLOG(4) << "inserting to shared_pool(" << name << "): " << c;
            lk.lock(); // re-lock before inserting to set
            set.insert(c);
            return c;
        }

        void destroy(res_ptr &c) { discard(c, false); }
        void release(res_ptr &c) { discard(c, true); }

        void discard(res_ptr &c, bool can_keep) {
            if (!c)
                return;

            std::unique_lock<qutex> lk(mut);

            if (!can_keep)
                set.erase(c);
            else if (!set.count(c))
                can_keep = false; // it must have exceeded lifetime

            const auto i = std::find(begin(avail), end(avail), c);
            if (i != end(avail))  // should never happen
                avail.erase(i);
            if (can_keep)
                avail.push_front(c);

            // give waiting threads a chance to reuse or create resource
            not_empty.wakeup();
            lk.unlock();

            c.reset();
            if (!can_keep)
                LOG(WARNING) << "destroyed shared resource from pool " << name;
        }
    };

    // this thread safety supports possible swapping of impl objects someday
    std::shared_ptr<pool_impl> _impl() const {
        std::lock_guard<std::mutex> lock(_im_mutex);
        return _im;
    }
    void _impl(std::shared_ptr<pool_impl> im) {
        std::lock_guard<std::mutex> lock(_im_mutex);
        _im = std::move(im);
    }

private:
    std::shared_ptr<pool_impl> _im;
    mutable std::mutex _im_mutex;

public:
    shared_pool(const std::string &name_,
                const creator_type &alloc_,
                decltype((_im->max_resources)) max_ = {})
    {
        auto m = std::make_shared<pool_impl>();
        m->name = name_;
        m->new_resource = alloc_;
        m->max_resources = max_;
        _im = std::move(m);
    }

    //! no copy
    shared_pool(const shared_pool &) = delete;
    shared_pool &operator =(const shared_pool &) = delete;

    //! yes move
    shared_pool(shared_pool &&) = default;
    shared_pool &operator =(shared_pool &&) = default;

    //! accessors
    const std::string &name() const { return _impl()->name; }
    size_t size() const             { return _impl()->size(); }
    void clear()                    { _impl()->clear(); }
};

namespace detail {

// do not use this class directly, instead use your shared_pool<>::scoped_resource
template <typename T>
class scoped_resource {
protected:
    using pool_type = shared_pool<T>;
    using pool_ref = typename std::add_lvalue_reference<pool_type>::type;
    using pool_impl = typename pool_type::pool_impl;
    using res_ptr = typename pool_type::res_ptr;

    std::shared_ptr<pool_impl> _pm; // no mutex here <- no contention
    res_ptr _c;
    bool _success;

public:
    //! RAII
    explicit scoped_resource(pool_ref p)
        : _pm{p._impl()},
          _c{_pm->acquire()},
          _success{}
    {}

    //! do not copy
    scoped_resource(const scoped_resource &) = delete;
    scoped_resource & operator =(const scoped_resource &) = delete;

    //! move is ok
    scoped_resource(scoped_resource &&) = default;
    scoped_resource & operator =(scoped_resource &&) = default;

    //! must call done() to return the resource to the pool
    //! otherwise we destroy it because a timeout or other exception
    //! could have occured causing the resource state to be in transition
    ~scoped_resource() {
        _pm->discard(_c, _success);
    }

    //! call this to allow resource reuse
    void done() {
        DCHECK(!_success);
        _success = true;
    }

    //! access resource
    T *get() const {
        if (!_c) throw std::runtime_error("null pointer");
        return _c.get();
    }
    T * operator -> () const { return get(); }
    T & operator * () const  { return *get(); }
};

} // end detail namespace

} // end namespace ten

#endif // LIBTEN_SHARED_POOL_HH
