#pragma once

#include "paired_iterator.hpp"
#include "utils/open_hash_map.hpp"
#include <vector>

namespace ecstl {

template<typename K, typename V, typename Hasher = std::hash<K>, typename Equal = std::equal_to<K> >
class IndexedFlatMap {
public:

    using VectorK = std::vector<K>;
    using VectorV = std::vector<V>;

    using iterator = paired_iterator<typename VectorK::const_iterator, typename VectorV::iterator>;
    using const_iterator = paired_iterator<typename VectorK::const_iterator, typename VectorV::const_iterator>;
    using insert_result = std::pair<iterator, bool>;
    


    template<typename Key, typename ... Args>
    requires (std::is_constructible_v<K, Key> && std::is_constructible_v<V, Args...>)
    constexpr insert_result try_emplace(Key &&key, Args && ... args) {
        auto iter = _index.find(key);
        if (iter != _index.end()) {            
            return insert_result(build_iterator(iter->second), false);
        }
        auto pos = _keys.size();
        _keys.emplace_back(std::forward<Key>(key));
        _values.emplace_back(std::forward<Args>(args)...);
        _index.emplace(_keys.back(), pos);
        return insert_result(build_iterator(pos), true);
    }

    template<typename Key, typename ... Args>
    requires(std::is_constructible_v<K, Key> && std::is_constructible_v<V, Args...>)
    constexpr auto emplace(Key &&key, Args && ... args) {
        return try_emplace(std::forward<Key>(key), std::forward<Args>(args)...);        

        
    }

    constexpr iterator begin() {return build_iterator(0);}
    constexpr iterator end() {return build_iterator(_keys.size());}
    constexpr const_iterator cbegin() const {return build_iterator(0);}
    constexpr const_iterator cend() const {return build_iterator(_keys.size());}
    constexpr const_iterator begin() const {return build_iterator(0);}
    constexpr const_iterator end() const {return build_iterator(_keys.size());}

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
        if (pos+1 < _keys.size()) {
            _index[_keys.back()] = pos;
            _keys[pos] = std::move(_keys.back());
            _values[pos] = std::move(_values.back());
        }
        _keys.pop_back();
        _values.pop_back();
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
    
    constexpr insert_result insert(std::pair<K, V> it) {
        return try_emplace(std::move(it.first), std::move(it.second));
    }

    constexpr std::size_t size() const {return _keys.size();}

    constexpr void reserve(std::size_t sz) {
        _keys.reserve(sz);
        _values.reserve(sz);

    }

protected:
    OpenHashMap<K, std::size_t, Hasher, Equal> _index;
    std::vector<K> _keys;
    std::vector<V> _values;

    constexpr iterator build_iterator(std::size_t pos) {
        auto kit = _keys.begin();
        auto vit = _values.begin();
        std::advance(kit, pos);
        std::advance(vit, pos);
        return iterator(kit, vit);
    }

    constexpr const_iterator build_iterator(std::size_t pos) const {
        auto kit = _keys.begin();
        auto vit = _values.begin();
        std::advance(kit, pos);
        std::advance(vit, pos);
        return const_iterator(kit, vit);
    }

};


}
