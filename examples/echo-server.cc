#include "ten/net.hh"
#include "ten/buffer.hh"
#include <iostream>

using namespace ten;

class echo_server : public netsock_server {
public:
    echo_server() : netsock_server{"echo"} {}
private:
    void on_connection(netsock &s) override {
        buffer buf{4*1024};
        for (;;) {
            buf.reserve(4*1024);
            ssize_t nr = s.recv(buf.back(), buf.available());
            if (nr <= 0) break;
            buf.commit(nr);
            ssize_t nw = s.send(buf.front(), buf.size());
            if (nw != nr) break;
            buf.remove(nw);
        }
    }
};

int main() {
    task::main([] {
        address addr{"127.0.0.1", 0};
        std::shared_ptr<echo_server> server = std::make_shared<echo_server>();
        server->serve(addr);
    });
}

