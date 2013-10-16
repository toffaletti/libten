.. _build:

########
Building
########

Dependencies
============

    * cmake >= 2.8
    * g++ >= 4.7.0
    * libssl-dev >= 0.9.8
    * libboost-dev = 1.51
    * libboost-date-time-dev = 1.51
    * libboost-program-options-dev = 1.51
    * libboost-context-dev = 1.51
    * ragel >= 6.5
    * libjansson >= 2.3
    * libc-ares >= 1.9

CMake
=====

libten uses CMake__ build system. A typical build might look like this::

    $ git clone git@github.com:toffaletti/libten.git
    $ mkdir libten-debug
    $ cd libten-debug
    $ cmake ../libten
    $ make

.. __: http://www.cmake.org/

This is what CMake calls an out-of-tree build, which means the source directory is untouched and all build related configuration and by-products are created inside the build directory. This allows creating several build directories for different configurations (debug, release, maybe different compilers)::
    
    $ mkdir libten-release
    $ cd libten-release
    $ cmake -DCMAKE_BUILD_TYPE=Release ../libten
    $ make

The recommended way for building libten based applications is using git submodule (or git subtree) to include libten in the applications git repository and link statically. An example CMakeLists.txt::

    project(my-app)
    cmake_minimum_required(VERSION 2.8)

    add_subdirectory(libten)
    include(${CMAKE_CURRENT_SOURCE_DIR}/libten/cmake/ten.cmake)

    add_executable(my-app my-app.cc)
    target_link_libraries(my-app ten boost_program_options)

Using git submodule or subtree links the application to a specific revision of libten. This allows great control and prevents changes to libten from breaking the application build.

``libten/cmake/defaults.cmake`` is where libten configures compiler flags

``libten/cmake/ten.cmake`` is used for including libten in a CMake-based application.


Tests
=====

libten has a unit test suite that can be run with ``make test``.

Extras
======

libten includes examples and benchmarks that can be built with ``make examples`` and ``make benchmarks``. ``make world`` will build both.

To build this documentation install python-sphinx and run ``make html`` from ``libten/doc/``.
