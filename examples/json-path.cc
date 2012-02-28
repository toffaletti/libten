#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/algorithm/string.hpp>

#include "json.hh"

using namespace ten;
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

static void showhelp(options &opts, std::ostream &os = std::cerr) {
    std::cerr << opts.visible << std::endl;
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

    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl << std::endl;
        showhelp(opts);
        exit(1);
    }
}

struct config {
    unsigned int indent;
    std::string query;
};

static config conf;

int main(int argc, char *argv[]) {
    options opts;

    opts.configuration.add_options()
        ("indent,i", po::value<unsigned int>(&conf.indent)->default_value(1),
         "pretty print with newlines and n spaces")
    ;

    opts.hidden.add_options()
        ("query", po::value<std::string>(&conf.query), "json path query")
    ;

    opts.pdesc.add("query", 1);
    parse_args(opts, argc, argv);

    try {
        std::cin >> std::noskipws;
        std::istream_iterator<char> it(std::cin);
        std::istream_iterator<char> end;
        std::string input(it, end);
        jsobj js(input);
        if (!conf.query.empty()) {
            js = js.path(conf.query);
        }
        std::cout << js.dump(JSON_ENCODE_ANY | JSON_INDENT(conf.indent)) << std::endl;
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
