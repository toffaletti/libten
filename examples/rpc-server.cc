#include "ten/rpc/server.hh"
#include "ten/rpc/client.hh"
#include "ten/task/main.icc"

using namespace ten;
using namespace msgpack::rpc;

static void client_task() {
    rpc_client c{"localhost", 5500};
    c.notify("notify_me");
    c.notify("notify_world", std::string("hi"));
    LOG(INFO) << "40+2=" << c.call<int>("add2", 40, 2);
    LOG(INFO) << "4-2=" << c.call<int>("subtract2", 4, 2);
    try {
        LOG(INFO) << "fail: " << c.call<int>("fail");
    } catch (errorx &e) {
        LOG(ERROR) << "fail got: " << e.what();
    }
    kernel::shutdown();
}

static int fail() {
    throw std::runtime_error{"fail"};
}

static int add2(int a, int b) {
    LOG(INFO) << "called add2(" << a << "," << b << ")";
    return a + b;
}

static int subtract2(int a, int b) {
    return a - b;
}

static void notify_me() {
    LOG(INFO) << "NOTIFY ME";
}

static void notify_world(std::string s) {
    LOG(INFO) << "NOTIFY WORLD " << s;
}

int taskmain(int argc, char *argv[]) {
    auto rpc = std::make_shared<rpc_server>();
    rpc->add_command("add2", add2);
    rpc->add_command("add2", add2);
    rpc->add_command("subtract2", subtract2);
    rpc->add_command("fail", fail);
    rpc->add_notify("notify_me", notify_me);
    rpc->add_notify("notify_world", notify_world);
    task::spawn(client_task);
    rpc->serve("0.0.0.0", 5500);
    return EXIT_SUCCESS;
}

