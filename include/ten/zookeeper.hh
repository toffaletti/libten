#ifndef TEN_ZK_HH
#define TEN_ZK_HH
#include <sys/epoll.h>
#include <zookeeper.h>
#include <memory>
#include <unordered_map>
#include <initializer_list>

#include "ten/error.hh"
#include "ten/task.hh"
#include "ten/logging.hh"
#include "ten/channel.hh"

namespace ten {

class zk_error : public std::exception {
private:
    char _buf[128];
public:
    int code;
    explicit zk_error(int code_) : code(code_) {
        snprintf(_buf, sizeof(_buf), "zk_error[%d]: %s", code, zerror(code));
    }
    const char *what() const noexcept override {
        return _buf;
    }
};

class zookeeper_client {
public:
    typedef channel<std::pair<int, std::string>> string_channel;
    typedef channel<std::pair<int, std::vector<std::string>>> strings_channel;
    typedef channel<std::pair<int, std::string>> data_channel;

    zookeeper_client(const std::string &server_list, int timeout_ms=10000) : zh(0) {
        zh = zookeeper_init(server_list.c_str(),
                zookeeper_client::watcher, timeout_ms, 0, this, 0);
        if (!zh) throw zk_error(ZSYSTEMERROR);
        taskspawn(std::bind(&zookeeper_client::task, this));
    }

    int create(const std::string &path, const std::string &value,
            const struct ACL_vector &acl, int flags,
            std::initializer_list<ZOO_ERRORS> ignore_errors,
            std::string &real_path)
    {
        return create(path, value, acl, flags, ignore_errors, &real_path);
    }

    int create(const std::string &path, const std::string &value,
            const struct ACL_vector &acl, int flags,
            std::initializer_list<ZOO_ERRORS> ignore_errors,
            std::string *real_path=0)
    {
        string_channel ch;
        VLOG(3) << "creating " << path << " ch: " << &ch;
        int status = zoo_acreate(zh,
                path.c_str(), value.c_str(), value.size(),
                &acl, flags, zookeeper_client::string_completion, &ch);
        auto p = ch.recv();
        if (real_path) {
            *real_path = p.second;
        }
        throw_errors(p.first, ignore_errors);
        return status;
    }

    std::string get(const std::string &path) {
        data_channel ch;
        int status = zoo_aget(zh,
                path.c_str(), 0, zookeeper_client::data_completion, &ch);
        if (status != ZOK) throw zk_error(status);
        auto p = ch.recv();
        throw_errors(p.first, {});
        return p.second;
    }

    void get_children(const std::string &path, std::vector<std::string> &children)
    {
        strings_channel ch;
        int status = zoo_aget_children(zh,
                path.c_str(), 0, zookeeper_client::strings_completion, &ch);
        if (status != ZOK) throw zk_error(status);
        auto p(ch.recv());
        throw_errors(p.first, {});
        children = p.second;
    }

    channel<int> wget_children(const std::string &path, std::vector<std::string> &children)
    {
        channel<int> watch_ch(1);
        CHECK(watch_channels.count(path) == 0);
        watch_channels.insert(std::make_pair(path, watch_ch));
        strings_channel ch;
        int status = zoo_awget_children(zh,
                path.c_str(),
                zookeeper_client::watcher, this,
                zookeeper_client::strings_completion, &ch);
        if (status != ZOK) throw zk_error(status);

        auto p(ch.recv());
        throw_errors(p.first, {});
        children = p.second;

        return watch_ch;
    }

    int state() {
        return zoo_state(zh);
    }

    ~zookeeper_client() {
        CHECK(zookeeper_close(zh) == ZOK);
    }
private:
    zhandle_t *zh;
    // TODO: this might need to be a multimap
    // if two tasks create a watch on the same node
    std::unordered_map<std::string, channel<int>> watch_channels;

    inline void throw_errors(int code, std::initializer_list<ZOO_ERRORS> ignore_errors) {
        if (code != ZOK &&
            std::find(ignore_errors.begin(), ignore_errors.end(), code) == ignore_errors.end())
        {
            throw zk_error(code);
        }
    }

