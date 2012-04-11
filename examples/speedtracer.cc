#include "app.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/server.hh"
#include "uri/uri.hh"
#include "logging.hh"
#include "net.hh"
#include "channel.hh"
#include <boost/lexical_cast.hpp>
#include "channel.hh"
#include "net/speedtracer.hh"

using namespace ten;
using namespace std;

const size_t default_stacksize=256*1024;

struct my_config : app_config {
    string http_address;
    unsigned int http_threads;
};

static my_config conf;

struct state : boost::noncopyable {
    application &app;
    http_server http;
    shared_ptr<tracer::session> tsess;

    state(application &app_) : app(app_) {}
};

static void log_request(http_server::request &h) {
    using namespace chrono;
    auto stop = monotonic_clock::now();
    LOG(INFO) << h.agent_ip() << " " <<
        h.req.method << " " <<
        h.req.uri << " " <<
        h.resp.status_code << " " <<
        h.resp.get("Content-Length") << " " <<
        duration_cast<milliseconds>(stop-h.start).count();
}

static void http_quit(shared_ptr<state> st, http_server::request &h) {
    LOG(INFO) << "quit requested over http";
    st->app.quit();
    h.resp = http_response(200,
        Headers(
        "Connection", "close",
        "Content-Length", 0
        )
    );
    h.send_response();
}

static void do_request(shared_ptr<state> st, channel<int> ch, string resource) {
    tasksleep(rand() % 100);
    st->tsess->children.push_back(move(tracer::event(st->tsess)));
    ch.send(0);
}

static void http_root(shared_ptr<state> st, http_server::request &h) {
    st->tsess = make_shared<tracer::session>();
    st->tsess->children.push_back(move(tracer::event(st->tsess)));
    //tracer::trace t(*st->tracer);

    channel<int> ch;
    taskspawn(bind(do_request, st, ch, "a"));
    taskspawn(bind(do_request, st, ch, "b"));
    taskspawn(bind(do_request, st, ch, "c"));
    ch.recv();
    ch.recv();
    ch.recv();

    h.resp = http_response(200);
    h.resp.set_body("Hello World!\n");

    stringstream ss;
    ss << "/trace?" << procnow().time_since_epoch().count();
    h.resp.set("X-TraceId", procnow().time_since_epoch().count());
    h.resp.set("X-TraceUrl", ss.str());
    h.send_response();
}

static void http_trace(shared_ptr<state> st, http_server::request &h) {
    if (st->tsess) {
        h.resp = http_response(200);
        h.resp.set_body(st->tsess->dump());
    } else {
        h.resp = http_response(404);
    }
    h.send_response();
}

static void start_http_server(shared_ptr<state> &st) {
    using namespace placeholders;
    string addr = conf.http_address;
    uint16_t port = 0;
    parse_host_port(addr, port);
    if (port == 0) return;
    st->http.set_log_callback(log_request);
    st->http.add_route("/quit", bind(http_quit, st, _1));
    st->http.add_route("/trace", bind(http_trace, st, _1));
    st->http.add_route("/*", bind(http_root, st, _1));
    st->http.serve(addr, port, conf.http_threads);
}

static void startup(application &app) {
    taskname("startup");

    shared_ptr<state> st(make_shared<state>(app));
    taskspawn(bind(start_http_server, st));
}

int main(int argc, char *argv[]) {
    application app("0.0.1", conf);
    namespace po = boost::program_options;
    app.opts.configuration.add_options()
        ("http,H", po::value<string>(&conf.http_address)->default_value("0.0.0.0:3080"),
         "http listen address:port")
        ("http-threads", po::value<unsigned int>(&conf.http_threads)->default_value(1), "http threads")
    ;
    app.parse_args(argc, argv);
    taskspawn(bind(startup, ref(app)));
    return app.run();
}
