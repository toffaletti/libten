#include "ten/synchronized.hh"
#include <iostream>

using namespace ten;

class A {
private:
    int i;
    int i_squared;
public:
    A(int _i=0) {
        set(_i);
    }

    void set(int _i) {
        i = _i;
        i_squared = i*i;
    }

    int get_squared() {
        return i_squared;
    }
};

int main() {
    synchronized<A> a(1);
    A b;
    synchronize(a, [&](A &a_) {
        a_.set(2);
        b = a_;
    });

    synchronize(a, [](A &a_) {
        a_.set(3);
        std::cout << "a: " << a_.get_squared() << "\n";
    });

    std::cout << "b: " << b.get_squared() << "\n";
}
