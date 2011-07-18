#ifndef TIMESPEC_HH
#define TIMESPEC_HH

#include <memory.h>
#include <time.h>
#include <ostream>

namespace {

std::ostream &operator << (std::ostream &out, const timespec &ts) {
    out << "{" << ts.tv_sec << "," << ts.tv_nsec << "}";
    return out;
}

bool operator == (const timespec &a, const timespec &b) {
    return memcmp(&a, &b, sizeof(timespec)) == 0;
}

bool operator > (const timespec &a, const timespec &b) {
    if (a.tv_sec > b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec > b.tv_nsec) return true;
    }
    return false;
}

bool operator < (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec < b.tv_nsec) return true;
    }
    return false;
}

bool operator <= (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec <= b.tv_nsec) return true;
    }
    return false;
}

timespec &operator += (timespec &a, const timespec &b) {
    a.tv_sec += b.tv_sec;
    a.tv_nsec += b.tv_nsec;
    if (a.tv_nsec >= 1000000000L) {
        a.tv_sec++;
        a.tv_nsec -= 1000000000L;
    }
    return a;
}

timespec operator - (const timespec &a, const timespec &b) {
    timespec result;
    result = b;
    if (a.tv_nsec < result.tv_nsec) {
        int nsec = (result.tv_nsec - a.tv_nsec) / 1000000000L + 1;
        result.tv_nsec -= 1000000000L * nsec;
        result.tv_sec += nsec;
    }
    if (a.tv_nsec - result.tv_nsec > 1000000000L) {
        int nsec = (a.tv_nsec - result.tv_nsec) / 1000000000L;
        result.tv_nsec += 1000000000L * nsec;
        result.tv_sec -= nsec;
    }

    result.tv_sec -= a.tv_sec;
    result.tv_nsec -= a.tv_nsec;
    return result;
}

} // end namespace fw

#endif // TIMESPEC_HH

