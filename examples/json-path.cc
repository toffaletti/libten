#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/algorithm/string.hpp>

#include "json.hh"
#include "jsonpack.hh"

using namespace ten;
using namespace std;
using namespace msgpack;

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
    unsigned int indent;
    string input_format;
    string output_format;
    string query;
    bool sort_keys;
};

static config conf;

int main(int argc, char *argv[]) {
    options opts;

    opts.configuration.add_options()
        ("indent,n", po::value<unsigned int>(&conf.indent)->default_value(1),
         "pretty print with newlines and n spaces")
        ("in,i", po::value<string>(&conf.input_format)->default_value("json"),
         "input format: json,msgpack")
        ("out,o", po::value<string>(&conf.output_format)->default_value("json"),
         "output format: json,msgpack")
        ("sort-keys", po::value<bool>(&conf.sort_keys)->zero_tokens(), "sort JSON object keys")
    ;

    opts.hidden.add_options()
        ("query", po::value<string>(&conf.query), "json path query")
    ;

    opts.pdesc.add("query", 1);
    parse_args(opts, argc, argv);

    try {
        cin >> noskipws;
        istream_iterator<char> it(cin);
        istream_iterator<char> end;
        string input(it, end);
        json js;
        
        if (conf.input_format == "json") {
            js = json::load(input);
        } else if (conf.input_format == "msgpack") {
            size_t offset = 0;
            zone z;
            object obj;
            unpack_return r = unpack(input.data(), input.size(), &offset, &z, &obj);
            if (r != UNPACK_SUCCESS) {
                throw std::runtime_error("error unpacking msgpack format");
            }
            js = obj.as<json>();
        }
        if (!conf.query.empty()) {
            js = js.path(conf.query);
        }
        if (conf.output_format == "json") {
            size_t flags = JSON_ENCODE_ANY | JSON_INDENT(conf.indent);
            if (conf.indent == 0) {
                flags |= JSON_COMPACT;
            }
            if (conf.sort_keys) {
                flags |= JSON_SORT_KEYS;
            }
            cout << js.dump(flags) << endl;
        } else if (conf.output_format == "msgpack") {
            sbuffer sbuf;
            pack(sbuf, js);
            cout.write(sbuf.data(), sbuf.size());
        }
    } catch (exception &e) {
        cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
