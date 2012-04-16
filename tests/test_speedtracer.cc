#define BOOST_TEST_MODULE speedtracer test
#include <boost/test/unit_test.hpp>
#include "ten/channel.hh"
#include "net/speedtracer.hh"

using namespace ten;
using namespace std;

const size_t default_stacksize=256*1024;

#if 0
static void do_request(tracer::trace &tr, channel<int> ch, string resource) {
    tracer::trace t(tr, resource, TRACE);
    tasksleep(rand() % 100);
    ch.send(0);
}

static void do_all(tracer::trace &tr) {
    tracer::trace t(tr, "do all", TRACE);
    channel<int> ch;
    taskspawn(bind(do_request, ref(t), ch, "a"));
    taskspawn(bind(do_request, ref(t), ch, "b"));
    taskspawn(bind(do_request, ref(t), ch, "c"));
    ch.recv();
    ch.recv();
    ch.recv();
}
#endif

// TODO: flesh this out
BOOST_AUTO_TEST_CASE(task_socket_dial) {
    //tracer::tracer tr(0, "test tracer");
    //tracer::trace t(tr);
    //procmain p;
    //taskspawn(bind(do_all, ref(t)));
    //p.main();
    //tr.finish();
    //LOG(INFO) << tr;
    //LOG(INFO) << tr.to_json().dump(2);
}

