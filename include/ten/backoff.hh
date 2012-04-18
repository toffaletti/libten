#ifndef LIBTEN_BACKOFF_HH
#define LIBTEN_BACKOFF_HH

#include <chrono>
#include <algorithm>
#include <random>
#include <tr1/random>

namespace ten {

//! calculate back off times for retrying operations
//! based on algorithm used by tcp
template <typename DurationT> class backoff {
private:
    std::minstd_rand _eng;
    std::tr1::variate_generator<std::minstd_rand, std::tr1::uniform_real<float>> _rand;
    DurationT _min_delay;
    DurationT _max_delay;
    uint64_t _retry;
public:
    backoff(const DurationT &min_delay, const DurationT &max_delay)
        : _eng(time(0)), _rand(_eng, std::tr1::uniform_real<float>(0.0, 1.0)),
        _min_delay(min_delay), _max_delay(max_delay), _retry(0)
    {
    }

    void reset() { _retry = 0; }

    DurationT next_delay()
    {
        double rnd = rand();
        typename DurationT::rep tmp = _min_delay.count() + (_min_delay.count() * rnd * (_retry + 1));
        typename DurationT::rep rep = std::min(_max_delay.count(), tmp);
        ++_retry;
        return DurationT(rep);
    }
};

} // end namespace ten

#endif // LIBTEN_BACKOFF_HH
