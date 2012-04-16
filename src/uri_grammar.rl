%%{
# translated to ragel from ABNF:
# http://tools.ietf.org/html/rfc3986#appendix-A
  machine uri_grammar;

# adding broken edge cases. see "escape" grammar in http11_parser_common.rl
  pct_encoded = "%%" | ("%" xdigit xdigit?) | ("%u" xdigit xdigit xdigit xdigit) | "%";
  unreserved = alnum | "-" | "." | "_" | "~";
  sub_delims = "!" | "$" | "&" | "'" | "(" | ")"
                   | "*" | "+" | "," | ";" | "=";
  gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@";
  reserved = gen_delims | sub_delims;
# people use |, [, ], {, }, :, ^ and @ in path/query even though they're not in RFC
# which is why they are appended to pchar
  pchar = unreserved | pct_encoded | sub_delims | ":" | "@" | '[' | ']' | '{' | '}' | '|' | '^';
  query = (pchar | "/" | "?")* %query;
  fragment = (pchar | "/" | "?")* %fragment;

  segment = pchar*;
  segment_nz = pchar+;
# NOTE: could implement segment_nz_nc as (pchar - ":") ?
  segment_nz_nc = (unreserved | pct_encoded | sub_delims | "@")+;

  path_abempty = ("/" segment)* %path;
  path_absolute = "/" (segment_nz ("/" segment)*)? %path;
  path_noscheme = segment_nz_nc ("/" segment)* %path;
  path_rootless = segment_nz ("/" segment)* %path;
  path_empty = "" %path;

  path = path_abempty |
    path_absolute |
    path_noscheme |
    path_rootless |
    path_empty;

  reg_name = (unreserved | pct_encoded | sub_delims)*;

  dec_octet = digit | # 0-9
    0x31..0x39 digit | # 10-99
    "1" digit{2} | # 100-199
    "2" 0x30..0x34 digit | # 200-249
    "25" 0x30..0x34; # 250-255

  IPv4address = dec_octet "." dec_octet "." dec_octet "." dec_octet;
  h16 = xdigit{1,4};
  ls32 = (h16 ":" h16) | IPv4address;
  IPv6address =                                 (h16 ":"){6} ls32
                 |                         "::" (h16 ":"){5} ls32
                 | (                h16 )? "::" (h16 ":"){4} ls32
                 | (( h16 ":" ){,1} h16 )? "::" (h16 ":"){3} ls32
                 | (( h16 ":" ){,2} h16 )? "::" (h16 ":"){2} ls32
                 | (( h16 ":" ){,3} h16 )? "::"  h16 ":"     ls32
                 | (( h16 ":" ){,4} h16 )? "::"              ls32
                 | (( h16 ":" ){,5} h16 )? "::"              h16
                 | (( h16 ":" ){,6} h16 )? "::";
  IPvFuture = "v" xdigit{1} "." ( unreserved | sub_delims | ":" ){1};
  IP_literal = "[" ( IPv6address | IPvFuture  ) "]";

  port = digit* >mark %port;
  host = IP_literal | IPv4address | reg_name >mark %host;
  userinfo = (unreserved | pct_encoded | sub_delims | ":")* >mark %userinfo;
  authority = (userinfo "@")? host (":" port)?;

  scheme = alpha >mark (alpha | digit | "+" | "-" | ".")* %scheme;

  relative_part = "//" authority %mark path_abempty |
    path_absolute |
    path_noscheme |
    path_empty;

  hier_part = "//" authority %mark path_abempty
    | path_absolute
    | path_rootless
    | path_empty;

  relative_ref = relative_part ("?" >mark query)? ("#" >mark fragment)?;
  absolute_URI = scheme ":" hier_part ("?" >mark query);
  URI = scheme ":" hier_part ("?" >mark query)? ("#" >mark fragment)?;
  URI_reference = URI | relative_ref;
}%%

