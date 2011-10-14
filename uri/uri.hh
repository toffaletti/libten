#ifndef URI_HH
#define URI_HH

#include <string>
#include <utility>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

/* TODO:
 * Normalization and Comparison
 * http://tools.ietf.org/html/rfc3986#section-6
 */

struct uri {
    std::string scheme;
    std::string userinfo;
    std::string host;
    std::string path;
    std::string query;
    std::string fragment;
    unsigned int port;

    uri() : port(0) {}
    uri(const std::string &uri_str) {
        const char *error_at = NULL;
        if (!parse(uri_str.c_str(), uri_str.size(), &error_at)) {
            if (error_at) {
                throw std::runtime_error(error_at);
            } else {
                throw std::runtime_error("uri parser error");
            }
        }
    }

    int parse(const char *buf, size_t len, const char **error_at = NULL);

    void clear() {
        scheme.clear();
        userinfo.clear();
        host.clear();
        path.clear();
        query.clear();
        fragment.clear();
        port = 0;
    }

    void normalize();

    std::string compose(bool path_only=false);

    void transform(uri &base, uri &relative);

    static std::string encode(const std::string& src);
    static std::string decode(const std::string& src);

    template <typename T> struct query_match {
        query_match(const T &k_) : k(k_) {}

        const T &k;

        bool operator()(const std::pair<T, T> &i) {
            return i.first == k;
        }
    };

    typedef std::vector<std::pair<std::string, std::string> > query_params;

    query_params parse_query();

    static query_params::iterator find_param(query_params &p, const std::string &k) {
        return std::find_if(p.begin(), p.end(), query_match<std::string>(k));
    }

    template <typename ParamT> static bool get_param(query_params &p, const std::string &k, ParamT &v) {
        auto i = std::find_if(p.begin(), p.end(), query_match<std::string>(k));
        if (i != p.end()) {
            try {
                v = boost::lexical_cast<ParamT>(i->second);
                return true;
            } catch (boost::bad_lexical_cast &e) {}
        }
        return false;
    }

    static void remove_param(query_params &p, const std::string &k) {
        query_params::iterator nend = std::remove_if(p.begin(), p.end(), query_match<std::string>(k));
        p.erase(nend, p.end());
    }

    static std::string params_to_query(const query_params &pms) {
        if (pms.empty()) return "";
        std::stringstream ss;
        ss << "?";
        query_params::const_iterator it = pms.begin();
        while (it!=pms.end()) {
            ss << it->first << "=" << it->second;
            ++it;
            if (it != pms.end()) {
                ss << "&";
            }
        }
        return ss.str();
    }

    struct match_char {
        char c;
        match_char(char c) : c(c) {}
        bool operator()(char x) const { return x == c; }
    };
    typedef std::vector<std::string> split_vector;
    split_vector path_splits() {
        split_vector splits;
        boost::split(splits, path, match_char('/'));
        return splits;
    }
};

#endif // URI_HH
