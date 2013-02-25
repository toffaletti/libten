#ifndef LIBTEN_METRICS_HH
#define LIBTEN_METRICS_HH

#include "ten/thread_local.hh"
#include "ten/synchronized.hh"
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
    ss << arg << ".";
    path_shift(ss, std::forward<Args>(args)...);
}
} // namespace impl

inline std::string metric_path(const std::string &arg) { return arg; }
inline std::string metric_path(std::string &&arg)      { return arg; }

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
    ~counter() override {};

    json to_json() const override {
        return ten::to_json(value());
    }

    value_type value() const {
        return _count;
    }


    void incr(value_type n = 1) {
        _count+=n;
    }

    void merge(const counter &other) {
        _count += other._count;
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
        return ten::json::array({value(), _incr, _decr});
    }

    void merge(const gauge &other) {
        _incr += other._incr;
        _decr += other._decr;
    }

    void incr(value_type n=1) {
        _incr+=n;
    }

    void decr(value_type n=1) {
        _decr+=n;
    }

    value_type value() const {
        return _incr - _decr;
    }

    std::pair<value_type, value_type> value_pair() const {
        return std::make_pair(_incr, _decr);
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
        // resolution: centiseconds
        auto cents = value().count() / 100;
        return ten::to_json((double)cents / 10.0);
    }

    // caller specifies desired duration type to avoid misinterpreting .count()
    value_type value() const {
        return std::chrono::duration_cast<value_type>(_elapsed);
    }

    void update(clock_type::duration elapsed) {
        _elapsed += elapsed;
    }

    void merge(const timer &other) {
        _elapsed += other._elapsed;
    }
};

// variant of all possible metrics
typedef boost::variant<counter, gauge, timer> any_metric;

struct merge_visitor : boost::static_visitor<> {
    any_metric &_lhs;

    merge_visitor(any_metric &lhs) : _lhs(lhs){}

    template <typename M>
        void operator()(const M &rhs) const {
            boost::get<M>(_lhs).merge(rhs);
        }
};

// collection of arbitrary metrics with arbitrary names

typedef std::unordered_map<std::string, any_metric> metric_map;

template <typename M>
    inline M &get(metric_map &mm, const std::string &name) {
        auto i = mm.find(name);
        if (i == end(mm)) {
            auto ii = mm.emplace(name, M());
            i = ii.first;
        }
        return boost::get<M>(i->second);
    }

template <typename M>
    inline typename M::value_type value(const metric_map &mm, const std::string &name) {
        auto i = mm.find(name);
        return (i == end(mm))
            ? typename M::value_type()
            : boost::get<M>(i->second).value();
    }

// a lockable wrapper around metric_map
// TODO: replace with synchronized<metric_map>

class metric_group {
    metric_map _map;
    mutable std::mutex _mtx;

public:
    struct mg_lock : std::unique_lock<std::mutex> {
        mg_lock(const metric_group &g) : unique_lock(g._mtx) {}
    };

    metric_group();
    ~metric_group();

    metric_group(const metric_group &) = delete;
    metric_group & operator = (const metric_group &) = delete;

    metric_group(metric_group &&) = default;
    metric_group & operator = (metric_group &&) = default;

    // unsafe accessors; typically should be used via chain

    template <typename M>
        M &get(const std::string &name)
            { return metrics::get<M>(_map, name); }

    template <typename M>
        typename M::value_type value(const std::string &name) const
            { return metrics::value<M>(_map, name); }

    // safe mass merge

    void safe_merge_to(metric_map &ag) const {
        mg_lock(*this);
        for (const auto &kv : _map) {
            // try to insert into merged map
            const auto i = ag.insert(kv);
            if (!i.second) {
                // merge with existing
                boost::apply_visitor(merge_visitor(i.first->second), kv.second);
            }
        }
    }
};

// a chain is a move-semantic locked accessor for a metric_group

class chain {
    metric_group * const _mg;
    metric_group::mg_lock _lock;

public:
    chain(metric_group *mg) : _mg(mg), _lock(*mg) {}

    chain(const chain &) = delete;
    chain & operator = (const chain &) = delete;

    chain(chain &&) = default;
    chain & operator = (chain &&) = default;

