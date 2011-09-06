/*
 * Copyright (C) 2011 Thomas Kemmer <tkemmer@computer.org>
 * 
 * http://code.google.com/p/stlencoders/
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef STLENCODERS_BASE32IMPL_HPP
#define STLENCODERS_BASE32IMPL_HPP

#include "dectbl.hpp"

namespace stlencoders {
    namespace impl {
        template<char C> struct b32 { enum { v = -1 }; };
        template<> struct b32<'A'> { enum { v = 0x00 }; };
        template<> struct b32<'a'> { enum { v = 0x00 }; };
        template<> struct b32<'B'> { enum { v = 0x01 }; };
        template<> struct b32<'b'> { enum { v = 0x01 }; };
        template<> struct b32<'C'> { enum { v = 0x02 }; };
        template<> struct b32<'c'> { enum { v = 0x02 }; };
        template<> struct b32<'D'> { enum { v = 0x03 }; };
        template<> struct b32<'d'> { enum { v = 0x03 }; };
        template<> struct b32<'E'> { enum { v = 0x04 }; };
        template<> struct b32<'e'> { enum { v = 0x04 }; };
        template<> struct b32<'F'> { enum { v = 0x05 }; };
        template<> struct b32<'f'> { enum { v = 0x05 }; };
        template<> struct b32<'G'> { enum { v = 0x06 }; };
        template<> struct b32<'g'> { enum { v = 0x06 }; };
        template<> struct b32<'H'> { enum { v = 0x07 }; };
        template<> struct b32<'h'> { enum { v = 0x07 }; };
        template<> struct b32<'I'> { enum { v = 0x08 }; };
        template<> struct b32<'i'> { enum { v = 0x08 }; };
        template<> struct b32<'J'> { enum { v = 0x09 }; };
        template<> struct b32<'j'> { enum { v = 0x09 }; };
        template<> struct b32<'K'> { enum { v = 0x0a }; };
        template<> struct b32<'k'> { enum { v = 0x0a }; };
        template<> struct b32<'L'> { enum { v = 0x0b }; };
        template<> struct b32<'l'> { enum { v = 0x0b }; };
        template<> struct b32<'M'> { enum { v = 0x0c }; };
        template<> struct b32<'m'> { enum { v = 0x0c }; };
        template<> struct b32<'N'> { enum { v = 0x0d }; };
        template<> struct b32<'n'> { enum { v = 0x0d }; };
        template<> struct b32<'O'> { enum { v = 0x0e }; };
        template<> struct b32<'o'> { enum { v = 0x0e }; };
        template<> struct b32<'P'> { enum { v = 0x0f }; };
        template<> struct b32<'p'> { enum { v = 0x0f }; };
        template<> struct b32<'Q'> { enum { v = 0x10 }; };
        template<> struct b32<'q'> { enum { v = 0x10 }; };
        template<> struct b32<'R'> { enum { v = 0x11 }; };
        template<> struct b32<'r'> { enum { v = 0x11 }; };
        template<> struct b32<'S'> { enum { v = 0x12 }; };
        template<> struct b32<'s'> { enum { v = 0x12 }; };
        template<> struct b32<'T'> { enum { v = 0x13 }; };
        template<> struct b32<'t'> { enum { v = 0x13 }; };
        template<> struct b32<'U'> { enum { v = 0x14 }; };
        template<> struct b32<'u'> { enum { v = 0x14 }; };
        template<> struct b32<'V'> { enum { v = 0x15 }; };
        template<> struct b32<'v'> { enum { v = 0x15 }; };
        template<> struct b32<'W'> { enum { v = 0x16 }; };
        template<> struct b32<'w'> { enum { v = 0x16 }; };
        template<> struct b32<'X'> { enum { v = 0x17 }; };
        template<> struct b32<'x'> { enum { v = 0x17 }; };
        template<> struct b32<'Y'> { enum { v = 0x18 }; };
        template<> struct b32<'y'> { enum { v = 0x18 }; };
        template<> struct b32<'Z'> { enum { v = 0x19 }; };
        template<> struct b32<'z'> { enum { v = 0x19 }; };
        template<> struct b32<'2'> { enum { v = 0x1a }; };
        template<> struct b32<'3'> { enum { v = 0x1b }; };
        template<> struct b32<'4'> { enum { v = 0x1c }; };
        template<> struct b32<'5'> { enum { v = 0x1d }; };
        template<> struct b32<'6'> { enum { v = 0x1e }; };
        template<> struct b32<'7'> { enum { v = 0x1f }; };

        template<char C> struct b32hex { enum { v = -1 }; };
        template<> struct b32hex<'0'> { enum { v = 0x00 }; };
        template<> struct b32hex<'1'> { enum { v = 0x01 }; };
        template<> struct b32hex<'2'> { enum { v = 0x02 }; };
        template<> struct b32hex<'3'> { enum { v = 0x03 }; };
        template<> struct b32hex<'4'> { enum { v = 0x04 }; };
        template<> struct b32hex<'5'> { enum { v = 0x05 }; };
        template<> struct b32hex<'6'> { enum { v = 0x06 }; };
        template<> struct b32hex<'7'> { enum { v = 0x07 }; };
        template<> struct b32hex<'8'> { enum { v = 0x08 }; };
        template<> struct b32hex<'9'> { enum { v = 0x09 }; };
        template<> struct b32hex<'A'> { enum { v = 0x0a }; };
        template<> struct b32hex<'a'> { enum { v = 0x0a }; };
        template<> struct b32hex<'B'> { enum { v = 0x0b }; };
        template<> struct b32hex<'b'> { enum { v = 0x0b }; };
        template<> struct b32hex<'C'> { enum { v = 0x0c }; };
        template<> struct b32hex<'c'> { enum { v = 0x0c }; };
        template<> struct b32hex<'D'> { enum { v = 0x0d }; };
        template<> struct b32hex<'d'> { enum { v = 0x0d }; };
        template<> struct b32hex<'E'> { enum { v = 0x0e }; };
        template<> struct b32hex<'e'> { enum { v = 0x0e }; };
        template<> struct b32hex<'F'> { enum { v = 0x0f }; };
        template<> struct b32hex<'f'> { enum { v = 0x0f }; };
        template<> struct b32hex<'G'> { enum { v = 0x10 }; };
        template<> struct b32hex<'g'> { enum { v = 0x10 }; };
        template<> struct b32hex<'H'> { enum { v = 0x11 }; };
        template<> struct b32hex<'h'> { enum { v = 0x11 }; };
        template<> struct b32hex<'I'> { enum { v = 0x12 }; };
        template<> struct b32hex<'i'> { enum { v = 0x12 }; };
        template<> struct b32hex<'J'> { enum { v = 0x13 }; };
        template<> struct b32hex<'j'> { enum { v = 0x13 }; };
        template<> struct b32hex<'K'> { enum { v = 0x14 }; };
        template<> struct b32hex<'k'> { enum { v = 0x14 }; };
        template<> struct b32hex<'L'> { enum { v = 0x15 }; };
        template<> struct b32hex<'l'> { enum { v = 0x15 }; };
        template<> struct b32hex<'M'> { enum { v = 0x16 }; };
        template<> struct b32hex<'m'> { enum { v = 0x16 }; };
        template<> struct b32hex<'N'> { enum { v = 0x17 }; };
        template<> struct b32hex<'n'> { enum { v = 0x17 }; };
        template<> struct b32hex<'O'> { enum { v = 0x18 }; };
        template<> struct b32hex<'o'> { enum { v = 0x18 }; };
        template<> struct b32hex<'P'> { enum { v = 0x19 }; };
        template<> struct b32hex<'p'> { enum { v = 0x19 }; };
        template<> struct b32hex<'Q'> { enum { v = 0x1a }; };
        template<> struct b32hex<'q'> { enum { v = 0x1a }; };
        template<> struct b32hex<'R'> { enum { v = 0x1b }; };
        template<> struct b32hex<'r'> { enum { v = 0x1b }; };
        template<> struct b32hex<'S'> { enum { v = 0x1c }; };
        template<> struct b32hex<'s'> { enum { v = 0x1c }; };
        template<> struct b32hex<'T'> { enum { v = 0x1d }; };
        template<> struct b32hex<'t'> { enum { v = 0x1d }; };
        template<> struct b32hex<'U'> { enum { v = 0x1e }; };
        template<> struct b32hex<'u'> { enum { v = 0x1e }; };
        template<> struct b32hex<'V'> { enum { v = 0x1f }; };
        template<> struct b32hex<'v'> { enum { v = 0x1f }; };
    }
}

#endif
