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

namespace ten {

namespace metrics {

class metric {
    virtual json to_json() const = 0;
};

class counter : public metric {
private:
    uint64_t _count = 0;
public:
    json to_json() const {
        return ten::to_json(value());
    }
    void incr() {
        ++_count;
    }

    int64_t value() const {
        return _count;
    }

    void add(const counter &other) {
        _count += other._count;
    }
};

// TODO: allow for ratio in json
// with incr/decr. provide names for numerator/denominator
class gauge : public metric {
private:
    uint64_t _incr = 0;
    uint64_t _decr = 0;
public:
    void add(const gauge &other) {
        _incr += other._incr;
        _decr += other._decr;
    }

    json to_json() const {
        return ten::to_json(value());
    }
    void incr() {
        ++_incr;
    }
    void decr() {
        ++_decr;
    }
    int64_t value() const {
        return _incr - _decr;
    }
    std::pair<uint64_t, uint64_t> value_pair() const {
        return std::make_pair(_incr, _decr);
    }
    double ratio() const {
        return _decr/_incr;
    }
};

// variant of all possible metrics
typedef boost::variant<counter, gauge> metric_type;

struct add_visitor : boost::static_visitor<> {
    metric_type &_lhs;

    add_visitor(metric_type &lhs) : _lhs(lhs){}

    template <typename T> void operator()(const T &rhs) const {
        boost::get<T>(_lhs).add(rhs);
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
    template <typename T> T &update(const std::string &name) {
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
                boost::apply_visitor(add_visitor(i.first->second), iter->second);
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

template <typename Func> void record(Func &&f) {
    metric_group *mg = thread_local_ptr<metric_group>();
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

} // end namespace metrics
} // end namespace ten

#endif
