#include "rpc/thunk.hh"
#include "rpc/server.hh"
#include "rpc/client.hh"

using namespace ten;
using namespace msgpack::rpc;
const size_t default_stacksize=256*1024;

static void client_task() {
    rpc_client c("localhost", 5500);
    LOG(INFO) << "40+2=" << c.call<int>("add2", 40, 2);
    LOG(INFO) << "44-2=" << c.call<int>("subtract2", 44, 2);
    procshutdown();
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
    rpc.add_command("add2", thunk<int, int, int>(add2));
    rpc.add_command("subtract2", thunk<int, int, int>(subtract2));
    taskspawn(client_task);
    rpc.serve("0.0.0.0", 5500);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(startup);
    return p.main(argc, argv);
}

