#ifndef LIBTEN_RPC_SERVER_HH
#define LIBTEN_RPC_SERVER_HH

#include "ten/logging.hh"
#include "ten/task/task.hh"
#include "ten/net.hh"
#include "ten/rpc/protocol.hh"
#include "ten/rpc/thunk.hh"

#include <sstream>
#include <unordered_map>
#include <vector>
#include <functional>

using namespace msgpack::rpc;

namespace ten {

class rpc_server : public netsock_server {
public:
    typedef std::function<msgpack::object (msgpack::object & , msgpack::zone *)> callback_type;
    typedef std::function<void (msgpack::object & , msgpack::zone *)> notify_func;

    rpc_server(size_t stacksize_=0)
        : netsock_server("msgpack-rpc", stacksize_)
    {
    }

    template <typename Func>
        void add_command(const std::string &cmd, Func &&f)
        {
            _cmds[cmd] = thunk(f);
        }

    template <typename Func>
        void add_notify(const std::string &cmd, Func &&f)
        {
            _notifs[cmd] = thunk(f);
        }

private:
    std::unordered_map<std::string, callback_type> _cmds;
    std::unordered_map<std::string, notify_func> _notifs;

    void on_connection(netsock &s) {
        size_t bsize = 4096;
        msgpack::unpacker pac;

        for (;;) {
            msgpack::zone z;
            pac.reserve_buffer(bsize);
            ssize_t nr = s.recv(pac.buffer(), bsize);
            if (nr <= 0) return;
            pac.buffer_consumed(nr);

            msgpack::unpacked result;
            while (pac.next(&result)) {
                msgpack::sbuffer sbuf;
                msgpack::object o = result.get();
                DVLOG(3) << "rpc call: " << o;
                msg_rpc msg;
                o.convert(&msg);
                if (msg.is_request()) {
                    msg_request<std::string, msgpack::object> req;
                    o.convert(&req);
                    auto it = _cmds.find(req.method);
                    if (it != _cmds.end()) {
                        try {
                            msgpack::object result = it->second(req.param, &z);
                            msg_response<msgpack::object, msgpack::object> resp(result, msgpack::object(), req.msgid);
                            msgpack::pack(sbuf, resp);
                        } catch (std::exception &e) {
                            msg_response<msgpack::object, std::string> resp(msgpack::object(), e.what(), req.msgid);
                            msgpack::pack(sbuf, resp);
                        }
                    } else {
                        std::stringstream ss;
                        ss << "method '" <<  req.method << "' not found";
                        msg_response<msgpack::object, std::string> resp(msgpack::object(), ss.str(), req.msgid);
                        msgpack::pack(sbuf, resp);
                    }

                    ssize_t nw = s.send(sbuf.data(), sbuf.size());
                    DVLOG(3) << "rpc server sent: " << nw << " bytes";
                    if (nw != (ssize_t)sbuf.size()) {
                        throw errorx("rpc call failed to send reply");
                    }
                } else if (msg.is_notify()) {
                    msg_notify<std::string, msgpack::object> notif;
                    o.convert(&notif);
                    auto it = _notifs.find(notif.method);
                    if (it != _notifs.end()) {
                        try {
                            it->second(notif.param, &z);
                        } catch (std::exception &e) {
                            LOG(ERROR) << "notify '" << notif.method << "' error: " << e.what();
                        }
                    } else {
                        LOG(WARNING) << "notify '" << notif.method << "' not found";
                    }
                }
            }
        }
    }
};

} // end namespace ten 

#endif // LIBTEN_RPC_SERVER_HH
