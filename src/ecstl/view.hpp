#pragma once
#include "component.hpp"
#include "utils/sequence.hpp"
#include <tuple>
#include <limits>
#include <array>
#include <optional>
#include <span>

namespace ecstl {

    // A basic concept for types that behave like pointers
    template <typename T>
    concept IsPointerLike = std::is_pointer_v<T> || requires(T p) {
    // Basic pointer operations (can be adjusted)
        {*p};        // Dereference (like std::unique_ptr, std::shared_ptr)
        {p.operator->()}; // Arrow operator (like unique_ptr, shared_ptr)
    };

    template<typename PoolsTuple>
    class View;
    

    template<IsPointerLike ... Pools>
    class View<std::tuple<Pools...> >: public std::ranges::view_interface<View<std::tuple<Pools... > > >{
    public:

        static_assert(sizeof...(Pools) >= 1, "At least one component type must be specified");

        using PoolsTuple = std::tuple<Pools...>;

        using Ranges = std::tuple<std::ranges::subrange<decltype(std::declval<Pools&>()->begin()),decltype(std::declval<Pools&>()->end())>...>;
        using Iterators = std::tuple<decltype(std::declval<Pools&>()->begin())...>;
        using Values = std::tuple<const Entity &, decltype(std::declval<Pools&>()->begin()->second)...>;


        class Sentinel {};

        class Iterator {
        public:

            using value_type = Values;
            using reference = const Values &;
            using pointer = const Values *;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;

            constexpr Iterator() = default;
            constexpr Iterator(const View *owner, Iterators iters, std::size_t master)
                :_owner(owner),_iters(std::move(iters)),_master(master) {
                    post_advance();
                }            
            constexpr ~Iterator() {
                if (_cache_filled) std::destroy_at(&_cache);
            }

            constexpr Iterator(const Iterator &other)
                :_owner(other._owner), _iters(other._iters), _master(other._master), _cache_filled(other._cache_filled) {
                    if (_cache_filled) {
                        std::construct_at(&_cache, other._cache);
                    }
            }

            constexpr Iterator &operator=(const Iterator &other) {
                if (this != &other) {
                    _owner = other._owner;
                    _iters = other._iters;
                    _master = other._master;
                    if (_cache_filled) std::destroy_at(&_cache);
                    _cache_filled = other._cache_filled;
                    if (_cache_filled) std::construct_at(&_cache, other._cache);
                }
                return *this;
            }


            constexpr bool operator==(const Iterator &other) const {
                return _iters == other._iters;
            }

            constexpr bool operator==(const Sentinel &) const {
                bool is_end = true;
                sequence_iterate<std::tuple_size_v<Iterators> >([&](auto idx){
                    if (idx == _master) is_end = std::get<idx>(_iters) == std::get<idx>(_owner->_pools)->end();                                        
                });
                return is_end;
            }

            constexpr Iterator &operator++() {
                advance();
                post_advance();
                return *this;
            }

            constexpr Iterator operator++(int) {
                auto save = *this;
                ++(*this);
                return save;
            }

            constexpr reference operator*() const {
                return _cache;
            }

            constexpr pointer operator->() const {
                return &_cache;
            }

        public:
            const View *_owner = nullptr;
            Iterators _iters = {};
            std::size_t _master = 0;
            union {
                mutable Values  _cache;                
            };
            mutable bool _cache_filled = false;

            constexpr void fill_cache() const {
                const Entity &ent = std::get<0>(_iters)->first;
                if (_cache_filled) std::destroy_at(&_cache);
                std::construct_at(&_cache,std::apply([&](auto &... iters){return std::tie(ent,iters->second...);}, _iters));
                _cache_filled = true;
            }


            constexpr std::optional<Entity> get_entity() const {
                return sequence_iterate<std::tuple_size_v<Iterators> >(std::optional<Entity>(), 
                    [&](std::optional<Entity> r, auto idx) -> std::optional<Entity> {
                        if (_master != idx) return r;                    
                        auto e = std::get<idx>(_owner->_pools)->end();
                        auto c = std::get<idx>(_iters);
                        if (e == c) return std::nullopt;
                        return c->first;
                    });
            }

            constexpr void advance() {
                sequence_iterate<std::tuple_size_v<Iterators> >([&](auto idx){
                    ++std::get<idx>(_iters);
                });
            }

            constexpr void post_advance() {
                while (true) {
                    auto r = get_entity();
                    if (!r.has_value()) break;
                    Entity ee = *r;
                    if (sequence_iterate<std::tuple_size_v<Iterators> >(true, [&](bool b, auto idx){
                        if (!b || idx == _master) return b;
                        auto &i = std::get<idx>(_iters);
                        auto &p = std::get<idx>(_owner->_pools);
                        auto e = p->end();
                        if (i != e && i->first == ee) return true;
                        i = p->find(ee);
                        return i != e;
                    })) {
                        fill_cache();
                        break;
                    }
                }
            }

        };

        constexpr View(PoolsTuple pools): _pools(pools){}

        constexpr Iterator begin() const {
            std::size_t volume = std::numeric_limits<std::size_t>::max();
            std::size_t best = 0;
            sequence_iterate<std::tuple_size_v<PoolsTuple> >([&](auto idx){
                auto &p = std::get<idx>(_pools);
                auto sz = p->size();
                if (sz < volume) {
                    best = idx;
                    volume = sz;
                }                
            });            
            return Iterator(this, std::apply([&](auto & ... ps) {
                return std::make_tuple(ps->begin()...);
            }, _pools), best);
        }

        constexpr Sentinel end() const {return {};}
        
                  // Free function helpers pro ranges machinery
        friend constexpr Iterator begin(View const &v) noexcept { return v.begin(); }
        friend constexpr Iterator end(View const &v) noexcept { return v.end(); }


    private:
        PoolsTuple _pools;
        
    };
}