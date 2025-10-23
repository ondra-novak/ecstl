#pragma once
#include <functional>

namespace ecstl {


    template<typename T>
    class FixedPrimitiveArray: public std::span<T> {
    public:

        constexpr FixedPrimitiveArray() = default;
        constexpr FixedPrimitiveArray(std::size_t cnt):std::span<T>(cnt?new T[cnt]:nullptr, cnt) {}
        constexpr ~FixedPrimitiveArray() {
            T *ptr = this->data();
            delete [] ptr;
        }

        constexpr FixedPrimitiveArray(const FixedPrimitiveArray &other)
            :FixedPrimitiveArray(other._size())
        {
            std::copy(other.begin(), other.end(), this->begin());
        }
        constexpr FixedPrimitiveArray &operator=(const FixedPrimitiveArray &other) {
            if (this != &other) {
                std::destroy_at(this);
                std::construct_at(this, other);
            }
            return *this;
        }
        constexpr FixedPrimitiveArray(FixedPrimitiveArray &&other):std::span<T>(std::move(other)) {
            other.std::span<T>::operator=(std::span<T>());
        }


        constexpr FixedPrimitiveArray &operator=(FixedPrimitiveArray &&other) {
            if (this != &other) {
                std::destroy_at(this);
                std::construct_at(this, std::move(other));
            }
            return *this;
        }
        


    };


    template<typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K> >
    class OpenHashMap {
    
        
        enum class State {
            empty = 0, 
            occupied,
            tombstone
        };

        struct KeyValue {
            const K first;
            V second;
        };

        struct Item {
            union {
                KeyValue key_value;
            };
            constexpr Item() {}
            constexpr ~Item() {}
        };

    public:

        constexpr OpenHashMap() = default;
        constexpr OpenHashMap(std::size_t size, Hash hasher = {}, Equal equal = {})
            :_hasher(std::move(hasher))
            ,_eq(std::move(equal))
            ,_items(size)
            ,_state(size+1)
            ,_size(0)
            {
                for (std::size_t i = 0; i<size;++i) _state[i] = State::empty;                
                _state[size] = State::occupied; //after last item there is occupied item serves as stop
            }
        
            
        constexpr ~OpenHashMap() {
            clear();
        }
        constexpr OpenHashMap(const OpenHashMap &other):OpenHashMap(other.size(), other._hasher, other._eq) {
            for (const auto &[k, v]: other) {
                emplace(k, v);
            }
        }
        constexpr OpenHashMap &operator=(const OpenHashMap &other) {
            if (this != &other) {
                clear();
                _hasher = other._hasher;
                _eq = other._eq;                
                for (const auto &[k, v]: other) {
                    emplace(k, v);
                }
            }
            return *this;
        }
        constexpr OpenHashMap(OpenHashMap &&other)
            :_hasher(std::move(other._hasher))
            ,_eq(std::move(other._eq))
            ,_items(std::move(other._items))
            ,_state(std::move(other._state))
            ,_size(std::move(other._size)) {
                other._size = 0;
            }
        constexpr OpenHashMap &operator=(OpenHashMap &&other) {
            if (this != &other) {
                clear();
                _hasher = std::move(other._hasher);
                _eq = std::move(other._eq);
                _items = std::move(other._items);
                _state = std::move(other._state);
                _size = std::move(other._size);
                other._size = 0;
            }
            return *this;
        }


        template<bool is_const>
        class iterator_base  {
        public:
            using iterator_concept  = std::bidirectional_iterator_tag;
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type        = std::conditional_t<is_const,const KeyValue, KeyValue >;
            using difference_type   = std::ptrdiff_t;
            using pointer           = value_type *;
            using reference         = value_type &;
            using owner             = std::conditional_t<is_const,const OpenHashMap *, OpenHashMap *>;
            
            constexpr iterator_base(owner own, std::size_t ofs):_owner(own), _offset(ofs) {}


            constexpr value_type & operator*() const {
                return _owner->_items[_offset].key_value;
            }
            constexpr value_type * operator->() const {
                return &_owner->_items[_offset].key_value;
            }
            constexpr bool operator==(const iterator_base &other) const {
                return _owner == other._owner && _offset == other._offset;
            }
            constexpr iterator_base & operator++() {
                do {
                    ++_offset;
                } while (_owner->_state[_offset] != State::occupied);
                return *this;
            }   

            constexpr iterator_base & operator--() {
                do {
                    --_offset;
                } while (_offset > 0 && _owner->_items[_offset].state != State::occupied);
                return *this;
            }   

            constexpr iterator_base operator++(int) {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            constexpr iterator_base operator--(int) {
                auto tmp = *this;
                --*this;
                return tmp;
            }

            operator iterator_base<true>() const requires(!is_const) {
                return iterator_base<true>(_owner, _offset);
            }

        protected:
            owner _owner;
            std::size_t _offset;

            friend class OpenHashMap;
        };

        using iterator = iterator_base<false>;
        using const_iterator = iterator_base<true>;

