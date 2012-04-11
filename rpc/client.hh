#ifndef RPC_CLIENT_HH
#define RPC_CLIENT_HH

#include "rpc/protocol.hh"

using namespace msgpack::rpc;

namespace ten {

#if 0
// more general interface that supports calls over udp/tcp/pipe
// TODO: allow server to call to the client
// session should be shared interface both sides use
// calls can create channels and wait on them
// allow multiple simultaneous calls
// out of order replies
// calls with no reply (notify)
class rpc_transport {
    virtual void send(msgpack::sbuffer &sbuf) = 0;
}

class rpc_session {
    // call()
};
#endif

struct rpc_failure : public errorx {
    rpc_failure(const std::string &msg) : errorx(msg) {}
};

class rpc_client {
public:
    rpc_client(const std::string &hostname_, uint16_t port_=0)
        : hostname(hostname_), port(port_), msgid(0)
    {
        parse_host_port(hostname, port);
    }

    rpc_client(const rpc_client &) = delete;
    rpc_client &operator(const rpc_client &) = delete;

    template <typename Result, typename ...Args>
        Result call(const std::string &method, Args ...args) {
            uint32_t mid = ++msgid;
            return rpcall<Result>(mid, method, args...);
        }

protected:
    netsock s;
    std::string hostname;
    uint16_t port;
    uint32_t msgid;
    msgpack::unpacker pac;

    void ensure_connection() {
        if (!s.valid()) {
            netsock tmp(AF_INET, SOCK_STREAM);
            std::swap(s, tmp);
            if (s.dial(hostname.c_str(), port) != 0) {
                throw rpc_failure("dial");
            }
        }
    }

    template <typename Result>
        Result rpcall(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf) {
            ensure_connection();
            ssize_t nw = s.send(sbuf.data(), sbuf.size());
            if (nw != (ssize_t)sbuf.size()) {
                s.close();
                throw rpc_failure("send");
            }

            size_t bsize = 4096;

            for (;;) {
                pac.reserve_buffer(bsize);
                ssize_t nr = s.recv(pac.buffer(), bsize);
                if (nr <= 0) {
                    s.close();
                    throw rpc_failure("recv");
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

} // end namespace ten
#endif
