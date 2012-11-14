#include "ten/term.hh"
#include <sys/ioctl.h>

namespace ten {

unsigned terminal_width() {
#ifdef TIOCGWINSZ
    winsize sz;
    if (ioctl(2, TIOCGWINSZ, &sz) == 0 && sz.ws_col)
        return sz.ws_col;
#endif
    return 80;
}

} // ten
