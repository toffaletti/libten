#ifndef APPLY_HH
#define APPLY_HH

// Copyright (c) 2011 Paul Preney. All Rights Reserved.
// see http://preney.ca/paul/archives/574

#include <cstddef>
#include <tuple>
#include <functional>

namespace ten {

template <std::size_t...>
struct indices;

template <std::size_t N, typename Indices, typename... Types>
struct make_indices_impl;

template <std::size_t N, std::size_t... Indices, typename Type, typename... Types>
struct make_indices_impl<N, indices<Indices...>, Type, Types...>
{
  typedef
    typename make_indices_impl<N+1, indices<Indices...,N>, Types...>::type
    type
  ;
};

template <std::size_t N, std::size_t... Indices>
struct make_indices_impl<N, indices<Indices...>>
{
  typedef indices<Indices...> type;
};

template <std::size_t N, typename... Types>
struct make_indices
{
  typedef typename make_indices_impl<0, indices<>, Types...>::type type;
};

//===========================================================================

template <typename Op, typename... Args>
auto apply(Op&& op, Args&&... args)
  -> typename std::result_of<Op(Args...)>::type
{
  return op( std::forward<Args>(args)... );
}

//===========================================================================

template <
  typename Indices
>
struct apply_tuple_impl;

template <
  template <std::size_t...> class I,
  std::size_t... Indices
>
struct apply_tuple_impl<I<Indices...>>
{
  template <
    typename Op,
    typename... OpArgs,
    template <typename...> class T = std::tuple
  >
  static auto apply_tuple(Op&& op, T<OpArgs...>&& t)
    -> typename std::result_of<Op(OpArgs...)>::type
  {
    return op( std::forward<OpArgs>(std::get<Indices>(t))... );
  }
};

template <
  typename Op,
  typename... OpArgs,
  typename Indices = typename make_indices<0, OpArgs...>::type,
  template <typename...> class T = std::tuple
>
auto apply_tuple(Op op, T<OpArgs...>&& t)
  -> typename std::result_of<Op(OpArgs...)>::type
{
  return apply_tuple_impl<Indices>::apply_tuple(
    std::forward<Op>(op),
    std::forward<T<OpArgs...>>(t)
  );
}

} // end namespace ten

#endif // APPLY_HH

