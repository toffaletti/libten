#include "gtest/gtest.h"
#include "ten/net.hh"
#include "ten/http/server.hh"
#include "ten/http/client.hh"
#include "ten/channel.hh"
#include <chrono>

using namespace ten;
using namespace std::chrono;

static void dial_google() {
    socket_fd s{AF_INET, SOCK_STREAM};
    bool ok = false;
    try {
        netdial(s.fd, "www.google.com", 80, {});
        ok = true;
    } catch (errorx &e) {
        VLOG(1) << "dial exception: " << e.what() << "\n" << e.backtrace_str();
    }
    EXPECT_TRUE(ok);
}


TEST(Net, SocketDial) {
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

static void http_slow_callback(http_exchange &ex) {
    this_task::sleep_for(milliseconds{10});
    ex.resp = { 200, {}, "Slow World" };
}

static void start_http_server(address &addr) {
    auto s = std::make_shared<http_server>();
    s->add_route("POST", "/foobar", http_post_callback);
    s->add_route("/slow", http_slow_callback);
    s->add_route("*", http_callback);
    s->serve(addr);
}

static void start_http_test() {
    address http_addr("0.0.0.0");
    auto server_task = task::spawn([=, &http_addr]() mutable {
        VLOG(1) << "server_task start";
        start_http_server(http_addr);
    });
    this_task::yield(); // allow server to bind, set http_addr, and listen

    http_response resp;
    {
        http_client c{http_addr.str()};
        resp = c.get("/");
        EXPECT_EQ("Hello World", resp.body);
        resp = c.get("/foobar");
        EXPECT_EQ("Hello World", resp.body);
        resp = c.post("/foobar", "");
        EXPECT_EQ("Post World", resp.body);
    }
    {
        http_client c{http_addr.str()};
        try {
            deadline dl{milliseconds{5}};
            resp = c.get("/slow");
        } catch (deadline_reached) {
            resp = { 408 };
        }
        EXPECT_EQ(408, resp.status_code);
    }

    server_task.cancel();
    server_task.join();
    this_task::sleep_for(milliseconds{10}); // ensure this is at least as long as any request sleeps

    {
        http_client c{http_addr.str()};
        try {
            resp = c.get("/blargh");
        } catch (http_dial_error &e) {
            resp = { 503 };
        }
        EXPECT_EQ(resp.status_code, 503); // connect refused -> 503
    }
}

TEST(Net, HttpServerClientGet) {
    task::main([] {
        task::spawn(start_http_test);
    });
}

