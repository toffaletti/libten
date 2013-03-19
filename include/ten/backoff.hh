#ifndef LIBTEN_BACKOFF_HH
#define LIBTEN_BACKOFF_HH

#include "ten/error.hh"
#include "ten/logging.hh" // temp
#include <chrono>
#include <algorithm>
#include <random>

namespace ten {

//! calculate back off times for retrying operations
//! based on algorithm used by tcp
template <typename DurationT> class backoff {
private:
    const DurationT _min_delay;
    const DurationT _max_delay;
    uint64_t _try;
    std::minstd_rand _eng;
    std::function<float ()> _rand;

public:
    backoff(DurationT min_delay, DurationT max_delay, float scale = 1.0f)
        : _min_delay(min_delay),
          _max_delay(max_delay),
          _try(0),
          _eng(time(nullptr)),
          _rand(std::bind(std::uniform_real_distribution<float>(0.0f, scale), std::ref(_eng)))
    {
        if (min_delay.count() < 0 || min_delay > max_delay)
            throw errorx("invalid backoff(%jd, %jd)", intmax_t(min_delay.count()), intmax_t(max_delay.count()));
    }

    void reset() { _try = 0; }

    DurationT next_delay() {
        ++_try;
        float r = _rand();
        float delta = 1.0f + (r * float(_try));
        const auto d = std::chrono::duration_cast<DurationT>(_min_delay * delta);
        return std::min(d, _max_delay);
    }
};

} // end namespace ten

#endif // LIBTEN_BACKOFF_HH
