#include "msgpack/msgpack.hpp"
#include <string>
#include <iostream>
#include <assert.h>
#include "ten/jsonpack.hh"

using namespace msgpack;
using namespace ten;
using namespace std;

struct myclass {
    int num;
    string str;

    myclass() : num(616), str("the string") {}

    MSGPACK_DEFINE(num, str);

    friend ostream &operator << (ostream &o, const myclass &m) {
        o << "num: " << m.num << " str: " << m.str;
        return o;
    }
};

int main(int argc, char *argv[]) {
    sbuffer sbuf;
    myclass m;
    size_t offset = 0;

    pack(sbuf, m);
    pack(sbuf, -1234);
    pack(sbuf, -1.5);
    pack(sbuf, 3.58);
    pack(sbuf, std::string("cool pack"));

    zone z;
    object obj;

    unpack_return r =
        unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_EXTRA_BYTES);
    cout << "obj: " << obj << "\n";
    cout << obj.as<myclass>() << "\n";

    r = unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_EXTRA_BYTES);
    cout << "obj: " << obj << "\n";
    cout << obj.as<int>() << "\n";

    r = unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_EXTRA_BYTES);
    cout << "obj: " << obj << "\n";
    cout << obj.as<float>() << "\n";

    r = unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_EXTRA_BYTES);
    cout << "obj: " << obj << "\n";
    cout << obj.as<double>() << "\n";

    r = unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_SUCCESS);
    cout << "obj: " << obj << "\n";
    cout << obj.as<string>() << "\n";

    json j{
        {"int", 1234},
        {"double", 1.14},
        {"string", "abcdef"},
        {"array", json::array({"string", 42, true, 9.0})},
    };

    pack(sbuf, j);

    r = unpack(sbuf.data(), sbuf.size(), &offset, &z, &obj);
    assert(r == UNPACK_SUCCESS);
    cout << "json: " << obj << "\n";
    cout << obj.as<json>() << "\n";
    return 0;
}
