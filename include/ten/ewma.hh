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
    std::atomic<uint64_t> _pending;
    double _alpha;
    double _rate = 0.0;
    TimeUnit _decay_interval;
public:
    ewma(TimeUnit decay_interval, TimeUnit tick_interval = TimeUnit{1})
        : _decay_interval(decay_interval)
    {
        _alpha = exp(-(double)tick_interval.count() / decay_interval.count());
    }

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
        using std::chrono::duration_cast;
        return _rate * duration_cast<TimeUnit>(RateUnit{1}).count();
    }
};

} // ten

#endif
