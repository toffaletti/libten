#ifndef LIBTEN_JSERIAL_SEQ_HH
#define LIBTEN_JSERIAL_SEQ_HH

#include <ten/jserial.hh>
#include <array>
#include <vector>

namespace ten {

//
// sequential collections, generically
//

template <class Seq>
struct json_traits_seq : public json_traits<typename Seq::value_type> {
    typedef typename Seq::value_type value_type;

    static Seq cast(const json &j) {
        if (!j.is_array())
            throw errorx("not an array: %s", j.dump().c_str());
        Seq s;
        s.capacity(j.asize());
        for (auto jel : const_cast<json &>(j).arr())
            s.push_back(json_traits<value_type>::cast(jel));
        return s;
    }

    static json make(const Seq &s) {
        json j(json::array());
        for (auto const & el : s)
            j.push(json_traits<value_type>::make(el));
        return j;
    }
};

// array<>

template <class T, size_t N>
struct json_traits<std::array<T, N>>
    : public json_traits_seq<std::array<T, N>> {};

// vector<>

template <class T, class A>
struct json_traits<std::vector<T, A>>
    : public json_traits_seq<std::vector<T, A>> {};

} // ten

#endif // LIBTEN_JSERIAL_SEQ_HH
