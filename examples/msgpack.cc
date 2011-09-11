#include "msgpack/msgpack.hpp"
#include <string>
#include <iostream>
#include <assert.h>

struct myclass {
    int num;
    std::string str;

    myclass() : num(616), str("the string") {}

    MSGPACK_DEFINE(num, str);

    friend std::ostream &operator << (std::ostream &o, const myclass &m) {
        o << "num: " << m.num << " str: " << m.str;
        return o;
    }
};

int main(int argc, char *argv[]) {
    msgpack::sbuffer sbuf;
    myclass m;
    size_t offset = 0;

    msgpack::pack(sbuf, m);
    msgpack::pack(sbuf, 1234);
    msgpack::pack(sbuf, std::string("cool pack"));

    msgpack::zone z;
    msgpack::object obj;

    msgpack::unpack_return r =
        msgpack::unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == msgpack::UNPACK_EXTRA_BYTES);
    std::cout << "obj: " << obj << "\n";
    std::cout << obj.as<myclass>() << "\n";

    r = msgpack::unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == msgpack::UNPACK_EXTRA_BYTES);
    std::cout << "obj: " << obj << "\n";
    std::cout << obj.as<int>() << "\n";

    r = msgpack::unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == msgpack::UNPACK_SUCCESS);
    std::cout << "obj: " << obj << "\n";
    std::cout << obj.as<std::string>() << "\n";
    return 0;
}