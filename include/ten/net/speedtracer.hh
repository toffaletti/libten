#ifndef LIBTEN_NET_SPEEDTRACER_HH
#define LIBTEN_NET_SPEEDTRACER_HH

#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include "ten/json.hh"
#include "ten/logging.hh"

// TODO: rework this to have a trace and event classes
// trace holds the list of events
// 

// http://code.google.com/webtoolkit/speedtracer/data-dump-format.html
// http://code.google.com/p/google-web-toolkit/source/browse/trunk/dev/core/src/com/google/gwt/dev/util/log/speedtracer/SpeedTracerLogger.java

namespace ten {
using std::move;
using std::string;
using namespace std::chrono;

#define TRACE \
    __FILE__, \
    __LINE__, \
    __PRETTY_FUNCTION__ \

namespace tracer {
typedef high_resolution_clock clock;

struct event;

struct session {
    clock::time_point start;
    std::vector<event> children;

    session() : start(clock::now()) {}

    string dump() const;
};

struct event {
    shared_ptr<session> s;
    int32_t type;
    clock::time_point start;
    std::vector<event> children;
    json data;

    event(shared_ptr<session> s_, int32_t type_=-2)
        : s(s_), type(type_), start(clock::now()), data(json::object())
    {
        data.set("message", "this is a log message");
    }

    //! milliseconds since start of session
    milliseconds time() const {
        return duration_cast<milliseconds>(start - s->start);
    }

    json to_json() const {
        json e(json::object());
        e.set("type", (json_int_t)type);
        e.set("typeName", "event");
        e.set("time", (json_int_t)time().count());
        e.set("duration", 10);
        e.set("data", data);
        e.set("color", "Crimson");
        //e.set("sequence", 0);
        return e;
    }
};

string session::dump() const {
    std::ostringstream ss;
    for (auto i=children.begin(); i!=children.end(); ++i) {
        ss << i->to_json() << "\n";
        break;
    }
    return ss.str();
}

} // end namespace tracer
} // end namespace ten

#endif // LIBTEN_NET_SPEEDTRACER_HH
