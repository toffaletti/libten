#include "rpc/server.hh"
#include "rpc/client.hh"

using namespace ten;
using namespace msgpack::rpc;
const size_t default_stacksize=256*1024;

static void client_task() {
    rpc_client c("localhost", 5500);
    LOG(INFO) << "40+2=" << c.call<int>("add2", 40, 2);
    LOG(INFO) << "4-2=" << c.call<int>("subtract2", 4, 2);
    try {
        LOG(INFO) << "fail: " << c.call<int>("fail");
    } catch (errorx &e) {
        LOG(ERROR) << "fail got: " << e.what();
    }
    procshutdown();
}

static int fail() {
    throw std::runtime_error("fail");
}

static int add2(int a, int b) {
    LOG(INFO) << "called add2(" << a << "," << b << ")";
    return a + b;
}

static int subtract2(int a, int b) {
    return a - b;
}

static void startup() {
    rpc_server rpc;
    rpc.add_command("add2", add2);
    rpc.add_command("add2", add2);
    rpc.add_command("subtract2", subtract2);
    rpc.add_command("fail", fail);
    taskspawn(client_task);
    rpc.serve("0.0.0.0", 5500);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(startup);
    return p.main(argc, argv);
}

