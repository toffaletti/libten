#ifndef LIBTEN_URI_HH
#define LIBTEN_URI_HH

#include <string>
#include <utility>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace ten {

class uri_error : std::exception {
public:
    uri_error(const std::string &uri_str, const char *where) {
        std::stringstream ss;
        ss << "uri parse(" << uri_str << ") error at " << where;
        _msg = ss.str();
    }

    uri_error(const std::string &uri_str) {
        std::stringstream ss;
        ss << "uri parser error on (" << uri_str << ")";
        _msg = ss.str();
    }

    const char *what() const throw() {
        return _msg.c_str();
    }
private:
    std::string _msg;
};

/* TODO:
 * comparison
 * http://tools.ietf.org/html/rfc3986#section-6
 */

//! uniform resource locator
//
//! parser based on ebnf in rfc3986, relaxed for the real-world
class uri {
private:
    int parse(const char *buf, size_t len, const char **error_at = NULL);
    
public:
    std::string scheme;
    std::string userinfo;
    std::string host;
    std::string path;
    std::string query;
    std::string fragment;
    uint16_t port;

    uri() : port(0) {}
    uri(const std::string &uri_str) {
        assign(uri_str);
    }

    void assign(const std::string &uri_str) {
        const char *error_at = NULL;
        if (!parse(uri_str.c_str(), uri_str.size(), &error_at)) {
            if (error_at) {
                throw uri_error(uri_str, error_at);
            } else {
                throw uri_error(uri_str);
            }
        }
    }

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

    std::string compose_path() { return compose(true); }
    std::string compose(bool path_only=false);

    void transform(uri &base, uri &relative);

    static std::string encode(const std::string& src);
    static std::string decode(const std::string& src);
private:
    friend class query_params;

    template <typename T> struct query_match {
        query_match(const T &k_) : k(k_) {}

        const T &k;

        bool operator()(const std::pair<T, T> &i) {
            return i.first == k;
        }
    };

public:
    //! map-like interface to uri query parameters
    class query_params {
    public:
        typedef std::vector<std::pair<std::string, std::string>> params_type;
        typedef params_type::iterator iterator;
    private:
        params_type _params;
    public:
        query_params(const std::string &query);

        template <typename ValueT, typename ...Args>
        query_params(std::string key, ValueT value, Args ...args) {
            _params.reserve(sizeof...(args));
            init(key, value, args...);
        }

        // init can go away with delegating constructor support
        void init() {}
        template <typename ValueT, typename ...Args>
        void init(std::string key, ValueT value, Args ...args) {
            append<ValueT>(key, value);
            init(args...);
        }

        void append(const std::string &key, const std::string &value) {
            _params.push_back(std::make_pair(key, value));
        }

        template <typename ValueT>
            void append(const std::string &field, const ValueT &value) {
                append(field, boost::lexical_cast<std::string>(value));
            }

        params_type::const_iterator find(const std::string &k) const {
            return std::find_if(_params.cbegin(), _params.cend(), query_match<std::string>(k));
        }

        params_type::iterator find(const std::string &k) {
            return std::find_if(_params.begin(), _params.end(), query_match<std::string>(k));
        }

        template <typename ParamT> bool get(const std::string &k, ParamT &v) const {
            auto i = find(k);
            if (i != _params.cend()) {
                try {
                    v = boost::lexical_cast<ParamT>(i->second);
                    return true;
                } catch (boost::bad_lexical_cast &e) {}
            }
            return false;
        }

        void erase(const std::string &k) {
            iterator nend = std::remove_if(_params.begin(), _params.end(), query_match<std::string>(k));
            _params.erase(nend, _params.end());
        }

        std::string str() const {
            if (_params.empty()) return "";
            std::stringstream ss;
            ss << "?";
            params_type::const_iterator it = _params.cbegin();
            while (it!=_params.end()) {
                ss << encode(it->first) << "=" << encode(it->second);
                ++it;
                if (it != _params.end()) {
                    ss << "&";
                }
            }
            return ss.str();
        }

        size_t size() const { return _params.size(); }

    };


public:
    query_params query_part() { return query_params(query); }

    struct match_char {
        char c;
        match_char(char c_) : c(c_) {}
        bool operator()(char x) const { return x == c; }
    };

    typedef std::vector<std::string> split_vector;
    split_vector path_splits() {
        split_vector splits;
        boost::split(splits, path, match_char('/'));
        return splits;
    }
};

} // end namespace ten

#endif // LIBTEN_URI_HH
