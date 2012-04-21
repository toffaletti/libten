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

#ifndef STLENCODERS_BASE32_HPP
#define STLENCODERS_BASE32_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
   \brief stlencoders namespace
*/
namespace stlencoders {
    namespace detail {
        template<char C> struct base32_table { enum { value = 0xff }; };

        template<> struct base32_table<'A'> { enum { value = 0x00 }; };
        template<> struct base32_table<'B'> { enum { value = 0x01 }; };
        template<> struct base32_table<'C'> { enum { value = 0x02 }; };
        template<> struct base32_table<'D'> { enum { value = 0x03 }; };
        template<> struct base32_table<'E'> { enum { value = 0x04 }; };
        template<> struct base32_table<'F'> { enum { value = 0x05 }; };
        template<> struct base32_table<'G'> { enum { value = 0x06 }; };
        template<> struct base32_table<'H'> { enum { value = 0x07 }; };
        template<> struct base32_table<'I'> { enum { value = 0x08 }; };
        template<> struct base32_table<'J'> { enum { value = 0x09 }; };
        template<> struct base32_table<'K'> { enum { value = 0x0a }; };
        template<> struct base32_table<'L'> { enum { value = 0x0b }; };
        template<> struct base32_table<'M'> { enum { value = 0x0c }; };
        template<> struct base32_table<'N'> { enum { value = 0x0d }; };
        template<> struct base32_table<'O'> { enum { value = 0x0e }; };
        template<> struct base32_table<'P'> { enum { value = 0x0f }; };
        template<> struct base32_table<'Q'> { enum { value = 0x10 }; };
        template<> struct base32_table<'R'> { enum { value = 0x11 }; };
        template<> struct base32_table<'S'> { enum { value = 0x12 }; };
        template<> struct base32_table<'T'> { enum { value = 0x13 }; };
        template<> struct base32_table<'U'> { enum { value = 0x14 }; };
        template<> struct base32_table<'V'> { enum { value = 0x15 }; };
        template<> struct base32_table<'W'> { enum { value = 0x16 }; };
        template<> struct base32_table<'X'> { enum { value = 0x17 }; };
        template<> struct base32_table<'Y'> { enum { value = 0x18 }; };
        template<> struct base32_table<'Z'> { enum { value = 0x19 }; };
        template<> struct base32_table<'2'> { enum { value = 0x1a }; };
        template<> struct base32_table<'3'> { enum { value = 0x1b }; };
        template<> struct base32_table<'4'> { enum { value = 0x1c }; };
        template<> struct base32_table<'5'> { enum { value = 0x1d }; };
        template<> struct base32_table<'6'> { enum { value = 0x1e }; };
        template<> struct base32_table<'7'> { enum { value = 0x1f }; };

        template<> struct base32_table<'a'> { enum { value = base32_table<'A'>::value }; };
        template<> struct base32_table<'b'> { enum { value = base32_table<'B'>::value }; };
        template<> struct base32_table<'c'> { enum { value = base32_table<'C'>::value }; };
        template<> struct base32_table<'d'> { enum { value = base32_table<'D'>::value }; };
        template<> struct base32_table<'e'> { enum { value = base32_table<'E'>::value }; };
        template<> struct base32_table<'f'> { enum { value = base32_table<'F'>::value }; };
        template<> struct base32_table<'g'> { enum { value = base32_table<'G'>::value }; };
        template<> struct base32_table<'h'> { enum { value = base32_table<'H'>::value }; };
        template<> struct base32_table<'i'> { enum { value = base32_table<'I'>::value }; };
        template<> struct base32_table<'j'> { enum { value = base32_table<'J'>::value }; };
        template<> struct base32_table<'k'> { enum { value = base32_table<'K'>::value }; };
        template<> struct base32_table<'l'> { enum { value = base32_table<'L'>::value }; };
        template<> struct base32_table<'m'> { enum { value = base32_table<'M'>::value }; };
        template<> struct base32_table<'n'> { enum { value = base32_table<'N'>::value }; };
        template<> struct base32_table<'o'> { enum { value = base32_table<'O'>::value }; };
        template<> struct base32_table<'p'> { enum { value = base32_table<'P'>::value }; };
        template<> struct base32_table<'q'> { enum { value = base32_table<'Q'>::value }; };
        template<> struct base32_table<'r'> { enum { value = base32_table<'R'>::value }; };
        template<> struct base32_table<'s'> { enum { value = base32_table<'S'>::value }; };
        template<> struct base32_table<'t'> { enum { value = base32_table<'T'>::value }; };
        template<> struct base32_table<'u'> { enum { value = base32_table<'U'>::value }; };
        template<> struct base32_table<'v'> { enum { value = base32_table<'V'>::value }; };
        template<> struct base32_table<'w'> { enum { value = base32_table<'W'>::value }; };
        template<> struct base32_table<'x'> { enum { value = base32_table<'X'>::value }; };
        template<> struct base32_table<'y'> { enum { value = base32_table<'Y'>::value }; };
        template<> struct base32_table<'z'> { enum { value = base32_table<'Z'>::value }; };

