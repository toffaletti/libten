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

#ifndef STLENCODERS_BASE16IMPL_HPP
#define STLENCODERS_BASE16IMPL_HPP

#include "dectbl.hpp"

namespace stlencoders {
    namespace impl {
        template<char C> struct b16 { enum { v = -1 }; };
        template<> struct b16<'0'> { enum { v = 0x00 }; };
        template<> struct b16<'1'> { enum { v = 0x01 }; };
        template<> struct b16<'2'> { enum { v = 0x02 }; };
        template<> struct b16<'3'> { enum { v = 0x03 }; };
        template<> struct b16<'4'> { enum { v = 0x04 }; };
        template<> struct b16<'5'> { enum { v = 0x05 }; };
        template<> struct b16<'6'> { enum { v = 0x06 }; };
        template<> struct b16<'7'> { enum { v = 0x07 }; };
        template<> struct b16<'8'> { enum { v = 0x08 }; };
        template<> struct b16<'9'> { enum { v = 0x09 }; };
        template<> struct b16<'A'> { enum { v = 0x0a }; };
        template<> struct b16<'a'> { enum { v = 0x0a }; };
        template<> struct b16<'B'> { enum { v = 0x0b }; };
        template<> struct b16<'b'> { enum { v = 0x0b }; };
        template<> struct b16<'C'> { enum { v = 0x0c }; };
        template<> struct b16<'c'> { enum { v = 0x0c }; };
        template<> struct b16<'D'> { enum { v = 0x0d }; };
        template<> struct b16<'d'> { enum { v = 0x0d }; };
        template<> struct b16<'E'> { enum { v = 0x0e }; };
        template<> struct b16<'e'> { enum { v = 0x0e }; };
        template<> struct b16<'F'> { enum { v = 0x0f }; };
        template<> struct b16<'f'> { enum { v = 0x0f }; };
    }
}

#endif
