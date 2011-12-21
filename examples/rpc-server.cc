#include "rpc/protocol.hh"
#include "rpc/thunk.hh"
#include "rpc_server.hh"

using namespace ten;
using namespace msgpack::rpc;
const size_t default_stacksize=256*1024;

class rpc_client : public boost::noncopyable {
public:
    rpc_client(const std::string &hostname, uint16_t port)
        : s(AF_INET, SOCK_STREAM), msgid(0)
    {
        if (s.dial(hostname.c_str(), port) != 0) {
            throw errorx("rpc client connection failed");
        }
    }

    template <typename Result, typename ...Args>
        Result call(const std::string &method, Args ...args) {
            uint32_t mid = ++msgid;
            return rpcall<Result>(mid, method, args...);
        }

protected:
    netsock s;
    uint32_t msgid;
    msgpack::unpacker pac;

    template <typename Result>
        Result rpcall(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf) {
            ssize_t nw = s.send(sbuf.data(), sbuf.size());
            if (nw != (ssize_t)sbuf.size()) {
                throw errorx("rpc call failed to send");
            }

            size_t bsize = 4096;

            for (;;) {
                pac.reserve_buffer(bsize);
                ssize_t nr = s.recv(pac.buffer(), bsize);
                if (nr <= 0) {
                    throw errorx("rpc client lost connection");
                }
                DVLOG(3) << "client recv: " << nr;
                pac.buffer_consumed(nr);

                msgpack::unpacked result;
                if (pac.next(&result)) {
                    msgpack::object o = result.get();
                    DVLOG(3) << "client got: " << o;
                    msg_response<msgpack::object, msgpack::object> resp;
                    o.convert(&resp);
                    if (resp.error.is_nil()) {
                        return resp.result.as<Result>();
                    } else {
                        LOG(ERROR) << "rpc error returned: " << resp.error;
                        throw errorx(resp.error.as<std::string>());
                    }
                }
            }
            // shouldn't get here.
            throw errorx("rpc client unknown error");
        }

    template <typename Result, typename Arg, typename ...Args>
        Result rpcall(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf, Arg arg, Args ...args) {
            pk.pack(arg);
            return rpcall<Result>(pk, sbuf, args...);
        }

    template <typename Result, typename ...Args>
        Result rpcall(uint32_t msgid, const std::string &method, Args ...args) {
            msgpack::sbuffer sbuf;
            msgpack::packer<msgpack::sbuffer> pk(&sbuf);
            pk.pack_array(4);
            pk.pack_uint8(0); // request message type
            pk.pack_uint32(msgid);
            pk.pack(method);
            pk.pack_array(sizeof...(args));
            return rpcall<Result>(pk, sbuf, args...);
        }


};

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

