#pragma once

#include <iterator>

namespace ecstl {

template<typename T, typename U>
class paired_iterator {
public:
    using T_ref = decltype(*std::declval<T>());
    using U_ref = decltype(*std::declval<U>());

    using value_type = std::pair<T_ref, U_ref>;
    using reference = std::add_lvalue_reference_t<value_type>;
    using pointer = std::add_pointer_t<value_type>;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    constexpr paired_iterator() {}
    constexpr paired_iterator(T it1, U it2) : _t_it(it1), _u_it(it2) {}
    constexpr paired_iterator(const paired_iterator &other)
        :_t_it(other._t_it),_u_it(other._u_it),_cache_filled(other._cache_filled) {
            if (_cache_filled) std::construct_at(&_cache, other._cache);
        }
    constexpr paired_iterator(paired_iterator &&other)
        :_t_it(std::move(other._t_it)),_u_it(std::move(other._u_it)),_cache_filled(other._cache_filled) {
            if (_cache_filled) std::construct_at(&_cache, std::move(other._cache));
        }
    constexpr paired_iterator &operator=(const paired_iterator &other) {
        if (this != &other) {
            _t_it = other._t_it;
            _u_it = other._u_it;
            if (_cache_filled) std::destroy_at(_cache);
            _cache_filled = other._cache_filled;
            if (_cache_filled) std::construct_at(_cache, other._cache);
        }
        return *this;
    }

    constexpr paired_iterator &operator=( paired_iterator &&other) {
        if (this != &other) {
            _t_it = std::move(other._t_it);
            _u_it = std::move(other._u_it);
            if (_cache_filled) std::destroy_at(&_cache);
            _cache_filled = other._cache_filled;
            if (_cache_filled) std::construct_at(&_cache,std::move(other._cache));
        }
        return *this;
    }

    constexpr ~paired_iterator() {
        if (_cache_filled) std::destroy_at(&_cache);
    }

    template<typename T2, typename U2>
    requires(std::is_convertible_v<T2, T> && std::is_convertible_v<U2, U>)
    constexpr paired_iterator(const paired_iterator<T2, U2> &other):_t_it(other._t_it),_u_it(other._u_it) {}

    template<typename T2, typename U2>
    requires(std::is_convertible_v<T2, T> && std::is_convertible_v<U2, U>)
    constexpr paired_iterator(paired_iterator<T2, U2> &&other):_t_it(std::move(other._t_it)),_u_it(std::move(other._u_it)) {}

    constexpr reference operator*() const {
        fill_cache();
        return _cache;
    }

    constexpr pointer operator->() const {
        fill_cache();
        return &_cache;
    }

    constexpr paired_iterator& operator++() {
        ++_t_it;
        ++_u_it;
        clear_cache();
        return *this;
    }

    constexpr paired_iterator& operator+=(difference_type diff) {
        _t_it+=diff;
        _u_it+=diff;
        clear_cache();
        return *this;
    }

    constexpr paired_iterator operator++(int) {
        paired_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr paired_iterator& operator--() {
        --_t_it;
        --_u_it;
        clear_cache();
        return *this;
    }

    constexpr paired_iterator& operator-=(difference_type diff) {
        _t_it-=diff;
        _u_it-=diff;
        clear_cache();
        return *this;
    }

    constexpr difference_type operator-(const paired_iterator &b) const {
        return _t_it - b._t_it;
    }

    constexpr paired_iterator operator--(int) {
        paired_iterator tmp = *this;
        --(*this);
        return tmp;
    }    
    constexpr bool operator==(const paired_iterator& other) const {
        return _t_it == other._t_it || _u_it == other._u_it;
    }


private:
    T _t_it;
    U _u_it;
    union {
        mutable value_type _cache;
    };
    mutable bool _cache_filled = false;

    constexpr void fill_cache() const {
        if (_cache_filled) return;
        std::construct_at(&_cache, value_type(*_t_it, *_u_it));
        _cache_filled = true;
    }

    constexpr void clear_cache() const {
        if (!_cache_filled) return;
        std::destroy_at(&_cache);
        _cache_filled = false;
    }

    template<typename , typename>
    friend class paired_iterator;
};
}

