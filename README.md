libten
======

C++11 library for network services on modern x86-64 Linux.
Heavily inspired by python-gevent, state-threads, plan9, and go-lang.

Features
--------

  * coroutines (boost.context)
  * typed channels for communicating between threads and coroutines
  * http client and server (based on http-parser)
  * fast uri parser (ragel generated)
  * JSON parser (jansson)
  * logging (glog)
  * rpc (based on msgpack)
  * epoll event loop

Dependencies
____________

  * cmake >= 2.8
  * g++ >= 4.4.3
  * libssl-dev >= 0.9.8
  * libboost-dev >= 1.40
  * libboost-date-time-dev >= 1.40
  * libboost-program-options-dev >= 1.40
  * libboost-test-dev >= 1.40
  * ragel >= 6.5

Bundled 3rd-party components
____________________________

  * boost.context-0.7.6 parts from https://github.com/olk/boost.context
  * http_parser forked from https://github.com/joyent/http-parser
  * glog-0.3.1 from http://code.google.com/p/google-glog/
  * stringencoders from http://code.google.com/p/stringencoders/
  * msgpack-0.5.7 from http://msgpack.org/
  * jansson-2.2.1 from http://www.digip.org/jansson/
  * miniz from http://code.google.com/p/miniz/

TODO
----
  * Optimizations (almost no profiling has been done, lots of room for improvement)
  * Better support for std::chrono all around (qlock/rendez)
  * qlock/rendez to follow C++11 mutex/condition_variable api
  * More tests and documentation and examples
