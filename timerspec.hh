#include <memory.h>
#include <time.h>
#include <ostream>

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


