#ifndef LIBTEN_APP_HH
#define LIBTEN_APP_HH

#include <sys/resource.h>
#include <syslog.h>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/algorithm/string.hpp>

#include "task.hh"
#include "logging.hh"
#include "term.hh"
#include "descriptors.hh"

namespace ten {

//! remove arguments from arg vector
inline void remove_arg(std::vector<std::string> &args, const std::string &arg, unsigned after=1) {
    struct starts_with {
        const std::string &prefix;
        starts_with(const std::string &prefix_) : prefix(prefix_) {}
        bool operator()(const std::string &x) const {
            return boost::starts_with(x, prefix);
        }
    };
    auto it = std::find_if(args.begin(), args.end(), starts_with(arg));
    if (it != args.end()) {
        auto end(it);
        std::advance(end, after+1);
        args.erase(it, end);
    }
}

static int set_maxrlimit(int resource, rlim_t &max)
{
    struct rlimit rl;
    if (getrlimit(resource, &rl) == -1) {
        return -1;
    }
    VLOG(3) << "resource: " << resource << " rlim_cur: " << rl.rlim_cur << " rlim_max: " << rl.rlim_max;
    max = rl.rlim_cur = rl.rlim_max;
    return setrlimit(resource, &rl);
}

//! inherit application config from this
struct app_config {
    std::string config_path;
    rlim_t min_fds;

    // google glog options
    int glog_min_level;
    int glog_stderr_level;
    int glog_syslog_level;
    int glog_syslog_facility;
    int glog_file_level;
    std::string glog_dir;
    int glog_maxsize;
    int glog_v;
    std::string glog_vmodule;

    void configure_glog(const char *name) const {
        FLAGS_logtostderr = false; // turn master switch back off

        FLAGS_minloglevel = glog_min_level;
        FLAGS_stderrthreshold = glog_stderr_level;
        FLAGS_log_dir = glog_dir;
        FLAGS_max_log_size = glog_maxsize;
        FLAGS_v = glog_v;
        FLAGS_vmodule = glog_vmodule;

        // TODO: expose these as simple flags?
        if (glog_syslog_level >= 0 && glog_syslog_facility >= 0)
            SetSyslogLogging(glog_syslog_level, glog_syslog_facility);

        // Log only at the requested level (if any).
        // Other levels disabled by setting filename to "" (not null).
        for (int i = 0; i < NUM_SEVERITIES; ++i)
            SetLogDestination(i, (i == glog_file_level) ? name : "");
    }
};

namespace po = boost::program_options;

//! setup basic options for all applications
struct options {
    const unsigned line_length;

    po::options_description generic;
    po::options_description configuration;
    po::options_description hidden;
    po::options_description visible;
    po::options_description cmdline_options;
    po::options_description config_file_options;
    po::positional_options_description pdesc;

    options(const char *appname, app_config &c) :
        line_length(terminal_width()),
        generic("Generic options",     line_length, line_length / 2),
        configuration("Configuration", line_length, line_length / 2),
        hidden("Hidden options",       line_length, line_length / 2),
        visible("Allowed options",     line_length, line_length / 2),
        cmdline_options(    line_length, line_length / 2),
        config_file_options(line_length, line_length / 2),
        pdesc()
    {
        generic.add_options()
            ("version,v", "Show version")
            ("help", "Show help message")
            ;

        std::string conffile(appname);
        conffile += ".conf";
        configuration.add_options()
            ("config", po::value(&c.config_path)->default_value(conffile), "config file path")
            ("min-fds", po::value<rlim_t>(&c.min_fds)->default_value(0), "minimum number of file descriptors required to run")
            ("glog-min", po::value(&c.glog_min_level)->default_value(0), "ignore log messages below this level")
            ("glog-stderr", po::value(&c.glog_stderr_level)->default_value(0), "log to stderr at or above this level")
            ("glog-syslog", po::value(&c.glog_syslog_level)->default_value(-1), "log to syslog at or above this level (if nonnegative)")
            ("glog-file", po::value(&c.glog_file_level)->default_value(-1), "log to file at or above this level (if nonnegative)")
            ("glog-dir", po::value(&c.glog_dir)->default_value(""), "write log files to this directory")
            ("glog-maxsize", po::value(&c.glog_maxsize)->default_value(1800), "max log size (in MB)")
            ("glog-v", po::value(&c.glog_v)->default_value(0), "log vlog messages at or below this value")
            ("glog-vmodule", po::value(&c.glog_vmodule), "comma separated <module>=<level>. overides glog-v")
            ;

        // TODO: make this cmdline-configurable
        c.glog_syslog_facility = LOG_USER;
    }

    void setup() {
        cmdline_options.add(generic).add(configuration).add(hidden);
        config_file_options.add(configuration).add(hidden);
        visible.add(generic).add(configuration);
    }
};

//! helper for a libten application with logging, config, and versioning
class application {
public:
    options opts;
    po::variables_map vm;
    std::string name;
    std::string version;
    std::string usage;
    std::string usage_example;

    application(const char *version_, app_config &c,
            const char *name_= program_invocation_short_name)
        : opts(name_, c), name(name_), version(version_), _conf(c),
        sigpi(O_NONBLOCK)
    {
        if (global_app != 0) {
            throw errorx("there can be only one application");
        }
        global_app = this;
    }

