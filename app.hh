#include <sys/resource.h>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>

#include "task.hh"
#include "logging.hh"

namespace fw {

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
    std::string glog_log_dir;
    int glog_minloglevel;
    int glog_v;
    std::string glog_vmodule;
    int glog_max_log_size;
};

namespace po = boost::program_options;

//! setup basic options for all applications
struct options {
    po::options_description generic;
    po::options_description configuration;
    po::options_description hidden;
    po::positional_options_description pdesc;

    po::options_description cmdline_options;
    po::options_description config_file_options;
    po::options_description visible;

    options(const char *appname, app_config &c) :
        generic("Generic options"),
        configuration("Configuration"),
        hidden("Hidden options"),
        visible("Allowed options")
    {
        generic.add_options()
            ("version,v", "Show version")
            ("help", "Show help message")
            ;

        std::string conffile(appname);
        conffile += ".conf";
        configuration.add_options()
            ("config", po::value<std::string>(&c.config_path)->default_value(conffile), "config file path")
            ("min-fds", po::value<rlim_t>(&c.min_fds)->default_value(0), "minimum number of file descriptors required to run")
            ("glog-log-dir", po::value<std::string>(&c.glog_log_dir)->default_value(""), "log files will be written to this directory")
            ("glog-minloglevel", po::value<int>(&c.glog_minloglevel)->default_value(0), "log messages at or above this level")
            ("glog-v", po::value<int>(&c.glog_v)->default_value(0), "show vlog messages for <= to value")
            ("glog-vmodule", po::value<std::string>(&c.glog_vmodule), "comma separated <module>=<level>. overides glog-v")
            ("glog-max-log-size", po::value<int>(&c.glog_max_log_size)->default_value(1800), "max log size (in MB)")
            ;
    }

    void setup() {
        cmdline_options.add(generic).add(configuration).add(hidden);
        config_file_options.add(configuration).add(hidden);
        visible.add(generic).add(configuration);
    }
};

class application {
public:
    options opts;
    po::variables_map vm;
    std::string name;
    std::string version;
    std::string usage;
    std::string usage_example;

    application(const char *name_, const char *version_, app_config &c)
        : opts(name_, c), name(name_), version(version_), _conf(c)
    {
    }

    ~application() {
        google::ShutdownGoogleLogging();
    }

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

            // configure glog since we use our own command line/config parsing
            // instead of the google arg parsing
            if (_conf.glog_log_dir.empty()) {
                FLAGS_logtostderr = true;
            }
            FLAGS_log_dir = _conf.glog_log_dir;
            FLAGS_minloglevel = _conf.glog_minloglevel;
            FLAGS_v = _conf.glog_v;
            FLAGS_vmodule = _conf.glog_vmodule;
            FLAGS_max_log_size = _conf.glog_max_log_size;

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
            std::cerr << "Error: " << e.what() << std::endl << std::endl;
            showhelp();
            exit(1);
        }
    }

    template <typename ConfigT>
    const ConfigT &conf() const {
        return static_cast<ConfigT &>(_conf);
    }

    int run() { return p.main(); }

    void quit() {
        // TODO: need a way to cleanly shutdown
    }
protected:
    app_config &_conf;
    procmain p;
};

} // end namespace fw
