#include "descriptors.hh"
#include <iostream>
#include "reactor.hh"
#include <boost/unordered_map.hpp>

struct packet {
    uint64_t xid;
    uint8_t  key[16];
    uint64_t value;
} __attribute__((packed));

class credit_client : boost::noncopyable {
public:
    typedef boost::function<void (uint64_t)> result_callback;
    typedef boost::unordered_map<uint64_t, result_callback> task_map;

    credit_client(reactor &r, const std::string &host_, uint16_t port = 9876)
        : saddr(host_.c_str(), port), s(AF_INET, SOCK_DGRAM), xid(0)
    {
        address baddr;
        s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
        s.bind(baddr);
        r.add(s.fd, EPOLLIN, boost::bind(&credit_client::event_cb, this, _1));
    }

    void get_ip_credits(const std::string &ip, const result_callback &cb) {
        inc_credits(ip, cb);
    }

    void send_packet(packet &pkt, const result_callback &cb) {
        pkt.xid = xid++;
        tasks[pkt.xid] = cb;
        ssize_t nw = s.sendto(&pkt, sizeof(pkt), saddr);
    }

    void inc_credits(const std::string &ip, const result_callback &cb, uint64_t value=0) {
        address addr(ip.c_str());
        packet pkt;

        memset(&pkt.key, 0, 16);
        memcpy(&pkt.key[12], &addr.addr.sa_in.sin_addr, 4);
        pkt.value = value;

        send_packet(pkt, cb);
    }

    size_t active_tasks() {
        return tasks.size();
    }

protected:
    address saddr;
    socket_fd s;
    uint64_t xid;

    task_map tasks;

    bool event_cb(int events) {
        address raddr;
        packet pkt;
        ssize_t nr = s.recvfrom(&pkt, sizeof(pkt), raddr);
        std::cout << "got " << nr << " bytes from " << raddr << " xid: " << pkt.xid << "\n";
        task_map::iterator it = tasks.find(pkt.xid);
        if (it != tasks.end()) {
            it->second(pkt.value);
            tasks.erase(it);
        }
        return true;
    }
};

void result1(uint64_t v, credit_client &c, reactor &r) {
    std::cout << "result1: " << v << "\n";
    if (c.active_tasks() == 1) r.quit();
}

void result2(uint64_t v, credit_client &c, reactor &r) {
    std::cout << "result2: " << v << "\n";
    if (c.active_tasks() == 1) r.quit();
}

void result3(uint64_t v, credit_client &c, reactor &r) {
    std::cout << "result3: " << v << "\n";
    if (c.active_tasks() == 1) r.quit();
}

int main(int argc, char *argv[]) {
    reactor r;
    credit_client cc(r, "0.0.0.0", 9800);
    cc.inc_credits("127.0.0.1", boost::bind(result1, _1, boost::ref(cc), boost::ref(r)), 1);
    cc.inc_credits("127.0.0.1", boost::bind(result2, _1, boost::ref(cc), boost::ref(r)), 1);
    cc.get_ip_credits("127.0.0.2", boost::bind(result3, _1, boost::ref(cc), boost::ref(r)));

    r.run();
}