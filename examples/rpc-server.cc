#include "rpc_server.hh"

using namespace ten;
const size_t default_stacksize=256*1024;

ssize_t rpcall(netsock &s, msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf) {
    ssize_t nw = s.send(sbuf.data(), sbuf.size());
    LOG(INFO) << "sent: " << nw;
    return nw;
}

template <typename Arg, typename ...Args>
ssize_t rpcall(netsock &s, msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf, Arg arg, Args ...args) {
    pk.pack(arg);
    return rpcall(s, pk, sbuf, args...);
}

template <typename ...Args>
ssize_t rpcall(netsock &s, uint32_t msgid, const std::string &method, Args ...args) {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(&sbuf);
    pk.pack_array(4);
    pk.pack_int(0);
    pk.pack_uint32(msgid);
    pk.pack(method);
    pk.pack_array(sizeof...(args));
    return rpcall(s, pk, sbuf, args...);
}

static void client_task() {
    netsock s(AF_INET, SOCK_STREAM);
    tasksleep(10);
    s.dial("localhost", 5500);
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(&sbuf);
    pk.pack_array(4);
    pk.pack_int(0);
    pk.pack_uint32(1);
    pk.pack(std::string("method"));
    pk.pack_array(0);
    ssize_t nw = s.send(sbuf.data(), sbuf.size());
    LOG(INFO) << "sent: " << nw;

    rpcall(s, 2, "method2", std::string("params"), 1, 5, 1.34);
}

static void startup() {
    rpc_server rpc;
    taskspawn(client_task);
    rpc.serve("0.0.0.0", 5500);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(startup);
    return p.main(argc, argv);
}

