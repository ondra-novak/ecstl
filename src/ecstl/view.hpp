#pragma once
#include "component.hpp"
#include <tuple>
#include <limits>
#include <array>
#include <span>

namespace ecstl {

    // A basic concept for types that behave like pointers
    template <typename T>
    concept PointerLike = std::is_pointer_v<T> || requires(T p) {
    // Basic pointer operations (can be adjusted)
        {*p};        // Dereference (like std::unique_ptr, std::shared_ptr)
        {p.operator->()}; // Arrow operator (like unique_ptr, shared_ptr)
    };


    ///View for iterating over entities with specific components
    /** @tparam Registry Type of the registry (pointer-like type, e.g., raw pointer, smart pointer). This could be useful when
     * holding a pointer also means holding a lock or other resource. 
     *  @tparam Components Types of components to be included in the view
     *  The view can be used in range-based for loops and other range algorithms. The view is compatible with the C++20 ranges library.
     **/
    template<typename Registry, typename ... Components>
    class View : public std::ranges::view_interface<View<Registry, Components ...> >{
    public:

        static_assert(PointerLike<Registry>, "Registry must be a pointer-like type (e.g., raw pointer, smart pointer)");
        static_assert(std::is_copy_constructible_v<Registry>, "Registry pointer must be copy-constructible (unique_ptr is not allowed)");

        static_assert(sizeof...(Components) >= 1, "At least one component type must be specified");

        using CompTuple = std::tuple<Components...>;
        using SubIDS = std::array<ComponentTypeID, std::tuple_size_v<CompTuple> >;
        using Ranges = std::tuple<decltype(std::declval<Registry>()->template all_of<Components>())...>;
        using Iterators = std::tuple<decltype(std::declval<Registry>()->template all_of<Components>().begin())...>;
        using Values = std::tuple<const Entity &, decltype(std::declval<Registry>()->template all_of<Components>().begin()->second)...>;
        

        constexpr View(Registry reg, std::span<const ComponentTypeID> subIds)
            :_reg(reg)
            ,_subIds(load_subIds(subIds))
            ,_ranges(load_ranges(std::index_sequence_for<Components...>{})) {
                
                find_best(_master);
        }

        class iterator {
        public:

            using value_type = Values;
            using reference = Values;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::input_iterator_tag;


            constexpr iterator() = default;
            constexpr iterator(Registry reg, Iterators iter, Iterators ends, SubIDS ids, unsigned int master)
                :_reg(reg), _iter(iter),_ends(ends), _ids(ids), _master(master) {
                    check_after_advance();
                }

            iterator &operator++() {
                advance();
                if (!check_iterators_valid()) {
                    check_after_advance();
                }
                return *this;
            }

            iterator operator++(int) {
                iterator x = *this;
                advance();
                return x;
            }

            bool operator==(const iterator &other) const {
                return is_equal<0>(other);
            }

            Values operator*() const {
                const Entity &ent = std::get<0>(_iter)->first;
                return std::apply([&](auto &... iters){
                    return std::tie(ent,iters->second...);
                }, _iter);
            }
            

        protected:
            Registry _reg = {};
            Iterators _iter = {};
            Iterators _ends = {};
            SubIDS _ids = {};
            unsigned int _master = 0;

            
            template<unsigned int N = 0>
            Entity get_entity() const {
                if constexpr(N == std::tuple_size_v<Iterators>) {
                    return {};
                } else {
                    if (N == _master) return std::get<N>(_iter)->first;
                    return get_entity<N+1>();
                }
            }

            template<unsigned int N = 0>
            bool setup_iterators(Entity e)  {
                if constexpr(N == std::tuple_size_v<Iterators>) {
                    return true;
                } else {
                    if (N != _master )  {
                        auto i = _reg->template find<std::tuple_element_t<N, CompTuple> >(e,_ids[N]);
                        if (i == std::get<N>(_ends)) return false;
                        std::get<N>(_iter) = i;
                    } 
                    return setup_iterators<N+1>(e);
                }
            }