        template<char C> struct base32hex_table { enum { value = 0xff }; };

        template<> struct base32hex_table<'0'> { enum { value = 0x00 }; };
        template<> struct base32hex_table<'1'> { enum { value = 0x01 }; };
        template<> struct base32hex_table<'2'> { enum { value = 0x02 }; };
        template<> struct base32hex_table<'3'> { enum { value = 0x03 }; };
        template<> struct base32hex_table<'4'> { enum { value = 0x04 }; };
        template<> struct base32hex_table<'5'> { enum { value = 0x05 }; };
        template<> struct base32hex_table<'6'> { enum { value = 0x06 }; };
        template<> struct base32hex_table<'7'> { enum { value = 0x07 }; };
        template<> struct base32hex_table<'8'> { enum { value = 0x08 }; };
        template<> struct base32hex_table<'9'> { enum { value = 0x09 }; };
        template<> struct base32hex_table<'A'> { enum { value = 0x0a }; };
        template<> struct base32hex_table<'B'> { enum { value = 0x0b }; };
        template<> struct base32hex_table<'C'> { enum { value = 0x0c }; };
        template<> struct base32hex_table<'D'> { enum { value = 0x0d }; };
        template<> struct base32hex_table<'E'> { enum { value = 0x0e }; };
        template<> struct base32hex_table<'F'> { enum { value = 0x0f }; };
        template<> struct base32hex_table<'G'> { enum { value = 0x10 }; };
        template<> struct base32hex_table<'H'> { enum { value = 0x11 }; };
        template<> struct base32hex_table<'I'> { enum { value = 0x12 }; };
        template<> struct base32hex_table<'J'> { enum { value = 0x13 }; };
        template<> struct base32hex_table<'K'> { enum { value = 0x14 }; };
        template<> struct base32hex_table<'L'> { enum { value = 0x15 }; };
        template<> struct base32hex_table<'M'> { enum { value = 0x16 }; };
        template<> struct base32hex_table<'N'> { enum { value = 0x17 }; };
        template<> struct base32hex_table<'O'> { enum { value = 0x18 }; };
        template<> struct base32hex_table<'P'> { enum { value = 0x19 }; };
        template<> struct base32hex_table<'Q'> { enum { value = 0x1a }; };
        template<> struct base32hex_table<'R'> { enum { value = 0x1b }; };
        template<> struct base32hex_table<'S'> { enum { value = 0x1c }; };
        template<> struct base32hex_table<'T'> { enum { value = 0x1d }; };
        template<> struct base32hex_table<'U'> { enum { value = 0x1e }; };
        template<> struct base32hex_table<'V'> { enum { value = 0x1f }; };

