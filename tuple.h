#pragma once

#include <tiffio.h>
#include <type_traits>

namespace detail {
template<size_t N, typename T>
struct elt_wrapper {
  constexpr elt_wrapper() : val() {}
  constexpr explicit elt_wrapper(T val) : val(std::move(val)) {}
  constexpr elt_wrapper(elt_wrapper const& other) : val(other.val) {}
  constexpr elt_wrapper(elt_wrapper&& other) : val(std::move(other.val)) {}

  template <typename Y>
  constexpr explicit elt_wrapper(Y val) : val(std::move(val)) {}

  template <typename Y>
  constexpr explicit elt_wrapper(elt_wrapper<N, Y> const& other) : val(other.val) {}

  template <typename Y>
  constexpr explicit elt_wrapper(elt_wrapper<N, Y>&& other) : val(std::move(other.val)) {}

  T val;
};

template<typename Seq, class... Types>
struct tuple_base;

////////////////////////////////////////////////////
template<std::size_t N, typename T>
struct tuple_element;

template<std::size_t N, typename Seq, typename Head, typename... Tail>
struct tuple_element<N, tuple_base<Seq, Head, Tail...>>
: tuple_element<N - 1, tuple_base<Seq, Tail...>> {};

template<typename Seq, typename Head, typename... Tail>
struct tuple_element<0, tuple_base<Seq, Head, Tail...>> {
using type = Head;
};

template<std::size_t N, typename T>
using tuple_element_t = typename tuple_element<N, T>::type;
////////////////////////////////

template<size_t... Idx, class... Types>
struct tuple_base<std::index_sequence<Idx...>, Types...> : elt_wrapper<Idx, Types>... {
  constexpr tuple_base() = default;

  template <size_t N = sizeof...(Types), typename std::enable_if_t<(N > 0), int> = 0>
  constexpr explicit tuple_base(Types const&... args) : elt_wrapper<Idx, Types>(args)... {}

  constexpr tuple_base(tuple_base const& other) : elt_wrapper<Idx, Types>(other.get<Idx>())... {}
  constexpr tuple_base(tuple_base&& other) : elt_wrapper<Idx, Types>(std::move(other.get<Idx>()))... {}

  template <typename... UTypes,
      size_t N = sizeof...(UTypes), size_t M = sizeof...(Types), typename std::enable_if_t<(N > 0 && N == M), int> = 0>
  constexpr explicit tuple_base(UTypes&&... args) : elt_wrapper<Idx, Types>(std::forward<UTypes>(args))... {}

  template <typename... UTypes>
  using other_base = tuple_base<std::index_sequence<Idx...>, UTypes...>;

  template <typename... UTypes>
  constexpr explicit tuple_base(other_base<UTypes...> const& other)
      : elt_wrapper<Idx, Types>(other.template get<Idx>())... {}

  template <typename... UTypes>
  constexpr explicit tuple_base(other_base<UTypes...>&& other)
      : elt_wrapper<Idx, Types>(std::move(other.template get<Idx>()))... {}

  template<std::size_t N>
  constexpr tuple_element_t<N, tuple_base<std::index_sequence<Idx...>, Types...>> const& get() const noexcept {
    return static_cast<detail::elt_wrapper<N, tuple_element_t<N, tuple_base<std::index_sequence<Idx...>, Types...>>> const &>(*this).val;
  }

  template<std::size_t N>
  constexpr tuple_element_t<N, tuple_base<std::index_sequence<Idx...>, Types...>> && get() noexcept {
    return static_cast<detail::elt_wrapper<N, tuple_element_t<N, tuple_base<std::index_sequence<Idx...>, Types...>>> &&>(*this).val;
  }
};

} // namespace detail

template<typename... Types>
struct tuple : detail::tuple_base<std::index_sequence_for<Types...>, Types...> {
  using base = detail::tuple_base<std::index_sequence_for<Types...>, Types...>;
  // same types constructors
  // note: think *when* you need to enable all of them
  constexpr tuple() = default;

