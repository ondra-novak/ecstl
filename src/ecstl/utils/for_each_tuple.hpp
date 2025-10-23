#include <tuple>
#include <utility>
#include <type_traits>

namespace ecstl {


template<std::size_t N, typename Fn, std::size_t Start = 0>
constexpr void sequence_iterate(Fn &&fn) {
    if constexpr(N <= Start) return ;
    else {
        fn(std::integral_constant<std::size_t, N>{});
        sequence_iterate(std::forward<Fn>(fn));
    }
}

template<std::size_t N, typename Initial, typename Fn, std::size_t Start = 0>
constexpr auto sequence_iterate(Initial &&initial, Fn &&fn) {
    if constexpr(N <= Start) return initial;
    else {
        return fn(sequence_iterate<N, Initial, Fn, Start+1>(std::forward<Initial>(initial), std::forward<Fn>(fn)));
    }
}


template <typename Tuple, typename F, std::size_t... Is>
constexpr void for_each_tuple_impl(Tuple&& tup, F&& f, std::index_sequence<Is...>) {
    // rozvine volání f(get<Is>(tup), index_constant)
    (f(std::get<Is>(std::forward<Tuple>(tup)),
       std::integral_constant<std::size_t, Is>{}), ...);
}

template <typename Tuple, typename F>
constexpr void for_each_tuple(Tuple&& tup, F&& f) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    for_each_tuple_impl(std::forward<Tuple>(tup),
                        std::forward<F>(f),
                        std::make_index_sequence<N>{});
}

template <typename Tuple, typename Initial, typename F, std::size_t... Is>
constexpr void reduce_tuple_impl(Tuple&& tup, Initial v, F&& f, std::index_sequence<Is...>) {
    // rozvine volání f(get<Is>(tup), index_constant)
    ((v = f(std::get<Is>(v, std::forward<Tuple>(tup)),std::integral_constant<std::size_t, Is>{})), ...);
    return v;
}

template <typename Tuple, typename Initial, typename F>
constexpr void reduce_tuple(Tuple&& tup, Initial v, F&& f) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    return for_each_tuple_impl(std::forward<Tuple>(tup), std::move(v), std::forward<F>(f),  std::make_index_sequence<N>{});
}

}