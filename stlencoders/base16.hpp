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

#ifndef STLENCODERS_BASE16_HPP
#define STLENCODERS_BASE16_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
   @brief stlencoders namespace
*/
namespace stlencoders {
    namespace detail {
        template<char C> struct base16_table { enum { value = 0xff }; };

        template<> struct base16_table<'0'> { enum { value = 0x00 }; };
        template<> struct base16_table<'1'> { enum { value = 0x01 }; };
        template<> struct base16_table<'2'> { enum { value = 0x02 }; };
        template<> struct base16_table<'3'> { enum { value = 0x03 }; };
        template<> struct base16_table<'4'> { enum { value = 0x04 }; };
        template<> struct base16_table<'5'> { enum { value = 0x05 }; };
        template<> struct base16_table<'6'> { enum { value = 0x06 }; };
        template<> struct base16_table<'7'> { enum { value = 0x07 }; };
        template<> struct base16_table<'8'> { enum { value = 0x08 }; };
        template<> struct base16_table<'9'> { enum { value = 0x09 }; };
        template<> struct base16_table<'A'> { enum { value = 0x0a }; };
        template<> struct base16_table<'B'> { enum { value = 0x0b }; };
        template<> struct base16_table<'C'> { enum { value = 0x0c }; };
        template<> struct base16_table<'D'> { enum { value = 0x0d }; };
        template<> struct base16_table<'E'> { enum { value = 0x0e }; };
        template<> struct base16_table<'F'> { enum { value = 0x0f }; };

        template<> struct base16_table<'a'> { enum { value = base16_table<'A'>::value }; };
        template<> struct base16_table<'b'> { enum { value = base16_table<'B'>::value }; };
        template<> struct base16_table<'c'> { enum { value = base16_table<'C'>::value }; };
        template<> struct base16_table<'d'> { enum { value = base16_table<'D'>::value }; };
        template<> struct base16_table<'e'> { enum { value = base16_table<'E'>::value }; };
        template<> struct base16_table<'f'> { enum { value = base16_table<'F'>::value }; };
    }

    /**
       @brief base16 traits template
    */
    template<class charT> struct base16_traits;

    /**
       @brief base16 character traits
    */
    template<>
    struct base16_traits<char> {
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
            return "0123456789ABCDEF"[c];
    	}

    	static char_type to_char_type_lower(const int_type& c) {
            return "0123456789abcdef"[c];
        }

    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base16_table>(c, inv());
    	}

        static int_type inv() {
            return detail::base16_table<'\0'>::value;
        }
    };

    /**
       @brief base16 wide character traits
    */
    template<>
    struct base16_traits<wchar_t>
    : public detail::wchar_encoding_traits<base16_traits> {
    };

    /**
       @brief base16 encoder/decoder
    */
    template<class charT, class traits = base16_traits<charT> >
    class base16 {
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
           @brief base16 encode a range of octets

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
                int_type c = *first;
            	*result = traits::to_char_type((c & 0xff) >> 4);
                ++result;
            	*result = traits::to_char_type((c & 0x0f));
                ++result;
            }

            return result;
        }

        /**
           @brief base16 encode a range of octets using the lowercase
           encoding alphabet

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
        static OutputIterator encode_lower(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef lower_encoding_traits<traits> lower_traits;
            return base16<charT, lower_traits>::encode(first, last, result);
        }

        /**
           @brief base16 encode a range of octets using the uppercase
           encoding alphabet

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
        static OutputIterator encode_upper(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef upper_encoding_traits<traits> upper_traits;
            return base16<charT, upper_traits>::encode(first, last, result);
        }

        /**
           @brief base16 decode a range of characters

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
           @brief base16 decode a range of characters

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
                    throw invalid_length("base16 decode error");
                }

                *result = c0 << 4 | c1;
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
            return n * 2;
        }

        /**
           @brief computes the maximum length of a decoded sequence

           @param n the length of the input character sequence

           @return the maximum size of the decoded octet sequence
        */
        template<class sizeT>
        static sizeT max_decode_size(sizeT n) {
            return n / 2;
        }

    private:
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
                } else if (!skip(c)) {
                    throw invalid_character("base16 decode error");
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
                } else {
                    throw invalid_character("base16 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
