#include "http_message.hh"
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <algorithm>

/*
 * normalize http11 message header field names.
 */
static std::string normalize_header_name(const std::string &field) {
  bool first_letter = true;
  std::stringstream ss;
  for (std::string::const_iterator i = field.begin(); i!=field.end(); ++i) {
    if (!first_letter) {
        char c = tolower(*i);
        if (c == '_') {
            c = '-';
            first_letter = true;
        } else if (c == '-') {
            first_letter = true;
        }
        ss << c;
    } else {
        ss << (char)toupper(*i);
        first_letter = false;
    }
  }
  return ss.str();
}

struct is_header {
    const std::string &field;
    is_header(const std::string &normal_field) : field(normal_field) {}

    bool operator()(const std::pair<std::string, std::string> &header) {
        return header.first == field;
    }
};

void http_base::append_header(const std::string &field, const std::string &value) {
    std::string normal_field = normalize_header_name(field);
    headers.push_back(std::make_pair(normal_field, value));
}

void http_base::append_header(const std::string &field, unsigned long long value) {
    append_header(field, boost::lexical_cast<std::string>(value));
}

bool http_base::remove_header(const std::string &field) {
    std::string normal_field = normalize_header_name(field);
    header_list::iterator i = std::remove_if(headers.begin(),
        headers.end(), is_header(normal_field));
    if (i != headers.end()) {
        headers.erase(i, headers.end()); // remove *all*
        return true;
    }
    return false;
}

std::string http_base::header_string(const std::string &field) {
    header_list::iterator i = std::find_if(headers.begin(),
        headers.end(), is_header(normalize_header_name(field)));
    if (i != headers.end()) {
        return i->second;
    }
    return "";
}

unsigned long long http_base::header_ull(const std::string &field) {
    header_list::iterator i = std::find_if(headers.begin(),
        headers.end(), is_header(normalize_header_name(field)));
    if (i != headers.end()) {
        return boost::lexical_cast<unsigned long long>(i->second);
    }
    return 0;
}

static void _request_method(void *data, const char *at, size_t length) {
  http_request *msg = (http_request *)data;
  msg->method.assign(at, length);
}

static void _request_uri(void *data, const char *at, size_t length) {
  http_request *msg = (http_request *)data;
  msg->uri.assign(at, length);
}

static void _fragment(void *data, const char *at, size_t length) {
  http_request *msg = (http_request *)data;
  msg->fragment.assign(at, length);
}

static void _request_path(void *data, const char *at, size_t length) {
  http_request *msg = (http_request *)data;
  msg->path.assign(at, length);
}

static void _query_string(void *data, const char *at, size_t length) {
  http_request *msg = (http_request *)data;
  msg->query_string.assign(at, length);
}

static void _http_version(void *data, const char *at, size_t length) {
  http_request *req = (http_request *)data;
  req->http_version.assign(at, length);
}

static void _header_done(void *data, const char *at, size_t length) {
  http_request *req = (http_request *)data;
  /* set body */
  /* TODO: not sure this logic is right. length might be wrong */
  if (length) {
    req->body.assign(at, length);
    req->body_length = length;
  }
}

static void _http_field(void *data, const char *field,
  size_t flen, const char *value, size_t vlen)
{
  http_request *req = (http_request *)data;
  req->append_header(std::string(field, flen), std::string(value, vlen));
}

void http_request::parser_init(http_request_parser *p) {
  http_request_parser_init(p);
  p->data = this;
  p->http_field = _http_field;
  p->request_method = _request_method;
  p->request_uri = _request_uri;
  p->fragment = _fragment;
  p->request_path = _request_path;
  p->query_string = _query_string;
  p->http_version = _http_version;
  p->header_done = _header_done;
}

std::string http_request::data() {
    std::stringstream ss;
    ss << method << " " << uri << " " << http_version << "\r\n";
    for (header_list::iterator i = headers.begin(); i!=headers.end(); ++i) {
        ss << i->first << ": " << i->second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

/* http_response_t */

static void http_field_cl(void *data, const char *field,
  size_t flen, const char *value, size_t vlen)
{
  http_response *resp = (http_response *)data;
  resp->append_header(std::string(field, flen), std::string(value, vlen));
}

static void reason_phrase_cl(void *data, const char *at, size_t length) {
  http_response *resp = (http_response *)data;
  resp->reason.assign(at, length);
}

static void status_code_cl(void *data, const char *at, size_t length) {
  http_response *resp = (http_response *)data;
  std::string tmp(at, length);
  resp->status_code = boost::lexical_cast<unsigned long>(tmp);
}

static void chunk_size_cl(void *data, const char *at, size_t length) {
  http_response *resp = (http_response *)data;
  std::string tmp(at, length);
  // NOTE: size is in base 16
  resp->chunk_size = strtoull(tmp.c_str(), NULL, 16);
}

static void http_version_cl(void *data, const char *at, size_t length) {
  http_response *resp = (http_response *)data;
  resp->http_version.assign(at, length);
}

static void header_done_cl(void *data, const char *at, size_t length) {
  http_response *resp = (http_response *)data;
  /* set body */
  /* TODO: the length is not right here */
  /* printf("HEADER DONE: %zu [%s]\n", length, at); */
  if (at || length) {
    resp->body.assign(at, length);
    resp->body_length = length;
  }
}

static void last_chunk_cl(void *data, const char *at, size_t length) {
  (void)at;
  (void)length;
  http_response *resp = (http_response *)data;
  resp->last_chunk = true;
}

void http_response::parser_init(http_response_parser *p) {
  http_response_parser_init(p);
  p->data = this;
  p->http_field = http_field_cl;
  p->reason_phrase = reason_phrase_cl;
  p->status_code = status_code_cl;
  p->chunk_size = chunk_size_cl;
  p->http_version = http_version_cl;
  p->header_done = header_done_cl;
  p->last_chunk = last_chunk_cl;
}


std::string http_response::data() {
    std::stringstream ss;
    ss << http_version << " " << status_code << " " << reason << "\r\n";
    for (header_list::iterator i = headers.begin(); i!=headers.end(); ++i) {
        ss << i->first << ": " << i->second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

