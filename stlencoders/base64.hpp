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

#ifndef STLENCODERS_BASE64_HPP
#define STLENCODERS_BASE64_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

#include <iostream>

/**
   \brief stlencoders namespace
*/
namespace stlencoders {
    namespace detail {
        template<char C> struct basic_base64_table { enum { value = 0xff }; };

        template<> struct basic_base64_table<'A'> { enum { value = 0x00 }; };
        template<> struct basic_base64_table<'B'> { enum { value = 0x01 }; };
        template<> struct basic_base64_table<'C'> { enum { value = 0x02 }; };
        template<> struct basic_base64_table<'D'> { enum { value = 0x03 }; };
        template<> struct basic_base64_table<'E'> { enum { value = 0x04 }; };
        template<> struct basic_base64_table<'F'> { enum { value = 0x05 }; };
        template<> struct basic_base64_table<'G'> { enum { value = 0x06 }; };
        template<> struct basic_base64_table<'H'> { enum { value = 0x07 }; };
        template<> struct basic_base64_table<'I'> { enum { value = 0x08 }; };
        template<> struct basic_base64_table<'J'> { enum { value = 0x09 }; };
        template<> struct basic_base64_table<'K'> { enum { value = 0x0a }; };
        template<> struct basic_base64_table<'L'> { enum { value = 0x0b }; };
        template<> struct basic_base64_table<'M'> { enum { value = 0x0c }; };
        template<> struct basic_base64_table<'N'> { enum { value = 0x0d }; };
        template<> struct basic_base64_table<'O'> { enum { value = 0x0e }; };
        template<> struct basic_base64_table<'P'> { enum { value = 0x0f }; };
        template<> struct basic_base64_table<'Q'> { enum { value = 0x10 }; };
        template<> struct basic_base64_table<'R'> { enum { value = 0x11 }; };
        template<> struct basic_base64_table<'S'> { enum { value = 0x12 }; };
        template<> struct basic_base64_table<'T'> { enum { value = 0x13 }; };
        template<> struct basic_base64_table<'U'> { enum { value = 0x14 }; };
        template<> struct basic_base64_table<'V'> { enum { value = 0x15 }; };
        template<> struct basic_base64_table<'W'> { enum { value = 0x16 }; };
        template<> struct basic_base64_table<'X'> { enum { value = 0x17 }; };
        template<> struct basic_base64_table<'Y'> { enum { value = 0x18 }; };
        template<> struct basic_base64_table<'Z'> { enum { value = 0x19 }; };
        template<> struct basic_base64_table<'a'> { enum { value = 0x1a }; };
        template<> struct basic_base64_table<'b'> { enum { value = 0x1b }; };
        template<> struct basic_base64_table<'c'> { enum { value = 0x1c }; };
        template<> struct basic_base64_table<'d'> { enum { value = 0x1d }; };
        template<> struct basic_base64_table<'e'> { enum { value = 0x1e }; };
        template<> struct basic_base64_table<'f'> { enum { value = 0x1f }; };
        template<> struct basic_base64_table<'g'> { enum { value = 0x20 }; };
        template<> struct basic_base64_table<'h'> { enum { value = 0x21 }; };
        template<> struct basic_base64_table<'i'> { enum { value = 0x22 }; };
        template<> struct basic_base64_table<'j'> { enum { value = 0x23 }; };
        template<> struct basic_base64_table<'k'> { enum { value = 0x24 }; };
        template<> struct basic_base64_table<'l'> { enum { value = 0x25 }; };
        template<> struct basic_base64_table<'m'> { enum { value = 0x26 }; };
        template<> struct basic_base64_table<'n'> { enum { value = 0x27 }; };
        template<> struct basic_base64_table<'o'> { enum { value = 0x28 }; };
        template<> struct basic_base64_table<'p'> { enum { value = 0x29 }; };
        template<> struct basic_base64_table<'q'> { enum { value = 0x2a }; };
        template<> struct basic_base64_table<'r'> { enum { value = 0x2b }; };
        template<> struct basic_base64_table<'s'> { enum { value = 0x2c }; };
        template<> struct basic_base64_table<'t'> { enum { value = 0x2d }; };
        template<> struct basic_base64_table<'u'> { enum { value = 0x2e }; };
        template<> struct basic_base64_table<'v'> { enum { value = 0x2f }; };
        template<> struct basic_base64_table<'w'> { enum { value = 0x30 }; };
        template<> struct basic_base64_table<'x'> { enum { value = 0x31 }; };
        template<> struct basic_base64_table<'y'> { enum { value = 0x32 }; };
        template<> struct basic_base64_table<'z'> { enum { value = 0x33 }; };
        template<> struct basic_base64_table<'0'> { enum { value = 0x34 }; };
        template<> struct basic_base64_table<'1'> { enum { value = 0x35 }; };
        template<> struct basic_base64_table<'2'> { enum { value = 0x36 }; };
        template<> struct basic_base64_table<'3'> { enum { value = 0x37 }; };
        template<> struct basic_base64_table<'4'> { enum { value = 0x38 }; };
        template<> struct basic_base64_table<'5'> { enum { value = 0x39 }; };
        template<> struct basic_base64_table<'6'> { enum { value = 0x3a }; };
        template<> struct basic_base64_table<'7'> { enum { value = 0x3b }; };
        template<> struct basic_base64_table<'8'> { enum { value = 0x3c }; };
        template<> struct basic_base64_table<'9'> { enum { value = 0x3d }; };

