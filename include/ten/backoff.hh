#ifndef LIBTEN_BACKOFF_HH
#define LIBTEN_BACKOFF_HH

#include <chrono>
#include <algorithm>
#include <random>
//#include <tr1/random>

namespace ten {

//! calculate back off times for retrying operations
//! based on algorithm used by tcp
template <typename DurationT> class backoff {
private:
    std::minstd_rand _eng;
    std::function<double ()> _rand;
    //std::variate_generator<std::minstd_rand, std::uniform_real<float>> _rand;
    DurationT _min_delay;
    DurationT _max_delay;
    uint64_t _retry;
public:
    backoff(const DurationT &min_delay, const DurationT &max_delay)
        : _eng(time(0)), _min_delay(min_delay), _max_delay(max_delay), _retry(0)
    {
        _rand = std::bind(std::uniform_real_distribution<float>(0.0f, 1.0f), _eng);
    }

    void reset() { _retry = 0; }

    DurationT next_delay()
    {
        double rnd = _rand();
        typename DurationT::rep tmp = _min_delay.count() + (_min_delay.count() * rnd * (_retry + 1));
        typename DurationT::rep rep = std::min(_max_delay.count(), tmp);
        ++_retry;
        return DurationT(rep);
    }
};

} // end namespace ten

#endif // LIBTEN_BACKOFF_HH
