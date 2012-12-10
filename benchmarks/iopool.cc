#include "ten/ioproc.hh"
#include "ten/logging.hh"

#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>

using namespace ten;
using namespace std;

namespace po = boost::program_options;

struct options {
    po::options_description generic;
    po::options_description configuration;
    po::options_description hidden;
    po::positional_options_description pdesc;

    po::options_description cmdline_options;
    po::options_description config_file_options;
    po::options_description visible;


    options() :
        generic("Generic options"),
        configuration("Configuration"),
        hidden("Hidden options"),
        visible("Allowed options")
    {
        generic.add_options()
            ("help", "Show help message")
            ;
    }

    void setup() {
        cmdline_options.add(generic).add(configuration).add(hidden);
        config_file_options.add(configuration).add(hidden);
        visible.add(generic).add(configuration);
    }
};

static void showhelp(options &opts, ostream &os = cerr) {
    cerr << opts.visible << endl;
}

static void parse_args(options &opts, int argc, char *argv[]) {
    po::variables_map vm;
    try {
        opts.setup();

        po::store(po::command_line_parser(argc, argv)
            .options(opts.cmdline_options).positional(opts.pdesc).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            showhelp(opts);
            exit(1);
        }

    } catch (exception &e) {
        cerr << "Error: " << e.what() << endl << endl;
        showhelp(opts);
        exit(1);
    }
}

struct config {
    unsigned threads;
    unsigned requests;
    unsigned work;
    unsigned deadline_ms;
    unsigned sleep_ms;
};

static config conf;

struct state {
    ioproc io;

    state()
        : io{default_stacksize, conf.threads, 1*1024*1024}
    {
    }
};

unsigned dowork(unsigned i) {
    taskstate(__FUNCTION__);
    //tasksleep(conf.sleep_ms);
    usleep(conf.sleep_ms * 1000);
    return i;
}

void dorequest(shared_ptr<state> st, unsigned reqn) {
    using namespace std::chrono;
    taskname(__FUNCTION__);
    deadline dl{milliseconds{conf.deadline_ms}};
    unsigned done = 0;
    try {
        iochannel reply_chan{conf.work, true};
        for (unsigned i=0; i<conf.work; ++i) {
            taskstate("spawning %d", i);
            iocallasync(st->io, bind(dowork, i), reply_chan);
        }
        for (unsigned i=0; i<conf.work; ++i) {
            done++;
            taskstate("waiting for %d", conf.work - i);
            LOG(INFO) << reqn <<  " got " << iowait<unsigned>(reply_chan);
        }
    } catch (deadline_reached &s) {
        LOG(INFO) << "task " << taskid() << " deadline reached: " << done << " done out of " << conf.work;
    }
    taskstate("exiting");
}

void startup() {
    taskname(__FUNCTION__);
    shared_ptr<state> st = make_shared<state>();
    for (unsigned i=0; i<conf.requests; ++i) {
        taskspawn(bind(dorequest, st, i));
    }
}

int main(int argc, char *argv[]) {
    options opts;

    opts.configuration.add_options()
        ("threads", po::value<unsigned>(&conf.threads)->default_value(1),
         "number of threads in ioproc pool")
        ("requests", po::value<unsigned>(&conf.requests)->default_value(10),
         "number of concurrent requests that need to do work")
        ("work", po::value<unsigned>(&conf.work)->default_value(3),
         "number of work items to submit")
        ("deadline", po::value<unsigned>(&conf.deadline_ms)->default_value(500),
         "milliseconds for deadline")
        ("sleep", po::value<unsigned>(&conf.sleep_ms)->default_value(200),
         "time to sleep while doing work")
    ;

    parse_args(opts, argc, argv);

    procmain p;
    taskspawn(startup);
    p.main();
}

