#include "ecs.h"

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


using namespace ecstl;

ecs_registry_t * ecs_create_registry()
{
    Registry *reg = new Registry();    
    return reinterpret_cast<ecs_registry_t *>(reg);
}

Registry *cast(ecs_registry_t * reg) {
    return reinterpret_cast<Registry *>(reg);
}
const Registry *cast(const ecs_registry_t * reg) {
    return reinterpret_cast<const Registry *>(reg);
}

void ecs_destroy_registry(ecs_registry_t * registry)
{
    delete cast(registry);
}

ecs_entity_t ecs_create_entity(ecs_registry_t * reg)
{    
    return cast(reg)->create_entity().id();
}

ecs_entity_t ecs_create_named_entity(ecs_registry_t * reg, const char *name)
{
    return cast(reg)->create_entity(name).id();
}

void ecs_destroy_entity(ecs_registry_t * reg, ecs_entity_t e)
{
    cast(reg)->destroy_entity(Entity(e));
}

size_t ecs_get_entity_name(ecs_registry_t * reg, ecs_entity_t e, char *buf, size_t bufsize)
{
    auto str = cast(reg)->get_entity_name(e);
    if (buf == nullptr) return str.size()+1;
    if (bufsize == 0) return 0;    
    str = str.substr(0, bufsize-1);
    *std::copy(str.begin(), str.end(), buf) = 0;
    return str.size()+1;
}

ecs_entity_t ecs_find_entity_by_name(ecs_registry_t * reg, const char *name)
{
    auto r = cast(reg)->find_by_name(name);
    return r.has_value()?r->id():0;    
}

ecs_component_t ecs_register_component(ecs_registry_t * reg, const char *name, ecs_component_deleter_t deleter)
{
    auto r = cast(reg);
    auto type = ComponentTypeID(name);
    auto pool = r->get_component_pool<BinaryComponentView>(type, true);
    pool->set_deleter(deleter);
    return type.get_id();
}

void ecs_remove_all(ecs_registry_t * reg, ecs_component_t component)
{
    cast(reg)->remove_all_of<BinaryComponentView>(ComponentTypeID(component));
}

int ecs_store(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component, const void *data, size_t size)
{
    auto r = cast(reg);
    auto pool = r->get_component_pool<BinaryComponentView>(ComponentTypeID(component), true);
    auto val =  ConstBinaryComponentView(reinterpret_cast<const char *>(data),size);
    auto ins = pool->try_emplace(Entity(entity),val);
    if (!ins.second) {
        if (ins.first == pool->end()) return -1;
        auto del = pool->get_deleter();
        if (del) del(ins.first->second.data(), ins.first->second.size());
        std::copy(val.begin(), val.end(), ins.first->second.begin());        
    }
    return 0;
}

const void *ecs_retrieve(const ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component)
{
    auto r = cast(reg)->get<BinaryComponentView>(Entity(entity),ComponentTypeID(component));
    if (r.has_value()) {
        auto rr = r.value();
        return rr.data();
    } else {
        return NULL;
    }
}

const void *ecs_retrieve_mut(ecs_registry_t *reg, ecs_entity_t entity, ecs_component_t component)
{
    auto r = cast(reg)->get<BinaryComponentView>(Entity(entity),ComponentTypeID(component));
    if (r.has_value()) {
        auto rr = r.value();
        return rr.data();
    } else {
        return NULL;
    }
}

void ecs_remove(ecs_registry_t *reg, ecs_entity_t entity, ecs_component_t component)
{
    cast(reg)->remove<BinaryComponentView>(Entity(entity), ComponentTypeID(component));
}



template<size_t> using ViewHelper = BinaryComponentView;