            template<unsigned int N = 0>
            bool is_end() const {
                if constexpr(N == std::tuple_size_v<Iterators>) {
                    return true;
                } else {
                    if (N == _master) return std::get<N>(_iter) == std::get<N>(_ends);
                    else return is_end<N+1>();
                }
            }

            void advance()  {
                std::apply([&](auto & ... iter){
                    (++iter,...);
                }, _iter);
            }


            template<unsigned int N = 0>
            bool check_iterators_valid() const {
                if constexpr (N == std::tuple_size_v<Iterators>) {
                    return true;
                } else {
                    if (std::get<N>(_iter) == std::get<N>(_ends)) return false;
                    if (std::get<N>(_iter)->first != std::get<0>(_iter)->first) return false;
                    return check_iterators_valid<N+1>();
                }
            }

            void check_after_advance() {
                while (!is_end<0>()) {
                    Entity e = get_entity();
                    if (setup_iterators<0>(e)) return;
                    advance();
                }
            }

            template<unsigned int N = 0>
            bool is_equal(const iterator &other) const {
                if constexpr(N == std::tuple_size_v<Iterators>) {
                    return true;
                } else {
                    if (N == _master) return std::get<N>(_iter) == std::get<N>(other._iter);
                    return is_equal<N+1>(other);
                }
            }
        };


        constexpr iterator begin() const {
            Iterators begins = std::apply([&](auto & ... r){return std::make_tuple(r.begin()...);}, _ranges);
            Iterators ends = std::apply([&](auto & ... r){return std::make_tuple(r.end()...);}, _ranges);
            return iterator(_reg,begins, ends, _subIds, _master);
        }
        constexpr iterator end() const {
            Iterators ends = std::apply([&](auto & ... r){return std::make_tuple(r.end()...);}, _ranges);
            return iterator(_reg,ends, ends, _subIds, _master);
        }
        constexpr operator bool() const {
            return std::apply([&](auto &...r){return (r.empty() || ...);}, _ranges);
        }
        constexpr bool empty() const {
            return std::apply([&](auto &...r){return (r.empty() || ...);}, _ranges);
        }
        constexpr std::size_t size() const {
            return get_size<0>();
        }
          // Free function helpers pro ranges machinery
        friend constexpr iterator begin(View const &v) noexcept { return v.begin(); }
        friend constexpr iterator end(View const &v) noexcept { return v.end(); }


    protected:
        Registry _reg = {};
        SubIDS _subIds = {};
        Ranges _ranges = {};
        unsigned int _master = {};

        template<unsigned int N = 0>
        std::ptrdiff_t find_best(unsigned int &ret) {
            if constexpr(N == std::tuple_size_v<Ranges>) {
                return std::numeric_limits<std::ptrdiff_t>::max();                
            } else {
                std::ptrdiff_t z = find_best<N+1>(ret);
                std::ptrdiff_t n = std::get<N>(_ranges).size();
                if (n < z) {
                    ret = N;                    
                    z = n;
                }
                return z;
            }
        }

        template< std::size_t... Is>
        constexpr Ranges load_ranges(std::index_sequence<Is...>) {
            return std::make_tuple(_reg->template all_of<Components>(std::get<Is>(_subIds))...);           
        }

        constexpr SubIDS load_subIds(std::span<const ComponentTypeID> ids) {
            SubIDS r;
            auto iter = ids.begin();
            for (auto &i: r) {
                if (iter == ids.end()) break;
                i = *iter;
                ++iter;
            }
            return r;
        }

        template<unsigned int N>
        constexpr std::size_t get_size() const {
            if constexpr(N == std::tuple_size_v<Ranges>) return 0;
            else {
                if (N == _master) return std::get<N>(_ranges).size();
            }
        }

        
    };


}