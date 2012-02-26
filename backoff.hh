#include <chrono>
#include <algorithm>
#include <random>
#include <tr1/random>

namespace ten {

using namespace std::chrono;

template <typename DurationT> class backoff {
public:
    backoff(const DurationT &min_delay_, const DurationT &max_delay_)
        : eng(time(0)), rand(eng, std::tr1::uniform_real<float>(0.0, 1.0)),
        min_delay(min_delay_), max_delay(max_delay_), retry(0)
    {
    }

    void reset() { retry = 0; }

    DurationT next_delay()
    {
        double rnd = rand();
        typename DurationT::rep tmp = min_delay.count() + (min_delay.count() * rnd * (retry + 1));
        typename DurationT::rep rep = std::min(max_delay.count(), tmp);
        ++retry;
        return DurationT(rep);
    }

protected:
    std::minstd_rand eng;
    std::tr1::variate_generator<std::minstd_rand, std::tr1::uniform_real<float>> rand;
    DurationT min_delay;
    DurationT max_delay;
    uint64_t retry;
};

} // end namespace ten
