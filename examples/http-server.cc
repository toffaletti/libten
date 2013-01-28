#include "ten/app.hh"
#include "ten/task.hh"
#include "ten/buffer.hh"
#include "ten/logging.hh"
#include "ten/net.hh"
#include "ten/channel.hh"
#include "ten/http/server.hh"

#include <boost/lexical_cast.hpp>

using namespace ten;
const size_t default_stacksize=256*1024;

struct my_config : app_config {
    std::string http_address;
    unsigned int http_threads;
};

static my_config conf;

struct state : boost::noncopyable {
    application &app;
    std::shared_ptr<http_server> http;

    state(application &app_) : app(app_) {
        http = std::make_shared<http_server>();
    }
};

static void log_request(http_exchange &ex) {
    using namespace std::chrono;
    const auto stop = steady_clock::now();
    auto agent_ip = ex.agent_ip();
    auto cl_hdr = ex.resp.get("Content-Length");
    VLOG(1) << (agent_ip ? *agent_ip : "noaddr") << ": " <<
        ex.req.method << " " <<
        ex.req.uri << " " <<
        ex.resp.status_code << " " <<
        (cl_hdr ? *cl_hdr : "nan") << " " <<
        duration_cast<milliseconds>(stop - ex.start).count();
}

static void http_quit(std::weak_ptr<state> wst, http_exchange &ex) {
    if (auto st = wst.lock()) {
        LOG(INFO) << "quit requested over http";
        st->app.quit();
    }
    ex.resp = { 200, { "Connection", "close" } };
    ex.resp.set_body("");
    ex.send_response();
}

static void http_root(std::weak_ptr<state> wst, http_exchange &ex) {
    ex.resp = { 200 };
    ex.resp.set_body("Hello World!\n", "text/plain");
    ex.send_response();
}

static void start_http_server(std::shared_ptr<state> &st) {
    using namespace std::placeholders;
    std::string addr = conf.http_address;
    uint16_t port = 0;
    parse_host_port(addr, port);
    if (port == 0) return;
    std::weak_ptr<state> wst{st};
    st->http->set_log_callback(log_request);
    st->http->add_route("/quit", std::bind(http_quit, wst, _1));
    st->http->add_route("/*", std::bind(http_root, wst, _1));
    st->http->serve(addr, port, conf.http_threads);
}

static void startup(application &app) {
    taskname("startup");

    std::shared_ptr<state> st = std::make_shared<state>(app);
    taskspawn(std::bind(start_http_server, st));
}

int main(int argc, char *argv[]) {
    application app{"0.0.1", conf};
    namespace po = boost::program_options;
    app.opts.configuration.add_options()
        ("http,H", po::value<std::string>(&conf.http_address)->default_value("0.0.0.0:3080"),
         "http listen address:port")
        ("http-threads", po::value<unsigned int>(&conf.http_threads)->default_value(1), "http threads")
    ;
    app.parse_args(argc, argv);
    taskspawn(std::bind(startup, std::ref(app)));
    return app.run();
}
