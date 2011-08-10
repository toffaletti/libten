#include "runner.hh"
#include "task.hh"
#include "channel.hh"
#include <boost/bind.hpp>
#include <sstream>
#include <iostream>

struct client {
    task::socket s;
    std::string nick;

    client(int sock) : s(sock), nick("unnamed") { }
};
typedef boost::shared_ptr<client> shared_client;
typedef std::list<shared_client> client_list;

static client_list clients;
static channel<std::string> bchan(10);

void broadcast(const shared_client &from, const std::string &msg) {
    std::stringstream ss;
    ss << from->nick << ": " << msg;
    std::string chat = ss.str();
    bchan.send(chat);
}

void chat_task(int sock) {
    shared_client c(new client(sock));
    clients.push_back(c);
    char buf[4096];
    c->s.send("enter nickname: ", 16);
    ssize_t nr = c->s.recv(buf, sizeof(buf));
    if (nr > 0) {
        c->nick.assign(buf, nr);
        c->nick.resize(c->nick.find_first_of(" \t\r\n"));
        for (;;) {
            nr = c->s.recv(buf, sizeof(buf));
            if (nr <= 0) break;
            broadcast(c, std::string(buf, nr));
        }
    }
    clients.remove(c);
}

void broadcast_task() {
    for (;;) {
        std::string chat = bchan.recv();
        for (client_list::iterator i=clients.begin(); i != clients.end(); ++i) {
            (*i)->s.send(chat.c_str(), chat.size());
        }
    }
}

void listen_task() {
    task::socket s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        address client_addr;
        int sock;
        while ((sock = s.accept(client_addr, 0, 60*1000)) > 0) {
            task::spawn(boost::bind(chat_task, sock));
        }
        std::cout << "accept timeout reached\n";
    }
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(broadcast_task);
    task::spawn(listen_task);
    return runner::main();
}

