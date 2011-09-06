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

#ifndef STLENCODERS_BASE64IMPL_HPP
#define STLENCODERS_BASE64IMPL_HPP

#include "decstr.hpp"
#include "dectbl.hpp"

namespace stlencoders {
    namespace impl {
        template<char C> struct b64 { enum { v = -1 }; };
        template<> struct b64<'A'> { enum { v = 0x00 }; };
        template<> struct b64<'a'> { enum { v = 0x1a }; };
        template<> struct b64<'B'> { enum { v = 0x01 }; };
        template<> struct b64<'b'> { enum { v = 0x1b }; };
        template<> struct b64<'C'> { enum { v = 0x02 }; };
        template<> struct b64<'c'> { enum { v = 0x1c }; };
        template<> struct b64<'D'> { enum { v = 0x03 }; };
        template<> struct b64<'d'> { enum { v = 0x1d }; };
        template<> struct b64<'E'> { enum { v = 0x04 }; };
        template<> struct b64<'e'> { enum { v = 0x1e }; };
        template<> struct b64<'F'> { enum { v = 0x05 }; };
        template<> struct b64<'f'> { enum { v = 0x1f }; };
        template<> struct b64<'G'> { enum { v = 0x06 }; };
        template<> struct b64<'g'> { enum { v = 0x20 }; };
        template<> struct b64<'H'> { enum { v = 0x07 }; };
        template<> struct b64<'h'> { enum { v = 0x21 }; };
        template<> struct b64<'I'> { enum { v = 0x08 }; };
        template<> struct b64<'i'> { enum { v = 0x22 }; };
        template<> struct b64<'J'> { enum { v = 0x09 }; };
        template<> struct b64<'j'> { enum { v = 0x23 }; };
        template<> struct b64<'K'> { enum { v = 0x0a }; };
        template<> struct b64<'k'> { enum { v = 0x24 }; };
        template<> struct b64<'L'> { enum { v = 0x0b }; };
        template<> struct b64<'l'> { enum { v = 0x25 }; };
        template<> struct b64<'M'> { enum { v = 0x0c }; };
        template<> struct b64<'m'> { enum { v = 0x26 }; };
        template<> struct b64<'N'> { enum { v = 0x0d }; };
        template<> struct b64<'n'> { enum { v = 0x27 }; };
        template<> struct b64<'O'> { enum { v = 0x0e }; };
        template<> struct b64<'o'> { enum { v = 0x28 }; };
        template<> struct b64<'P'> { enum { v = 0x0f }; };
        template<> struct b64<'p'> { enum { v = 0x29 }; };
        template<> struct b64<'Q'> { enum { v = 0x10 }; };
        template<> struct b64<'q'> { enum { v = 0x2a }; };
        template<> struct b64<'R'> { enum { v = 0x11 }; };
        template<> struct b64<'r'> { enum { v = 0x2b }; };
        template<> struct b64<'S'> { enum { v = 0x12 }; };
        template<> struct b64<'s'> { enum { v = 0x2c }; };
        template<> struct b64<'T'> { enum { v = 0x13 }; };
        template<> struct b64<'t'> { enum { v = 0x2d }; };
        template<> struct b64<'U'> { enum { v = 0x14 }; };
        template<> struct b64<'u'> { enum { v = 0x2e }; };
        template<> struct b64<'V'> { enum { v = 0x15 }; };
        template<> struct b64<'v'> { enum { v = 0x2f }; };
        template<> struct b64<'W'> { enum { v = 0x16 }; };
        template<> struct b64<'w'> { enum { v = 0x30 }; };
        template<> struct b64<'X'> { enum { v = 0x17 }; };
        template<> struct b64<'x'> { enum { v = 0x31 }; };
        template<> struct b64<'Y'> { enum { v = 0x18 }; };
        template<> struct b64<'y'> { enum { v = 0x32 }; };
        template<> struct b64<'Z'> { enum { v = 0x19 }; };
        template<> struct b64<'z'> { enum { v = 0x33 }; };
        template<> struct b64<'0'> { enum { v = 0x34 }; };
        template<> struct b64<'1'> { enum { v = 0x35 }; };
        template<> struct b64<'2'> { enum { v = 0x36 }; };
        template<> struct b64<'3'> { enum { v = 0x37 }; };
        template<> struct b64<'4'> { enum { v = 0x38 }; };
        template<> struct b64<'5'> { enum { v = 0x39 }; };
        template<> struct b64<'6'> { enum { v = 0x3a }; };
        template<> struct b64<'7'> { enum { v = 0x3b }; };
        template<> struct b64<'8'> { enum { v = 0x3c }; };
        template<> struct b64<'9'> { enum { v = 0x3d }; };
        template<> struct b64<'+'> { enum { v = 0x3e }; };
        template<> struct b64<'/'> { enum { v = 0x3f }; };

        template<char C> struct b64url: public b64<C> { };
        template<> struct b64url<'+'> { enum { v = -1 }; };
        template<> struct b64url<'/'> { enum { v = -1 }; };
        template<> struct b64url<'-'> { enum { v = 0x3e }; };
        template<> struct b64url<'_'> { enum { v = 0x3f }; };
    }
}

#endif
