#include "json.hh"
#include <list>
#include <boost/lexical_cast.hpp>

//debug
#include <iostream>

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
        json_array_extend(result, r);
    } else {
        json_array_append(result, r);
    }
}

static void recursive_descent(json_t *root, jsobj &result, const std::string &match) {
    //std::cout << "descending into: " << root << "\n";
    if (json_is_object(root)) {
        void *iter = json_object_iter(root);
        while (iter) {
            //std::cout << "k: " << json_object_iter_key(iter) << "\n";
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
        recursive_descent(result, tmp, match);
        result = tmp;
    } else {
        std::string match = tokens.front();
        tokens.pop_front();
        jsobj tmp(json_array());
        match_node(result, tmp, match);
        result = tmp;
    }
}

jsobj jsobj::path(const std::string &path) {
    size_t i = 0;
    std::string tok;
    std::list<std::string> tokens;
    while (next_token(path, i, tok)) {
        //std::cout << tok << "\n";
        tokens.push_back(tok);
    }

    jsobj result = p.get();
    while (!tokens.empty()) {
        if (tokens.front() == "/") {
            // select current node
            select_node(result, tokens);
        } else if (tokens.front() == "*") {
            tokens.pop_front();
            if (json_is_object((json_t *)result)) {
                json_t *tmp = json_array();
                void *iter = json_object_iter(result);
                while (iter) {
                    json_array_append(tmp, json_object_iter_value(iter));
                    iter = json_object_iter_next(result, iter);
                }
                result = tmp;
            }
        } else if (tokens.front() == "[") {
            tokens.pop_front();
            size_t index = boost::lexical_cast<size_t>(tokens.front());
            tokens.pop_front();
            tokens.pop_front(); // pop ']'
            result = json_array_get(result, index);
        }
        
    }
    return result;
}

} // end namespace ten
