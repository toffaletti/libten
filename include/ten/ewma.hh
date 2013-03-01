#ifndef TEN_EWMA_HH
#define TEN_EWMA_HH

#include <chrono>
#include <cmath>
#include <atomic>
#include "ten/optional.hh"

namespace ten {

template <class TimeUnit>
class ewma {
private:
    TimeUnit _decay_interval;
    double _alpha;
    std::atomic<uint64_t> _pending {};
    double _rate {};
public:
    ewma(TimeUnit decay_interval, TimeUnit tick_interval = TimeUnit{1})
        : _decay_interval{decay_interval},
          _alpha{exp(-(double)tick_interval.count() / decay_interval.count())}
    {}

    void update(uint64_t n) {
        _pending.fetch_add(n);
    }

    void tick() {
        double count = _pending.exchange(0);
        double r = _rate;
        r *= _alpha;
        r += count / (double)_decay_interval.count();
        _rate = r;
    }

    template <class RateUnit=std::chrono::seconds>
    double rate() {
        using cvt = std::ratio_divide< typename RateUnit::period, typename TimeUnit::period >;
        return (_rate * cvt::num) / cvt::den;
    }
};

} // ten

#endif
