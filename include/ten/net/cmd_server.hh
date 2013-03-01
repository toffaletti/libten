#ifndef LIBTEN_NET_CMD_SERVER_HH
#define LIBTEN_NET_CMD_SERVER_HH

#include "ten/logging.hh"
#include "ten/task.hh"
#include "ten/net.hh"

#include <sstream>
#include <unordered_map>
#include <vector>
#include <functional>

#include <boost/algorithm/string.hpp>
#include <netinet/tcp.h>

namespace ten {

template <typename T> bool is_empty(const T &c) { return c.empty(); }

//! telnet-like command server
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

    void on_connection(netsock &s) override {
        buffer buf(4*1024);

        s.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
        std::stringstream ss;
        std::string line;

        env_type env;
        env["PROMPT"] = "$ ";

        ssize_t nw;
        if (!welcome.empty()) {
            nw = s.send(welcome.data(), welcome.size());
        }
        while (s.valid()) {
            std::string prompt = env["PROMPT"];
            nw = s.send(prompt.data(), prompt.size());
            (void)nw;
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = s.recv(buf.back(), buf.available());
                if (nr < 0) return;
                buf.commit(nr);
                if (nr == 0) return;
                ss.write(buf.front(), buf.size());
                buf.remove(buf.size());
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

} // end namespace ten 

#endif // LIBTEN_NET_CMD_SERVER_HH
