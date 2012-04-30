#ifndef LIBTEN_JSERIAL_ASSOC_HH
#define LIBTEN_JSERIAL_ASSOC_HH

#include <boost/version.hpp>
#if BOOST_VERSION >= 104800
#include <boost/container/flat_map.hpp>
#endif
#include <ten/jserial.hh>
#include <map>
#include <unordered_map>

namespace ten {
using std::move;
using std::map;
using std::unordered_map;
#if BOOST_VERSION >= 104800
using boost::container::flat_map;
#endif

//
// associative collections
//

template <class Assoc>
struct json_traits_assoc : public json_traits<typename Assoc::mapped_type> {
    typedef typename Assoc::mapped_type mapped_type;
    typedef typename Assoc::value_type  value_type;

    static Assoc cast(const json &j) {
        if (!j.is_object())
            throw errorx("not an object: %s", j.dump().c_str());
        Assoc a;
        for (auto jel : const_cast<json &>(j).obj())
            a.insert(value_type(jel.first, json_traits<mapped_type>::cast(jel.second)));
        return a;
    }

    static json make(const Assoc &a) {
        json j({});
        for (auto const & el : a)
            j.set(el.first, json_traits<mapped_type>::make(el.second));
        return j;
    }
};

//
// map<>, unordered_map<>, flat_map<>
//

template <class K, class M, class A>
struct json_traits<map<K, M, A>> 
    : public json_traits_assoc<map<K, M, A>> {};

template <class K, class M, class A>
struct json_traits<unordered_map<K, M, A>>
    : public json_traits_assoc<unordered_map<K, M, A>> {};

#if BOOST_VERSION >= 104800
template <class K, class M, class A>
struct json_traits<flat_map<K, M, A>>
    : public json_traits_assoc<flat_map<K, M, A>> {};
#endif

} // ten

#endif // LIBTEN_JSERIAL_ASSOC_HH
