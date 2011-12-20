#include "logging.hh"
#include "task.hh"
#include "net.hh"
#include "msgpack/msgpack.hpp"
#include "rpc/protocol.hh"

#include <sstream>
#include <boost/utility.hpp>
#include <unordered_map>
#include <vector>
#include <functional>

using namespace msgpack::rpc;

namespace ten {

class rpc_server : public netsock_server {
public:
    typedef std::function<void (netsock &s)> callback_type;

    rpc_server(size_t stacksize_=default_stacksize)
        : netsock_server("msgpack-rpc", stacksize_)
    {
    }

    void add_command(const std::string &cmd,
            const callback_type &callback)
    {
        _cmds[cmd] = callback;
    }

    std::string welcome;
private:
    std::unordered_map<std::string, callback_type> _cmds;

    void on_shutdown() {
        // release any memory held by bound callbacks
        _cmds.clear();
    }

    void on_connection(netsock &s) {
        size_t bsize = 4096;
        msgpack::unpacker pac;
        msgpack::sbuffer sbuf;

        for (;;) {
            pac.reserve_buffer(bsize);
            ssize_t nr = s.recv(pac.buffer(), bsize);
            if (nr <= 0) return;
            pac.buffer_consumed(nr);

            msgpack::unpacked result;
            while (pac.next(&result)) {
                msgpack::object o = result.get();
                DVLOG(3) << "rpc call: " << o;
                msg_request<std::string, msgpack::object> req;
                o.convert(&req);
                msg_response<int, int> resp(12345, 0, req.msgid);
                msgpack::pack(sbuf, resp);

                ssize_t nw = s.send(sbuf.data(), sbuf.size());
                if (nw != (ssize_t)sbuf.size()) {
                    throw errorx("rpc call failed to send reply");
                }
            }
        }
    }
};

} // end namespace ten 

