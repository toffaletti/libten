#include "app.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/server.hh"
#include "uri/uri.hh"
#include "logging.hh"
#include "net.hh"
#include "channel.hh"
#include <boost/lexical_cast.hpp>

using namespace ten;
const size_t default_stacksize=256*1024;

struct my_config : app_config {
    std::string http_address;
};

static my_config conf;

struct state : boost::noncopyable {
    application &app;
    http_server http;

    state(application &app_) : app(app_) {}
};

static void log_request(http_server::request &h) {
    using namespace std::chrono;
    auto stop = monotonic_clock::now();
    VLOG(1) << h.agent_ip() << " " <<
        h.req.method << " " <<
        h.req.uri << " " <<
        h.resp.status_code << " " <<
        h.resp.get("Content-Length") << " " <<
        duration_cast<milliseconds>(stop-h.start).count();
}

static void http_quit(std::shared_ptr<state> st, http_server::request &h) {
    LOG(INFO) << "quit requested over http";
    st->app.quit();
    h.resp = http_response(200, "OK",
        Headers(
        "Connection", "close",
        "Content-Length", 0
        )
    );
    h.send_response();
}

static void http_root(std::shared_ptr<state> st, http_server::request &h) {
    h.resp = http_response(200, "OK");
    h.resp.set_body("Hello World!\n");
    h.send_response();
}

static void start_http_server(std::shared_ptr<state> &st) {
    using namespace std::placeholders;
    std::string addr = conf.http_address;
    uint16_t port = 0;
    parse_host_port(addr, port);
    if (port == 0) return;
    st->http.set_log_callback(log_request);
    st->http.add_route("/quit", std::bind(http_quit, st, _1));
    st->http.add_route("/*", std::bind(http_root, st, _1));
    st->http.serve(addr, port);
}

static void startup(application &app) {
    taskname("startup");

    std::shared_ptr<state> st(std::make_shared<state>(app));
    taskspawn(std::bind(start_http_server, st));
}

int main(int argc, char *argv[]) {
    application app("0.0.1", conf);
    namespace po = boost::program_options;
    app.opts.configuration.add_options()
        ("http,H", po::value<std::string>(&conf.http_address)->default_value("0.0.0.0:8080"),
         "http listen address:port")
    ;
    app.parse_args(argc, argv);
    taskspawn(std::bind(startup, std::ref(app)));
    return app.run();
}
