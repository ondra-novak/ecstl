#pragma once

#include "component.hpp"
#include "optional_ref.hpp"
#include "view.hpp"

#include <string>
#include <concepts>
#include <unordered_map>
#include <memory>
#include <algorithm>

namespace ecstl {

template<typename T>
struct HashOfKey {
    constexpr std::size_t operator()(const T &val) const {return get_hash(val);}
};


class EntityName {
public:

    constexpr EntityName(std::string_view n):name(n.begin(), n.end()) {}
    constexpr operator std::string_view() const {return std::string_view(name.data(), name.size());}  
    constexpr bool operator==(const EntityName &other) const {
        return static_cast<std::string_view>(*this) == static_cast<std::string_view>(other);
    }
    template<typename IO>
    friend IO &operator<<(IO &io, const EntityName &en)  {      
        return (io << static_cast<std::string_view>(en));
    }

protected:
    std::vector<char> name;
};

#if __cpp_lib_constexpr_memory >= 202202L
template<typename T>
using UniquePtrConstexpr = std::unique_ptr<T>;
template<typename T, typename ... Args>
constexpr UniquePtrConstexpr<T> make_unique_constexpr(Args && ... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
#else
template<typename T>
class UniquePtrConstexpr {
public:
    constexpr UniquePtrConstexpr() = default;
    constexpr UniquePtrConstexpr(T *ptr):_ptr(ptr) {}
    constexpr UniquePtrConstexpr(const UniquePtrConstexpr &) = delete;
    constexpr UniquePtrConstexpr(UniquePtrConstexpr &&other) noexcept : _ptr(other._ptr) {
        other._ptr = nullptr;
    }
    template<typename U>
    requires std::is_convertible_v<U*, T*>
    constexpr UniquePtrConstexpr(UniquePtrConstexpr<U> &&other) noexcept : _ptr(other._ptr) {
        other._ptr = nullptr;
    }
    constexpr UniquePtrConstexpr & operator=(const UniquePtrConstexpr &) = delete;
    constexpr UniquePtrConstexpr & operator=(UniquePtrConstexpr &&other) noexcept {
        if (this != &other) {
            delete _ptr;
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }
    constexpr ~UniquePtrConstexpr() {
        delete _ptr;
    }
    constexpr std::add_pointer_t<T> operator->() const {return _ptr;}
    constexpr std::add_lvalue_reference_t<T> operator*() const {return *_ptr;}
    constexpr explicit operator bool() const {return _ptr != nullptr;}
    constexpr std::add_pointer_t<T> get() const {return _ptr;}

protected:
    T *_ptr=nullptr;

    template<typename U>
    friend class UniquePtrConstexpr;
};
template<typename T, typename ... Args>
constexpr UniquePtrConstexpr<T> make_unique_constexpr(Args && ... args) {
    return UniquePtrConstexpr<T>(new T(std::forward<Args>(args)...));
}


#endif

///Concepts for component visitor functions (non-const)
template<typename T>
concept ComponentVisitor = std::is_invocable_v<T, AnyRef>
    || std::is_invocable_v<T, AnyRef, ComponentTypeID>
    || std::is_invocable_v<T, AnyRef, ComponentTypeID, ComponentTypeID>;

///Concepts for component visitor functions (const)
template<typename T>
concept ConstComponentVisitor = std::is_invocable_v<T, ConstAnyRef>
    || std::is_invocable_v<T, ConstAnyRef, ComponentTypeID>
    || std::is_invocable_v<T, ConstAnyRef, ComponentTypeID, ComponentTypeID>;


    ///Generic registry class template
/**
 * GenericRegistry is the main class template of the ECS database.
 * It manages entities and their associated components.
 * Template parameters:
 * - PoolType: a template with one parameter that provides component pool implementation
 * - Storage: a template with two parameters (key and value) that provides storage for component pools
 */

 
template<template<class> class PoolType, template<class,class> class Storage >
class GenericRegistry {
public:

    constexpr GenericRegistry() = default;

    ///Key for component storage
    struct Key {
        /// Type of the component - ID is computed from type T
        ComponentTypeID _type_id;
        /// variant of the component - ID is provided by user - allows multiple components of the same type
        /** This can be useful for components imported from C ABI or other sources where type information is not available,
         * or where the component is wrapped in a type-erased container like AnyRef.
         *  Default value is 0.
         */
        ComponentTypeID _variant_id;    
        constexpr auto operator<=>(const Key&) const = default;
        constexpr friend std::size_t get_hash(const Key &key ) {
            return (key._type_id+key._variant_id).get_id();
        }
    };

    ///Type of component pool pointer
    using PPool = UniquePtrConstexpr<IComponentPool>;

    ///Create a new entity
    static constexpr Entity create_entity() {
        return Entity::create();
    }

    ///Create a new entity with a name
    /** @param name Name of the entity
     *  @return Newly created entity with the given name
     *  The name is stored in a component of type EntityName
     */ 
    constexpr Entity create_entity(std::string_view name) {
        auto e = create_entity();
        set<EntityName>(e, EntityName{std::string(name)});
        return e;
    }

    ///Destroy an entity and all its components
    /**
     * @param entity Entity to be destroyed
     * This removes all components associated with the entity.
     */
    constexpr void destroy_entity(Entity entity) {
        for (auto &[k,v]: _storage) {
            v->erase(entity);
        }
    }

    ///Get the name of an entity (if it has EntityName component)
    /** @param entity Entity whose name is to be retrieved
     *  @return Name of the entity or empty string_view if not set
     */
    constexpr std::string_view get_entity_name(Entity entity) const {
        auto c = get<EntityName>(entity);
        if (c) return c.value().name;
        else return {};
    }

    constexpr void set_entity_name(Entity entity, std::string_view name) {
        set<EntityName>(entity, EntityName{std::string(name)});
    }

    ///Add or set a component for an entity
    /**
     * If component doesn't exist, it is created. If it exists, it is updated.
     * @tparam T Type of the component to be removed
     * @param e Entity to which the component is to be added or updated
     * @param data Data of the component to be set
     * @return Reference to the component data stored in the registry
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr T & set(Entity e, T data) {
        return set<T>(e, {}, std::move(data));
    }

    ///Add or set a component for an entity with a specific component variant ID
    /**
     * If component doesn't exist, it is created. If it exists, it is updated.
     * @tparam T Type of the component to be removed
     * @param e Entity to which the component is to be added or updated
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @param data Data of the component to be set
     * @return Reference to the component data stored in the registry
     */
    template<typename T>
    constexpr T & set(Entity e, ComponentTypeID variant_id, const T &data) {
        return set<T>(e, variant_id, T(data));
    }

    ///Add or set a component for an entity with a specific component variant ID (rvalue overload)
    /**
     * If component doesn't exist, it is created. If it exists, it is updated.
     * @tparam T Type of the component to be removed
     * @param e Entity to which the component is to be added or updated
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @param data Data of the component to be set (moved)
     * @return Reference to the component data stored in the registry
     */
    template<typename T>
    constexpr T & set(Entity e, ComponentTypeID variant_id, T &&data) {
        PoolType<T> *p = create_component_if_needed<T>(variant_id);
        auto r = p->try_emplace(e, std::move(data));
        if (!r.second) r.first->second = std::move(data);
        return r.first->second;
    }

    ///Add a component for an entity
    /** @tparam T Type of the component to be added
     *  @tparam Arg0 Type of the first argument for component construction or ComponentTypeID for variant ID
     *  @tparam Args Types of additional arguments for component construction
     *  @param e Entity to which the component is to be added
     *  @param variant_id Optional component variant ID to differentiate multiple components of the same type. 
     *          If not provided, default component variant ID (0) is used. In this case, 
     *          Arg0 is treated as the first argument for component construction.
     *  @param args Arguments for constructing the component
     *  @return Reference to the component data stored in the registry
     * This overload uses default component variant ID (0) if variant_id is not provided.
     */
    template<typename T, typename Arg0, typename ... Args>
    constexpr T &emplace(Entity e, Arg0 &&variant_id, Args && ... args) {
        if constexpr(std::is_same_v<std::decay_t<Arg0>, ComponentTypeID>) {
            PoolType<T> *p = create_component_if_needed<T>(variant_id);
            auto r = p->try_emplace(e, std::forward<Args>(args)...);
            if (!r.second) {
                std::destroy_at(std::addressof(r.first->second));
                std::construct_at(std::addressof(r.first->second), std::forward<Args>(args)...);
            }
            return r.first->second;        
        } else {
            return emplace(e, ComponentTypeID{}, std::forward<Arg0>(variant_id), std::forward<Args>(args)...);
        }
    }

    ///Add a component for an entity with default component variant ID (0)
    /** @tparam T Type of the component to be added
     *  @param e Entity to which the component is to be added
     *  @return Reference to the component data stored in the registry
     * This overload uses default component variant ID (0).
     * 
     * This overload is just special case of the above emplace() method with default variant ID.
     */
    template<typename T>
    constexpr T &emplace(Entity e) {
        PoolType<T> *p = create_component_if_needed<T>({});
        auto r = p->try_emplace(e);
        if (!r.second) {
            std::destroy_at(std::addressof(r.first->second));
            std::construct_at(std::addressof(r.first->second));
        }
        return r.first->second;        
    }


    ///Remove a component of type T from an entity (if it exists)
    /** @tparam T Type of the component to be removed
     *  @param e Entity from which the component is to be removed
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr void remove(Entity e) {
        remove<T>(e, {});
    }

    ///Remove a component of type T with specific component variant ID from an entity (if it exists)
    /** @tparam T Type of the component to be removed
     *  @param e Entity from which the component is to be removed
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     */
    template<typename T>
    constexpr void remove(Entity e, ComponentTypeID variant_id) {
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return;
        iter->second->erase(e);
    }

    ///Get a const reference to a component of type T for an entity (if it exists)
    /**
     * @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr const OptionalRef<const T> get(Entity e) const {
        return get(e, {});
    }

    ///Get a const reference to a component of type T with specific component variant ID for an entity (if it exists)
    /**
     * @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved   
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     */
    template<typename T>
    constexpr const OptionalRef<const T> get(Entity e, ComponentTypeID variant_id) const {
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return {};
        auto pp = static_cast<const PoolType<T> *>(iter->second.get());
        auto iter2 = pp->find(e);
        if (iter2 == pp->end()) return {};
        return {iter2->second};
    }

    ///Get a reference to a component of type T for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr OptionalRef<T> get(Entity e) {
        return get(e, {});
    }

    ///Get a reference to a component of type T with specific component variant ID for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     */
    template<typename T>
    constexpr OptionalRef<T> get(Entity e, ComponentTypeID variant_id) {
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return {};
        auto pp = static_cast<PoolType<T> *>(iter->second.get());
        auto iter2 = pp->find(e);
        if (iter2 == pp->end()) return {};
        return {iter2->second};

    }

    ///Find component of type T with specific component variant ID for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return Iterator to the component data if it exists, end iterator otherwise
     */
    template<typename T>
    constexpr auto find(Entity e, ComponentTypeID variant_id) const {
        using Iter = decltype(std::declval<const PoolType<T> >().begin());
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return Iter();
        auto pp = static_cast<const PoolType<T> *>(iter->second.get());
        return pp->find(e);
    }

    ///Find component of type T for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @return Iterator to the component data if it exists, end iterator otherwise
     *  This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr auto find(Entity e) const {
        return get(e, {});
    }


    ///Get all components of type T with specific component variant ID (non-const version)
    /** @tparam T Type of the component to be retrieved
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return Range of all components of type T with the given component variant ID   
     */
    template<typename T>    
    constexpr auto all_of(ComponentTypeID variant_id) {
        using Iter = decltype(std::declval<PoolType<T> >().begin());
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return std::ranges::subrange<Iter,Iter>(Iter{}, Iter{});
        PoolType<T> *p = static_cast<PoolType<T> *>(iter->second.get());
        return std::ranges::subrange<Iter, Iter>(p->begin(), p->end());        
    }

    ///Get all components of type T (non-const version)
    /** @tparam T Type of the component to be retrieved
     *  @return Range of all components of type T with the default component variant ID (0)   
     */
    template<typename T>
    constexpr auto all_of() {
        return all_of<T>({});
    }


    ///Get all components of type T with specific component variant ID (const version)
    /** @tparam T Type of the component to be retrieved
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return Range of all components of type T with the given component variant ID   
     */
    template<typename T>
    constexpr auto all_of(ComponentTypeID variant_id) const {
        using Iter = decltype(std::declval<const PoolType<T> >().begin());
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return std::ranges::subrange(Iter{}, Iter{});
        const PoolType<T> *p = static_cast<const PoolType<T> *>(iter->second.get());
        return std::ranges::subrange(p->begin(), p->end());
    }

    ///Get all components of type T (const version)
    /** @tparam T Type of the component to be retrieved
     *  @return Range of all components of type T with the default component variant ID (0)   
     */
    template<typename T>
    constexpr auto all_of() const {
        return all_of<T>({});

    }

    ///Remove all components of type T 
    /** @tparam T Type of the component to be removed
     */
    template<typename T>
    constexpr void remove_all_of() {
        _storage.erase(Key{component_type_id<T>, {}});
    }

    ///Remove all components of type T 
    /** @tparam T Type of the component to be removed
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     */
    template<typename T>
    constexpr void remove_all_of(ComponentTypeID variant_id) {
        _storage.erase(Key{component_type_id<T>, variant_id});
    }

    /// Iterate over all components of an entity and invoke a visitor function for each component (non-const version)
    /** @tparam Fn Type of the visitor function
     *  @param e Entity whose components are to be visited
     *  @param fn Visitor function to be invoked for each component
     * The visitor function can have one of the following signatures:
     * - void(AnyRef)
     * - void(AnyRef, ComponentTypeID)
     * - void(AnyRef, ComponentTypeID, ComponentTypeID)
     * where the parameters are:
     * - AnyRef: reference to the component data
     * - ComponentTypeID: component variant ID
     * - ComponentTypeID: type-component ID 
     * The function is invoked for each component of the entity.
     */ 
    template<ComponentVisitor Fn>
    void for_each_component(Entity e, Fn &&fn) {
        for (const auto &[k, v]: _storage) {
            IComponentPool *p = v.get();
            AnyRef c = p->entity(e);
            if (c) {
                if constexpr(std::is_invocable_v<Fn, AnyRef>) {
                    fn(c);
                } else if constexpr(std::is_invocable_v<Fn, AnyRef, ComponentTypeID>) {
                    fn(c,k._variant_id);
                } else {
                    static_assert(std::is_invocable_v<Fn, AnyRef, ComponentTypeID, ComponentTypeID>);
                    fn(c,k._variant_id, k._type_id);
                }                    
            }
        }
    }

    /// Iterate over all components of an entity and invoke a visitor function for each component (const version)
    /** @tparam Fn Type of the visitor function
     *  @param e Entity whose components are to be visited
     *  @param fn Visitor function to be invoked for each component
     * The visitor function can have one of the following signatures:
     * - void(ConstAnyRef)
     * - void(ConstAnyRef, ComponentTypeID)
     * - void(ConstAnyRef, ComponentTypeID, ComponentTypeID)
     * where the parameters are:
     * - ConstAnyRef: const reference to the component data
     * - ComponentTypeID: component variant ID
     * - ComponentTypeID: type-component ID
     * The function is invoked for each component of the entity.
     */
    template<ConstComponentVisitor Fn>
    auto for_each_component(Entity e, Fn &&fn) const {
        for (const auto &[k, v]: _storage) {
            ConstAnyRef c = v->entity(e);
            if (c) {
                if constexpr(std::is_invocable_v<Fn, AnyRef>) {
                    fn(c);
                } else if constexpr(std::is_invocable_v<Fn, AnyRef, ComponentTypeID>) {
                    fn(c,k._variant_id);
                } else {
                    static_assert(std::is_invocable_v<Fn, AnyRef, ComponentTypeID, ComponentTypeID>);
                    fn(c,k._variant_id, k._type_id);
                }                    
            }
        }

    }

    ///Create a view for iterating over entities with specific components
    /** @tparam Components Types of components to be included in the view
     *  @param ids Optional list of component variant IDriants to include in the view. If empty, default component variant ID (0) is used for all components.
     *  @return View object for iterating over entities with the specified components
     * The view can be used in range-based for loops and other range algorithms. The view is compatible with the C++20 ranges library.
     * Each element of the view is a tuple containing the entity and references to the requested components.
     * 
     * 
     */
    template<typename ... Components>
    constexpr View<GenericRegistry *, Components...> view(std::initializer_list<ComponentTypeID> ids = {}) {
        return View<GenericRegistry *, Components...>(this,ids);
    }

    template<typename ... Components>
    constexpr View<GenericRegistry *, Components...> view(std::span<const ComponentTypeID> ids) {
        return View<GenericRegistry *, Components...>(this,ids);
    }


    ///Check whether an entity has a component of type T with specific component variant ID
    /** @tparam Component Type of the component to be checked
     *  @param e Entity to be checked
     *  @param subid component variant ID to differentiate multiple components of the same type
     *  @return true if the entity has the component, false otherwise   
     */
    template<typename Component>
    constexpr bool contains(Entity e, ComponentTypeID subid = {}) const {
        return get<Component>(e,subid);
    }

    ///Check whether an entity has all specified components (with optional component variant IDs)
    /** @tparam Components Types of components to be checked
     *  @param e Entity to be checked
     *  @param variants Optional list of component variant IDs to check. If empty, default component variant ID (0) is used for all components.
     *  @return true if the entity has all specified components, false otherwise
     */
    template<typename ... Components>
    constexpr bool contains(Entity e, std::span<const ComponentTypeID> variants) const {
        return contains_impl<Components ...>(e, variants);
    }

    ///Check whether an entity has all specified components (with optional component variant IDs)
    /** @tparam Components Types of components to be checked
     *  @param e Entity to be checked
     *  @param variants Optional list of component variant IDs to check. If empty, default component variant ID (0) is used for all components.
     *  @return true if the entity has all specified components, false otherwise
     */
    template<typename ... Components>
    constexpr bool contains(Entity e, std::initializer_list<ComponentTypeID> variants) const {
        return contains_impl<Components ...>(e, variants);
    }


    ///Check whether an entity is known (has any component)
    /** @param e Entity to be checked
     *  @return true if the entity has any component, false otherwise   
     */
    constexpr bool is_known(Entity e) const {
        for (const auto &[k, p]: _storage) {
            if (p->entity(e)) return true;
        }
        return false;
    }

    ///Optimize the storage layout of a component pool by separating entities with specific components into a new pool
    /**
     * @tparam T Type of the component whose pool is to be optimized
     * @tparam Uvs Types of components that should be present in the new pool
     * @param variant_t Component variant ID of the component T to be optimized (default is 0)
     * @param variant_uvs List of component variant IDs for components Uvs (default is empty)
     * @return true if the pool was optimized, false if no entities matched the criteria
     *
     * Chains all components of entities in pool T together for all entities that exist in the other pools.
     * The components are also sorted by entity to ensure the order is always unambiguous. When iterating
     * over this combination, sequential traversal is used, resulting in more efficient iteration. For this
     * operation to be sufficiently efficient, all pools in the given combination should be optimized.
     */
    template<typename T, typename ... Uvs>
    bool optimize_pool(ComponentTypeID variant_t  = {}, std::span<const ComponentTypeID> variant_uvs = {}) {
        auto mitr = _storage.find(Key{component_type_id<T>, variant_t});
        if (mitr == _storage.end() ) return false;
        auto c = mitr->second.get();
        PoolType<T> *ct = static_cast<PoolType<T> *>(c);

        auto b = ct->begin();
        auto e = ct->end();

        auto st = std::find_if(b,e,[&](const auto &itm){
            return contains<Uvs...>(itm.first, variant_uvs);
        });

        if (st == e) return false;

        std::vector<Entity> sortMap;
        sortMap.reserve(std::distance(st, e));
        std::for_each(st,e, [&](const auto &itm){
            if (contains<Uvs...>(itm.first, variant_uvs)) {
                sortMap.push_back(itm.first);
            }
        });
        std::sort(sortMap.begin(), sortMap.end());

        auto new_pool = std::make_unique<PoolType<T> >();
        new_pool->reserve(std::distance(b, e));
        std::for_each(b, st, [&](auto &&itm){
            new_pool->emplace(std::move(itm.first), std::move(itm.second));
        });
        std::for_each(sortMap.begin(), sortMap.end(), [&](Entity e){
            auto iter = ct->find(e);
            new_pool->emplace(std::move(iter->first), std::move(iter->second));
        });
        std::for_each(st, e, [&](auto &&itm){
            auto x = std::lower_bound(sortMap.begin(), sortMap.end(), itm.first);
            if (x == sortMap.end() || *x != itm.first) {
                new_pool->emplace(std::move(itm.first), std::move(itm.second));
            }
        });
        mitr->second = std::move(new_pool);
        return true;
    }   

    template<typename T, typename ... Uvs>
    bool optimize_pool(ComponentTypeID variant_t, std::initializer_list<ComponentTypeID> variant_uvs ) {
        return optimize_pool<T, Uvs...>(variant_t, std::span<const ComponentTypeID>(variant_uvs));
    }

    template<typename ... Components>
    bool group(std::span<const ComponentTypeID> variants = {}) {
        return optimize_rotate<0, Components...>(variants);
    }

    template<typename ... Components>
    bool group(std::initializer_list<ComponentTypeID> variants) {
        return optimize_rotate<0, Components...>(variants);
    }


protected:
    Storage<Key, PPool> _storage;

    template<typename T>
    constexpr PoolType<T> *create_component_if_needed(ComponentTypeID sub) {
        Key k{component_type_id<T>, sub};
        auto iter = _storage.find(k);
        if (iter == _storage.end()) {
            return  static_cast<PoolType<T> *>(_storage.try_emplace(k, make_unique_constexpr<PoolType<T> >()).first->second.get());
        } else {
            return static_cast<PoolType<T> *>(iter->second.get());
        }

    }

    template<typename T, typename ... Components>
    constexpr bool contains_impl(Entity e, std::span<const ComponentTypeID> variants) const {
        if (variants.empty()) {
            if (!contains<T>(e)) return false;            
        } else {
            if (!contains<T>(e, variants.front())) return false;            
            variants = variants.subspan<1>();
        }
        if constexpr(sizeof...(Components) == 0) return true;
        else return contains_impl<Components...>(e, variants);
    }

    template<unsigned int N, typename T, typename ... Components>
    constexpr bool optimize_rotate(std::span<const ComponentTypeID> variants)  {
        constexpr auto cnt = sizeof...(Components)+1;
        if constexpr(N == cnt) {
            return true;
        } else {
            auto sub = variants.subspan<1>();
            if (!optimize_pool<T, Components...>(variants.front(), sub)) return false;
            std::array<ComponentTypeID, cnt> tmp;        
            auto iter = std::copy(sub.begin(), sub.end(), tmp.begin());
            *iter = variants.front();
            return optimize_rotate<N+1, Components..., T>(tmp);
        }


    }

};



}