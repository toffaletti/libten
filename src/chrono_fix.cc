#include <chrono>
#include <time.h>

namespace std {
    namespace chrono {
        steady_clock::time_point steady_clock::now() noexcept {
            timespec ts{};
            ::clock_gettime(CLOCK_MONOTONIC, &ts);
            return time_point(duration(
                        static_cast<system_clock::rep>( ts.tv_sec ) * 1000000000 + ts.tv_nsec));
        }
    }
}

