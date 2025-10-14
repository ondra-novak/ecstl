#pragma once

#include "component.hpp"
#include "optional_ref.hpp"
#include "view.hpp"

#include <string>
#include <concepts>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <vector>
#include "polyfill/unique_ptr.hpp"

namespace ecstl {

template<typename T>
struct HashOfKey {
    constexpr std::size_t operator()(const T &val) const {return get_hash(val);}
};


class EntityName {
public:

    constexpr explicit EntityName(std::string_view n):name(n.begin(), n.end()) {}
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
    using PPool = unique_ptr<IComponentPool>;

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
        if (c) return c.value();
        else return {};
    }

    constexpr void set_entity_name(Entity entity, std::string_view name) {
        set<EntityName>(entity, EntityName(name));
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


    ///Get a const reference to a component of type T with specific component variant ID for an entity (if it exists)
    /**
     * @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved   
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     */
    template<typename T>
    constexpr auto get(Entity e, ComponentTypeID variant_id) const {
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        using RetT = OptionalRef<std::remove_reference_t<decltype(std::declval<const PoolType<T> *>()->begin()->second)> >;
        if (iter == _storage.end()) return RetT(std::nullopt);
        auto pp = static_cast<const PoolType<T> *>(iter->second.get());
        auto iter2 = pp->find(e);
        if (iter2 == pp->end()) return RetT(std::nullopt);
        return RetT{iter2->second};
    }

        ///Get a const reference to a component of type T for an entity (if it exists)
    /**
     * @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr auto get(Entity e) const {
        return get<T>(e, {});
    }


    ///Get a reference to a component of type T with specific component variant ID for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     */
    template<typename T>
    constexpr auto get(Entity e, ComponentTypeID variant_id) {
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        using RetT = OptionalRef<std::remove_reference_t<decltype(std::declval<PoolType<T> *>()->begin()->second)> >;
        if (iter == _storage.end()) return RetT(std::nullopt);;
        auto pp = static_cast<PoolType<T> *>(iter->second.get());
        auto iter2 = pp->find(e);
        if (iter2 == pp->end()) return RetT(std::nullopt);;
        return RetT{iter2->second};

    }

        ///Get a reference to a component of type T for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved
     * @return OptionalRef to the component data if it exists, empty OptionalRef otherwise
     * This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr auto get(Entity e) {
        return get(e, {});
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

        ///Find component of type T with specific component variant ID for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return Iterator to the component data if it exists, end iterator otherwise
     */
    template<typename T>
    constexpr auto find(Entity e, ComponentTypeID variant_id) {
        using Iter = decltype(std::declval< PoolType<T> >().begin());
        auto iter = _storage.find(Key{component_type_id<T>, variant_id});
        if (iter == _storage.end()) return Iter();
        auto pp = static_cast<PoolType<T> *>(iter->second.get());
        return pp->find(e);
    }

    ///Find component of type T for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     *  @param e Entity whose component is to be retrieved   
     *  @return Iterator to the component data if it exists, end iterator otherwise
     *  This overload uses default component variant ID (0).
     */
    template<typename T>
    constexpr auto find(Entity e)  {
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
    template<typename ... Components>
    constexpr View<const GenericRegistry *, Components...> view(std::initializer_list<ComponentTypeID> ids = {}) const {
        return View<const GenericRegistry *, Components...>(this,ids);
    }

    template<typename ... Components>
    constexpr View<const GenericRegistry *, Components...> view(std::span<const ComponentTypeID> ids) const {
        return View<const GenericRegistry *, Components...>(this,ids);
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
        static_assert(sizeof...(Components) > 0);
        if constexpr (sizeof...(Components) == 1)  {
            return contains<Components...>(e, variants.empty()?ComponentTypeID():ComponentTypeID(variants.front()));
        } else {
            return contains_impl<Components ...>(e, variants);
        }
    }

    ///Check whether an entity has all specified components (with optional component variant IDs)
    /** @tparam Components Types of components to be checked
     *  @param e Entity to be checked
     *  @param variants Optional list of component variant IDs to check. If empty, default component variant ID (0) is used for all components.
     *  @return true if the entity has all specified components, false otherwise
     */
    template<typename ... Components>
    constexpr bool contains(Entity e, std::initializer_list<ComponentTypeID> variants) const {
        return contains<Components ...>(e, std::span<const ComponentTypeID>(variants));
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

 
    ///Group entities in a component pool
    /**
     * marked entities are moved to be closer to each other.
     * Use predicate to mark these entities. This can help to
     * iterate over the pool where entities should match the conditions
     * 
     * @tparam Component type component to optimize
     * @tparam Fn predicate function. It receives entity and component,and 
     * it must return true to bring entity to close others marked
     * @param variant component variant
     * @param predicate instance of preficate function 
     */
    template<typename Component, std::invocable<Entity, Component>  Fn>
    constexpr bool group_entities(ComponentTypeID variant, Fn &&predicate) {
        auto mitr = _storage.find(Key{component_type_id<Component>, variant});
        if (mitr == _storage.end() ) return false;
        auto c = mitr->second.get();
        PoolType<Component> *ct = static_cast<PoolType<Component> *>(c);

        auto b = ct->begin();
        auto e = ct->end();

        auto st = std::find_if(b,e,[&](const auto &itm){
            return predicate(itm.first, itm.second);
        });

        if (st == e) return false;

        std::vector<Entity> sortMap;
        sortMap.reserve(std::distance(st, e));
        std::for_each(st,e, [&](const auto &itm){
            if (predicate(itm.first, itm.second)) {
                sortMap.push_back(itm.first);
            }
        });
        std::sort(sortMap.begin(), sortMap.end());

        auto new_pool = alloc_component<Component>();
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
    constexpr bool optimize_pool(ComponentTypeID variant_t  = {}, std::span<const ComponentTypeID> variant_uvs = {}) {
        return group_entities<T>(variant_t, [&](const Entity &e, const auto &){
            return contains<Uvs...>(e,variant_uvs);
        });
    }



    template<typename T, typename ... Uvs>
    constexpr bool optimize_pool(ComponentTypeID variant_t, std::initializer_list<ComponentTypeID> variant_uvs ) {
        static_assert(sizeof...(Uvs) > 0);
        return optimize_pool<T, Uvs...>(variant_t, std::span<const ComponentTypeID>(variant_uvs));
    }

    template<typename ... Components>
    constexpr bool group(std::span<const ComponentTypeID> variants = {}) {
        static_assert(sizeof...(Components) > 1);
        return optimize_rotate<Components...>(variants);
    }

    template<typename ... Components>
    constexpr bool group(std::initializer_list<ComponentTypeID> variants) {
        static_assert(sizeof...(Components) > 1);
        return optimize_rotate<Components...>(variants);
    }

    ///Find an entity by its name (if it has EntityName component)
    /** @param name Name of the entity to be found
     *  @return Optional containing the entity if found, empty Optional otherwise
     *  
     * @note in case of multiple entities with the same name, the first one found is returned
     */      
    constexpr std::optional<Entity> find_by_name(const std::string_view name) const {
        auto view = this->view<EntityName>();
        for (auto [e, en]: view) {
            if (static_cast<std::string_view>(en) == name) return e;
        }
        return {};
    }

    template<typename Component>
    PoolType<Component> *get_component_pool(ComponentTypeID variant = {}, bool create = false) {
        if (create) {
            return create_component_if_needed<Component>(variant);
        } else {
            auto r = _storage.find(Key{component_type_id<Component>, variant});
            if (r == _storage.end()) return nullptr;
            return static_cast<PoolType<Component> *>(r->second.get());
        }
    }

    template<typename Component>
    const PoolType<Component> *get_component_pool(ComponentTypeID variant = {}) const {
        auto r = _storage.find(Key{component_type_id<Component>, variant});
        if (r == _storage.end()) return nullptr;
        return static_cast<PoolType<Component> *>(r->second.get());
    }

protected:
    Storage<Key, PPool> _storage;

    template<typename T>
    constexpr PoolType<T> *create_component_if_needed(ComponentTypeID sub) {
        Key k{component_type_id<T>, sub};
        auto iter = _storage.find(k);
        if (iter == _storage.end()) {
            return  static_cast<PoolType<T> *>(_storage.try_emplace(k, alloc_component<T>()).first->second.get());
        } else {
            return static_cast<PoolType<T> *>(iter->second.get());
        }

    }

    template<typename T, typename ... Components>
    constexpr bool contains_impl(Entity e, std::span<const ComponentTypeID> variants) const {
        if constexpr(sizeof...(Components) == 0) return true;
        else {
            if (variants.empty()) {
                if (!contains<T>(e)) return false;            
            } else {
                if (!contains<T>(e, variants.front())) return false;            
                variants = variants.subspan<1>();
            }
            return contains_impl<Components...>(e, variants);
        }
    }

    template<typename T, typename ... Components>
    constexpr bool optimize_rotate(std::span<const ComponentTypeID> variants)  {
        static_assert(sizeof...(Components) > 0);
        constexpr auto cnt = sizeof...(Components)+1;
        std::array<ComponentTypeID, cnt> tmp;        
        std::copy(variants.begin(), variants.end(), tmp.begin());
        return optimize_rotate_2<0, cnt, T, Components...>(tmp);
    }


    template<unsigned int N, std::size_t cnt,  typename T, typename ... Components>
    constexpr bool optimize_rotate_2(std::array<ComponentTypeID, cnt> &var_tmp)  {
        if constexpr(N == cnt) {
            return true;
        } else {            
            auto sub = std::span<const ComponentTypeID>(var_tmp).subspan<1>();
            auto front = var_tmp.front();
            if (!optimize_pool<T, Components...>(front, sub)) return false;            
            auto iter = std::copy(sub.begin(), sub.end(), var_tmp.begin());
            *iter = front;
            return optimize_rotate_2<N+1, cnt, Components..., T>(var_tmp);
        }


    }

    template<typename X>
    constexpr unique_ptr<PoolType<X> > alloc_component() {
        return  unique_ptr<PoolType<X> >(new  PoolType<X>);
    }

};



}