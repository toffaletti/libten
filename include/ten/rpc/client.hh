#ifndef LIBTEN_RPC_CLIENT_HH
#define LIBTEN_RPC_CLIENT_HH

#include "ten/net.hh"
#include "ten/shared_pool.hh"
#include "ten/rpc/protocol.hh"
#include <boost/lexical_cast.hpp>

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

//! exception thrown when rpc call fails on transport
struct rpc_failure : public errorx {
    rpc_failure(const std::string &msg) : errorx(msg) {}
};

//! client to rpc_server. based on msgpack-rpc
class rpc_client {
public:
    rpc_client(const std::string &hostname_, uint16_t port_=0)
        : hostname(hostname_), port(port_), msgid(0)
    {
        parse_host_port(hostname, port);
    }

    rpc_client(const rpc_client &) = delete;
    rpc_client &operator =(const rpc_client &) = delete;

    //! make a remote procedure call and wait for the result
    template <typename Result, typename ...Args>
        Result call(const std::string &method, Args ...args) {
            uint32_t mid = ++msgid;
            return rpcall<Result>(mid, method, args...);
        }

    //! make a remote procedure call with no result
    template <typename ...Args>
        void notify(const std::string &method, Args ...args) {
            rnotify(method, args...);
        }

protected:
    netsock s;
    std::string hostname;
    uint16_t port;
    uint32_t msgid;
    msgpack::unpacker pac;

    void ensure_connection() {
        if (!s.valid()) {
            netsock cs{AF_INET, SOCK_STREAM};
            if (!cs.valid()) {
                throw rpc_failure("socket");
            }
            try {
                cs.dial(hostname.c_str(), port);
            }
            catch (const std::exception &e) {
                throw rpc_failure(e.what());
            }
            cs.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
            s = std::move(cs);
        }
    }

    void rnotify(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf) {
        ensure_connection();
        ssize_t nw = s.send(sbuf.data(), sbuf.size());
        if (nw != (ssize_t)sbuf.size()) {
            s.close();
            throw rpc_failure("send");
        }
    }

    template <typename Arg, typename ...Args>
        void rnotify(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf, Arg arg, Args ...args) {
            pk.pack(arg);
            rnotify(pk, sbuf, args...);
        }

    template <typename ...Args>
        void rnotify(const std::string &method, Args ...args) {
            msgpack::sbuffer sbuf;
            msgpack::packer<msgpack::sbuffer> pk(&sbuf);
            pk.pack_array(3);
            pk.pack_uint8(2); // request message type
            pk.pack(method);
            pk.pack_array(sizeof...(args));
            rnotify(pk, sbuf, args...);
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

class rpc_pool : public shared_pool<rpc_client> {
public:

    rpc_pool(const std::string &host_, uint16_t port_, optional<size_t> max_conn = nullopt)
        : shared_pool<rpc_client>(
                "rpc:" + host_ + ":" + boost::lexical_cast<std::string>(port_),
            std::bind(&rpc_pool::new_resource, this),
            max_conn
        ),
        host(host_), port(port_) {}

    rpc_pool(const std::string &host_, optional<size_t> max_conn = nullopt)
        : shared_pool<rpc_client>("rpc:" + host_,
            std::bind(&rpc_pool::new_resource, this),
            max_conn
        ),
        host(host_), port(0)
    {
        parse_host_port(host, port);
    }

protected:
    std::string host;
    uint16_t port;

    std::shared_ptr<rpc_client> new_resource() {
        VLOG(3) << "new rpc_client resource " << host << ":" << port;
        return std::make_shared<rpc_client>(host, port);
    }
};

} // end namespace ten

#endif // LIBTEN_RPC_CLIENT_HH
