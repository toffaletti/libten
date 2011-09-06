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

#ifndef STLENCODERS_BASE64_HPP
#define STLENCODERS_BASE64_HPP

#include "base64impl.hpp"

#include <stdexcept>

/**
   \brief stlencoders namespace
*/
namespace stlencoders {
    template<class charT> struct base64_traits;

    /**
       @brief base64 character traits
    */
    template<> struct base64_traits<char> {
        typedef char char_type;

    	typedef int int_type;

    	static char_type encode(int_type c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[c];
        }

    	static int_type decode(char_type c) {
            return impl::dectbl<impl::b64>(c);
    	}

    	static char_type pad() {
            return '=';
    	}

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base64 wide character traits
    */
    template<> struct base64_traits<wchar_t> {
        typedef wchar_t char_type;

    	typedef int int_type;

    	static char_type encode(int_type c) {
            return L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[c];
        }

    	static int_type decode(char_type c) {
            return impl::decstr(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", c);
    	}

    	static char_type pad() {
            return L'=';
    	}

        static int_type nchar() {
            return -1;
        }
    };

    template<class charT> struct base64url_traits;

    /**
       @brief base64 url character traits
    */
    template<> struct base64url_traits<char> {
        typedef char char_type;

    	typedef int int_type;

    	static char_type encode(int_type c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[c];
        }

    	static int_type decode(char_type c) {
            return impl::dectbl<impl::b64url>(c);
    	}

    	static char_type pad() {
            return '=';
    	}

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base64 url wide character traits
    */
    template<> struct base64url_traits<wchar_t> {
        typedef wchar_t char_type;

    	typedef int int_type;

    	static char_type encode(int_type c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[c];
        }

    	static int_type decode(char_type c) {
            return impl::decstr(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", c);
    	}

    	static char_type pad() {
            return L'=';
    	}

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base64 encoder/decoder
    */
    template<class charT, class traits = base64_traits<charT> > class base64 {
    private:
        struct nskip {
            bool operator()(charT) const { 
                throw std::runtime_error("base64 decode error"); 
            }
        };

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
        static OutputIterator encode(InputIterator first, InputIterator last, OutputIterator result)
    	{
            while (first != last) {
                typename traits::int_type c0 = *first++ & 0xff;
                *result++ = traits::encode((c0 >> 2) & 0x3f);

                if (first != last) {
                    typename traits::int_type c1 = *first++ & 0xff;
                    *result++ = traits::encode(((c0 << 4) | (c1 >> 4)) & 0x3f);

                    if (first != last) {
                    	typename traits::int_type c2 = *first++ & 0xff;
                        *result++ = traits::encode(((c1 << 2) | (c2 >> 6)) & 0x3f);
                        *result++ = traits::encode(c2 & 0x3f);
                    } else {
                        *result++ = traits::encode((c1 << 2) & 0x3f);
                        *result++ = traits::pad();
                    }
                } else {
                    *result++ = traits::encode((c0 << 4) & 0x3f);
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                }
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
        static OutputIterator decode(InputIterator first, InputIterator last, 
                                     OutputIterator result)
    	{
            return decode(first, last, result, nskip());
        }

        /**
           @brief base64 decode a range of characters

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
            typename traits::int_type c0, c1, c2, c3;

            outer: while (first != last) {
                if ((c0 = traits::decode(c = *first++)) != traits::nchar()) {
                    while (first != last) {
                        if ((c1 = traits::decode(c = *first++)) != traits::nchar()) {
                            *result++ = ((c0 << 2) | (c1 >> 4));
                            while (first != last) {
                                if ((c2 = traits::decode(c = *first++)) != traits::nchar()) {
                                    *result++ = ((c1 << 4) | (c2 >> 2));
                                    while (first != last) {
                                        if ((c3 = traits::decode(c = *first++)) != traits::nchar()) {
                                            *result++ = ((c2 << 6) | c3);
                                            goto outer;
                                        } else if (c != traits::pad() && !ignore(c)) {
                                            return result;
                                        }
                                    }
                                } else if (c != traits::pad() && !ignore(c)) {
                                    return result;
                                }
                            }
                        } else if (c != traits::pad() && !ignore(c)) {
                            return result;
                        }
                    }
                } else if (c != traits::pad() && !ignore(c)) {
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

    };
}

#endif