    void task() {
        taskname("zookeeper::task");
        // TODO: should catch exceptions and cleanly exit the task
        // let the caller know a new task is needed.
        // XXX: see is_unrecoverable(zh)
        for (;;) {
            poll();
        }
        // XXX: example of failure when dev009 went into swap
        // 2012-05-11 14:12:36,130:3421:ZOO_WARN@zookeeper_interest@1461:
        // Exceeded deadline by 7948ms
        // 2012-05-11 14:13:04,721:3421:ZOO_ERROR@handle_socket_error_msg@1528:
        // Socket [10.5.7.28:2181] zk retcode=-7, errno=110(Connection timed
        // out): connection timed out (exceeded timeout by 4614ms)
        // I0511 14:13:30.302278  3421 zookeeper.hh:178] watcher type: -1 state:
        // 1 path: 
        // E0511 14:14:43.228332  3421 task_impl.hh:128] unhandled error in
        // [0x2065f50 3 task[3] |poll fd 7 r: 1 w: 0 3333l ms| sys: 0 exiting: 0
        // canceled: 0]: zk_error[-7]: operation timeout
        //
    }

    static void data_completion(int rc,
            const char *value, int value_len,
            const struct Stat *stat, const void *data)
    {
        data_channel *ch = (data_channel *)data;
        std::string s;
        if (rc == 0) {
            s.assign(value, value_len);
        }
        ch->send(std::make_pair(rc, s));
        ch->close();
    }

    static void strings_completion(int rc,
            const struct String_vector *strings, const void *data)
    {
        strings_channel *ch = (strings_channel *)data;
        std::vector<std::string> vec;
        if (rc == 0) {
            for (int i=0; i<strings->count; ++i) {
                vec.push_back(strings->data[i]);
            }
        }
        ch->send(std::make_pair(rc, vec));
        ch->close();
    }

    static void string_completion(int rc, const char *value, const void *data) {
        string_channel *ch = (string_channel *)data;
        std::string s;
        if (rc == 0) {
            s.assign(value);
        }
        VLOG(3) << "string complete[" << ch << "," << rc << "]: " << s;
        ch->send(std::make_pair(rc, s));
        ch->close();
    }

    static void watcher(zhandle_t *zzh,
            int type, int state,
            const char *path,
            void *ctx)
    {
        zookeeper_client *self = (zookeeper_client *)ctx;
        LOG(INFO) << "watcher type: " << type << " state: " << state << " path: " << path;
        // TODO: maybe we need to wait for ZOO_CONNECTED_STATE?
        if (path && path[0] != 0) {
            auto i = self->watch_channels.find(path);
            CHECK(i != self->watch_channels.end());
            auto wch = i->second;
            // remove, watches are single fire
            self->watch_channels.erase(i);
            // TODO: might need to send type and state?
            wch.send(std::move(type));
        }
    }

    void poll() {
        pollfd pfd = {};
        int events = 0;
        struct timeval tv = {};
        int status = zookeeper_interest(zh, &pfd.fd, &events, &tv);
        VLOG(3) << "zk interest: " << status
            << " fd: " << pfd.fd
            << " events: " << events;
        if (status != ZOK) throw zk_error(status);
        if (events & ZOOKEEPER_READ) {
            pfd.events |= EPOLLIN;
        }
        if (events & ZOOKEEPER_WRITE) {
            pfd.events |= EPOLLOUT;
        }
        using std::chrono::milliseconds;
        if (taskpoll(&pfd, 1,
                    milliseconds{(tv.tv_sec * 1000) + (tv.tv_usec / 1000)}))
        {
            events = 0;
            if (pfd.revents & EPOLLIN) {
                events |= ZOOKEEPER_READ;
            }
            if (pfd.revents & EPOLLOUT) {
                events |= ZOOKEEPER_WRITE;
            }
            status = zookeeper_process(zh, events);
            VLOG(3) << "zk process: " << status;
            if (status != ZOK) throw zk_error(status);
        }
    }
};

} // namespace
#endif
