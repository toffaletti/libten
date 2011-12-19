#include "logging.hh"
#include "task.hh"
#include "net.hh"
#include "msgpack/msgpack.hpp"

#include <sstream>
#include <boost/utility.hpp>
#include <unordered_map>
#include <vector>
#include <functional>

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

        for (;;) {
            pac.reserve_buffer(bsize);
            ssize_t nr = s.recv(pac.buffer(), bsize);
            if (nr <= 0) return;
            pac.buffer_consumed(nr);

            msgpack::unpacked result;
            while (pac.next(&result)) {
                DVLOG(3) << "rpc call: " << result.get();
            }
        }
    }
};

} // end namespace ten 

