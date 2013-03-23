#ifndef LIBTEN_METRICS_HH
#define LIBTEN_METRICS_HH

#include "ten/thread_local.hh"
#include "ten/synchronized.hh"
#include "ten/task.hh"
#include "ten/optional.hh"
#include "ten/logging.hh"
#include "ten/json.hh"
#include <boost/utility.hpp>
#include <boost/variant.hpp>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <sstream>

namespace ten {
namespace metrics {

// convenience wrappers to create metric names.
//   join('.', @arg)  # equivalent Perl

namespace impl {
template <typename Arg>
inline void path_shift(std::stringstream &ss, Arg&& arg) {
    ss << std::forward<Arg>(arg);
}

template <typename Arg, typename ...Args>
inline void path_shift(std::stringstream &ss, Arg&& arg, Args&& ...args) {
    ss << std::forward<Arg>(arg) << ".";
    path_shift(ss, std::forward<Args>(args)...);
}
} // namespace impl

inline std::string metric_path(const std::string &arg) { return arg; }
inline std::string metric_path(std::string &&arg)      { return std::move(arg); }

template <typename Arg, typename ...Args>
inline std::string metric_path(Arg&& arg, Args&& ...args) {
    std::stringstream ss;
    impl::path_shift(ss, std::forward<Arg>(arg), std::forward<Args>(args)...);
    return ss.str();
}

// metric classes with common base

class metric {
public:
    virtual ~metric() {}
    virtual json to_json() const = 0;
};

class counter : public metric {
public:
    using value_type = int64_t;

private:
    value_type _count = 0;

public:
    ~counter() override {}

    json to_json() const override {
        return ten::to_json(value());
    }

    value_type value() const {
        return _count;
    }

    void merge(const counter &other) {
        _count += other._count;
    }

    void deduct(const counter &other) {
        _count -= other._count;
    }

    // counter-specific:

    void incr(value_type n=1) {
        _count += n;
    }
};

// TODO: allow for ratio in json
// with incr/decr. provide names for numerator/denominator
class gauge : public metric {
public:
    using value_type = int64_t;

private:
    value_type _incr = 0;
    value_type _decr = 0;

public:
    ~gauge() override {};

    json to_json() const override {
        return json::array({value(), _incr, _decr});
    }

    value_type value() const {
        return _incr - _decr;
    }

    void merge(const gauge &other) {
        _incr += other._incr;
        _decr += other._decr;
    }

    void deduct(const gauge &other) {
        _incr -= other._incr;
        _decr -= other._decr;
    }

    // gauge-specific:

    void incr(value_type n=1) {
        _incr += n;
    }

    void decr(value_type n=1) {
        _decr += n;
    }

    double ratio() const {
        return _decr/_incr;
    }
};

// report in JSON as floating point milliseconds; keep as tenths of milliseconds
class timer : public metric {
public:
    using value_type = std::chrono::microseconds;
    using clock_type = std::chrono::high_resolution_clock;

private:
    clock_type::duration _elapsed;

public:
    ~timer() override {};

    json to_json() const override {
        // value units: microseconds
        // display units: milliseconds
        return ten::to_json(static_cast<double>(value().count()) / 1000.0);
    }

    value_type value() const {
        return std::chrono::duration_cast<value_type>(_elapsed);
    }

    void merge(const timer &other) {
        _elapsed += other._elapsed;
    }

    void deduct(const timer &other) {
        _elapsed -= other._elapsed;
    }

    // timer-specific:

    void update(clock_type::duration elapsed) {
        _elapsed += elapsed;
    }
};

//----------------------------------------------------------------
// metric groups
//   any_metric - variant<*>
//   group_map - maps string -> any_metric
//   group - has a lock and is automatically aggregated on destruction
//

using any_metric = boost::variant<counter, gauge, timer>;

// derive rather than alias to trigger ADL

struct group_map;
void merge_to(group_map &to, const group_map &from);
void deduct_from(group_map &to, const group_map &from);

struct group_map : std::unordered_map<std::string, any_metric> {
    // and while we're here, let's throw in some operators
    group_map & operator += (const group_map &b) { merge_to(*this, b);    return *this; }
    group_map & operator -= (const group_map &b) { deduct_from(*this, b); return *this; }

    friend group_map operator + (group_map a, const group_map &b) { merge_to(a, b);    return a; }
    friend group_map operator - (group_map a, const group_map &b) { deduct_from(a, b); return a; }
};

// group_map accessors:
//   get() allows update, adds keys as needed
//   value() does neither

template <typename M, typename ...Args>
    inline M &get(group_map &gm, Args&&... args) {
        const std::string name = metric_path(std::forward<Args>(args)...);
        auto i = gm.find(name);
        if (i == end(gm)) {
            auto ii = gm.emplace(name, M());
            i = ii.first;
        }
        return boost::get<M>(i->second);
    }

template <typename ...Args>
    counter &get_counter(group_map &gm, Args&& ...args) { return get<counter>(gm, std::forward<Args>(args)...); }
template <typename ...Args>
    gauge   &get_gauge(  group_map &gm, Args&& ...args) { return get<gauge  >(gm, std::forward<Args>(args)...); }
template <typename ...Args>
    timer   &get_timer(  group_map &gm, Args&& ...args) { return get<timer  >(gm, std::forward<Args>(args)...); }

template <typename M, typename ...Args>
    inline auto value(const group_map &gm, Args&&... args) -> typename M::value_type {
        const std::string name = metric_path(std::forward<Args>(args)...);
        auto i = gm.find(name);
        return (i == end(gm))
            ? typename M::value_type()
            : boost::get<M>(i->second).value();
    }

// how to make json from any_metric

struct json_visitor : boost::static_visitor<json> {
    template <typename Met>
        json operator()(const Met &m) const {
            return m.to_json();
        }
};

inline json to_json(const any_metric &met) {
    return boost::apply_visitor(json_visitor(), met);
}

// group - locked, automatically aggregated
// don't put anything in here you don't want summed

struct group : public synchronized<group_map> {
    group();
    ~group();

