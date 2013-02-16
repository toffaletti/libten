#define BOOST_TEST_MODULE net test
#include <boost/test/unit_test.hpp>
#include "ten/net.hh"
#include "ten/http/server.hh"
#include "ten/http/client.hh"

using namespace ten;
using namespace ten::compat;

static void dial_google() {
    socket_fd s{AF_INET, SOCK_STREAM};
    bool ok = false;
    try {
        netdial(s.fd, "www.google.com", 80, {});
        ok = true;
    }
    catch (errorx &e) {
        BOOST_MESSAGE("dial exception: " << e.what() << "\n" << e.backtrace_str());
    }
    BOOST_CHECK(ok);
}


BOOST_AUTO_TEST_CASE(task_socket_dial) {
    procmain p;
    taskspawn(dial_google);
    p.main();
}

static void http_callback(http_exchange &ex) {
    ex.resp = { 200 };
    ex.resp.set_body("Hello World");
    ex.send_response();
}

static void start_http_server(address &addr) {
    auto s = std::make_shared<http_server>();
    s->add_route("*", http_callback);
    s->serve(addr);
}

static void start_http_test() {
    address http_addr("0.0.0.0");
    uint64_t server_tid = taskspawn(
            std::bind(start_http_server, std::ref(http_addr))
            ); 
    taskyield(); // allow server to bind and listen
    http_client c{http_addr.str()};
    http_response resp = c.get("/");
    BOOST_CHECK_EQUAL("Hello World", resp.body);
    resp = c.get("/foobar");
    BOOST_CHECK_EQUAL("Hello World", resp.body);
    taskcancel(server_tid);
}

BOOST_AUTO_TEST_CASE(http_server_client_get) {
    procmain p;
    taskspawn(start_http_test);
    p.main();
}

