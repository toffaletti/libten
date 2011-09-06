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

#ifndef STLENCODERS_BASE16_HPP
#define STLENCODERS_BASE16_HPP

#include "base16impl.hpp"

#include <cwchar>
#include <stdexcept>

/**
   \brief stlencoders namespace
*/
namespace stlencoders {
    template<class charT> struct base16_traits;

    /**
       @brief base16 character traits
    */
    template<> struct base16_traits<char> {
        typedef char char_type;

    	typedef int int_type;

    	static char_type encode_lower(int_type c) {
            return "0123456789abcdef"[c];
        }

    	static char_type encode_upper(int_type c) {
            return "0123456789ABCDEF"[c];
    	}

    	static int_type decode(char_type c) {
            return impl::dectbl<impl::b16>(c);
    	}

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base16 wide character traits
    */
    template<> struct base16_traits<wchar_t> {
        typedef wchar_t char_type;

    	typedef int int_type;

    	static char_type encode_lower(int_type c) {
            return L"0123456789abcdef"[c];
        }

    	static char_type encode_upper(int_type c) {
            return L"0123456789ABCDEF"[c];
    	}

    	static int_type decode(char_type c) {
            const wchar_t* s = L"00112233445566778899AaBbCcDdEeFf";
            if (const wchar_t* p = std::wcschr(s, c)) {
                return (p - s) / 2;
            } else {
                return -1;
            }
    	}

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base16 encoder/decoder
    */
    template<class charT, class traits = base16_traits<charT> > 
    class base16 {
    private:
        struct nskip {
            bool operator()(charT) const { 
                throw std::runtime_error("base16 decode error"); 
            }
        };

        struct lower_encoder {
            charT operator()(typename traits::int_type c) const { 
                return traits::encode_lower(c); 
            }
        };

        struct upper_encoder {
            charT operator()(typename traits::int_type c) const { 
                return traits::encode_upper(c); 
            }
        };

#ifdef STLENCODERS_DEFAULT_ENCODE_UPPER
        typedef upper_encoder default_encoder;
#else
        typedef lower_encoder default_encoder;
#endif

        template<class InputIterator, class OutputIterator, class Encoder>
        static OutputIterator encode(InputIterator first, InputIterator last, 
                                     OutputIterator result, Encoder enc)
        {
            while (first != last) {
            	typename traits::int_type c = *first++ & 0xff;
            	*result++ = enc((c >> 4) & 0x0f);
            	*result++ = enc(c & 0x0f);
            }

            return result;
        }

    public:
        /**
           @brief encoding character type
        */
        typedef charT char_type;

        /**
           @brief traits type
        */
    	typedef traits traits_type;

        /**
           @brief base16 encode a range of bytes

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode(InputIterator first, InputIterator last, 
                                     OutputIterator result)
        {
            return encode(first, last, result, default_encoder());
        }

        /**
           @brief base16 encode a range of bytes using the lowercase
           encoding alphabet

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_lower(InputIterator first, InputIterator last, 
                                           OutputIterator result)
        {
            return encode(first, last, result, lower_encoder());
        }

        /**
           @brief base16 encode a range of bytes using the uppercase
           encoding alphabet

           @param first an input iterator to the first position in the
           byte sequence to be encoded

           @param last an input iterator to the final position in the
           byte sequence to be encoded

           @param result an output iterator to the initial position in
           the destination octet sequence

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_upper(InputIterator first, InputIterator last, 
                                           OutputIterator result)
        {
            return encode(first, last, result, upper_encoder());
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
            static OutputIterator decode(InputIterator first, InputIterator last, 
                                         OutputIterator result)
        {
            return decode(first, last, result, nskip());
        }

        /**
           @brief base16 decode a range of characters

           @param first an input iterator to the first position in the
           character sequence to be decoded

           @param last an input iterator to the final position in the
           character sequence to be decoded

           @param result an output iterator to the initial position in
           the destination octet sequence
           
           @param ignore a function object that, when applied to a
           character not in the encoding set, returns true if the
           character is to be ignored, false if decoding is to be
           stopped, or throws an exception which is propagated to the
           caller

           @return an iterator to the last element of the destination
           sequence
        */
        template<class InputIterator, class OutputIterator, class Predicate>
        static OutputIterator decode(InputIterator first, InputIterator last, 
                                     OutputIterator result, Predicate ignore)
        {
            typename traits::char_type c;
            typename traits::int_type c0, c1;

            outer: while (first != last) {
                if ((c0 = traits::decode(c = *first++)) != traits::nchar()) {
                    while (first != last) {
                        if ((c1 = traits::decode(c = *first++)) != traits::nchar()) {
                            *result++ = (c0 << 4) | c1;
                            goto outer;
                        } else if (!ignore(c)) {
                            return result;
                        } 
                    }
                } else if (!ignore(c)) {
                    return result;
                }
            }

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
    };
}

#endif
