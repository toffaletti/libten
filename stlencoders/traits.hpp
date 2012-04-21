/*
 * Copyright (c) 2012 Thomas Kemmer <tkemmer@computer.org>
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

#ifndef STLENCODERS_TRAITS_HPP
#define STLENCODERS_TRAITS_HPP

namespace stlencoders {
    namespace detail {
        template<template<class charT> class traits>
        struct wchar_encoding_traits {
            typedef wchar_t char_type;

            typedef typename traits<char>::int_type int_type;

            static bool eq(const char_type& lhs, const char_type& rhs) {
                return lhs == rhs;
            }

            static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
                return traits<char>::eq_int_type(lhs, rhs);
            }

            static char_type to_char_type(const int_type& c) {
                return static_cast<wchar_t>(traits<char>::to_char_type(c));
            }

            static char_type to_char_type_lower(const int_type& c) {
                return static_cast<wchar_t>(traits<char>::to_char_type_lower(c));
            }

            static char_type to_char_type_upper(const int_type& c) {
                return static_cast<wchar_t>(traits<char>::to_char_type_upper(c));
            }

            static int_type to_int_type(const char_type& c) {
                if (c == static_cast<wchar_t>(static_cast<char>(c))) {
                    return traits<char>::to_int_type(static_cast<char>(c));
                } else {
                    return traits<char>::inv();
                }
            }

            static char_type pad() {
                return static_cast<wchar_t>(traits<char>::pad());
            }

            static int_type inv() {
                return traits<char>::inv();
            }
        };
    }

    template<class traits>
    struct lower_encoding_traits : public traits {
        typedef typename traits::char_type char_type;
        typedef typename traits::int_type int_type;

    	static char_type to_char_type(const int_type& c) {
            return traits::to_char_type_lower(c);
        }
    };

    template<class traits>
    struct upper_encoding_traits : public traits {
        typedef typename traits::char_type char_type;
        typedef typename traits::int_type int_type;

    	static char_type to_char_type(const int_type& c) {
            return traits::to_char_type_upper(c);
        }
    };
}

#endif
