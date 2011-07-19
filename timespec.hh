#ifndef TIMESPEC_HH
#define TIMESPEC_HH

#include <memory.h>
#include <time.h>
#include <ostream>
#include <iostream>

inline bool operator == (const timespec &a, const timespec &b) {
    return memcmp(&a, &b, sizeof(timespec)) == 0;
}

inline std::ostream &operator << (std::ostream &out, const timespec &ts) {
    out << "{" << ts.tv_sec << "," << ts.tv_nsec << "}";
    return out;
}

inline bool operator > (const timespec &a, const timespec &b) {
    if (a.tv_sec > b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec > b.tv_nsec) return true;
    }
    return false;
}

inline bool operator < (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec < b.tv_nsec) return true;
    }
    return false;
}

inline bool operator <= (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec <= b.tv_nsec) return true;
    }
    return false;
}

inline timespec &operator += (timespec &a, const timespec &b) {
    a.tv_sec += b.tv_sec;
    a.tv_nsec += b.tv_nsec;
    if (a.tv_nsec >= 1000000000L) {
        a.tv_sec++;
        a.tv_nsec -= 1000000000L;
    }
    return a;
}

inline timespec operator - (const timespec &a, const timespec &b) {
    timespec tmp;
    if ((a.tv_nsec - b.tv_nsec) < 0) {
        tmp.tv_sec = a.tv_sec - b.tv_sec - 1;
        tmp.tv_nsec = 1000000000 + a.tv_nsec - b.tv_nsec;
    } else {
        tmp.tv_sec = a.tv_sec - b.tv_sec;
        tmp.tv_nsec = a.tv_nsec - b.tv_nsec;
    }
    return tmp;
}

#endif // TIMESPEC_HH

