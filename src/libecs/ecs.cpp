#include "ecs.h"
#include "binary_component.hpp"
#include "c_teleport.hpp"


using namespace ecstl;

ecs_registry_t * ecs_create_registry()
{
    Registry *reg = new Registry();    
    return reinterpret_cast<ecs_registry_t *>(reg);
}

void ecs_destroy_registry(ecs_registry_t * registry)
{
    delete cast_from_c(registry);
}

ecs_entity_t ecs_create_entity(ecs_registry_t * reg)
{    
    return cast_from_c(reg)->create_entity().id();
}

ecs_entity_t ecs_create_named_entity(ecs_registry_t * reg, const char *name)
{
    return cast_from_c(reg)->create_entity(name).id();
}

void ecs_destroy_entity(ecs_registry_t * reg, ecs_entity_t e)
{
    cast_from_c(reg)->destroy_entity(Entity(e));
}

size_t ecs_get_entity_name(ecs_registry_t * reg, ecs_entity_t e, char *buf, size_t bufsize)
{
    auto str = cast_from_c(reg)->get_entity_name(e);
    if (buf == nullptr) return str.size()+1;
    if (bufsize == 0) return 0;    
    str = str.substr(0, bufsize-1);
    *std::copy(str.begin(), str.end(), buf) = 0;
    return str.size()+1;
}

ecs_entity_t ecs_find_entity_by_name(ecs_registry_t * reg, const char *name)
{
    auto r = cast_from_c(reg)->find_by_name(name);
    return r.has_value()?r->id():0;    
}

ecs_component_t ecs_register_component(ecs_registry_t * reg, const char *name, ecs_component_deleter_t deleter)
{
    auto r = cast_from_c(reg);
    auto type = ComponentTypeID(name);
    auto pool = r->get_component_pool<BinaryComponentView>(type, true);
    pool->set_deleter(deleter);
    return type.get_id();
}

void ecs_unregister_component(ecs_registry_t * reg, ecs_component_t component)
{
    cast_from_c(reg)->remove_all_of<BinaryComponentView>(ComponentTypeID(component));
}

int ecs_store(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component, const void *data, size_t size)
{
    auto r = cast_from_c(reg);
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
    auto r = cast_from_c(reg)->get<BinaryComponentView>(Entity(entity),ComponentTypeID(component));
    if (r.has_value()) {
        auto rr = r.value();
        return rr.data();
    } else {
        return NULL;
    }
}

void *ecs_retrieve_mut(ecs_registry_t *reg, ecs_entity_t entity, ecs_component_t component)
{
    auto r = cast_from_c(reg)->get<BinaryComponentView>(Entity(entity),ComponentTypeID(component));
    if (r.has_value()) {
        auto rr = r.value();
        return rr.data();
    } else {
        return NULL;
    }
}

void ecs_remove(ecs_registry_t *reg, ecs_entity_t entity, ecs_component_t component)
{
    cast_from_c(reg)->remove<BinaryComponentView>(Entity(entity), ComponentTypeID(component));
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
    return ViewIter<true>::do_iter(cast_from_c(reg),component_count,components, callback, context);
    
}
int ecs_view_iterate_mut(ecs_registry_t *reg, int component_count, const ecs_component_t *components, int (*callback)(ecs_entity_t, void **data, void *context), void *context)
{
    if (component_count < 1) return -1;
    return ViewIter<false>::do_iter(cast_from_c(reg),component_count,components, callback, context);
    
}

template<std::size_t ... Is>
static int make_group_impl(Registry *reg, const ecs_component_t *components, std::index_sequence<Is...> sq) {
    ComponentTypeID variants[sizeof...(Is)];
    std::transform(components, components+sq.size(), variants, [](auto c){return ComponentTypeID(c);});

    bool r = reg->group<ViewHelper<Is>...>(std::span<ComponentTypeID>(variants, sq.size()));
    return r?1:0;
}

int ecs_has(const ecs_registry_t *reg, ecs_entity_t entity, int component_count, const ecs_component_t *components) {
    auto r = cast_from_c(reg);
    Entity e(entity);
    for (int i = 0; i < component_count; ++i) {
        if (!r->has<BinaryComponentView>(e, {ComponentTypeID(components[i])})) return 0;
    }
    return 1;
}


int ecs_group(ecs_registry_t *reg, int component_count, const ecs_component_t *components)
{
    if (component_count < 2) return 0;
    auto r = cast_from_c(reg);
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