  template <size_t N = sizeof...(Types), typename std::enable_if_t<(N > 0), int> = 0>
  constexpr explicit tuple(const Types&... args) : base(args...) {}

  tuple(const tuple& other) : base(static_cast<base const &>(other)) {}
  tuple(tuple&& other) : base(std::move(static_cast<base&&>(other))) {}

  // no need for conditional explicitness
  // convert types constructors

  template<typename... UTypes,
      size_t N = sizeof...(UTypes), size_t M = sizeof...(Types), typename std::enable_if_t<(N > 0 && N == M), int> = 0>
  constexpr explicit tuple(UTypes&&... args) : base(std::forward<UTypes>(args)...) {}

  template<typename... UTypes>
  constexpr explicit tuple(const tuple<UTypes...>& other) : base(static_cast<detail::tuple_base<std::index_sequence_for<UTypes...>, UTypes...> const &>(other)) {}
  template<typename... UTypes>
  constexpr explicit tuple(tuple<UTypes...>&& other) : base(std::move(static_cast<detail::tuple_base<std::index_sequence_for<UTypes...>, UTypes...>&&>(other))) {}
};

// make_tuple -- constructor with auto deducing types
// note references are dereferenced
// see cppreference

template <class T>
struct unwrap_refwrapper {
  using type = T;
};

template <class T>
struct unwrap_refwrapper<std::reference_wrapper<T>> {
  using type = T&;
};

template <typename T>
using unwrap_decay_t = typename unwrap_refwrapper<std::decay_t<T>>::type;

template<typename... Types>
constexpr tuple<unwrap_decay_t<Types>...> make_tuple(Types&&... args) {
  return tuple<unwrap_decay_t<Types>...>(std::forward<Types>(args)...);
}

// NOTE: if not tuple passed to helpers -- must be compiler error
// tuple_element -- return type by it's number (type field)
template<std::size_t N, typename T>
struct tuple_element;

template<std::size_t N, typename Head, typename... Tail>
struct tuple_element<N, tuple<Head, Tail...>>
    : tuple_element<N - 1, tuple<Tail...>> {};
//  using type = typename tuple_element<N - 1, tuple<Tail...>>::type;
//};

template<typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
  using type = Head;
};

template<std::size_t N, typename T>
using tuple_element_t = typename tuple_element<N, T>::type;

// tuple_size -- value equals number of elements (you can use std::integral_constant)
template<typename... Types>
struct tuple_size;

template<typename... Types>
struct tuple_size<tuple<Types...>> : std::integral_constant<size_t, sizeof...(Types)> {};

template<typename T>
inline constexpr size_t tuple_size_v = tuple_size<T>::value;

// get -- main function to interact with tuple
// NOTE: think how to replace int in return type
// NOTE: compiler error if N > tuple_size
template<std::size_t N, typename... Types>
constexpr tuple_element_t<N, tuple<Types...>>& get(tuple<Types...>& t) noexcept {
  static_assert(N < sizeof...(Types));
  return static_cast<detail::elt_wrapper<N, tuple_element_t<N, tuple<Types...>>>&>(t).val;
}

template<std::size_t N, typename... Types>
constexpr tuple_element_t<N, tuple<Types...>>&& get(tuple<Types...>&& t) noexcept {
  static_assert(N < sizeof...(Types));
  using type = tuple_element_t<N, tuple<Types...>>;
  return std::forward<type>(static_cast<detail::elt_wrapper<N, type>&&>(t).val);
}

template<std::size_t N, typename... Types>
constexpr const tuple_element_t<N, tuple<Types...>>& get(const tuple<Types...>& t) noexcept {
  static_assert(N < sizeof...(Types));
  return static_cast<detail::elt_wrapper<N, tuple_element_t<N, tuple<Types...>>> const &>(t).val;
}

