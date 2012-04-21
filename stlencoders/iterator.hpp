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

#ifndef STLENCODERS_ITERATOR_HPP
#define STLENCODERS_ITERATOR_HPP

#include <algorithm>
#include <iterator>
#include <string>

namespace stlencoders {
    template<class Iterator, class charT, class traits = std::char_traits<charT> >
    class line_wrap_iterator
    : public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
    public:
        typedef Iterator iterator_type;

        typedef charT char_type;
        typedef traits traits_type;
        typedef typename traits::off_type off_type;

        line_wrap_iterator(Iterator i, off_type n, const charT* s) :
            current(i), pos(0), len(n), endl(s)
        {
        }

        Iterator base() const {
            return current;
        }

        line_wrap_iterator& operator=(char_type c) {
            if (len > 0) {
                if (pos == len) {
                    current = std::copy(endl, endl + traits::length(endl), current);
                    pos = 0;
                }
                ++pos;
            }

            *current = c;
            ++current;
            return *this;
        }

        line_wrap_iterator& operator*() {
            return *this;
        }

        line_wrap_iterator& operator++() {
            return *this;
        }

        line_wrap_iterator& operator++(int) {
            return *this;
        }

    protected:
        Iterator current;

    private:
        off_type pos;
        const off_type len;
        const charT* endl;
    };

    template<class Iterator, class charT, class sizeT>
    inline line_wrap_iterator<Iterator, charT> line_wrapper(
        Iterator i, sizeT n, const charT* s
        )
    {
        return line_wrap_iterator<Iterator, charT>(i, n, s);
    }
}

#endif