        template<char C> struct base64_table : public basic_base64_table<C> { };
        template<> struct base64_table<'+'> { enum { value = 0x3e }; };
        template<> struct base64_table<'/'> { enum { value = 0x3f }; };

        template<char C> struct base64url_table : public basic_base64_table<C> { };
        template<> struct base64url_table<'-'> { enum { value = 0x3e }; };
        template<> struct base64url_table<'_'> { enum { value = 0x3f }; };
    }

    /**
       @brief base64 traits template
    */
    template<class charT> struct base64_traits;

    /**
       @brief base64 character traits
    */
    template<>
    struct base64_traits<char> {
        typedef char char_type;

    	typedef unsigned char int_type;

        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

    	static char_type to_char_type(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[c];
        }

    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base64_table>(c, inv());
    	}

        static int_type inv() {
            return detail::base64url_table<'\0'>::value;
        }

    	static char_type pad() {
            return '=';
    	}
    };

    /**
       @brief base64 wide character traits
    */
    template<>
    struct base64_traits<wchar_t>
    : public detail::wchar_encoding_traits<base64_traits> {
    };

    /**
       @brief base64url traits template
    */
    template<class charT> struct base64url_traits;

    /**
       @brief base64url character traits
    */
    template<>
    struct base64url_traits<char> {
        typedef char char_type;

    	typedef unsigned char int_type;

        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

    	static char_type to_char_type(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[c];
        }

    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base64url_table>(c, inv());
    	}

        static int_type inv() {
            return detail::base64url_table<'\0'>::value;
        }

    	static char_type pad() {
            return '=';
    	}
    };

    /**
       @brief base64url wide character traits
    */
    template<>
    struct base64url_traits<wchar_t>
    : public detail::wchar_encoding_traits<base64url_traits> {
    };

    /**
       @brief base64 encoder/decoder
    */
    template<class charT, class traits = base64_traits<charT> >
    class base64 {
    private:
        struct noskip { };

    public:
        /**
           @brief encoding character type
        */
        typedef charT char_type;

        /**
           @brief integral type
        */
        typedef typename traits::int_type int_type;

        /**
           @brief traits type
        */
    	typedef traits traits_type;

        /**
           @brief base64 encode a range of octets

           @param first an input iterator to the first position in the
           octet sequence to be encoded

           @param last an input iterator to the final position in the
           octet sequence to be encoded

           @param result an output iterator to the initial position in
           the destination character sequence

           @return an iterator to the last element of the destination
           sequence
        */
    	template<class InputIterator, class OutputIterator>
        static OutputIterator encode(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            for (; first != last; ++first) {
                int_type c0 = *first;
                *result = traits::to_char_type((c0 & 0xff) >> 2);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c0 & 0x03) << 4);
                    return pad_n(++result, 2);
                }

                int_type c1 = *first;
                *result = traits::to_char_type((c0 & 0x03) << 4 | (c1 & 0xff) >> 4);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c1 & 0x0f) << 2);
                    return pad_n(++result, 1);
                }

                int_type c2 = *first;
                *result = traits::to_char_type((c1 & 0x0f) << 2 | (c2 & 0xff) >> 6);
                ++result;
                *result = traits::to_char_type((c2 & 0x3f));
                ++result;
            }

            return result;
        }

        /**
           @brief base64 decode a range of characters

           @param first an input iterator to the first position in the
           character sequence to be decoded

           @param last an input iterator to the final position in the
           character sequence to be decoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @return an iterator to the last element of the destination
           sequence
        */
    	template<class InputIterator, class OutputIterator>
        static OutputIterator decode(
            InputIterator first, InputIterator last, OutputIterator result
            )
    	{
            return decode(first, last, result, noskip());
        }

        /**
           @brief base64 decode a range of characters

           @param first an input iterator to the first position in the
           character sequence to be decoded

           @param last an input iterator to the final position in the
           character sequence to be decoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @param skip a function object that, when applied to a
           character not in the encoding set, returns true if the
           character is to be ignored, or false if decoding is to be
           stopped by throwing an exception

           @return an iterator to the last element of the destination
           sequence
        */
    	template<class InputIterator, class OutputIterator, class Predicate>
        static OutputIterator decode(
            InputIterator first, InputIterator last, OutputIterator result,
            Predicate skip
            )
        {
            do {
                int_type c0 = seek(first, last, skip);
                if (traits::eq_int_type(c0, traits::inv())) {
                    return result;
                }

                int_type c1 = seek(first, last, skip);
                if (traits::eq_int_type(c1, traits::inv())) {
                    throw invalid_length("base64 decode error");
                }

                *result = c0 << 2 | c1 >> 4;
                ++result;

                int_type c2 = seek(first, last, skip);
                if (traits::eq_int_type(c2, traits::inv())) {
                    return result;
                }

                *result = (c1 & 0x0f) << 4 | c2 >> 2;
                ++result;

                int_type c3 = seek(first, last, skip);
                if (traits::eq_int_type(c3, traits::inv())) {
                    return result;
                }

                *result = (c2 & 0x03) << 6 | c3;
                ++result;
            } while (first != last);

            return result;
        }

        /**
           @brief computes the maximum length of an encoded sequence

           @param n the length of the input octet sequence

           @return the maximum size of the encoded character sequence
        */
        template<class sizeT>
        static sizeT max_encode_size(sizeT n) {
            return (n + 2) / 3 * 4;
        }

        /**
           @brief computes the maximum length of a decoded sequence

           @param n the length of the input character sequence

           @return the maximum size of the decoded octet sequence
        */
        template<class sizeT>
        static sizeT max_decode_size(sizeT n) {
            return (n + 3) / 4 * 3;
        }

    private:
        template<class OutputIterator, class sizeT>
        static OutputIterator pad_n(OutputIterator result, sizeT n) {
            for (; n > 0; --n) {
                *result++ = traits::pad();
            }
            return result;
        }

        template<class InputIterator, class Predicate>
        static int_type seek(
            InputIterator& first, const InputIterator& last, Predicate& skip
            )
        {
            while (first != last) {
                char_type c = *first;
                ++first;

                int_type v = traits::to_int_type(c);
                if (!traits::eq_int_type(v, traits::inv())) {
                    return v;
                } else if (skip(c)) {
                    continue;
                } else if (traits::eq(c, traits::pad())) {
                    break;
                } else {
                    throw invalid_character("base64 decode error");
                }
            }

            // check padding
            for (; first != last; ++first) {
                char_type c = *first;
                if (!traits::eq(c, traits::pad()) && !skip(c)) {
                    throw invalid_padding("base64 decode error");
                }
            }

            return traits::inv();
        }

        template<class InputIterator>
        static int_type seek(
            InputIterator& first, const InputIterator& last, noskip&
            )
        {
            if (first != last) {
                char_type c = *first;
                ++first;

                int_type v = traits::to_int_type(c);
                if (!traits::eq_int_type(v, traits::inv())) {
                    return v;
                } else if (!traits::eq(c, traits::pad())) {
                    throw invalid_character("base64 decode error");
                }
            }

            // check padding
            for (; first != last; ++first) {
                char_type c = *first;
                if (!traits::eq(c, traits::pad())) {
                    throw invalid_padding("base64 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
