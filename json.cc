#include "json.hh"
#include <list>
#include <vector>
#include <boost/lexical_cast.hpp>

namespace ten {

static bool next_token(const std::string &path, size_t &i, std::string &tok) {
    size_t start = i;
    while (i < path.size()) {
        switch (path[i])  {
        case '/':
        case '*':
        case '[':
        case ']':
        case ':':
        case ',':
        case '=':
            goto done;
        }
        ++i;
    }
done:
    if (start == i) ++i;
    tok.assign(path.substr(start, i-start));
    return !tok.empty();
}

static void add_result(jsobj &result, json_t *r) {
    if (json_is_array(r)) {
        json_array_extend(result.ptr(), r);
    } else {
        json_array_append(result.ptr(), r);
    }
}

static void recursive_descent(json_t *root, jsobj &result, const std::string &match) {
    if (json_is_object(root)) {
        void *iter = json_object_iter(root);
        while (iter) {
            if (strcmp(json_object_iter_key(iter), match.c_str()) == 0) {
                add_result(result, json_object_iter_value(iter));
            }
            recursive_descent(json_object_iter_value(iter), result, match);
            iter = json_object_iter_next(root, iter);
        }
    } else if (json_is_array(root)) {
        for (size_t i=0; i<json_array_size(root); ++i) {
            recursive_descent(json_array_get(root, i), result, match);
        }
    }
}

static void match_node(json_t *root, jsobj &result, const std::string &match) {
    if (json_is_object(root)) {
        if (match == "*") {
            // match all
            void *iter = json_object_iter(root);
            while (iter) {
                add_result(result, json_object_iter_value(iter));
                iter = json_object_iter_next(root, iter);
            }
        } else {
            add_result(result, json_object_get(root, match.c_str()));
        }
    } else if (json_is_array(root)) {
        for (size_t i=0; i<json_array_size(root); ++i) {
            json_t *r = json_array_get(root, i);
            match_node(r, result, match);
        }
    }
}

static void select_node(jsobj &result, std::list<std::string> &tokens) {
    tokens.pop_front(); // remove '/'
    if (tokens.front() == "/") {
        // recursive decent!
        tokens.pop_front();
        std::string match = tokens.front();
        tokens.pop_front();
        jsobj tmp(json_array());
        recursive_descent(result.ptr(), tmp, match);
        result = tmp;
    } else {
        std::string match = tokens.front();
        if (match == "[") return;
        tokens.pop_front();
        jsobj tmp(json_array());
        match_node(result.ptr(), tmp, match);
        if (json_array_size(tmp.ptr()) == 1) {
            result = json_incref(json_array_get(tmp.ptr(), 0));
        } else {
            result = tmp;
        }
    }
}

static void slice_op(jsobj &result, std::list<std::string> &tokens) {
    tokens.pop_front();
    std::vector<std::string> args;
    while (!tokens.empty() && tokens.front() != "]") {
        args.push_back(tokens.front());
        tokens.pop_front();
    }
    tokens.pop_front(); // pop ']'

    //using ::operator<<;
    //std::cout << "args: " << args << "\n";
    if (args.size() == 1) {
        size_t index = boost::lexical_cast<size_t>(args.front());
        result = result[index];
    } else if (args.size() == 3) {
        std::string op = args[1];
        if (op == ":") {
            ssize_t start = boost::lexical_cast<ssize_t>(args[0]);
            ssize_t end = boost::lexical_cast<ssize_t>(args[2]);
        } else if (op == "=") {
            std::string key = args[0];
            jsobj filter(args[2]);
            jsobj tmp(json_array());
            for (size_t i=0; i<result.size(); ++i) {
                if (result[i][key] == filter) {
                    add_result(tmp, result[i].ptr());
                }
            }
            result = tmp;
        }
    }
}

static void visit_all(json_t *root, const jsobj::visitor_func_t &visitor) {
    if (json_is_object(root)) {
        void *iter = json_object_iter(root);
        while (iter) {
            if (!visitor(root, json_object_iter_key(iter), json_object_iter_value(iter))) {
                return;
            }
            visit_all(json_object_iter_value(iter), visitor);
            iter = json_object_iter_next(root, iter);
        }
    } else if (json_is_array(root)) {
        for (size_t i=0; i<json_array_size(root); ++i) {
            visit_all(json_array_get(root, i), visitor);
        }
    }
}

void jsobj::visit(const visitor_func_t &visitor) {
    visit_all(p.get(), visitor);
}

jsobj jsobj::path(const std::string &path) {
    size_t i = 0;
    std::string tok;
    std::list<std::string> tokens;
    while (next_token(path, i, tok)) {
        tokens.push_back(tok);
    }

    jsobj result = *this;
    while (!tokens.empty()) {
        if (tokens.front() == "/") {
            // select current node
            select_node(result, tokens);
        } else if (tokens.front() == "*") {
            tokens.pop_front();
            if (json_is_object(result.ptr())) {
                json_t *tmp = json_array();
                void *iter = json_object_iter(result.ptr());
                while (iter) {
                    json_array_append(tmp, json_object_iter_value(iter));
                    iter = json_object_iter_next(result.ptr(), iter);
                }
                result = tmp;
            }
        } else if (tokens.front() == "[") {
            slice_op(result, tokens);
        }
    }
    return result;
}

} // end namespace ten