template<std::size_t N, typename... Types>
constexpr const tuple_element_t<N, tuple<Types...>>&& get(const tuple<Types...>&& t) noexcept {
  static_assert(N < sizeof...(Types));
  using type = tuple_element_t<N, tuple<Types...>>;
  return std::forward<type>(static_cast<detail::elt_wrapper<N, type> const &&>(t).val);
}


template <typename T, typename... Types>
constexpr size_t idx_by_type() {
  constexpr size_t size = sizeof...(Types);
  size_t idx = size;
  constexpr bool same[size] = { std::is_same_v<T, Types>... };

  for (size_t i = 0; i < sizeof...(Types); ++i) {
    if (same[i]) {
      if (idx != size) {
        return size;
      }
      idx = i;
    }
  }
  return idx;
}

// gets by type
// NOTE: compiler error if many T in one tuple
template<typename T, typename... Types>
constexpr T& get(tuple<Types...>& t) noexcept {
  constexpr size_t idx = idx_by_type<T, Types...>();
  static_assert(idx < sizeof...(Types));

  return static_cast<detail::elt_wrapper<idx, T>&>(t).val;
}

template<typename T, typename... Types>
constexpr T&& get(tuple<Types...>&& t) noexcept {
  constexpr size_t idx = idx_by_type<T, Types...>();
  static_assert(idx < sizeof...(Types));

  return std::forward<T>(static_cast<detail::elt_wrapper<idx, T>&&>(t).val);
}

template<typename T, typename... Types>
constexpr const T& get(const tuple<Types...>& t) noexcept {
  constexpr size_t idx = idx_by_type<T, Types...>();
  static_assert(idx < sizeof...(Types));

  return static_cast<detail::elt_wrapper<idx, T>&>(t).val;
}

template<typename T, typename... Types>
constexpr const T&& get(const tuple<Types...>&& t) noexcept {
  constexpr size_t idx = idx_by_type<T, Types...>();
  static_assert(idx < sizeof...(Types));

  return std::forward<T>(static_cast<detail::elt_wrapper<idx, T>&&>(t).val);
}

// swap specialization
template <size_t I, typename... TTypes, typename... UTypes>
constexpr void swap(tuple<TTypes...>& first, tuple<UTypes...>& second) {
  using std::swap;
  swap(get<I>(first), get<I>(second));
  if constexpr (I + 1 < sizeof...(TTypes))
    swap<I + 1>(first, second);
}

template<typename... TTypes, typename... UTypes>
void swap(tuple<TTypes...>& first, tuple<UTypes...>& second) {
  static_assert(sizeof...(TTypes) == sizeof...(UTypes));
  swap<0>(first, second);
}

// compare operators

template<size_t I, typename... TTypes, typename... UTypes>
constexpr bool less(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  if constexpr (I >= sizeof...(TTypes) - 1) {
    return false;
  }

  return (get<I>(first) < get<I>(second))
         || ((get<I>(second) == get<I>(first)) && less<I + 1>(first, second));
}

template <size_t I, typename... TTypes, typename... UTypes>
constexpr bool eq(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  if constexpr (I >= sizeof...(TTypes)) {
    return true;
  }

  return (get<I>(first) == get<I>(second))
         && eq<I + 1>(first, second);
}

template<typename... TTypes, typename... UTypes>
constexpr bool operator==(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  static_assert(sizeof...(TTypes) == sizeof...(UTypes));

  return eq<0>(first, second);
}
template<typename... TTypes, typename... UTypes>
constexpr bool operator!=(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  return !(first == second);
}

template<typename... TTypes, typename... UTypes>
constexpr bool operator<(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  static_assert(sizeof...(TTypes) == sizeof...(UTypes));

  return less<0>(first, second);
}
template<typename... TTypes, typename... UTypes>
constexpr bool operator>(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  return second < first;
}
template<typename... TTypes, typename... UTypes>
constexpr bool operator<=(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  return !(second < first);
}
template<typename... TTypes, typename... UTypes>
constexpr bool operator>=(const tuple<TTypes...>& first, const tuple<UTypes...>& second) {
  return !(first < second);
}