        template<> struct base32hex_table<'a'> { enum { value = base32hex_table<'A'>::value }; };
        template<> struct base32hex_table<'b'> { enum { value = base32hex_table<'B'>::value }; };
        template<> struct base32hex_table<'c'> { enum { value = base32hex_table<'C'>::value }; };
        template<> struct base32hex_table<'d'> { enum { value = base32hex_table<'D'>::value }; };
        template<> struct base32hex_table<'e'> { enum { value = base32hex_table<'E'>::value }; };
        template<> struct base32hex_table<'f'> { enum { value = base32hex_table<'F'>::value }; };
        template<> struct base32hex_table<'g'> { enum { value = base32hex_table<'G'>::value }; };
        template<> struct base32hex_table<'h'> { enum { value = base32hex_table<'H'>::value }; };
        template<> struct base32hex_table<'i'> { enum { value = base32hex_table<'I'>::value }; };
        template<> struct base32hex_table<'j'> { enum { value = base32hex_table<'J'>::value }; };
        template<> struct base32hex_table<'k'> { enum { value = base32hex_table<'K'>::value }; };
        template<> struct base32hex_table<'l'> { enum { value = base32hex_table<'L'>::value }; };
        template<> struct base32hex_table<'m'> { enum { value = base32hex_table<'M'>::value }; };
        template<> struct base32hex_table<'n'> { enum { value = base32hex_table<'N'>::value }; };
        template<> struct base32hex_table<'o'> { enum { value = base32hex_table<'O'>::value }; };
        template<> struct base32hex_table<'p'> { enum { value = base32hex_table<'P'>::value }; };
        template<> struct base32hex_table<'q'> { enum { value = base32hex_table<'Q'>::value }; };
        template<> struct base32hex_table<'r'> { enum { value = base32hex_table<'R'>::value }; };
        template<> struct base32hex_table<'s'> { enum { value = base32hex_table<'S'>::value }; };
        template<> struct base32hex_table<'t'> { enum { value = base32hex_table<'T'>::value }; };
        template<> struct base32hex_table<'u'> { enum { value = base32hex_table<'U'>::value }; };
        template<> struct base32hex_table<'v'> { enum { value = base32hex_table<'V'>::value }; };
    }

    template<class charT> struct base32_traits;

    template<>
    struct base32_traits<char> {
    	typedef char char_type;

     	typedef unsigned char int_type;

        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

    	static char_type to_char_type(const int_type& c) {
            return to_char_type_upper(c);
        }

    	static char_type to_char_type_upper(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[c];
    	}

    	static char_type to_char_type_lower(const int_type& c) {
            return "abcdefghijklmnopqrstuvwxyz234567"[c];
        }

    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base32_table>(c, inv());
    	}

        static int_type inv() {
            return detail::base32_table<'\0'>::value;
        }

