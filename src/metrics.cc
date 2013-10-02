#include "ten/metrics.hh"
#include "ten/error.hh"
#include "ten/task.hh"
#include <algorithm>

namespace ten {
namespace metrics {

global_group global;

thread_cached<metric_tag, group> tls_group;

// merge

namespace {

struct merge_visitor : boost::static_visitor<> {
    any_metric &lhs;
    merge_visitor(any_metric &lhs_) : lhs(lhs_) {}
    template <typename M>
        void operator()(const M &rhs) const {
            boost::get<M>(lhs).merge(rhs);
        }
};

struct deduct_visitor : boost::static_visitor<> {
    any_metric &lhs;
    deduct_visitor(any_metric &lhs_) : lhs(lhs_) {}
    template <typename M>
        void operator()(const M &rhs) const {
            boost::get<M>(lhs).deduct(rhs);
        }
};

} // anon namespace

void merge_to(group_map &to, const group_map &from) {
    for (const auto &fkv : from) {
        // try to insert into merged map
        const auto i = to.insert(fkv);
        if (!i.second) {
            // merge with existing
            boost::apply_visitor(merge_visitor{i.first->second}, fkv.second);
        }
    }
}

void deduct_from(group_map &to, const group_map &from) {
    for (const auto &fkv : from) {
        // target key must end up existing, or information will be lost
        boost::apply_visitor(deduct_visitor{to[fkv.first]}, fkv.second);
    }
}

//----------------------------------------------------------------
// time sequence collection

intervals::intervals(interval_type interval, size_t depth)
    : _interval(interval), _depth(depth)
{
    if (interval.count() <= 0 || !depth)
        throw errorx("invalid metric intervals: interval=%jd depth=%zu", intmax_t(interval.count()), depth);
    _task = task::spawn([=] {
        _collector_main();
    });
}

intervals::~intervals() {
    _task.cancel();
}

void intervals::_collector_main() {
    using namespace std::chrono;
    const auto depth = _depth;
    for (;;) {
        this_task::sleep_for(_interval);
        auto ag = global.aggregate();
        const time_t now = ::time(nullptr);
        _queue([&ag, now, depth](queue_type &q) mutable {
            using dt = queue_type::difference_type;
            const dt excess = dt(q.size()) - dt(depth);
            if (excess > 0)
                q.erase(begin(q), begin(q) + excess);
            q.emplace_back(now, std::move(ag));
        });
    }
}

auto intervals::last(interval_type span) const -> optional<result_type> {
    using ret_t = optional<result_type>;
    using std::chrono::seconds;

    if (span <= interval_type{0})
        return nullopt;

    const auto interval = _interval;
    return _queue([span, interval](const queue_type &q) -> ret_t {
        const auto to = q.rbegin();
        auto from = std::find_if(
            q.rbegin(),
            q.rend(),
            [=](const value_type &v) { return seconds{to->first - v.first} + (interval / 2) >= span; }
        );
        return from == q.rend()
                ? ret_t()
                : ret_t(in_place, seconds{to->first - from->first}, to->second - from->second);
    });
}

} // metrics
} // ten
