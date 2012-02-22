#include "msgpack/msgpack.hpp"
#include "error.hh"

using namespace ten;



template <typename Result>
msgpack::object return_thunk(const std::function<Result ()> &f, msgpack::object &o, msgpack::zone *z) {
    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 0) throw errorx("wrong number of method params. expected %d got %d.", 0, o.via.array.size);

    Result r = f();
    return msgpack::object(r, z);
}

template <typename Result, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result>, f);
}


template <typename Result, typename A0>
msgpack::object return_thunk(const std::function<Result (A0)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 1) throw errorx("wrong number of method params. expected %d got %d.", 1, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    
    Result r = f(p.a0);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0>, f, _1);
}

template <typename Result, typename A0, typename A1>
msgpack::object return_thunk(const std::function<Result (A0, A1)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 2) throw errorx("wrong number of method params. expected %d got %d.", 2, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    
    Result r = f(p.a0, p.a1);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1>, f, _1, _2);
}

template <typename Result, typename A0, typename A1, typename A2>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 3) throw errorx("wrong number of method params. expected %d got %d.", 3, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    
    Result r = f(p.a0, p.a1, p.a2);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2>, f, _1, _2, _3);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 4) throw errorx("wrong number of method params. expected %d got %d.", 4, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3>, f, _1, _2, _3, _4);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3, A4)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        A4 a4;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 5) throw errorx("wrong number of method params. expected %d got %d.", 5, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    o.via.array.ptr[4].convert(&p.a4);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3, p.a4);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3, A4>, f, _1, _2, _3, _4, _5);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3, A4, A5)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        A4 a4;
        A5 a5;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 6) throw errorx("wrong number of method params. expected %d got %d.", 6, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    o.via.array.ptr[4].convert(&p.a4);
    o.via.array.ptr[5].convert(&p.a5);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3, p.a4, p.a5);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3, A4, A5>, f, _1, _2, _3, _4, _5, _6);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3, A4, A5, A6)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        A4 a4;
        A5 a5;
        A6 a6;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 7) throw errorx("wrong number of method params. expected %d got %d.", 7, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    o.via.array.ptr[4].convert(&p.a4);
    o.via.array.ptr[5].convert(&p.a5);
    o.via.array.ptr[6].convert(&p.a6);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3, p.a4, p.a5, p.a6);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3, A4, A5, A6>, f, _1, _2, _3, _4, _5, _6, _7);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3, A4, A5, A6, A7)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        A4 a4;
        A5 a5;
        A6 a6;
        A7 a7;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 8) throw errorx("wrong number of method params. expected %d got %d.", 8, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    o.via.array.ptr[4].convert(&p.a4);
    o.via.array.ptr[5].convert(&p.a5);
    o.via.array.ptr[6].convert(&p.a6);
    o.via.array.ptr[7].convert(&p.a7);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3, p.a4, p.a5, p.a6, p.a7);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3, A4, A5, A6, A7>, f, _1, _2, _3, _4, _5, _6, _7, _8);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
msgpack::object return_thunk(const std::function<Result (A0, A1, A2, A3, A4, A5, A6, A7, A8)> &f, msgpack::object &o, msgpack::zone *z) {
    struct params {
        A0 a0;
        A1 a1;
        A2 a2;
        A3 a3;
        A4 a4;
        A5 a5;
        A6 a6;
        A7 a7;
        A8 a8;
        
    };

    params p;

    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != 9) throw errorx("wrong number of method params. expected %d got %d.", 9, o.via.array.size);

    o.via.array.ptr[0].convert(&p.a0);
    o.via.array.ptr[1].convert(&p.a1);
    o.via.array.ptr[2].convert(&p.a2);
    o.via.array.ptr[3].convert(&p.a3);
    o.via.array.ptr[4].convert(&p.a4);
    o.via.array.ptr[5].convert(&p.a5);
    o.via.array.ptr[6].convert(&p.a6);
    o.via.array.ptr[7].convert(&p.a7);
    o.via.array.ptr[8].convert(&p.a8);
    
    Result r = f(p.a0, p.a1, p.a2, p.a3, p.a4, p.a5, p.a6, p.a7, p.a8);
    return msgpack::object(r, z);
}

template <typename Result, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename F>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(F *f) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, A0, A1, A2, A3, A4, A5, A6, A7, A8>, f, _1, _2, _3, _4, _5, _6, _7, _8, _9);
}

