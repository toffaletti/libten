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

#ifndef STLENCODERS_BASE32_HPP
#define STLENCODERS_BASE32_HPP

#include "base32impl.hpp"

#include <cwchar>
#include <stdexcept>

/**
   \brief stlencoders namespace
*/
namespace stlencoders {
    template<class charT> struct base32_traits;

    template<> struct base32_traits<char> {
    	typedef char char_type;

     	typedef int int_type;

     	static char_type encode_lower(int_type c) {
            return "abcdefghijklmnopqrstuvwxyz234567"[c];
        }

     	static char_type encode_upper(int_type c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[c];
     	}

     	static int_type decode(char_type c) {
            return impl::dectbl<impl::b32>(c);
     	}

     	static char_type pad() {
            return '=';
        }

        static int_type nchar() {
            return -1;
        }
    };

    template<> struct base32_traits<wchar_t> {
    	typedef wchar_t char_type;

     	typedef int int_type;

     	static char_type encode_lower(int_type c) {
            return L"abcdefghijklmnopqrstuvwxyz234567"[c];
        }

     	static char_type encode_upper(int_type c) {
            return L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[c];
     	}

     	static int_type decode(char_type c) {
            const wchar_t* s = L"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz223344556677";
            if (const wchar_t* p = std::wcschr(s, c)) {
                return (p - s) / 2;
            } else {
                return -1;
            }
     	}

     	static char_type pad() {
            return L'=';
        }

        static int_type nchar() {
            return -1;
        }
    };

    template<class charT> struct base32hex_traits;

    template<> struct base32hex_traits<char> {
    	typedef char char_type;

     	typedef int int_type;

     	static char_type encode_lower(int_type c) {
            return "0123456789abcdefghijklmnopqrstuv"[c];
        }

     	static char_type encode_upper(int_type c) {
            return "0123456789ABCDEFGHIJKLMNOPQRSTUV"[c];
     	}

     	static int_type decode(char_type c) {
            return impl::dectbl<impl::b32hex>(c);
     	}

     	static char_type pad() {
            return '=';
        }

        static int_type nchar() {
            return -1;
        }
    };

    template<> struct base32hex_traits<wchar_t> {
    	typedef wchar_t char_type;

     	typedef int int_type;

     	static char_type encode_lower(int_type c) {
            return L"0123456789abcdefghijklmnopqrstuv"[c];
        }

     	static char_type encode_upper(int_type c) {
            return L"0123456789ABCDEFGHIJKLMNOPQRSTUV"[c];
     	}

     	static int_type decode(char_type c) {
            const wchar_t* s = L"00112233445566778899AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVv";
            if (const wchar_t* p = std::wcschr(s, c)) {
                return (p - s) / 2;
            } else {
                return -1;
            }
     	}

     	static char_type pad() {
            return L'=';
        }

        static int_type nchar() {
            return -1;
        }
    };

    /**
       @brief base32 encoder/decoder
    */
    template<class charT, class traits = base32_traits<charT> > class base32 {
    private:
        struct nskip {
            bool operator()(charT) const { 
                throw std::runtime_error("base32 decode error"); 
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
            	typename traits::int_type c0 = *first++ & 0xff;
                *result++ = enc((c0 >> 3) & 0x1f);

                if (first != last) {
                    typename traits::int_type c1 = *first++ & 0xff;
                    *result++ = enc(((c0 << 2) | (c1 >> 6)) & 0x1f);
                    *result++ = enc((c1 >> 1) & 0x1f);

                    if (first != last) {
                    	typename traits::int_type c2 = *first++ & 0xff;
                        *result++ = enc(((c1 << 4) | (c2 >> 4)) & 0x1f);

                        if (first != last) {
                            typename traits::int_type c3 = *first++ & 0xff;
                            *result++ = enc(((c2 << 1) | (c3 >> 7)) & 0x1f);
                            *result++ = enc((c3 >> 2) & 0x1f);
                    
                            if (first != last) {
                            	typename traits::int_type c4 = *first++ & 0xff;
                                *result++ = enc(((c3 << 3) | (c4 >> 5)) & 0x1f);
                                *result++ = enc(c4 & 0x1f);
                            } else {
                                *result++ = enc((c3 << 3) & 0x1f);
                                *result++ = traits::pad();
                            }
                        } else {
                            *result++ = enc((c2 << 1) & 0x1f);
                            *result++ = traits::pad();
                            *result++ = traits::pad();
                            *result++ = traits::pad();
                        }
                    } else {
                        *result++ = enc((c1 << 4) & 0x1f);
                        *result++ = traits::pad();
                        *result++ = traits::pad();
                        *result++ = traits::pad();
                        *result++ = traits::pad();
                    }
                } else {
                    *result++ = enc((c0 << 2) & 0x1f);
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                    *result++ = traits::pad();
                }
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
           @brief base32 encode a range of bytes

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
        static OutputIterator encode(InputIterator first, InputIterator last, OutputIterator result)
        {
            return encode(first, last, result, default_encoder());
        }

        /**
           @brief base32 encode a range of bytes using the lowercase
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
        static OutputIterator encode_lower(InputIterator first, InputIterator last, OutputIterator result)
        {
            return encode(first, last, result, lower_encoder());
        }

        /**
           @brief base32 encode a range of bytes using the uppercase
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
        static OutputIterator encode_upper(InputIterator first, InputIterator last, OutputIterator result)
        {
            return encode(first, last, result, upper_encoder());
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
            static OutputIterator decode(InputIterator first, InputIterator last, 
                                         OutputIterator result)
        {
            return decode(first, last, result, nskip());
        }

        /**
           @brief base32 decode a range of characters

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
            typename traits::int_type c0, c1, c2, c3, c4, c5, c6, c7;

            outer: while (first != last) {
                if ((c0 = traits::decode(c = *first++)) != traits::nchar()) {
                    while (first != last) {
                        if ((c1 = traits::decode(c = *first++)) != traits::nchar()) {
                            *result++ = ((c0 << 3) | (c1 >> 2));
                            while (first != last) {
                                if ((c2 = traits::decode(c = *first++)) != traits::nchar()) {
                                    while (first != last) {
                                        if ((c3= traits::decode(c = *first++)) != traits::nchar()) {
                                            *result++ = ((c1 << 6) | (c2 << 1) | (c3 >> 4));
                                            while (first != last) {
                                                if ((c4 = traits::decode(c = *first++)) != traits::nchar()) {
                                                    *result++ = ((c3 << 4) | (c4 >> 1));
                                                    while (first != last) {
                                                        if ((c5 = traits::decode(c = *first++)) != traits::nchar()) {
                                                            while (first != last) {
                                                                if ((c6 = traits::decode(c = *first++)) != traits::nchar()) {
                                                                    *result++ = ((c4 << 7) | (c5 << 2) | (c6 >> 3));
                                                                    while (first != last) {
                                                                        if ((c7 = traits::decode(c = *first++)) != traits::nchar()) {
                                                                            *result++ = ((c6 << 5) | c7);
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
    };
}

#endif
