#pragma once
#include <functional>

namespace ecstl {


    template<typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K> >
    class OpenHashMap {
    
        
        enum class State {
            empty,
            occupied,
            tombstone
        };

        struct KeyValue {
            const K first;
            V second;
        };

        struct Item {
            State state;
            union {
                KeyValue key_value;
            };
            constexpr Item():state(State::empty) {}
            constexpr  ~Item() {
                clear();
            }
            constexpr void clear() {
                if (state == State::occupied) {
                    std::destroy_at(&key_value);
                    state = State::empty;
                }
            }
            constexpr Item(Item &&other) noexcept : state(other.state) {
                if (state == State::occupied) {
                    std::construct_at(&key_value, std::move(other.key_value));
                    other.state = State::empty;
                }
            }
            constexpr Item & operator=(Item &&other) noexcept {
                clear();
                state = other.state;
                if (state == State::occupied) {
                    std::construct_at(&key_value, std::move(other.key_value));
                    other.state = State::empty;
                }
                return *this;
            }
        };

        using Vec = std::vector<Item>;

    public:
        


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
                } while (_offset < _owner->_items.size() && _owner->_items[_offset].state != State::occupied);
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
                switch (_items[idx].state) {
                case State::tombstone:
                    if (tombstone_idx == std::size_t(-1)) tombstone_idx = idx;
                    break;
                case State::empty:
                    if (tombstone_idx == std::size_t(-1)) tombstone_idx = idx;
                    std::construct_at(&_items[tombstone_idx].key_value, std::move(key), V(std::forward<Args>(args)...));
                    _items[tombstone_idx].state = State::occupied;
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
            return std::pair(end(), true);

        }

        template<typename Key, typename... Args>
        constexpr auto emplace(Key &&key, Args && ... args) {
            return try_emplace(std::forward<Key>(key), std::forward<Args>(args)...);
        }

        constexpr iterator begin() {
            std::size_t idx = 0;
            while (idx < _items.size() && _items[idx].state != State::occupied) ++idx;
            return iterator(this, idx);
        }

        constexpr iterator end() {
            return iterator(this, _items.size());
        }

        constexpr const_iterator begin() const {
            std::size_t idx = 0;
            while (idx < _items.size() && _items[idx].state != State::occupied) ++idx;
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
            for (auto &item: _items) {
                item.clear();
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

        std::vector<Item> _items;
        std::size_t _size = 0;

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
            auto newsz = std::max<std::size_t>(5,_items.size())*2+1;
            auto old = std::move(_items);
            _items.resize(newsz);
            for (Item &c: old) {
                if (c.state == State::occupied) {
                    try_emplace(std::move(c.key_value.first), std::move(c.key_value.second));
                }
            }            
        }

        constexpr std::size_t find_index(const K &key) const {
            if (_items.size() == 0) return std::size_t(-1);
            auto idx = map_key(key);
            auto start = idx;
            do {
                switch (_items[idx].state) {
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
            if (idx >= _items.size() || _items[idx].state != State::occupied) return;
            std::destroy_at(&_items[idx].key_value);
            _items[idx].state = State::tombstone;
            --_size;
        }



    };
    

}