     	static char_type pad() {
            return '=';
        }
    };

    template<>
    struct base32_traits<wchar_t>
    : public detail::wchar_encoding_traits<base32_traits> {
    };

    template<class charT> struct base32hex_traits;

    template<>
    struct base32hex_traits<char> {
    	typedef char char_type;

     	typedef unsigned char int_type;

        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

    	static char_type to_char_type(const int_type& c) {
            return to_char_type_upper(c);
        }

    	static char_type to_char_type_upper(const int_type& c) {
            return "0123456789ABCDEFGHIJKLMNOPQRSTUV"[c];
    	}

    	static char_type to_char_type_lower(const int_type& c) {
            return "0123456789abcdefghijklmnopqrstuv"[c];
        }

    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base32hex_table>(c, inv());
    	}

        static int_type inv() {
            return detail::base32hex_table<'\0'>::value;
        }

     	static char_type pad() {
            return '=';
        }
    };

    template<>
    struct base32hex_traits<wchar_t>
    : detail::wchar_encoding_traits<base32hex_traits> {
    };

    /**
       @brief base32 encoder/decoder
    */
    template<class charT, class traits = base32_traits<charT> >
    class base32 {
    private:
        struct noskip { };

    public:
        /**
           @brief encoding character type
        */
        typedef charT char_type;

        /**
           @brief octet type
        */
        typedef typename traits::int_type int_type;

        /**
           @brief traits type
        */
    	typedef traits traits_type;

        /**
           @brief base32 encode a range of bytes

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

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
                *result = traits::to_char_type((c0 & 0xff) >> 3);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c0 & 0x07) << 2);
                    return pad_n(++result, 6);
                }

                int_type c1 = *first;
                *result = traits::to_char_type((c0 & 0x07) << 2 | (c1 & 0xff) >> 6);
                ++result;
                *result = traits::to_char_type((c1 & 0x3f) >> 1);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c1 & 0x01) << 4);
                    return pad_n(++result, 4);
                }

                int_type c2 = *first;
                *result = traits::to_char_type((c1 & 0x01) << 4 | (c2 & 0xff) >> 4);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c2 & 0x0f) << 1);
                    return pad_n(++result, 3);
                }

                int_type c3 = *first;
                *result = traits::to_char_type((c2 & 0x0f) << 1 | (c3 & 0xff) >> 7);
                ++result;
                *result = traits::to_char_type((c3 & 0x7f) >> 2);
                ++result;

                if (++first == last) {
                    *result = traits::to_char_type((c3 & 0x03) << 3);
                    return pad_n(++result, 1);
                }

                int_type c4 = *first;
                *result = traits::to_char_type((c3 & 0x03) << 3 | (c4 & 0xff) >> 5);
                ++result;
                *result = traits::to_char_type((c4 & 0x1f));
                ++result;
            }

            return result;
        }

        /**
           @brief base32 encode a range of bytes using the lowercase
           encoding alphabet

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

           @param result an output iterator to the initial position in
           the destination character sequence

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_lower(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef lower_encoding_traits<traits> lower_traits;
            return base32<charT, lower_traits>::encode(first, last, result);
        }

        /**
           @brief base32 encode a range of bytes using the uppercase
           encoding alphabet

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

           @param result an output iterator to the initial position in
           the destination character sequence

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_upper(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef upper_encoding_traits<traits> upper_traits;
            return base32<charT, upper_traits>::encode(first, last, result);
        }

        /**
           @brief base32 decode a range of characters

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
           @brief base32 decode a range of characters

           @param first an input iterator to the first position in the
           character sequence to be decoded

           @param last an input iterator to the final position in the
           character sequence to be decoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @param skip a function object that, when applied to a
           character not in the encoding set, returns true if the
           character is to be ignored, or false to stop decoding by
           throwing an exception

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator, class Predicate>
        static OutputIterator decode(
            InputIterator first, InputIterator last, OutputIterator result,
            Predicate skip)
        {
            do {
                int_type c0 = seek(first, last, skip);
                if (traits::eq_int_type(c0, traits::inv())) {
                    return result;
                }

                int_type c1 = seek(first, last, skip);
                if (traits::eq_int_type(c1, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = c0 << 3 | c1 >> 2;
                ++result;

                int_type c2 = seek(first, last, skip);
                if (traits::eq_int_type(c2, traits::inv())) {
                    return result;
                }

                int_type c3 = seek(first, last, skip);
                if (traits::eq_int_type(c3, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = (c1 & 0x03) << 6 | c2 << 1 | c3 >> 4;
                ++result;

                int_type c4 = seek(first, last, skip);
                if (traits::eq_int_type(c4, traits::inv())) {
                    return result;
                }

                *result = (c3 & 0x0f) << 4 | c4 >> 1;
                ++result;

                int_type c5 = seek(first, last, skip);
                if (traits::eq_int_type(c5, traits::inv())) {
                    return result;
                }

                int_type c6 = seek(first, last, skip);
                if (traits::eq_int_type(c6, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = (c4 & 0x01) << 7 | c5 << 2 | c6 >> 3;
                ++result;

                int_type c7 = seek(first, last, skip);
                if (traits::eq_int_type(c7, traits::inv())) {
                    return result;
                }

                *result = (c6 & 0x07) << 5 | c7;
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
            return (n + 4) / 5 * 8;
        }

        /**
           @brief computes the maximum length of a decoded sequence

           @param n the length of the input character sequence

           @return the maximum size of the decoded octet sequence
        */
        template<class sizeT>
        static sizeT max_decode_size(sizeT n) {
            return (n + 7) / 8 * 5;
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
                    throw invalid_character("base32 decode error");
                }
            }

            // check padding
            for (; first != last; ++first) {
                char_type c = *first;
                if (!traits::eq(c, traits::pad()) && !skip(c)) {
                    throw invalid_padding("base32 decode error");
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
                    throw invalid_character("base32 decode error");
                }
            }

            // check padding
            for (; first != last; ++first) {
                char_type c = *first;
                if (!traits::eq(c, traits::pad())) {
                    throw invalid_padding("base32 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
