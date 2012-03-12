#define BOOST_TEST_MODULE speedtracer test
#include <boost/test/unit_test.hpp>
#include "channel.hh"
#include "speedtracer.hh"

using namespace ten;
using namespace std;

const size_t default_stacksize=256*1024;

static void do_request(tracer::tracer &tr, channel<int> ch, string resource) {
    tracer::trace t(tr, resource, TRACE, false);
    tasksleep(rand() % 100);
    ch.send(0);
}

static void do_all(tracer::tracer &tr) {
    tracer::trace t(tr, "do all", TRACE);
    channel<int> ch;
    taskspawn(bind(do_request, ref(tr), ch, "a"));
    taskspawn(bind(do_request, ref(tr), ch, "b"));
    taskspawn(bind(do_request, ref(tr), ch, "c"));
    ch.recv();
    ch.recv();
    ch.recv();
}

// TODO: flesh this out
BOOST_AUTO_TEST_CASE(task_socket_dial) {
    tracer::tracer tr(0, "test tracer");
    procmain p;
    taskspawn(bind(do_all, ref(tr)));
    p.main();
    tr.finish();
    LOG(INFO) << tr;
    LOG(INFO) << tr.to_json().dump(2);
}

