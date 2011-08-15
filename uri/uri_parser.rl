#include "uri_parser.hh"
#include <boost/algorithm/string.hpp>
#include <list>
#include <sstream>
#include <boost/smart_ptr/scoped_array.hpp>

%%{
  machine uri_parser;

  action mark {
    mark = fpc;
  }

  action scheme {
    scheme.assign(mark, fpc-mark);
  }

  action userinfo {
    userinfo.assign(mark, fpc-mark);
  }

  action host {
    /* host may be called multiple times because
     * the parser isn't able to figure out the difference
     * between the userinfo and host until after the @ is encountered
     */
    host.assign(mark, fpc-mark);
  }

  action port {
    if (mark) port = strtol(mark, NULL, 0);
  }

  action path {
    path.assign(mark, fpc-mark);
  }

  action query {
    query.assign(mark, fpc-mark);
  }

  action fragment {
    fragment.assign(mark, fpc-mark);
  }

  include uri_grammar "uri_grammar.rl";

  main := URI;
}%%

%% write data;

int uri::parse(const char *buf, size_t len, const char **error_at) {
  clear();

  const char *mark = NULL;
  const char *p, *pe, *eof;
  int cs = 0;

  if (error_at != NULL) {
    *error_at = NULL;
  }

  %% write init;

  p = buf;
  pe = buf+len;
  eof = pe;

  %% write exec;

  if (cs == uri_parser_error && error_at != NULL) {
    *error_at = p;
  }

  return (cs != uri_parser_error && cs >= uri_parser_first_final);
}

%%{
# simple machine to normalize percent encoding in uris
  machine normalize_pct_encoded;

  action pct_unicode_encoded {
    char *d1 = (char *)(fpc-3);
    char *d2 = (char *)(fpc-2);
    char *d3 = (char *)(fpc-1);
    char *d4 = (char *)fpc;

    /* convert hex digits to upper case */
    *d1 = toupper(*d1);
    *d2 = toupper(*d2);
    *d3 = toupper(*d3);
    *d4 = toupper(*d4);
  }

  action pct_encoded {
    char *d1 = (char *)(fpc-1);
    char *d2 = (char *)fpc;

    /* convert hex digits to upper case */
    *d1 = toupper(*d1);
    *d2 = toupper(*d2);

    /* convert hex to character */
    char c = 0;
    for (int i=0; i<2; i++) {
      if (d1[i] < 57) {
        c += (d1[i]-48)*(1<<(4*(1-i)));
      } else {
        c += (d1[i]-55)*(1<<(4*(1-i)));
      }
    }

    /* unescape characters that shouldn't be escaped */
    if (
      ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A)) /* alpha */
      || (c >= 0x30 && c <= 0x39) /* digit */
      || (c == 0x2D) /* hyphen */
      || (c == 0x2E) /* period */
      || (c == 0x5F) /* underscore */
      || (c == 0x7E) /* tilde */
      )
    {
      /* replace encoded value with character */
      *(d1-1) = c;
      memmove(d1, d2+1, pe-(d2+1));
      /* adjust internal state to match the buffer after memmove */
      pe = pe-2;
      /* add null terminator */
      *((char *)pe) = 0;
      fexec fpc-2;
    }
  }

  pct_noencode = ("%" ^xdigit?);
  pct_unicode_encoded = ("%u" xdigit xdigit xdigit xdigit) @pct_unicode_encoded;
  pct_encoded = ("%" xdigit xdigit) @pct_encoded;

  main := (^"%"+ | pct_unicode_encoded | pct_noencode | pct_encoded)*;
}%%

%% write data;

static void normalize_pct_encoded(std::string &str) {
  char *p, *pe;
  int cs = 0;

  %% write init;

  // very naughty. modifying std::string buffer directly!!!
  p = (char *)str.data();
  pe = p+str.size();

  %% write exec;

  assert (cs != normalize_pct_encoded_error);
  str.resize(strlen(str.data()));
}

typedef std::list<std::string> string_splits;

struct match_char {
  char c;
  match_char(char c) : c(c) {}
  bool operator()(char x) const { return x == c; }
};

static void remove_dot_segments(uri &u) {
  if (u.path.empty()) {
    u.path = "/";
    return;
  } else if (u.path == "/") {
    return;
  }

  string_splits splits;
  boost::split(splits, u.path, match_char('/'));
  for (string_splits::iterator i = splits.begin(); i!=splits.end(); ++i) {
    // use clear instead of erase to help preserve trailing slashes
    if (*i == ".") {
      i->clear();
    } else if (*i == "..") {
      i = splits.erase(i);
      --i;
      while (i->empty() && i != splits.begin()) {
        --i;
      }
      i->clear();
    }
  }

  /* reassemble path from segments */
  std::stringstream ss;
  for (string_splits::iterator i = splits.begin(); i!=splits.end(); ++i) {
    if (!i->empty()) {
      ss << "/" << *i;
    }
  }
  // preserve trailing slash
  if (splits.back().empty()) {
    ss << "/";
  }
  u.path = ss.str();
}

void uri::normalize() {
  boost::to_lower(scheme);
  normalize_pct_encoded(userinfo);
  boost::to_lower(host);
  normalize_pct_encoded(host);
  normalize_pct_encoded(path);
  remove_dot_segments(*this);
  normalize_pct_encoded(query);
  normalize_pct_encoded(fragment);
}

std::string uri::compose(bool path_only) {
  std::stringstream ss;
  if (!path_only) {
    if (!scheme.empty()) {
      ss << scheme << ":";
    }
    /* authority */
    if (!userinfo.empty() || !host.empty()) {
      ss << "//";
    }
    if (!userinfo.empty()) {
      ss << userinfo << "@";
    }
    if (!host.empty()) {
      ss << host;
      if (port) {
        ss << ":" << port;
      }
    }
  }
  if (!path.empty()) {
    if (!boost::starts_with(path, "/")) {
      ss << "/";
    }
    ss << path;
  }
  ss << query;
  ss << fragment;
  return ss.str();
}

