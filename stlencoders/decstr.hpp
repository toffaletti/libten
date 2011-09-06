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

#ifndef STLENCODERS_DECSTR_HPP
#define STLENCODERS_DECSTR_HPP

#include <cstring>
#include <cwchar>

namespace stlencoders {
    namespace impl {
        inline int decstr(const char* s, char c) {
            if (const char* p = std::strchr(s, c)) {
                return p - s;
            } else {
                return -1;
            }
        }

        inline int decstr(const wchar_t* s, wchar_t c) {
            if (const wchar_t* p = std::wcschr(s, c)) {
                return p - s;
            } else {
                return -1;
            }
    	}

        inline int decistr(const char* s, char c) {
            if (const char* p = std::strchr(s, c)) {
                return (p - s) / 2;
            } else {
                return -1;
            }
        }

        inline int decistr(const wchar_t* s, wchar_t c) {
            if (const wchar_t* p = std::wcschr(s, c)) {
                return (p - s) / 2;
            } else {
                return -1;
            }
    	}
    }
}

#endif
