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
#include <sstream>

namespace ten {

namespace metrics {

template <typename Arg>
void metric_path(std::stringstream &ss, Arg arg) {
    ss << arg;
}

template <typename Arg, typename ...Args>
void metric_path(std::stringstream &ss, Arg arg, Args... args) {
    ss << arg << ".";
    metric_path(ss, args...);
}

std::string metric_path(const std::string &arg) {
    return arg;
}

template <typename ...Args>
std::string metric_path(Args... args) {
    std::stringstream ss;
    metric_path(ss, args...);
    return ss.str();
}



class metric {
    virtual json to_json() const = 0;
};

class counter : public metric {
private:
    int64_t _count = 0;
public:
    json to_json() const {
        return ten::to_json(value());
    }

    void incr(int64_t n=1) {
        _count+=n;
    }

    int64_t value() const {
        return _count;
    }

    void merge(const counter &other) {
        _count += other._count;
    }
};

// TODO: allow for ratio in json
// with incr/decr. provide names for numerator/denominator
class gauge : public metric {
private:
    int64_t _incr = 0;
    int64_t _decr = 0;
public:
    void merge(const gauge &other) {
        _incr += other._incr;
        _decr += other._decr;
    }

    json to_json() const {
        return ten::json::array({value(), _incr, _decr});
    }

    void incr(int64_t n=1) {
        _incr+=n;
    }

    void decr(int64_t n=1) {
        _decr+=n;
    }

    int64_t value() const {
        return _incr - _decr;
    }

    std::pair<int64_t, int64_t> value_pair() const {
        return std::make_pair(_incr, _decr);
    }

    double ratio() const {
        return _decr/_incr;
    }
};

class timer : public metric {
public:
    typedef std::chrono::high_resolution_clock clock_type;
private:
    clock_type::duration _elapsed;
public:
    std::chrono::milliseconds value() const {
        using namespace std::chrono;
        return duration_cast<milliseconds>(_elapsed);
    }

    json to_json() const {
        return ten::to_json(value().count());
    }

    void update(const clock_type::duration &elapsed) {
        _elapsed += elapsed;
    }

    void merge(const timer &other) {
        _elapsed += other._elapsed;
    }
};

// variant of all possible metrics
typedef boost::variant<counter, gauge, timer> metric_type;

struct merge_visitor : boost::static_visitor<> {
    metric_type &_lhs;

    merge_visitor(metric_type &lhs) : _lhs(lhs){}

    template <typename T> void operator()(const T &rhs) const {
        boost::get<T>(_lhs).merge(rhs);
    }
};


class metric_group : boost::noncopyable {
public: // TODO: protected
    typedef std::unordered_map<std::string, metric_type> map_type;
    std::mutex _mtx;
    map_type _metrics;
    metric_group();
    ~metric_group();
public:
    template <typename T> T &get(const std::string &name) {
        auto i = _metrics.find(name);
        if (i == _metrics.end()) {
            auto ii = _metrics.insert(std::make_pair(name, T()));
            i = ii.first;
        }
        return boost::get<T>(i->second);
    }

    void merge_to(map_type &ag) {
        std::lock_guard<std::mutex> lock(_mtx);
        for (auto iter = std::begin(_metrics); iter !=  std::end(_metrics); ++iter) {
            // try to insert into merged map
            auto i = ag.insert(*iter);
            if (!i.second) {
                // merge with existing
                boost::apply_visitor(merge_visitor(i.first->second), iter->second);
            }
        }
    }
};

class metric_global {
public:
    typedef std::vector<metric_group *> container_type;
    struct holder {
        typedef std::vector<metric_group *> container_type;
        metric_group::map_type removed;
        container_type groups;
    };

    synchronized<holder> _data;

    metric_global();
    ~metric_global();

    void push_back(metric_group *g) {
        synchronize(_data, [&](holder &h) {
            h.groups.push_back(g);
        });
    }

    // TODO: dont actually want to remove it until the stats have been merged
    // otherwise a thread could spawn and exit before an aggregation and the
    // stats would be lost. this suggests the stats should be on the heap
    void remove(metric_group *g) {
        synchronize(_data, [&](holder &h) {
            auto i = std::find(std::begin(h.groups), std::end(h.groups), g);
            CHECK(i != std::end(h.groups));
            (*i)->merge_to(h.removed); // keep stats for removed threads around
            h.groups.erase(i);
        });
    }

    std::unordered_map<std::string, metric_type> aggregate();
};

static metric_global global;

struct metric_tag {};
thread_local<metric_tag, metric_group> tls_metric_group;

struct chain {
    metric_group *_mg;
    std::unique_lock<std::mutex> _lock;

    chain(metric_group *mg)
        : _mg(mg), _lock(mg->_mtx) {}

    chain(const chain &) = delete;
    chain(chain &&other)
        : _mg(other._mg), _lock(std::move(other._lock)) {}

    template <typename ...Args>
        metrics::counter &counter(Args ...args) {
            return _mg->get<metrics::counter>(metric_path(args...));
        }

    template <typename ...Args>
        metrics::gauge &gauge(Args ...args) {
            return _mg->get<metrics::gauge>(metric_path(args...));
        }
};

chain record() {
    return chain(tls_metric_group.get());
}

template <typename Func> void record(Func &&f) {
    metric_group *mg = tls_metric_group.get();
    std::lock_guard<std::mutex> lock(mg->_mtx);
    f(std::ref(*mg));
}

template <typename T> auto value(const metric_group::map_type &mg, const std::string &name)
    -> decltype(boost::get<T>(mg.at(name)).value())
{
    return boost::get<T>(mg.at(name)).value();
}

struct json_visitor : boost::static_visitor<json> {
    template <typename T> json operator()(const T &m) const {
        return m.to_json();
    }
};

metric_global::metric_global() {
}

metric_global::~metric_global() {
}

std::unordered_map<std::string, metric_type> metric_global::aggregate() {
    std::unordered_map<std::string, metric_type> ag;
    synchronize(_data, [&](metric_global::holder &h) {
        ag = h.removed; // copy all the removed stats into aggregate
        for (auto mg : h.groups) {
            mg->merge_to(ag);
        }
    });
    return ag;
}

metric_group::metric_group() {
    global.push_back(this);
}

metric_group::~metric_group() {
    global.remove(this);
}

struct time_op {
private:
    typedef timer::clock_type clock_type;
    bool _stopped = false;
    std::string _name;
    clock_type::time_point _start;
public:
    template <typename ...Args>
        time_op(Args ...args)
        : _name(metric_path(args...)), _start(clock_type::now()) {}

    time_op(const std::string &name)
        : _name(name), _start(clock_type::now()) {}

    void stop() {
        // can't stop more than once
        if (_stopped) return;
        _stopped = true;
        auto stop = clock_type::now();
        record([&](metric_group &g) {
            g.get<timer>(_name).update(stop - _start);
        });
    }

    ~time_op() {
        stop();
    }
};


} // end namespace metrics
} // end namespace ten

#endif