        template<typename Key, typename ... Args>
        constexpr auto try_emplace(Key &&key, Args && ... args) {
            if ((_items.size()*3/5) <= size()) {
                expand();
            }
            auto idx = map_key(key);            
            auto start = idx;
            auto tombstone_idx = std::size_t(-1);
            do {
                switch (_state[idx]) {
                case State::tombstone:
                    if (tombstone_idx == std::size_t(-1)) tombstone_idx = idx;
                    break;
                case State::empty:
                    if (tombstone_idx == std::size_t(-1)) tombstone_idx = idx;
                    std::construct_at(&_items[tombstone_idx].key_value, std::move(key), V(std::forward<Args>(args)...));
                    _state[tombstone_idx] = State::occupied;
                    ++_size;
                    return std::pair(iterator(this, tombstone_idx), true);
                case State::occupied:
                    if (_eq(_items[idx].key_value.first, key)) {
                        return std::pair(iterator(this, idx), false);
                    }
                    break;
                }
                idx = (idx+1) % _items.size();
            } while (idx != start);
            //unreachable code
            return std::pair(end(), false);

        }

        template<typename Key, typename... Args>
        constexpr auto emplace(Key &&key, Args && ... args) {
            return try_emplace(std::forward<Key>(key), std::forward<Args>(args)...);
        }

        constexpr iterator begin() {
            std::size_t idx = 0;
            while (idx < _items.size() && _state[idx] != State::occupied) ++idx;
            return iterator(this, idx);
        }

        constexpr iterator end() {
            return iterator(this, _items.size());
        }

        constexpr const_iterator begin() const {
            std::size_t idx = 0;
            while (idx < _items.size() && _state[idx] != State::occupied) ++idx;
            return const_iterator(this, idx);
        }

        constexpr const_iterator end() const {
            return const_iterator(this, _items.size());
        }

        constexpr std::size_t size() const {
            return _size;
        }

        constexpr std::size_t capacity() const {
            return _items.size();
        }

        constexpr iterator find(const K &key) {
            auto idx = find_index(key);
            if (idx == std::size_t(-1)) return end();
            return iterator(this, idx);
        }

        constexpr const_iterator find(const K &key) const {
            auto idx = find_index(key);
            if (idx == std::size_t(-1)) return end();
            return const_iterator(this, idx);
        }

        template<bool is_const>
        constexpr iterator_base<is_const> erase(iterator_base<is_const> it) {
            auto idx = it._offset;
            erase_index(idx);
            return ++it;
        }

        constexpr void erase(const K &key) {
            auto idx = find_index(key);
            if (idx != std::size_t(-1)) {
                erase_index(idx);
            }
        }

        constexpr void clear() {
            std::size_t ofs = 0;
            for (auto &item: _items) { 
                if (_state[ofs] == State::occupied) {
                            std::destroy_at(&item.key_value);
                }
                ++ofs;
            }
            _size = 0;
        }

        constexpr V& operator[](const K &key) {
            auto res = try_emplace(key, V{});
            return res.first->second;
        }


    protected:
        
        [[no_unique_address]] Hash _hasher = {};
        [[no_unique_address]] Equal _eq = {};

        FixedPrimitiveArray<Item> _items;
        FixedPrimitiveArray<State> _state;
        std::size_t _size = 0;

        static constexpr std::array<size_t, 28> prime_sizes = {
            5ul, 11ul, 23ul, 47ul, 97ul, 197ul, 397ul,
            797ul, 1597ul, 3203ul, 6421ul, 12853ul, 25717ul,
            51437ul, 102877ul, 205759ul, 411527ul, 823117ul,
            1646237ul, 3292489ul, 6584983ul, 13169977ul,
            26339969ul, 52679969ul, 105359939ul, 210719881ul,
            421439783ul, 842879579ul
        };

        static constexpr size_t next_capacity(size_t current) {
            for (size_t p : prime_sizes)
                if (p > current) return p;
            return current * 2 + 1; 
        }


        constexpr std::size_t map_key(const K &k) const {
            std::size_t hash = _hasher(k);
            if constexpr(sizeof(std::size_t) == 4) {
                constexpr uint32_t multiplier = 2654435761U;
                hash ^= (hash >> 5) ^ (hash << 7);
                hash *= multiplier;
            } else {
                constexpr uint64_t multiplier = 11400714819323198485ULL;
                hash ^= (hash >> 7) ^ (hash << 11);
                hash *= multiplier;
            }
            return hash % _items.size();
        }

        constexpr void expand() {
            auto newsz = next_capacity(_items.size());
            OpenHashMap newMap(newsz, std::move(_hasher), std::move(_eq));
            for (auto &kv : *this) {
                newMap.try_emplace(std::move(kv.first), std::move(kv.second));
            }            
            *this = std::move(newMap);
        }

        constexpr std::size_t find_index(const K &key) const {
            if (_items.size() == 0) return std::size_t(-1);
            auto idx = map_key(key);
            auto start = idx;
            do {
                switch (_state[idx]) {
                case State::tombstone:
                    break;
                case State::empty:
                    return std::size_t(-1);
                case State::occupied:
                    if (_eq(_items[idx].key_value.first, key)) {
                        return idx;
                    }
                    break;
                }
                idx = (idx+1) % _items.size();
            } while (idx != start);
            return std::size_t(-1);
        }

        constexpr void erase_index(std::size_t idx) {
            if (idx >= _items.size() || _state[idx] != State::occupied) return;
            std::destroy_at(&_items[idx].key_value);
            _state[idx] = State::tombstone;
            --_size;
        }



    };
    

}