#ifndef LIBTEN_APP_HH
#define LIBTEN_APP_HH

#include <sys/resource.h>
#include <syslog.h>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/algorithm/string.hpp>

#include "ten/task.hh"
#include "ten/logging.hh"
#include "ten/term.hh"
#include "ten/descriptors.hh"

namespace ten {

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

    void configure_glog(const char *name) const;
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
            const char *name_= program_invocation_short_name);
    ~application();

    application(const application &) = delete;
    application &operator = (const application &) = delete;

    void showhelp(std::ostream &os = std::cerr);
    void parse_args(int argc, char *argv[]);

    template <typename ConfigT>
    const ConfigT &conf() const {
        return static_cast<ConfigT &>(_conf);
    }

    void run();

    std::vector<std::string> vargs();

    void restart(std::vector<std::string> vargs);

    void quit();
private:
    app_config &_conf;
    static application *global_app;
};


} // end namespace ten 

#endif // APP_HH
