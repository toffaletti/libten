#define BOOST_TEST_MODULE net test
#include <boost/test/unit_test.hpp>
#include "ten/net.hh"
#include "ten/http/server.hh"
#include "ten/http/client.hh"

using namespace ten;

const size_t default_stacksize=256*1024;

static void dial_google() {
    socket_fd s(AF_INET, SOCK_STREAM);
    int status = netdial(s.fd, "www.google.com", 80);
    BOOST_CHECK_EQUAL(status, 0);
}


BOOST_AUTO_TEST_CASE(task_socket_dial) {
    procmain p;
    taskspawn(dial_google);
    p.main();
}

static void http_callback(http_server::request &env) {
    env.resp = http_response(200);
    env.resp.set_body("Hello World");
    env.send_response();
}

static void start_http_server(address &addr) {
    http_server s;
    s.add_route("*", http_callback);
    s.serve(addr);
}

static void start_http_test() {
    address http_addr;
    uint64_t server_tid = taskspawn(
            std::bind(start_http_server, std::ref(http_addr))
            ); 
    taskyield(); // allow server to bind and listen
    http_client c(http_addr.str());
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

