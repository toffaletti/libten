/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */

#ifndef HTTP_RESPONSE_PARSER_HH
#define HTTP_RESPONSE_PARSER_HH

#include <sys/types.h>

#if defined(_WIN32)
#include <stddef.h>
#endif

typedef void (*cl_element_cb)(void *data, const char *at, size_t length);
typedef void (*cl_field_cb)(void *data, const char *field, size_t flen, const char *value, size_t vlen);

typedef struct http_response_parser {
  int cs;
  size_t body_start;
  int content_len;
  size_t nread;
  size_t mark;
  size_t field_start;
  size_t field_len;

  void *data;

  cl_field_cb http_field;
  cl_element_cb reason_phrase;
  cl_element_cb status_code;
  cl_element_cb chunk_size;
  cl_element_cb http_version;
  cl_element_cb header_done;
  cl_element_cb last_chunk;

} http_response_parser;

int http_response_parser_init(http_response_parser *parser);
int http_response_parser_finish(http_response_parser *parser);
size_t http_response_parser_execute(http_response_parser *parser, const char *data, size_t len, size_t off);
int http_response_parser_has_error(http_response_parser *parser);
int http_response_parser_is_finished(http_response_parser *parser);

#define http_response_parser_nread(parser) (parser)->nread

#endif
