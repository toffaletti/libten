libten
======

C++11 library for network services on modern x86-64 Linux.
Heavily inspired by python-gevent, state-threads, plan9, and go-lang.

Features
--------

  * coroutines (built on boost.context)
  * typed channels for communicating between threads and coroutines
  * http client and server (built on http-parser)
  * fast uri parser (ragel generated)
  * JSON parser (wrapper around jansson)
  * logging (glog)
  * rpc (built on msgpack)
  * epoll event loop

Why
---
Or rather why not libevent, libev, or boost.asio? libten is designed
around the concept of task-based concurrency, while the other
libraries are designed for event driven callback based concurrency.
They are not entirely at odds, libten's event loop could be built on
any of these libraries. However, another major difference is that
these libraries strive to provide a cross platform solution to event
driven network programming. They are great if you need portable
code that works across many versions and platforms. However,
that feature doesn't come for free. There is added complexity in
the code base, more code, and compromises that effect performance.
libten's approach is to focus only on modern Linux, modern compilers,
and high-performance APIs. For example, the libten event loop uses 
epoll, timerfd, and signalfd. It also trades memory for speed
by using the socket fd numbers as indexes into an array. Lastly,
libten does not stop at the event loop, it tries to be more of a complete
package by providing logging, JSON, URI, http, rpc, zookeeper and more.

Dependencies
------------

  * cmake >= 2.8
  * g++ >= 4.7.0
  * libssl-dev >= 0.9.8
  * libboost-dev >= 1.51
  * libboost-date-time-dev >= 1.51
  * libboost-program-options-dev >= 1.51
  * libboost-test-dev >= 1.51
  * libboost-context-dev >= 1.51
  * ragel >= 6.5
  * libjansson >= 2.3

Bundled 3rd-party components
____________________________

  * http_parser v2 from https://github.com/joyent/http-parser
  * glog-0.3.2 from http://code.google.com/p/google-glog/
  * stlencoders 1.1.1 from http://code.google.com/p/stlencoders/
  * msgpack-0.5.7 from http://msgpack.org/
  * miniz v1.14 from http://code.google.com/p/miniz/

TODO
----
  * Optimizations (almost no profiling has been done, lots of room for improvement)
  * Better support for std::chrono all around (qlock/rendez)
  * qlock/rendez to follow C++11 mutex/condition_variable api
  * More tests and documentation and examples
