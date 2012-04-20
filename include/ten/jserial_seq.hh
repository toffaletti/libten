#ifndef LIBTEN_JSERIAL_SEQ_HH
#define LIBTEN_JSERIAL_SEQ_HH

#include <ten/jserial.hh>
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
};

template <class Seq>
inline json to_json_seq(const Seq &s) {
    json j(json::array());
    for (auto const & el : s)
        j.push(to_json(el));
    return j;
}

// vector<>

template <class T, class A>
struct json_traits<std::vector<T, A>> : public json_traits_seq<std::vector<T, A>> {};

template <class T, class A>
inline json to_json(const std::vector<T, A> &v) { return to_json_seq(v); }

} // ten

#endif // LIBTEN_JSERIAL_SEQ_HH
