#ifndef LIBTEN_BACKOFF_HH
#define LIBTEN_BACKOFF_HH

#include "ten/error.hh"
#include <chrono>
#include <chrono_io>
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
    float _scale;

public:
    backoff(DurationT min_delay, DurationT max_delay, float scale = 1.0f)
        : _min_delay(min_delay),
          _max_delay(max_delay),
          _try(0),
          _eng((long unsigned int)time(nullptr)),
          _scale(scale)
    {
        if (min_delay.count() < 0 || min_delay > max_delay)
            throw_stream() << "invalid backoff(" << min_delay << ", " << max_delay << ")";
    }

    void reset() { _try = 0; }

    DurationT next_delay() {
        std::uniform_real_distribution<float> rand_real{0.0f, _scale};
        ++_try;
        float r = rand_real(_eng);
        float delta = 1.0f + (r * float(_try));
        const auto d = std::chrono::duration_cast<DurationT>(_min_delay * delta);
        return std::min(d, _max_delay);
    }
};

template <typename D1, typename D2,
          typename DurationT = typename std::common_type<D1, D2>::type>
inline backoff<DurationT> make_backoff(D1 d1, D2 d2, float scale = 1.0f) {
    return backoff<DurationT>(d1, d2, scale);
}

} // end namespace ten

#endif // LIBTEN_BACKOFF_HH
