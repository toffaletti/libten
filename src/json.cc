#include "ten/json.hh"
#include "ten/error.hh"
#include "ten/logging.hh"
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <deque>

namespace ten {
using namespace std;


//----------------------------------------------------------------
// to/from JSON strings and streams
//

extern "C" {
static int ostream_json_dump_callback(const char *buffer, size_t size, void *osptr) {
    ostream *o = reinterpret_cast<ostream *>(osptr);
    o->write(buffer, size);
    return 0;
}
} // "C"

void dump(ostream &o, const json_t *j, unsigned flags) {
    if (j)
        json_dump_callback(j, ostream_json_dump_callback, &o, flags);
}

string json::dump(unsigned flags) const {
    ostringstream ss;
    if (_p)
        json_dump_callback(_p.get(), ostream_json_dump_callback, static_cast<ostream *>(&ss), flags);
    return ss.str();
}

json json::load(const char *s, size_t len, unsigned flags) {
    json_error_t err;
    json j(json_loadb(s, len, flags, &err), json_take);
    if (!j)
        throw errorx("%s", err.text);
    return j;
}

//
// metaprogramming for conversions
//

namespace impl {

// json -> string

template <> string json_traits_conv<string>::cast(const json &j) {
    if (!j.is_string()) throw errorx("not string: %s", j.dump().c_str());
    return j.str();
}

// json -> const char *

template <> const char * json_traits_conv<const char *>::cast(const json &j) {
    if (!j.is_string()) throw errorx("not string: %s", j.dump().c_str());
    return j.c_str();
}

// json -> integral types

template <class T> inline T integral_cast(const json &j) {
    if (!j.is_integer()) throw errorx("not integral: %s", j.dump().c_str());
    const auto i = j.integer();
    constexpr auto
        lowest    = std::numeric_limits<T>::min(),
        highest   = std::numeric_limits<T>::max();
    if (i < lowest || i > highest)
        throw errorx("%lld: out of range for %s", i, typeid(T).name());
    return static_cast<T>(i);
};
template <> short          json_traits_conv<short         >::cast(const json &j) { return integral_cast<short         >(j); }
template <> int            json_traits_conv<int           >::cast(const json &j) { return integral_cast<int           >(j); }
template <> long           json_traits_conv<long          >::cast(const json &j) { return integral_cast<long          >(j); }
template <> long long      json_traits_conv<long long     >::cast(const json &j) { return integral_cast<long long     >(j); }
template <> unsigned short json_traits_conv<unsigned short>::cast(const json &j) { return integral_cast<unsigned short>(j); }
template <> unsigned       json_traits_conv<unsigned      >::cast(const json &j) { return integral_cast<unsigned      >(j); }
#if ULONG_MAX < LLONG_MAX
template <> unsigned long  json_traits_conv<unsigned long >::cast(const json &j) { return integral_cast<unsigned long >(j); }
#endif

// json real

template <> double json_traits_conv<double>::cast(const json &j) {
    if (!j.is_real()) throw errorx("not real: %s", j.dump().c_str());
    return j.real();
}
template <> float json_traits_conv<float>::cast(const json &j) {
    if (!j.is_real()) throw errorx("not real: %s", j.dump().c_str());
    const auto n = j.real();
    constexpr auto
        lowest  = std::numeric_limits<float>::lowest(), // min() is useless here
        highest = std::numeric_limits<float>::max();
    if (n < lowest || n > highest)
        throw errorx("out of range for float: %g", n);
    return j.real();
}

// json boolean

template <> bool json_traits_conv<bool>::cast(const json &j) {
    if (!j.is_boolean()) throw errorx("not boolean: %s", j.dump().c_str());
    return j.boolean();
}

} // end impl


//
// simple visit of all objects
//

void json::visit(const json::visitor_func_t &visitor) {
    if (is_object()) {
        for (auto kv : obj()) {
            if (!visitor(get(), kv.first, kv.second.get()))
                return;
            json(kv.second).visit(visitor);
        }
    }
    else if (is_array()) {
        for (auto j : arr()) {
            json(j).visit(visitor);
        }
    }
}

//
// path
//

static ostream & operator << (ostream &o, const vector<string> &v) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o << " ";
        o << v[i];
    }
    return o;
}