    application(const application &) = delete;
    application &operator = (const application &) = delete;

    ~application() {}

    void showhelp(std::ostream &os = std::cerr) {
        if (!usage.empty())
            std::cerr << usage << std::endl;
        std::cerr << opts.visible << std::endl;
        if (!usage_example.empty())
            std::cerr << usage_example << std::endl;
    }

    void parse_args(int argc, char *argv[]) {
        try {
            opts.setup();

            po::store(po::command_line_parser(argc, argv)
                .options(opts.cmdline_options).positional(opts.pdesc).run(), vm);
            po::notify(vm);

            if (vm.count("help")) {
                showhelp();
                exit(1);
            }

            std::ifstream config_stream(_conf.config_path.c_str());
            po::store(po::parse_config_file(config_stream, opts.config_file_options), vm);
            po::notify(vm);

            if (vm.count("version")) {
                std::cerr << version << std::endl;
                exit(1);
            }

            // apply glog options
            _conf.configure_glog(name.c_str());

            // setrlimit to increase max fds
            // http://www.kernel.org/doc/man-pages/online/pages/man2/getrlimit.2.html
            rlim_t rmax = 0;
            if (set_maxrlimit(RLIMIT_NOFILE, rmax)) {
                PLOG(ERROR) << "setting fd limit failed";
            }
            // add 1 because NOFILE is 0 indexed and min_fds is a count
            if (rmax+1 < _conf.min_fds) {
                LOG(ERROR) << "could not set RLIMIT_NOFILE high enough: " << rmax+1 << " < " << _conf.min_fds;
                exit(1);
            }
            // turn on core dumps
            if (set_maxrlimit(RLIMIT_CORE, rmax)) {
                PLOG(ERROR) << "setting max core size limit failed";
            }
        } catch (std::exception &e) {
            std::cerr << "argv[";
            for (int i=0; i<argc; ++i) {
                std::cerr << argv[i] << " ";
            }
            std::cerr << "]\n";
            std::cerr << "Error: " << e.what() << std::endl << std::endl;
            showhelp();
            exit(1);
        }
    }

    template <typename ConfigT>
    const ConfigT &conf() const {
        return static_cast<ConfigT &>(_conf);
    }

    int run() { 
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        // install SIGINT handler
        throw_if(sigaction(SIGINT, NULL, &act) == -1);
        if (act.sa_handler == SIG_DFL) {
            act.sa_sigaction = application::signal_handler;
            act.sa_flags = SA_RESTART | SA_SIGINFO;
            throw_if(sigaction(SIGINT, &act, NULL) == -1);
            taskspawn(std::bind(&application::signal_task, this));
        }
        return p.main();
    }

    std::vector<std::string> vargs() {
        struct match_char {
            char c;
            match_char(char c_) : c(c_) {}
            bool operator()(char x) const { return x == c; }
        };
        std::string cmdline;
        {
            std::ifstream ifs("/proc/self/cmdline", std::ios::binary);
            std::stringstream ss;
            ss << ifs.rdbuf();
            cmdline = ss.str();
        }
        std::vector<std::string> splits;
        boost::split(splits, cmdline, match_char('\0'));
        std::vector<std::string> args;
        // filter out empty strings
        for (auto arg : splits) {
            if (!arg.empty()) {
                args.push_back(arg);
            }
        }
        return args;
    }

    void restart(std::vector<std::string> vargs) {
        char exe_path[PATH_MAX];
        throw_if(realpath("/proc/self/exe", exe_path) == nullptr);
        pid_t child_pid;
        if ((child_pid = fork()) != 0) {
            // parent
            // pid == -1 is error, no child was started
            throw_if(child_pid == -1);
            LOG(INFO) << "restart child pid: " << child_pid;
            quit();
        } else {
            // child
            char *args[vargs.size()+1];
            for (unsigned i=0; i<vargs.size(); ++i) {
                // evil cast away const
                args[i] = (char *)vargs[i].c_str();
            }
            args[vargs.size()] = nullptr;
            throw_if(execv(exe_path, args) == -1);
        }
    }

    void quit() {
        procshutdown();
    }
private:
    app_config &_conf;
    procmain p;
    pipe_fd sigpi;
    static application *global_app;

    void signal_task() {
        taskname("app::signal_task");
        int sig_num = 0;
        for (;;) {
            fdwait(sigpi.r.fd, 'r');
            ssize_t nr = sigpi.read(&sig_num, sizeof(sig_num));
            CHECK(nr == sizeof(sig_num)) << "short read on signal pipe";
            LOG(WARNING) << strsignal(sig_num) << " received";
            switch (sig_num) {
                case SIGINT:
                    quit();
                    return;
                default:
                    LOG(ERROR) << "BUG: unhandled sig num: " << sig_num;
                    return;
            }
        }
    }

    static void signal_handler(int sig_num, siginfo_t *info, void *ctxt) {
        ssize_t nw = global_app->sigpi.write(&sig_num, sizeof(sig_num));
        (void)nw; // not much we can do if this fails
    }
};

application *application::global_app = 0;

} // end namespace ten 

#endif // APP_HH
