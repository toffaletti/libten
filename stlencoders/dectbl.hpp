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

#ifndef STLENCODERS_DECTBL_HPP
#define STLENCODERS_DECTBL_HPP

namespace stlencoders {
    namespace impl {
        template<template<char> class T> inline int dectbl(char c) {
            static const int table[] = {
                T<'\000'>::v, T<'\001'>::v, T<'\002'>::v, T<'\003'>::v,
                T<'\004'>::v, T<'\005'>::v, T<'\006'>::v, T<'\007'>::v,
                T<'\010'>::v, T<'\011'>::v, T<'\012'>::v, T<'\013'>::v,
                T<'\014'>::v, T<'\015'>::v, T<'\016'>::v, T<'\017'>::v,
                T<'\020'>::v, T<'\021'>::v, T<'\022'>::v, T<'\023'>::v,
                T<'\024'>::v, T<'\025'>::v, T<'\026'>::v, T<'\027'>::v,
                T<'\030'>::v, T<'\031'>::v, T<'\032'>::v, T<'\033'>::v,
                T<'\034'>::v, T<'\035'>::v, T<'\036'>::v, T<'\037'>::v,
                T<'\040'>::v, T<'\041'>::v, T<'\042'>::v, T<'\043'>::v,
                T<'\044'>::v, T<'\045'>::v, T<'\046'>::v, T<'\047'>::v,
                T<'\050'>::v, T<'\051'>::v, T<'\052'>::v, T<'\053'>::v,
                T<'\054'>::v, T<'\055'>::v, T<'\056'>::v, T<'\057'>::v,
                T<'\060'>::v, T<'\061'>::v, T<'\062'>::v, T<'\063'>::v,
                T<'\064'>::v, T<'\065'>::v, T<'\066'>::v, T<'\067'>::v,
                T<'\070'>::v, T<'\071'>::v, T<'\072'>::v, T<'\073'>::v,
                T<'\074'>::v, T<'\075'>::v, T<'\076'>::v, T<'\077'>::v,
                T<'\100'>::v, T<'\101'>::v, T<'\102'>::v, T<'\103'>::v,
                T<'\104'>::v, T<'\105'>::v, T<'\106'>::v, T<'\107'>::v,
                T<'\110'>::v, T<'\111'>::v, T<'\112'>::v, T<'\113'>::v,
                T<'\114'>::v, T<'\115'>::v, T<'\116'>::v, T<'\117'>::v,
                T<'\120'>::v, T<'\121'>::v, T<'\122'>::v, T<'\123'>::v,
                T<'\124'>::v, T<'\125'>::v, T<'\126'>::v, T<'\127'>::v,
                T<'\130'>::v, T<'\131'>::v, T<'\132'>::v, T<'\133'>::v,
                T<'\134'>::v, T<'\135'>::v, T<'\136'>::v, T<'\137'>::v,
                T<'\140'>::v, T<'\141'>::v, T<'\142'>::v, T<'\143'>::v,
                T<'\144'>::v, T<'\145'>::v, T<'\146'>::v, T<'\147'>::v,
                T<'\150'>::v, T<'\151'>::v, T<'\152'>::v, T<'\153'>::v,
                T<'\154'>::v, T<'\155'>::v, T<'\156'>::v, T<'\157'>::v,
                T<'\160'>::v, T<'\161'>::v, T<'\162'>::v, T<'\163'>::v,
                T<'\164'>::v, T<'\165'>::v, T<'\166'>::v, T<'\167'>::v,
                T<'\170'>::v, T<'\171'>::v, T<'\172'>::v, T<'\173'>::v,
                T<'\174'>::v, T<'\175'>::v, T<'\176'>::v, T<'\177'>::v,
                T<'\200'>::v, T<'\201'>::v, T<'\202'>::v, T<'\203'>::v,
                T<'\204'>::v, T<'\205'>::v, T<'\206'>::v, T<'\207'>::v,
                T<'\210'>::v, T<'\211'>::v, T<'\212'>::v, T<'\213'>::v,
                T<'\214'>::v, T<'\215'>::v, T<'\216'>::v, T<'\217'>::v,
                T<'\220'>::v, T<'\221'>::v, T<'\222'>::v, T<'\223'>::v,
                T<'\224'>::v, T<'\225'>::v, T<'\226'>::v, T<'\227'>::v,
                T<'\230'>::v, T<'\231'>::v, T<'\232'>::v, T<'\233'>::v,
                T<'\234'>::v, T<'\235'>::v, T<'\236'>::v, T<'\237'>::v,
                T<'\240'>::v, T<'\241'>::v, T<'\242'>::v, T<'\243'>::v,
                T<'\244'>::v, T<'\245'>::v, T<'\246'>::v, T<'\247'>::v,
                T<'\250'>::v, T<'\251'>::v, T<'\252'>::v, T<'\253'>::v,
                T<'\254'>::v, T<'\255'>::v, T<'\256'>::v, T<'\257'>::v,
                T<'\260'>::v, T<'\261'>::v, T<'\262'>::v, T<'\263'>::v,
                T<'\264'>::v, T<'\265'>::v, T<'\266'>::v, T<'\267'>::v,
                T<'\270'>::v, T<'\271'>::v, T<'\272'>::v, T<'\273'>::v,
                T<'\274'>::v, T<'\275'>::v, T<'\276'>::v, T<'\277'>::v,
                T<'\300'>::v, T<'\301'>::v, T<'\302'>::v, T<'\303'>::v,
                T<'\304'>::v, T<'\305'>::v, T<'\306'>::v, T<'\307'>::v,
                T<'\310'>::v, T<'\311'>::v, T<'\312'>::v, T<'\313'>::v,
                T<'\314'>::v, T<'\315'>::v, T<'\316'>::v, T<'\317'>::v,
                T<'\320'>::v, T<'\321'>::v, T<'\322'>::v, T<'\323'>::v,
                T<'\324'>::v, T<'\325'>::v, T<'\326'>::v, T<'\327'>::v,
                T<'\330'>::v, T<'\331'>::v, T<'\332'>::v, T<'\333'>::v,
                T<'\334'>::v, T<'\335'>::v, T<'\336'>::v, T<'\337'>::v,
                T<'\340'>::v, T<'\341'>::v, T<'\342'>::v, T<'\343'>::v,
                T<'\344'>::v, T<'\345'>::v, T<'\346'>::v, T<'\347'>::v,
                T<'\350'>::v, T<'\351'>::v, T<'\352'>::v, T<'\353'>::v,
                T<'\354'>::v, T<'\355'>::v, T<'\356'>::v, T<'\357'>::v,
                T<'\360'>::v, T<'\361'>::v, T<'\362'>::v, T<'\363'>::v,
                T<'\364'>::v, T<'\365'>::v, T<'\366'>::v, T<'\367'>::v,
                T<'\370'>::v, T<'\371'>::v, T<'\372'>::v, T<'\373'>::v,
                T<'\374'>::v, T<'\375'>::v, T<'\376'>::v, T<'\377'>::v
            };

            return table[c & 0xff];
        }
    }
}

#endif