    template <typename ...Args>
        metrics::counter &counter(Args&& ...args) {
            return _mg->get<metrics::counter>(metric_path(std::forward<Args>(args)...));
        }

    template <typename ...Args>
        metrics::gauge &gauge(Args&& ...args) {
            return _mg->get<metrics::gauge>(metric_path(std::forward<Args>(args)...));
        }

    template <typename ...Args>
        metrics::timer &timer(Args&& ...args) {
            return _mg->get<metrics::timer>(metric_path(std::forward<Args>(args)...));
        }

};

struct metric_tag {};
extern thread_cached<metric_tag, metric_group> tls_metric_group;

inline chain record() {
    return chain(tls_metric_group.get());
}

template <typename Func>
inline void record(Func &&f) {
    metric_group *mg = tls_metric_group.get();
    metric_group::mg_lock lk(*mg);
    f(std::ref(*mg));
}

struct json_visitor : boost::static_visitor<json> {
    template <typename Met>
        json operator()(const Met &m) const {
            return m.to_json();
        }
};

// singleton global metrics, accumulated from individual activities

class metric_global;
extern metric_global global;

class metric_global {
public:
    struct data {
        std::vector<metric_group *> groups;
        metric_map removed;
    };
    synchronized<data> _data;

    metric_global() {}
    ~metric_global() {}

    void push_back(metric_group *g) {
        synchronize(_data, [&](data &d) {
            d.groups.push_back(g);
        });
    }

    // TODO: dont actually want to remove the group until the stats have been merged
    // otherwise a thread could spawn and exit before an aggregation and the metrics
    // would be lost.  this suggests that groups should be on the heap
    void remove(metric_group *g) {
        synchronize(_data, [&](data &d) {
            const auto i = std::find(std::begin(d.groups), std::end(d.groups), g);
            CHECK(i != std::end(d.groups));
            (*i)->safe_merge_to(d.removed); // keep stats for removed threads around
            d.groups.erase(i);
        });
    }

    metric_map aggregate() {
        metric_map ag;
        synchronize(_data, [&](data &d) {
            ag = d.removed; // copy all the removed stats into aggregate
            for (auto g : d.groups) {
                g->safe_merge_to(ag);
            }
        });
        return ag;
    }
};

inline metric_group::metric_group()   { global.push_back(this); }
inline metric_group::~metric_group()  { global.remove(this); }

// scoped timer convenience

template <class MG>
class time_op_base {
protected:
    using clock_type = timer::clock_type;

    MG &_mg;
    std::string _name;
    clock_type::time_point _start;
    bool _stopped = false;

    template <typename ...Args>
        time_op_base(MG &mg, Args&& ...args)
            : _mg(mg),
              _name(metric_path(std::forward<Args>(args)...)),
              _start(clock_type::now()) {}

    time_op_base(const time_op_base &) = delete;
    time_op_base & operator = (const time_op_base &) = delete;

    time_op_base(time_op_base &&) = default;
    time_op_base & operator = (time_op_base &&) = default;
};

class time_op : public time_op_base<metric_group> {
public:
    template <typename ...Args>
        time_op(metric_group &mg, Args&& ...args)
            : time_op_base(mg, std::forward<Args>(args)...) {}

    template <typename ...Args>
        time_op(Args&& ...args)
            : time_op_base(*tls_metric_group.get(), std::forward<Args>(args)...) {}

    ~time_op() {
        stop();
    }
    void stop() {
        // can't stop more than once
        if (_stopped) return;
        _stopped = true;
        const auto dur = clock_type::now() - _start;
        metric_group::mg_lock lk(_mg);
        _mg.get<timer>(_name).update(dur);
    }
};

// use only when you are safe from multithreading
class map_time_op : public time_op_base<metric_map> {
public:
    template <typename ...Args>
        map_time_op(metric_map &mm, Args&& ...args)
            : time_op_base(mm, std::forward<Args>(args)...) {}

    ~map_time_op() {
        stop();
    }
    void stop() {
        // can't stop more than once
        if (_stopped) return;
        _stopped = true;
        const auto dur = clock_type::now() - _start;
        get<timer>(_mg, _name).update(dur);
    }
};

} // end namespace metrics
} // end namespace ten

#endif
