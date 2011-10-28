#include "logging.hh"
#include "task.hh"
#include "net.hh"

#include <sstream>
#include <boost/utility.hpp>
#include <unordered_map>
#include <vector>
#include <functional>

// TODO: this shares a lot of code with http_server
// they should have a common base class netsock_server

namespace fw {

template <typename T> bool is_empty(const T &c) { return c.empty(); }

class cmd_server : public netsock_server {
public:
    typedef std::unordered_map<std::string, std::string> env_type;
    typedef std::vector<std::string> args_type;
    typedef std::function<void (netsock &s, const args_type &, env_type &)> callback_type;

    cmd_server(const std::string &welcome_="", size_t stacksize_=default_stacksize)
        : netsock_server("cmds", stacksize_), welcome(welcome_) 
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
        buffer buf(16*1024);

        s.s.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
        std::stringstream ss;
        std::string line;

        env_type env;
        env["PROMPT"] = "$ ";

        ssize_t nw;
        buffer::slice rb = buf(0);
        if (!welcome.empty()) {
            nw = s.send(welcome.data(), welcome.size());
        }
        for (;;) {
            std::string prompt = env["PROMPT"];
            nw = s.send(prompt.data(), prompt.size());
            (void)nw;
            for (;;) {
                ssize_t nr = s.recv(rb.data(), rb.size());
                if (nr < 0) return;
                if (nr == 0) return;
                ss.write(rb.data(), nr);
                while (std::getline(ss, line)) {
                    VLOG(3) << "CMD LINE: " << line;
                    args_type args;
                    boost::split(args, line, boost::is_any_of(" \r"));
                    // remove empty args
                    auto end = std::remove_if(args.begin(), args.end(), is_empty<std::string>);
                    args.erase(end, args.end());

                    if (!args.empty()) {
                        if (args.front() == "\4") return; // ^D
                        auto it = _cmds.find(args.front());
                        if (it != _cmds.end()) {
                            try {
                                it->second(s, args, env);
                            } catch (std::exception &e) {
                                std::string reply = e.what();
                                reply += "\n";
                                nw = s.send(reply.data(), reply.size());
                                (void)nw;
                            }
                        } else {
                            std::string reply = args.front() + ": command not found\n";
                            nw = s.send(reply.data(), reply.size());
                            (void)nw;
                        }
                    }

                }
                ss.clear();
                break;
            }
        }
    }
};

} // end namespace fw

