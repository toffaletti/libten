/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */

#ifndef HTTP_REQUEST_PARSER_HH
#define HTTP_REQUEST_PARSER_HH

#include <sys/types.h>

#if defined(_WIN32)
#include <stddef.h>
#endif

typedef void (*element_cb)(void *data, const char *at, size_t length);
typedef void (*field_cb)(void *data, const char *field, size_t flen, const char *value, size_t vlen);

typedef struct http_request_parser {
    int cs;
    size_t body_start;
    int content_len;
    size_t nread;
    size_t mark;
    size_t field_start;
    size_t field_len;
    size_t query_start;

    void *data;

    field_cb http_field;
    element_cb request_method;
    element_cb request_uri;
    element_cb fragment;
    element_cb request_path;
    element_cb query_string;
    element_cb http_version;
    element_cb header_done;

} http_request_parser;

int http_request_parser_init(http_request_parser *parser);
int http_request_parser_finish(http_request_parser *parser);
size_t http_request_parser_execute(http_request_parser *parser, const char *data, size_t len, size_t off);
int http_request_parser_has_error(http_request_parser *parser);
int http_request_parser_is_finished(http_request_parser *parser);

#define http_request_parser_nread(parser) (parser)->nread

#endif
