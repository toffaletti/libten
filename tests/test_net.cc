#define BOOST_TEST_MODULE net test
#include <boost/test/unit_test.hpp>
#include "ten/net.hh"
#include "ten/http/server.hh"
#include "ten/http/client.hh"

using namespace ten;

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
    task::main([] {
        task::spawn(dial_google);
    });
}

static void http_callback(http_exchange &ex) {
    ex.resp = { 200, {}, "Hello World" };
}

static void http_post_callback(http_exchange &ex) {
    ex.resp = { 200, {}, "Post World" };
}

static void start_http_server(address &addr) {
    auto s = std::make_shared<http_server>();
    s->add_route("POST", "/foobar", http_post_callback);
    s->add_route("*", http_callback);
    s->serve(addr);
}

static void start_http_test() {
    address http_addr("0.0.0.0");
    task server_task = task::spawn([&] {
        start_http_server(http_addr);
    });
    this_task::yield(); // allow server to bind, set http_addr, and listen
    http_client c{http_addr.str()};
    http_response resp = c.get("/");
    BOOST_CHECK_EQUAL("Hello World", resp.body);
    resp = c.get("/foobar");
    BOOST_CHECK_EQUAL("Hello World", resp.body);
    resp = c.post("/foobar", "");
    BOOST_CHECK_EQUAL("Post World", resp.body);
    server_task.cancel();
}

BOOST_AUTO_TEST_CASE(http_server_client_get) {
    task::main([] {
        task::spawn(start_http_test);
    });
}

