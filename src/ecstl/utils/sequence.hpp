#include <tuple>
#include <utility>
#include <type_traits>

namespace ecstl {



/// Calls a function for each index from 0 to N-1 at compile time
/**
 * @tparam N Number of iterations
 * @tparam Fn Type of the function to be called. The function should accept a single
 * parameter of type std::integral_constant<std::size_t, I> where I is the current index.
 * This constant can be used to index into tuples or other compile-time structures.
 * @tparam Start Start index (used for recursion, default is 0)
 * @param fn Function to be called for each index
 * This function is evaluated at compile time and can be used to unroll loops
 * or perform other compile-time computations.
 */
template<std::size_t N, typename Fn, std::size_t Start = 0>
constexpr void sequence_iterate(Fn &&fn) {
    if constexpr(N <= Start) return ;
    else {
        fn(std::integral_constant<std::size_t, Start>{});
        sequence_iterate<N, Fn, Start+1>(std::forward<Fn>(fn));
    }
}

/// Calls a function for each index from 0 to N-1 at compile time, accumulating a result
/**
 * @tparam N Number of iterations
 * @tparam Initial Type of the initial value and the accumulated result
 * @tparam Fn Type of the function to be called. The function should accept two
 * parameters: the accumulated result so far and the current index as
 * std::integral_constant<std::size_t, I> where I is the current index.
 * The function should return the updated accumulated result.
 * @tparam Start Start index (used for recursion, default is 0)
 * @param initial Initial value for the accumulation
 * @param fn Function to be called for each index
 * @return The final accumulated result after all iterations
 * This function is evaluated at compile time and can be used to unroll loops
 * or perform other compile-time computations that produce a result.
 */
template<std::size_t N, typename Initial, typename Fn, std::size_t Start = 0>
constexpr auto sequence_iterate(Initial &&initial, Fn &&fn) {
    if constexpr(N <= Start) return initial;
    else {
        return fn(sequence_iterate<N, Initial, Fn, Start+1>(
                            std::forward<Initial>(initial), std::forward<Fn>(fn)), 
                std::integral_constant<std::size_t, Start>{});
    }
}



}