static void merge_path(uri &base, uri &r, uri &t) {
  if (base.path.empty() || base.path == "/") {
    std::stringstream ss;
    ss << "/" << r.path;
    t.path = ss.str();
  } else if (!r.path.empty()) {
    /* NOTE: skip the last character because the path might end in /
     * but we need to skip the entire last path *segment*
     */
    size_t pos = base.path.find_last_of("/");
    if (pos == base.path.size()-1) {
        pos = base.path.find_last_of("/", pos);
    }
    std::string p = base.path.substr(0, pos+1);
    p += r.path;
    t.path = p;
  } else if (!base.path.empty()) {
    t.path = base.path;
  }
}

void uri::transform(uri &base, uri &r) {
  clear();
  if (!r.scheme.empty()) {
    scheme = r.scheme;
    userinfo = r.userinfo;
    host = r.host;
    port = r.port;
    path = r.path;
    remove_dot_segments(*this);
    query = r.query;
  } else {
    /* authority */
    if (!r.userinfo.empty() || !r.host.empty()) {
      userinfo = r.userinfo;
      host = r.host;
      port = r.port;
      path = r.path;
      remove_dot_segments(*this);
      query = r.query;
    } else {
      if (r.path.empty()) {
        path = base.path;
        if (!r.query.empty()) {
          query = r.query;
        } else if (!base.query.empty()) {
          query = base.query;
        }
      } else {
        if (!r.path.empty() && boost::starts_with(r.path, "/")) {
          path = r.path;
          remove_dot_segments(*this);
        } else {
          merge_path(base, r, *this);
          remove_dot_segments(*this);
        }
        query = r.query;
      }
      /* authority */
      userinfo = base.userinfo;
      host = base.host;
      port = base.port;
    }
    scheme = base.scheme;
  }
  fragment = r.fragment;
}

static std::string char2hex(char c)
{
  std::string result;
  char first, second;

  first = (c & 0xF0) / 16;
  first += first > 9 ? 'A' - 10 : '0';
  second = c & 0x0F;
  second += second > 9 ? 'A' - 10 : '0';

  result.append(1, first);
  result.append(1, second);

  return result;
}

static char hex2char(char first, char second)
{
  int digit;

  digit = (first >= 'A' ? ((first & 0xDF) - 'A') + 10 : (first - '0'));
  digit *= 16;
  digit += (second >= 'A' ? ((second & 0xDF) - 'A') + 10 : (second - '0'));
  return static_cast<char>(digit);
}

/*
   From the HTML standard:
   <http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1>

   application/x-www-form-urlencoded

   This is the default content type. Forms submitted with this content
   type must be encoded as follows:

   1. Control names and values are escaped. Space characters are
   replaced by `+', and then reserved characters are escaped as
   described in [RFC1738], section 2.2: Non-alphanumeric characters
   are replaced by `%HH', a percent sign and two hexadecimal digits
   representing the ASCII code of the character. Line breaks are
   represented as "CR LF" pairs (i.e., `%0D%0A').
   2. The control names/values are listed in the order they appear in
   the document. The name is separated from the value by `=' and
   name/value pairs are separated from each other by `&'.


   Note RFC 1738 is obsoleted by RFC 2396.  Basically it says to
   escape out the reserved characters in the standard %xx format.  It
   also says this about the query string:

   query         = *uric
   uric          = reserved | unreserved | escaped
   reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
   "$" | ","
   unreserved    = alphanum | mark
   mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
   "(" | ")"
   escaped = "%" hex hex
*/

std::string uri::encode(const std::string& src)
{
  std::string result;
  std::string::const_iterator iter;

  for(iter = src.begin(); iter != src.end(); ++iter) {
    switch(*iter) {
    case ' ':
      result.append(1, '+');
      break;
      // alnum
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
    case 'v': case 'w': case 'x': case 'y': case 'z':
    case '0': case '1': case '2': case '3': case '4': case '5': case '6':
    case '7': case '8': case '9':
      // mark
    case '-': case '_': case '.': case '!': case '~': case '*': case '\'':
    case '(': case ')':
      result.append(1, *iter);
      break;
      // escape
    default:
      result.append(1, '%');
      result.append(char2hex(*iter));
      break;
    }
  }

  return result;
}

std::string uri::decode(const std::string& src)
{
  std::string result;
  std::string::const_iterator iter;
  char c;

  for(iter = src.begin(); iter != src.end(); ++iter) {
    switch(*iter) {
    case '+':
      result.append(1, ' ');
      break;
    case '%':
      // Don't assume well-formed input
      if(std::distance(iter, src.end()) >= 2
        && std::isxdigit(*(iter + 1)) && std::isxdigit(*(iter + 2))) {
        c = *++iter;
        result.append(1, hex2char(c, *++iter));
      } else {
        // just pass the % through untouched
        result.append(1, '%');
      }
      break;

    default:
      result.append(1, *iter);
      break;
    }
  }

  return result;
}

uri::query_params uri::parse_query() {
  query_params result;
  if (query.size() <= 1) return result;
  string_splits splits;
  // skip the ? at the start of query
  std::string query_ = query.substr(1);
  boost::split(splits, query_, match_char('&'));
  for (string_splits::iterator i = splits.begin(); i!=splits.end(); ++i) {
    size_t pos = i->find_first_of('=');
    if (pos != std::string::npos) {
      result[i->substr(0, pos)] = decode(i->substr(pos+1));
    } else {
      result[*i] = "";
    }
  }
  return result;
}
