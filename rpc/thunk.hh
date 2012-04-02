#include "msgpack/msgpack.hpp"
#include "error.hh"
#include "apply.hh"

namespace ten {

template <size_t I = 0, typename ...Args>
    typename std::enable_if<I == sizeof...(Args), void>::type
    convert_helper(msgpack::object *ptr, std::tuple<Args...> &t) {}

template <size_t I = 0, typename ...Args>
    typename std::enable_if<I < sizeof...(Args), void>::type
    convert_helper(msgpack::object *ptr, std::tuple<Args...> &t) {
        ptr[I].convert(&std::get<I>(t));
        convert_helper<I+1, Args...>(ptr, t);
    }

template <typename Result, typename ...Args>
msgpack::object return_thunk(Result (*f)(Args...), msgpack::object &o, msgpack::zone *z) {
    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != sizeof...(Args)) {
        throw errorx("wrong number of method params. expected %lu got %u.", sizeof...(Args), o.via.array.size);
    }

    std::tuple<Args...> p;
    convert_helper(o.via.array.ptr, p);
    Result r = apply_tuple(*f, std::forward<std::tuple<Args...>>(p));
    return msgpack::object(r, z);
}

template <typename Result, typename ...Args>
msgpack::object return_thunk2(const std::function<Result (Args...)> &f, msgpack::object &o, msgpack::zone *z) {
    if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    if (o.via.array.size != sizeof...(Args)) {
        throw errorx("wrong number of method params. expected %lu got %u.", sizeof...(Args), o.via.array.size);
    }

    std::tuple<Args...> p;
    convert_helper(o.via.array.ptr, p);
    Result r = apply_tuple(f, std::forward<std::tuple<Args...>>(p));
    return msgpack::object(r, z);
}

template <typename Result, typename ...Args>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(Result (*f)(Args...)) {
    using namespace std::placeholders;
    return std::bind(return_thunk<Result, Args...>, f, _1, _2);
}

template <typename Result, typename ...Args>
std::function<msgpack::object (msgpack::object &o, msgpack::zone *z)> thunk(const std::function<Result (Args...)> &f) {
    using namespace std::placeholders;
    return std::bind(return_thunk2<Result, Args...>, f, _1, _2);
}

} // end namespace ten

