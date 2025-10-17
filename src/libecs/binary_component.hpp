#pragma once
#include "../ecstl/ecstl.hpp"
#include <span>

using BinaryComponentView = std::span<char>;
using ConstBinaryComponentView = std::span<const char>;

namespace ecstl {

    template<typename K,  typename Hasher, typename Equal  >
    class IndexedFlatMap<K, BinaryComponentView, Hasher, Equal> {
    public:


        template<bool is_const>
        class bin_iterator {
        public:
            using iterator_concept  = std::random_access_iterator_tag;
            using iterator_category = std::random_access_iterator_tag;
            using value_type        = std::conditional_t<is_const,const ConstBinaryComponentView, BinaryComponentView >;
            using difference_type   = std::ptrdiff_t;
            using pointer           = void;
            using reference         = value_type;
            using iter_ptr          = std::conditional_t<is_const,const char *, char *>;
            
            constexpr bin_iterator() = default;
            constexpr bin_iterator(iter_ptr pos, std::size_t step):_pos(pos),_step(step) {}
            constexpr bool operator==(const bin_iterator &other) const = default;
            constexpr value_type operator*() const {
                return value_type(_pos, _step);
            }
            bin_iterator &operator++() {
                _pos+=_step;
                return *this;
            }
            bin_iterator &operator--() {
                _pos-=_step;
                return *this;
            }
            bin_iterator &operator+=(difference_type diff) {
                _pos+=_step*diff;
                return *this;
            }
            bin_iterator &operator-=(difference_type diff) {
                _pos-=_step*diff;
                return *this;
            }
            difference_type operator-(const bin_iterator &other) const {
                return (_pos - other._pos)/_step;
            }
            bin_iterator operator++(int) {
                auto tmp = *this; operator++(); return tmp;
            }
            bin_iterator operator--(int) {
                auto tmp = *this; operator++(); return tmp;
            }
            operator bin_iterator<true>() const requires(!is_const) {
                return bin_iterator<true>(_pos, _step);
            }


        protected:
            iter_ptr _pos = nullptr;
            std::size_t _step = 1;
        };

        using iterator = paired_iterator<const K *, bin_iterator<false> >;
        using const_iterator = paired_iterator<const K *, bin_iterator<true> >;
        using insert_result = std::pair<iterator, bool>;


        template<typename Key>
        requires (std::is_constructible_v<K, Key>)
        constexpr insert_result try_emplace(Key &&key, const ConstBinaryComponentView &value) {
        auto iter = _index.find(key);
        if (iter != _index.end()) {            
            return insert_result(build_iterator(iter->second), false);
        }
        if (_values.empty()) {
            _component_size = value.size();
        } else if (value.size() != _component_size) {
            return insert_result(end(), false);
        }

        auto pos = _keys.size();
        _keys.emplace_back(std::forward<Key>(key));
        _values.insert(_values.end(), value.begin(), value.end());
        _index.emplace(_keys.back(), pos);
        return insert_result(build_iterator(pos), true);
    }

    constexpr IndexedFlatMap() = default;
    constexpr ~IndexedFlatMap() {
        clear();
    }

    template<typename Key>
    requires(std::is_constructible_v<K, Key> )
    constexpr auto emplace(Key &&key, const ConstBinaryComponentView &value) {
        return try_emplace(std::forward<Key>(key), value);        

    }

    constexpr iterator find(const K &key) {
        auto iter = _index.find(key);
        if (iter == _index.end()) return end();
        return build_iterator(iter->second);
    }

    constexpr const_iterator find(const K &key) const {
        auto iter = _index.find(key);
        if (iter == _index.end()) return end();
        return build_iterator(iter->second);
    }

    constexpr bool erase(const K &key) {
        auto iter = _index.find(key);
        if (iter == _index.end()) return false;
        std::size_t pos = iter->second;
        _index.erase(iter);        
        auto datapos = pos * _component_size;
        auto enddatapos = _values.size() - _component_size;
        if (_deleter) _deleter(_values.data()+datapos, _component_size);
        if (pos+1 < _keys.size()) {
            _index[_keys.back()] = pos;
            _keys[pos] = std::move(_keys.back());
            _values[pos] = std::move(_values.back());
            std::copy_n(_values.data()+enddatapos, _component_size, _values.data()+datapos);
        }
        _keys.pop_back();
        _values.resize(enddatapos);
        return true;
    }

    constexpr iterator erase(iterator it) {
        erase(it->first);
        return it;
    }
    constexpr const_iterator erase(const_iterator it) {
        erase(it->first);
        return it;
    }
    
    constexpr insert_result insert(std::pair<K, ConstBinaryComponentView> it) {
        return try_emplace(std::move(it.first), std::move(it.second));
    }

    constexpr std::size_t size() const {return _keys.size();}

    constexpr void reserve(std::size_t sz) {
        _keys.reserve(sz);
        _values.reserve(sz*_component_size);

    }

    constexpr iterator begin() {return build_iterator(0);}
    constexpr iterator end() {return build_iterator(_keys.size());}
    constexpr const_iterator cbegin() const {return build_iterator(0);}
    constexpr const_iterator cend() const {return build_iterator(_keys.size());}
    constexpr const_iterator begin() const {return build_iterator(0);}
    constexpr const_iterator end() const {return build_iterator(_keys.size());}

    void set_deleter(ecs_component_deleter_t del) {
        _deleter = del;
    }

    ecs_component_deleter_t get_deleter() {
        return _deleter;
    }
    constexpr void clear() {
        if (_deleter) {
            auto sz = _values.size();
            for (std::size_t i = 0; i < sz; i+=_component_size) {
                _deleter(_values.data()+i, _component_size);
            }
        }
        _index.clear();
        _values.clear();
        _keys.clear();
    }

    protected:
        std::size_t _component_size = 0;
        OpenHashMap<K, std::size_t, Hasher, Equal> _index;
        std::vector<K> _keys;
        std::vector<char> _values;
        ecs_component_deleter_t _deleter = nullptr;

    constexpr iterator build_iterator(std::size_t pos) {
        auto vit = bin_iterator<false>(_values.data(), _component_size);
        std::advance(vit, pos);
        return iterator(_keys.data()+pos, vit);
    }

    constexpr const_iterator build_iterator(std::size_t pos) const {
        auto vit = bin_iterator<true>(_values.data(), _component_size);
        std::advance(vit, pos);
        return const_iterator(_keys.data()+pos, vit);
    }

    };


}