static void add_result(json &result, json r) {
    if (r.is_array())
        result.concat(r);
    else
        result.push(r);
}

static bool next_path_token(const string &path, size_t &i, string &tok) {
    size_t start = i;
    bool inquotes = false;
    while (i < path.size()) {
        switch (path[i])  {
        case '/':
        case '*':
        case '[':
        case ']':
        case ':':
        case ',':
        case '=':
            if (!inquotes)
                goto done;
            break;
        case '"':
            if (inquotes) {
                ++i;
                goto done;
            }
            inquotes = true;
            break;
        }
        ++i;
    }
done:
    if (start == i) ++i;
    tok.assign(path.substr(start, i - start));
    VLOG(5) << "tok: " << tok;
    return !tok.empty();
}

static void recursive_elements(json root, json &result, const string &match) {
    if (root.is_object()) {
        for (auto kv : root.obj()) {
            if (kv.first == match)
                add_result(result, kv.second);
            recursive_elements(json(kv.second), result, match);
        }
    }
    else if (root.is_array()) {
        for (auto el : root.arr())
            recursive_elements(el, result, match);
    }
}

static void match_node(json root, json &result, const string &match) {
    if (root.is_object()) {
        if (match == "*") {
            for (auto kv : root.obj())
                add_result(result, kv.second);
        }
        else {
            add_result(result, root[match]);
        }
    }
    else if (root.is_array()) {
        for (auto r : root.arr())
            match_node(r, result, match);
    }
}

static void select_node(json &result, deque<string> &tokens) {
    tokens.pop_front(); // remove '/'
    if (tokens.empty()) {
        return;
    } else if (tokens.front() == "/") {
        // recursive decent!
        tokens.pop_front();
        string match = tokens.front();
        tokens.pop_front();
        json tmp(json::array());
        recursive_elements(result, tmp, match);
        result = tmp;
    } else {
        string match = tokens.front();
        if (match == "[") return;
        tokens.pop_front();
        json tmp(json::array());
        match_node(result, tmp, match);
        result = (tmp.asize() == 1) ? tmp[0] : tmp;
    }
}

static void slice_op(json &result, deque<string> &tokens) {
    tokens.pop_front();
    vector<string> args;
    while (!tokens.empty() && tokens.front() != "]") {
        args.push_back(tokens.front());
        tokens.pop_front();
    }
    if (tokens.empty())
        throw errorx("path query missing ]");
    tokens.pop_front(); // pop ']'

    DVLOG(5) << "args: " << args;
    if (args.size() == 1) {
        try {
            size_t index = boost::lexical_cast<size_t>(args.front());
            result = result[index];
        } catch (boost::bad_lexical_cast &e) {
            string key = args.front();
            auto tmp(json::array());
            for (auto r : result.arr()) {
                if (r[key])
                    add_result(tmp, r);
            }
            result = tmp;
        }
    } else if (args.size() == 3) {
        string op = args[1];
        if (op == ":") {
            // TODO: make slice ranges work
            //ssize_t start = boost::lexical_cast<ssize_t>(args[0]);
            //ssize_t end = boost::lexical_cast<ssize_t>(args[2]);
        }
        else if (op == "=") {
            string key = args[0];
            json filter(json::load(args[2]));
            DVLOG(5) << "filter: " << filter;
            json tmp(json::array());
            for (auto r : result.arr()) {
                if (r[key] == filter)
                    add_result(tmp, r);
            }
            result = tmp;
        }
    }
}

json json::path(const string &path) {
    size_t i = 0;
    string tok;
    deque<string> tokens;
    while (next_path_token(path, i, tok))
        tokens.push_back(tok);

    json result = *this;
    while (!tokens.empty()) {
        if (tokens.front() == "/") {
            // select current node
            select_node(result, tokens);
        }
        else if (tokens.front() == "*") {
            tokens.pop_front();
            if (result.is_object()) {
                auto tmp(json::array());
                for (auto kv : result.obj())
                    tmp.push(kv.second);
                result = tmp;
            }
        }
        else if (tokens.front() == "[") {
            slice_op(result, tokens);
        }
    }
    return result;
}

} // TS
