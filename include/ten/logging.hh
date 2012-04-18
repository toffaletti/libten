#ifndef LIBTEN_LOGGING_HH
#define LIBTEN_LOGGING_HH

#include <glog/logging.h>
#include <glog/stl_logging.h>
#include <sstream>

namespace ten {

//! hexify memory
template <typename T> std::string to_hex(T &in) {
  static const char hexs[] = "0123456789ABCDEF";
  std::stringstream ss;
  for (typename T::const_iterator i = in.begin(); i!=in.end(); i++) {
    ss << hexs[(*i>>4) & 0xF];
    ss << hexs[*i & 0x0F];
  }
  return ss.str();
}

} // end namespace ten

#endif // LIBTEN_LOGGING_HH
