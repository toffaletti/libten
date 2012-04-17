#ifndef LIBTEN_RPC_PROTOCOL_HH
#define LIBTEN_RPC_PROTOCOL_HH

#include <msgpack.hpp>

namespace msgpack {
namespace rpc {


typedef uint32_t msgid_t;
typedef uint32_t method_t;

typedef uint8_t message_type_t;
typedef uint8_t error_type_t;

static const message_type_t REQUEST  = 0;
static const message_type_t RESPONSE = 1;
static const message_type_t NOTIFY   = 2;

static const error_type_t NO_METHOD_ERROR = 0x01;
static const error_type_t ARGUMENT_ERROR  = 0x02;


struct msg_rpc {
    msg_rpc() { }

    message_type_t type;

    bool is_request()  const { return type == REQUEST;  }
    bool is_response() const { return type == RESPONSE; }
    bool is_notify()   const { return type == NOTIFY;   }

    MSGPACK_DEFINE(type);
};

template <typename Method, typename Parameter>
struct msg_request {
    msg_request() :
        type(REQUEST),
        msgid(0) { }

    msg_request(
            Method method,
            typename msgpack::type::tuple_type<Parameter>::transparent_reference param,
            msgid_t msgid) :
        type(REQUEST),
        msgid(msgid),
        method(method),
        param(param) { }

    message_type_t type;
    msgid_t msgid;
    Method method;
    Parameter param;

    MSGPACK_DEFINE(type, msgid, method, param);
};

template <typename Result, typename Error>
struct msg_response {
    msg_response() :
        type(RESPONSE),
        msgid(0) { }

    msg_response(
            typename msgpack::type::tuple_type<Result>::transparent_reference result,
            typename msgpack::type::tuple_type<Error >::transparent_reference error,
            msgid_t msgid) :
        type(RESPONSE),
        msgid(msgid),
        error(error),
        result(result) { }

    message_type_t type;
    msgid_t msgid;
    Error error;
    Result result;

    MSGPACK_DEFINE(type, msgid, error, result);
};

template <typename Method, typename Parameter>
struct msg_notify {
    msg_notify() :
        type(NOTIFY) { }

    msg_notify(
            Method method,
            typename msgpack::type::tuple_type<Parameter>::transparent_reference param) :
        type(NOTIFY),
        method(method),
        param(param) { }

    message_type_t type;
    Method method;
    Parameter param;

    MSGPACK_DEFINE(type, method, param);
};


}  // namespace rpc
}  // namespace msgpack

#endif // LIBTEN_RPC_PROTOCOL_HH