template<bool is_const>
struct ViewIter {
    using Reg = std::conditional_t<is_const, const Registry *, Registry *>;
    using DataPtr = std::conditional_t<is_const, const void *, void *>;

    
    static int do_iter(Reg reg, int component_count, const ecs_component_t *components, int (*callback)(ecs_entity_t, DataPtr *data, void *context), void *context) {
        using PoolPtr = decltype(reg->template get_component_pool<BinaryComponentView>());
        using Iterator = decltype(reg->template get_component_pool<BinaryComponentView>()->begin());

        
        DataPtr results[ECS_MAX_COMPONENT_COUNT_IN_VIEW];
        PoolPtr pools[ECS_MAX_COMPONENT_COUNT_IN_VIEW];
        Iterator iters[ECS_MAX_COMPONENT_COUNT_IN_VIEW];
        Iterator ends[ECS_MAX_COMPONENT_COUNT_IN_VIEW];
        

        for (int i = 0; i < component_count; ++i) {
            auto pool = reg->template get_component_pool<BinaryComponentView>(ComponentTypeID(components[i]));
            if (pool == nullptr) return 0;            
            pools[i] = pool;
            iters[i] = pool->begin();
            ends[i] = pool->end();
        }

        std::size_t domain = static_cast<std::size_t>(-1);
        int iter_idx = 0;
        for (int i = 0; i < component_count; ++i) {
            auto sz = pools[i]->size();
            if (sz < domain) {
                domain = sz;
                iter_idx = i;
            }
        }

        while (iters[iter_idx] != ends[iter_idx]) {
            auto e = iters[iter_idx]->first;
            int i;
            for (i = 0; i < component_count; ++i) {
                if (iters[i]->first != e) {
                    for (i = 0; i < component_count; ++i) if (i != iter_idx) {
                        iters[i] = pools[i]->find(e);                        
                    }
                    
                }
            }

            for (i = 0; i < component_count; ++i) {
                if (iters[i] == ends[i]) break;
                results[i] = iters[i]->second.data();
                ++iters[i];
            }
            if (i == component_count) {
                int cbr = callback(e.id(), results, context);
                if (cbr) return cbr;
            }
        }
        return 0;
    }
};



int ecs_view_iterate(ecs_registry_t *reg, int component_count, const ecs_component_t *components, int (*callback)(ecs_entity_t, const void **data, void *context), void *context)
{
    if (component_count < 1) return -1;
    return ViewIter<true>::do_iter(cast(reg),component_count,components, callback, context);
    
}
int ecs_view_iterate_mut(ecs_registry_t *reg, int component_count, const ecs_component_t *components, int (*callback)(ecs_entity_t, void **data, void *context), void *context)
{
    if (component_count < 1) return -1;
    return ViewIter<false>::do_iter(cast(reg),component_count,components, callback, context);
    
}

template<std::size_t ... Is>
static int make_group_impl(Registry *reg, const ecs_component_t *components, std::index_sequence<Is...> sq) {
    ComponentTypeID variants[sizeof...(Is)];
    std::transform(components, components+sq.size(), variants, [](auto c){return ComponentTypeID(c);});

    bool r = reg->group<ViewHelper<Is>...>(std::span<ComponentTypeID>(variants, sq.size()));
    return r?1:0;
}

int ecs_has(const ecs_registry_t *reg, ecs_entity_t entity, int component_count, const ecs_component_t *components) {
    auto r = cast(reg);
    Entity e(entity);
    for (int i = 0; i < component_count; ++i) {
        if (!r->has<BinaryComponentView>(e, {ComponentTypeID(components[i])})) return 0;
    }
    return 1;
}


int ecs_group(ecs_registry_t *reg, int component_count, const ecs_component_t *components)
{
    if (component_count < 2) return 0;
    auto r = cast(reg);
    for (int i = 0; i < component_count; ++i) {
        r->group_entities<BinaryComponentView>(ComponentTypeID(components[i]),[&](const Entity &e, const auto &){
            for (int j = 0; j < component_count; ++j) {
                if (j!= i && !r->has<BinaryComponentView>(e,{ComponentTypeID(components[i])})) return false;
            }
            return true;
        });
    }



    return 0;
}
