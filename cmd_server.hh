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

class cmd_server : boost::noncopyable {
public:
    typedef std::unordered_map<std::string, std::string> env_type;
    typedef std::vector<std::string> args_type;
    typedef std::function<void (netsock &s, const args_type &, env_type &)> callback_type;

    cmd_server(size_t stacksize_=default_stacksize)
        : sock(AF_INET, SOCK_STREAM), stacksize(stacksize_)
    {
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    }

    void add_command(const std::string &cmd,
            const callback_type &callback)
    {
        _cmds[cmd] = callback;
    }

    //! listen and accept connections
    void serve(const std::string &ipaddr, uint16_t port) {
        address baddr(ipaddr.c_str(), port);
        sock.bind(baddr);
        sock.getsockname(baddr);
        LOG(INFO) << "listening for cmds on: " << baddr;
        sock.listen();
        try {
            for (;;) {
                address client_addr;
                int fd;
                while ((fd = sock.accept(client_addr, 0)) > 0) {
                    taskspawn(std::bind(&cmd_server::client_task, this, fd), stacksize);
                }
            }
        } catch (...) {
            // release any memory held by bound callbacks
            _cmds.clear();
            throw;
        }
    }

private:
    netsock sock;
    std::unordered_map<std::string, callback_type> _cmds;
    size_t stacksize;

    void client_task(int fd) {
        netsock s(fd);
        buffer buf(16*1024);

        s.s.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
        std::stringstream ss;
        std::string line;

        env_type env;
        env["PROMPT"] = "$ ";

        ssize_t nw;
        try {
            buffer::slice rb = buf(0);
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
                                it->second(s, args, env);
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
        } catch (std::exception &e) {
            LOG(ERROR) << "unhandled client task error: " << e.what();
        }
    }
};

} // end namespace fw

