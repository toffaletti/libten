#include <chrono>
#include <deque>
#include <string>
#include <ostream>
#include "json.hh"
#include "logging.hh"

namespace ten {
using std::ostream;
using std::move;
using std::string;
using namespace std::chrono;

#define TRACE \
    __FILE__, \
    __LINE__, \
    __PRETTY_FUNCTION__ \

namespace tracer {

typedef high_resolution_clock clock;

struct record {
    uint64_t id;
    clock::time_point start;
    clock::time_point end;

    record(uint64_t id_) : id(id_), start(move(clock::now())) {}

    bool finished() const {
        return end.time_since_epoch().count() != 0;
    }

    void finish() {
        if (finished()) return;
        end = clock::now();
    }

    clock::duration duration() const {
        if (finished()) {
            return end - start;
        } else {
            return clock::now() - start;
        }
    }

    json range() {
        json r(json::object());
        r.oset("duration", (json_int_t)duration_cast<milliseconds>(duration()).count());
        json s(json::array());
        uint64_t ms = duration_cast<milliseconds>(start.time_since_epoch()).count();
        s.apush((json_int_t)ms / 1000);
        s.apush((json_int_t)ms % 1000);
        r.oset("start", s);

        json e(json::array());
        ms = duration_cast<milliseconds>(end.time_since_epoch()).count();
        e.apush((json_int_t)ms / 1000);
        e.apush((json_int_t)ms % 1000);
        r.oset("end", e);

        return r;
    }
};

struct event : record {
    string label;
    const char *file;
    uint32_t line;
    const char *method;
    std::deque<event> children;

    //! must use string literals for these
    event(uint64_t id,
            string label_,
            const char *file_=0,
            uint32_t line_=0,
            const char *method_=0
         )
        : record(id), label(move(label_)), file(file_), line(line_), method(method_)
    {
    }

    json to_json() {
        json e(json::object());
        e.oset("range", range());
        e.oset("id", (json_int_t)id);

        json op(json::object());
        op.oset("type", "METHOD");
        op.oset("label", label);
        
        json scl(json::object());
        string fn(file);
        scl.oset("className", fn.substr(fn.rfind("/")+1));
        scl.oset("methodName", method);
        scl.oset("lineNumber", (json_int_t)line);
        op.oset("sourceCodeLocation", scl);

        e.oset("operation", op);

        json ch(json::array());
        for (auto i=children.begin(); i!=children.end(); ++i) {
            ch.apush(i->to_json());
        }
        e.oset("children", ch);
        return e;
    }

    friend ostream &operator <<(ostream &o, const event &e) {
        using ::operator<<;
        o << e.label << "[" << e.method << "@" << e.file << ":" << e.line << "] " << 
            duration_cast<milliseconds>(e.duration()).count() << "ms\n" << e.children;
        return o;
    }
};

//! stack based timer for event record
struct timer {
    record &r;

    timer(record &r_) : r(r_) {}
    void stop() { r.finish(); }
    ~timer() { stop(); }
};

struct tracer : event {
    uint64_t event_id;
    tracer(uint64_t id, string label) : event(id, label), event_id(0) {
    }

    uint64_t next_id() {
        return ++event_id;
    }

    event &trace(
        string label_,
        const char *file_,
        uint32_t line_,
        const char *method_
        )
    {
        event::children.push_back(
                event(next_id(),
                    move(label_),
                    file_,
                    line_,
                    method_)
                );
        return event::children.back();
    }

    json to_json() {
        json trace(json::object());
        trace.oset("date", (json_int_t)duration_cast<seconds>(start.time_since_epoch()).count());
        trace.oset("application", "libten");
        trace.oset("range", range());
        trace.oset("id", (json_int_t)id);

        json frame(json::object());
        frame.oset("range", range());
        frame.oset("id", (json_int_t)id);

        json op(json::object());
        op.oset("type", "HTTP");
        op.oset("label", label);
        frame.oset("operation", op);

        trace.oset("frameStack", frame);

        json ch(json::array());
        for (auto i=children.begin(); i!=children.end(); ++i) {
            ch.apush(i->to_json());
        }
        trace.oset("children", ch);
        return trace;
    }

    friend ostream &operator <<(ostream &o, const tracer &t) {
        using ::operator<<;
        o << t.label << " " << duration_cast<milliseconds>(t.duration()).count() << "ms\n" << t.children;
        return o;
    }
};

} // end namespace tracer
} // end namespace ten