    group(const group &) = delete;
    group(group &&other);
    group & operator = (const group &) = delete;
    group & operator = (group &&other) = delete;
};

// group accessors
//   accessor that grabs lock for access
//   no use now; kept here in case the pattern is useful later.

template <typename M, typename ...Args>
    inline auto value(const group &g, Args&&... args) -> typename M::value_type {
        return value<M>(*sync_view(g), std::forward<Args>(args)...);
    }

// locked_group - convenience wrapper to hold a group lock and make multiple changes

class locked_group {
    group::view _gview;

public:
    locked_group(group &g) : _gview(sync_view(g)) {}

    group_map &map() { return *_gview; }

    template <typename ...Args>
        metrics::counter &counter(Args&& ...args) {
            return get<metrics::counter>(*_gview, std::forward<Args>(args)...);
        }
    template <typename ...Args>
        metrics::gauge &gauge(Args&& ...args) {
            return get<metrics::gauge>(*_gview, std::forward<Args>(args)...);
        }
    template <typename ...Args>
        metrics::timer &timer(Args&& ...args) {
            return get<metrics::timer>(*_gview, std::forward<Args>(args)...);
        }
};

// merge when one or the other needs a lock

template <class T, class F>
inline void merge_to(T &to, const F &from) {
    maybe_sync(from, [&](const group_map &m_from) mutable {
        maybe_sync(to, [&](group_map &m_to) mutable {
            merge_to(m_to, m_from);
        });
    });
}

//----------------------------------------------------------------
// thread-local metric group

struct metric_tag {};
extern thread_cached<metric_tag, group> tls_group;

inline locked_group record() {
    return locked_group(*tls_group.get());
}

template <typename Func>
inline void record(Func &&f) {
    synchronize(*tls_group.get(), f);
}

// singleton global metrics, accumulated from individual activities,
//   which are not themselves synchronized

class global_group;
extern global_group global;

class global_group {
    struct data {
        std::vector<const group *> groups;
        group_map removed;
    };
    synchronized<data> _data;

public:
    global_group() {}
    ~global_group() {}

    void push_back(const group *g) {
        _data([g](data &d) {
            d.groups.push_back(g);
        });
    }

    // TODO: dont actually want to remove the group until the stats have been merged
    // otherwise a thread could spawn and exit before an aggregation and the metrics
    // would be lost.  this suggests that groups should be on the heap
    void remove(const group *g) {
        _data([&](data &d) {
            const auto i = std::find(std::begin(d.groups), std::end(d.groups), g);
            CHECK(i != std::end(d.groups));
            merge_to(d.removed, *g); // keep around total stats for removed threads
            d.groups.erase(i);
        });
    }

    group_map aggregate() {
        group_map ag;
        _data([&](data &d) {
            merge_to(ag, d.removed);  // from removed threads
            for (auto g : d.groups)
                merge_to(ag, *g);  // from live threads
        });
        return ag;
    }
};

inline group::group()   { global.push_back(this); }
inline group::~group()  { global.remove(this); }

//----------------------------------------------------------------
// scoped timer convenience

class time_op {
protected:
    using clock_type = timer::clock_type;

    group &_g;
    const std::string _name;
    clock_type::time_point _start;
    timer::value_type _elapsed;
    bool _stopped = false;

public:
    template <typename ...Args>
        time_op(group &g, Args&& ...args)
            : _g(g),
              _name(metric_path(std::forward<Args>(args)...)),
              _start(clock_type::now()),
              _elapsed() {}

    template <typename ...Args>
        time_op(Args&& ...args)
            : time_op(*tls_group.get(), std::forward<Args>(args)...) {}

    ~time_op() {
        stop();
    }

    time_op(const time_op &) = delete;
    time_op & operator = (const time_op &) = delete;

    time_op(time_op &&) = default;
    time_op & operator = (time_op &&) = default;

    void stop() {
        // can't stop more than once
        if (_stopped) return;
        _stopped = true;
        _elapsed = std::chrono::duration_cast<timer::value_type>(clock_type::now() - _start);
        _g([this](group_map &m){ 
            get<timer>(m, _name).update(_elapsed);
        });
    }

    timer::value_type elapsed() const { return _elapsed; }
};

//----------------------------------------------------------------
// time sequence collection

class intervals {
    using interval_type = std::chrono::seconds;
    using value_type = std::pair<time_t, group_map>;
    using queue_type = std::deque<value_type>;

    const interval_type _interval;
    const size_t _depth;
    synchronized<queue_type> _queue;
    task _task;

    void _collector_main();

public:
    intervals(interval_type interval, size_t depth);
    ~intervals();

    interval_type interval() const { return _interval; }
    size_t        depth()    const { return _depth; }
    interval_type duration() const { return _interval * _depth; }

    using result_type = std::pair<interval_type, group_map>;
    optional<result_type> last(interval_type span) const;
};

} // end namespace metrics
} // end namespace ten

#endif
