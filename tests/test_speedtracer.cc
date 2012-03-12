#define BOOST_TEST_MODULE speedtracer test
#include <boost/test/unit_test.hpp>
#include "net.hh"
#include "speedtracer.hh"

using namespace ten;
using namespace std;

const size_t default_stacksize=256*1024;

static void dial_google(tracer::tracer &tr) {
    tracer::timer et(tr.trace("dial www.google.com", TRACE));
    socket_fd s(AF_INET, SOCK_STREAM);
    int status = netdial(s.fd, "www.google.com", 80);
    BOOST_CHECK_EQUAL(status, 0);
}

// TODO: flesh this out
BOOST_AUTO_TEST_CASE(task_socket_dial) {
    tracer::tracer tr(0, "test tracer");
    procmain p;
    taskspawn(bind(dial_google, ref(tr)));
    p.main();
    tr.finish();
    LOG(INFO) << tr;
    LOG(INFO) << tr.to_json().dump(1);
}

