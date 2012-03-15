#include "json.hh"
#include "error.hh"
#include "logging.hh"
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <deque>

namespace ten {
using namespace std;


extern "C" {
    static int ostream_json_dump_callback(const char *buffer, size_t size, void *osptr) {
        ostream *o = (ostream *)osptr;
        o->write(buffer, size);
        return 0;
    }
}

ostream & operator << (ostream &o, const json_t *j) {
    if (j)
        json_dump_callback(j, ostream_json_dump_callback, &o, JSON_ENCODE_ANY);
    return o;
}


json json::load(const char *s, size_t len, unsigned flags) {
    json_error_t err;
    json j(json_loadb(s, len, flags, &err), json_take);
    if (!j)
        throw errorx("%s", err.text);
    return j;
}

string json::dump(unsigned flags) {
    ostringstream ss;
    if (_p)
        json_dump_callback(_p.get(), ostream_json_dump_callback, static_cast<ostream *>(&ss), flags);
    return ss.str();
}

// simple visit of all objects

#ifdef TEN_JSON_CXX11
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
#else
void json::visit(const json::visitor_func_t &visitor) {
    if (is_object()) {
        for (auto kv = obj().begin(); kv != obj().end(); ++kv) {
            if (!visitor(get(), (*kv).first, (*kv).second.get()))
                return;
            json((*kv).second).visit(visitor);
        }
    }
    else if (is_array()) {
        for (auto j = arr().begin(); j != arr().end(); ++j) {
            (*j).visit(visitor);
        }
    }
}
#endif

// for debugging
static ostream & operator << (ostream &o, const vector<string> &v) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o << " ";
        o << v[i];
    }
    return o;
}

static void add_result(json &result, json r) {
    if (r.is_array())
        result.aconcat(r);
    else
        result.apush(r);
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

#ifdef TEN_JSON_CXX11
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
#else
static void recursive_elements(json root, json &result, const string &match) {
    if (root.is_object()) {
        for (auto kv = root.obj().begin(); kv != root.obj().end(); ++kv) {
            if ((*kv).first == match)
                add_result(result, (*kv).second);
            recursive_elements(json((*kv).second), result, match);
        }
    }
    else if (root.is_array()) {
        for (auto el = root.arr().begin(); el != root.arr().end(); ++el)
            recursive_elements(*el, result, match);
    }
}

static void match_node(json root, json &result, const string &match) {
    if (root.is_object()) {
        if (match == "*") {
            for (auto kv = root.obj().begin(); kv != root.obj().end(); ++kv) {
                add_result(result, (*kv).second);
            }
        }
        else {
            add_result(result, root[match]);
        }
    }
    else if (root.is_array()) {
        for (auto el = root.arr().begin(); el != root.arr().end(); ++el) {
            match_node(*el, result, match);
        }
    }
}
#endif

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
        result = (tmp.asize() == 1) ? tmp(0) : tmp;
    }
}

#ifdef TEN_JSON_CXX11
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
            result = result(index);
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
            ssize_t start = boost::lexical_cast<ssize_t>(args[0]);
            ssize_t end = boost::lexical_cast<ssize_t>(args[2]);
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
                    tmp.apush(kv.second);
                result = tmp;
            }
        }
        else if (tokens.front() == "[") {
            slice_op(result, tokens);
        }
    }
    return result;
}
#else
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
            result = result(index);
        } catch (boost::bad_lexical_cast &e) {
            string key = args.front();
            auto tmp(json::array());
            for (auto r = result.arr().begin(); r != result.arr().end(); ++r) {
                if ((*r)[key])
                    add_result(tmp, *r);
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
            for (auto r = result.arr().begin(); r != result.arr().end(); ++r) {
                if ((*r)[key] == filter)
                    add_result(tmp, *r);
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
                for (auto kv = result.obj().begin(); kv != result.obj().end(); ++kv)
                    tmp.apush((*kv).second);
                result = tmp;
            }
        }
        else if (tokens.front() == "[") {
            slice_op(result, tokens);
        }
    }
    return result;
}
#endif

} // TS
