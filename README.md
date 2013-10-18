libten
======

C++11 library for network services on modern x86-64 Linux.

Features
--------

  * lightweight cooperative tasks (using boost.context)
  * typed channels for communicating between threads and tasks
  * http client and server
  * fast uri parser
  * JSON parser (wrapper around jansson)
  * logging (glog)
  * rpc (using msgpack)
  * epoll event loop

Why
---
Or rather why not libevent, libev, or boost.asio? libten is designed
around the concept of task-based concurrency, while the other
libraries are designed for event driven concurrency with callbacks.
They are not entirely at odds, libten's event loop could be built on
any of these libraries. However, another major difference is that
other libraries strive to provide a cross platform solution to event
driven network programming. They are great if you need portable
code that works across many versions and platforms. libten's approach
is to focus only on modern Linux, modern compilers, and performance.
This makes libten's code base smaller and easier to maintain, with
very minimal abstraction. libten uses epoll, timerfd, and signalfd.
In addition to networking and concurrency it provides logging, JSON,
URI, http client and server, rpc, zookeeper and more.

API Stability
-------------
libten is not a stable API yet. Don't let this scare you away though.
Most changes at this point are minor and incremental.

Dependencies
------------

  * cmake >= 2.8
  * g++ >= 4.7.0
  * libssl-dev >= 0.9.8
  * libboost-dev >= 1.51
  * libboost-date-time-dev >= 1.51
  * libboost-program-options-dev >= 1.51
  * libboost-context-dev >= 1.51
  * ragel >= 6.5
  * libjansson-dev >= 2.3
  * libc-ares-dev

Bundled 3rd-party components
____________________________

  * http_parser v2 from https://github.com/joyent/http-parser
  * glog-0.3.2 from http://code.google.com/p/google-glog/
  * stlencoders 1.1.1 from http://code.google.com/p/stlencoders/
  * msgpack-0.5.7 from http://msgpack.org/
  * miniz v1.14 from http://code.google.com/p/miniz/

[![Build Status](https://travis-ci.org/toffaletti/libten.png)](https://travis-ci.org/toffaletti/libten)
