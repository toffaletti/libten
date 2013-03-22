#include "ten/app.hh"

//! remove arguments from arg vector
#if 0
static void remove_arg(std::vector<std::string> &args, const std::string &arg, unsigned after=1) {
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
#endif

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

namespace ten {

application *application::global_app = nullptr;

void app_config::configure_glog(const char *name) const {
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
    // Ensure sane umask if we know we're writing log files.
    // If logfiles are configure manually later, you're on your own.
    if (glog_file_level >= 0 && glog_file_level < NUM_SEVERITIES) {
        const int oldmask = umask(0777);
        const int newmask = oldmask & ~0555;
        umask(newmask);
        if (newmask != oldmask) {
            LOG(WARNING) << "umask changed to " << std::oct << std::showbase << newmask
                << " (was " << oldmask << ") to ensure readable logfiles";
        }
    }
}


application::application(const char *version_, app_config &c,
        const char *name_)
    : opts(name_, c), name(name_), version(version_), _conf(c)
{
    task::spawn([] {
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        signal_fd sigfd{sigset, SFD_NONBLOCK};
        signalfd_siginfo si;
        fdwait(sigfd.fd, 'r');
        sigfd.read(si);
        LOG(INFO) << strsignal(si.ssi_signo);
        sigfd.close();
        kernel::shutdown();
    });
    this_task::yield(); // allow signal task to setup

    if (global_app != 0) {
        throw errorx("there can be only one application");
    }
    global_app = this;
}

application::~application() {
    // this is useful for when an exception is thrown
    // in task::main and you want that to shutdown the app
    quit();
}

void application::showhelp(std::ostream &os) {
    if (!usage.empty())
        std::cerr << usage << std::endl;
    std::cerr << opts.visible << std::endl;
    if (!usage_example.empty())
        std::cerr << usage_example << std::endl;
}

void application::parse_args(int argc, char *argv[]) {
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

std::vector<std::string> application::vargs() {
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

void application::restart(std::vector<std::string> vargs) {
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
            args[i] = const_cast<char *>(vargs[i].c_str());
        }
        args[vargs.size()] = nullptr;
        throw_if(execv(exe_path, args) == -1);
    }
}

void application::run() {
    kernel::wait_for_tasks();
}

void application::quit() {
    kernel::shutdown();
}

} // ten

