#pragma once

#include "component.hpp"
#include "utils/optional_ref.hpp"
#include "view.hpp"
#include "utils/indexed_flat_map.hpp"

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

///Stores entity name
/**
 * @note Entity name is droppable object. You need to call drop explicitly to clean memory
 * (however, the Registry is calling drop automatically when)
 * 
 */
class EntityName {
public:

    constexpr explicit EntityName(std::string_view n) {
        _name = new char[n.size()+1];
        *std::copy(n.begin(), n.end(), _name)  = 0;  
        _size =n.size();      
    }
    constexpr operator std::string_view() const {return std::string_view(_name, _size);}  

    constexpr bool operator==(const EntityName &other) const {
        return static_cast<std::string_view>(*this) == static_cast<std::string_view>(other);
    }
    template<typename IO>
    friend IO &operator<<(IO &io, const EntityName &en)  {      
        return (io << static_cast<std::string_view>(en));
    }

    constexpr void drop() {
        delete [] _name;                
    }

protected:
    char *_name = nullptr;
    std::size_t _size = 0;
};

///Concepts for component visitor functions
template<typename T>
concept ComponentVisitor = std::is_invocable_v<T, AnyRef>
    || std::is_invocable_v<T, AnyRef, ComponentTypeID>
    || std::is_invocable_v<T, AnyRef, ComponentTypeID, ComponentTypeID>;




///Default registry traist (for singlethreading)
struct DefaultRegistryTraits {

    template<typename K, typename V>
    class PoolStorage: public IndexedFlatMap<K, V, HashOfKey<K>, std::equal_to<K> > {};

    template<typename K, typename V>
    class RegistryStorage: public OpenHashMap<K, V, HashOfKey<K>, std::equal_to<K> > {};





    ///type which normalizes T to component type
    /** This performs removing const volatile reference as these qualifiers are not used */
    template<typename T>
    using ComponentNormalized = std::remove_cvref_t<T>;

    
    ///type which specifies type used to store components of T
    template<typename T>
    using ComponentPool = GenericComponentPool<ComponentNormalized<T>, PoolStorage>;   

    ///smart pointer to hold abstract component pool
    using PoolSmartPtr = unique_ptr<IComponentPool>;
    
    
    ///pointer like object to point to component tool
    template<typename T>
    using ComponentPoolPtr =  std::conditional_t<std::is_const_v<T>, const ComponentPool<T> *, ComponentPool<T> *>;
    
    
    template<typename T>
    static constexpr auto component_type_id = ComponentTraits<ComponentNormalized<T> >::id;


    ///casts smart pointer to pointer-like object
    /**
     * @param ptr smart pointer ti abstract component tool
     * @return pointer-like object to component pool of type T
     */
    template<typename T>
    static constexpr auto cast_to_component_pool_ptr(const PoolSmartPtr &ptr) {
        return static_cast<ComponentPoolPtr<T> >(ptr.get());
    }

    ///allocates new pool
    /**
     * @return PoolSmartPtr with new pool
     */
    template<typename T>
    static constexpr PoolSmartPtr create_pool() {
        return make_unique<ComponentPool<T> >();
    }   

    ///Component data reference (contains reference to data or empty)
    /** Useful if we need to hold a lock during holding this reference */
    template<typename T>
    using Ref = OptionalRef<T>;

    ///Creates ref from data and pointer to component where data are located
    /**
     * @param data reference to a data
     * @param ptr pointer to component pool which holds data
     * @return ref object
     */
    template<typename T>
    static constexpr Ref<T> create_ref(T &data, [[maybe_unused]] const auto &component_pool_ptr) {
        return Ref<T>(data);
    }
    ///Create empty ref object
    template<typename T>
    static constexpr Ref<T> create_ref() {
        return Ref<T>(std::nullopt);
    }
    
};

struct ConceptTestComponent {};

template<typename T>
concept RegistryTraits = requires {    
    typename T::PoolSmartPtr;
    typename T::template ComponentNormalized<ConceptTestComponent>;
    typename T::template ComponentPool<ConceptTestComponent>;
    typename T::template ComponentPoolPtr<ConceptTestComponent>;
    typename T::template Ref<ConceptTestComponent>;
    typename T::template RegistryStorage<int, typename T::PoolSmartPtr>;
    requires IsPointerLike<typename T::template ComponentPoolPtr<ConceptTestComponent> >;
    requires IsPointerLike<typename T::template ComponentPoolPtr<const ConceptTestComponent> >;
    
    {T::template cast_to_component_pool_ptr<ConceptTestComponent>(std::declval<const typename T::PoolSmartPtr &>())} 
            -> std::same_as<typename T::template ComponentPoolPtr<ConceptTestComponent> >;
    {T::template create_pool<ConceptTestComponent>()} -> std::same_as<typename T::PoolSmartPtr>;
    {T::template component_type_id<ConceptTestComponent>} -> std::convertible_to<ComponentTypeID>;
    {T::template create_ref<ConceptTestComponent>(std::declval<ConceptTestComponent &>(), std::declval<typename T::template ComponentPoolPtr<ConceptTestComponent> >())} 
            -> std::same_as<typename T::template Ref<ConceptTestComponent> >;
    {T::template create_ref<ConceptTestComponent>()} -> std::same_as<typename T::template Ref<ConceptTestComponent> >;
    
};

static_assert(RegistryTraits<DefaultRegistryTraits>);




///GenericRegistry class template
/**
 * GenericRegistry is the main class template of the ECS database.
 * It manages entities and their associated components.
 * Template parameters:
 * - PoolType: a template with one parameter that provides component pool implementation
 * - Storage: a template with two parameters (key and value) that provides storage for component pools
 */

 
template<typename Traits = DefaultRegistryTraits>
class GenericRegistry {
public:

    template<typename T> using Ref = typename Traits::template Ref<T>;

    template<typename T> 
    using PoolType = typename Traits::template ComponentPool<T>;
    template<typename T> 
    using PoolPtr = typename Traits::template ComponentPoolPtr<T>;

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
    using PPool = typename Traits::PoolSmartPtr;

    using Storage = typename Traits::template RegistryStorage<Key, PPool>;


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
        set<EntityName>(e, EntityName(name));
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
     *  @retval true component has been created
     *  @retval false component has been replaced
     */
    template<typename T>
    constexpr bool set(Entity e, T data) {
        return emplace<T, ComponentTypeID, T>(e, ComponentTypeID(), std::move(data));
    }

    ///Add or set a component for an entity with a specific component variant ID
    /**
     * If component doesn't exist, it is created. If it exists, it is updated.
     * @tparam T Type of the component to be removed
     * @param e Entity to which the component is to be added or updated
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @param data Data of the component to be set
     *  @retval true component has been created
     *  @retval false component has been replaced
     */
    template<typename T>
    constexpr bool set(Entity e, ComponentTypeID variant_id, T data) {
        return emplace<T, ComponentTypeID &, T>(e, variant_id, std::move(data));
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
     *  @retval true component has been created
     *  @retval false component has been replaced
     */
    template<typename T, typename Arg0, typename ... Args>
    constexpr bool emplace(Entity e, Arg0 &&variant_id, Args && ... args) {
        if constexpr(std::is_same_v<std::decay_t<Arg0>, ComponentTypeID>) {
            auto p = create_component_if_needed<T>(variant_id);
            auto r = p->try_emplace(e, std::forward<Args>(args)...);
            if (!r.second) {
                if constexpr(is_droppable<decltype(r.first->second)>) {
                    drop(r.first->second);
                }
                std::destroy_at(std::addressof(r.first->second));
                std::construct_at(std::addressof(r.first->second), std::forward<Args>(args)...);
                return false;
            }
            return true;
        } else {
            return emplace(e, ComponentTypeID{}, std::forward<Arg0>(variant_id), std::forward<Args>(args)...);
        }
    }

    ///Add a component for an entity with default component variant ID (0)
    /** @tparam T Type of the component to be added. Extra qualifiers are removed.
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

    ///Remove a component of type T with specific component variant ID from an entity (if it exists)
    /** @tparam T Type of the component to be removed. Note extra qualifiers are removed.
     *  @param e Entity from which the component is to be removed
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     */
    template<typename T>
    constexpr void remove(Entity e, ComponentTypeID variant_id = {}) {
        auto iter = _storage.find(Key{Traits::template component_type_id<T>, variant_id});
        if (iter == _storage.end()) return;
        iter->second->erase(e);
    }

    ///Get a reference to a component of type T with specific component variant ID for an entity (if it exists)
    /** @tparam T Type of the component to be retrieved
     * @param e Entity whose component is to be retrieved
     * @param variant_id component variant ID to differentiate multiple components of the same type
     * @return Ref to the component data if it exists, empty Ref otherwise
     * @note Even if this function is const, it can return a non-const reference to the component data.
     * If you need a const reference, specify const T as the template parameter.
     */
    template<typename T>
    constexpr Ref<T> get(Entity e, ComponentTypeID variant_id = {}) const {
        auto iter = _storage.find(Key{Traits::template component_type_id<T>, variant_id});
        if (iter == _storage.end()) return Ref<T>(std::nullopt);
        auto pp = Traits::template cast_to_component_pool_ptr<T>(iter->second);
        auto iter2 = pp->find(e);   
        if (iter2 == pp->end()) return Traits::template create_ref<T>();
        return Traits::template create_ref<T>(iter2->second, pp);       
    }

    ///Get all components of type T with specific component variant ID (const version)
    /** @tparam T Type of the component to be retrieved
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     *  @return Range of all components of type T with the given component variant ID   
     */
    template<typename T>
    constexpr auto all_of(ComponentTypeID variant_id = {}) const {
        typename Traits::template ComponentPoolPtr<T> p = {};
        auto iter = _storage.find(Key{Traits::template component_type_id<T>, variant_id});
        if (iter != _storage.end()) p = Traits::template cast_to_component_pool_ptr<T>(iter->second);
        return std::ranges::subrange(safe_begin(p), safe_end(p));
    }

    ///Remove all components of type T 
    /** @tparam T Type of the component to be removed
     *  @param variant_id component variant ID to differentiate multiple components of the same type
     */
    template<typename T>
    constexpr void remove_all_of(ComponentTypeID variant_id = {}) {
        _storage.erase(Key{Traits::template component_type_id<T>, variant_id});
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
    template<ComponentVisitor Fn>
    auto for_each_component(Entity e, Fn &&fn) const {
        for (const auto &[k, v]: _storage) {
            AnyRef c = v->entity(e);
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
    /** @tparam Components Types of components to be included in the view. You can use
     * qualifier const to indicate read-only components.
     *  @param ids Optional list of component variant IDs to include in the view.
     *  If empty, default component variant ID (0) is used for all components.
     *  @return View object for iterating over entities with the specified components
     * The view can be used in range-based for loops and other range algorithms. The view is compatible with the C++20 ranges library.
     * Each element of the view is a tuple containing the entity and references to the requested components.
     */
    template<typename ... Components>
    constexpr auto view(std::span<const ComponentTypeID> ids) const {
        std::array<ComponentTypeID, sizeof...(Components)> idsarr;
        auto itr = ids.begin();
        for (auto  &x: idsarr) {
            if (itr == ids.end()) x = {};
            else {
                x = *itr;
                ++itr;
            }
        }
        using Pools = std::tuple<typename Traits::template ComponentPoolPtr<Components> ...>;
        Pools pools;

        using ComponentTuple = std::tuple<Components ...>;

        sequence_iterate<sizeof...(Components)>([&](auto idx){
            using T = std::tuple_element_t<idx, ComponentTuple>;
            auto f = _storage.find(Key{Traits::template component_type_id<T>, idsarr[idx]});
            if (f == _storage.end()) {
                std::get<idx>(pools) = nullptr;
            } else {
                std::get<idx>(pools) = Traits::template cast_to_component_pool_ptr<T>(f->second);
            }
        });

        return View<Pools>(pools);
    }

    template<typename ... Components>
    constexpr auto view(std::initializer_list<ComponentTypeID> ids = {}) const {
        return view<Components...>(std::span<const ComponentTypeID>(ids));
    }
    

    ///Check whether an entity has a component of type T with specific component variant ID
    /** @tparam Component Type of the component to be checked
     *  @param e Entity to be checked
     *  @param subid component variant ID to differentiate multiple components of the same type
     *  @return true if the entity has the component, false otherwise   
     */
    template<typename Component>
    constexpr bool has(Entity e, ComponentTypeID subid = {}) const {
        return get<Component>(e,subid);
    }

    ///Check whether an entity has all specified components (with optional component variant IDs)
    /** @tparam Components Types of components to be checked
     *  @param e Entity to be checked
     *  @param variants Optional list of component variant IDs to check. If empty, default component variant ID (0) is used for all components.
     *  @return true if the entity has all specified components, false otherwise
     */
    template<typename ... Components>
    constexpr bool has(Entity e, std::span<const ComponentTypeID> variants) const {
        static_assert(sizeof...(Components) > 0);
        if constexpr (sizeof...(Components) == 1)  {
            return has<Components...>(e, variants.empty()?ComponentTypeID():ComponentTypeID(variants.front()));
        } else {
            return has_impl<Components ...>(e, variants);
        }
    }

    ///Check whether an entity has all specified components (with optional component variant IDs)
    /** @tparam Components Types of components to be checked
     *  @param e Entity to be checked
     *  @param variants Optional list of component variant IDs to check. If empty, default component variant ID (0) is used for all components.
     *  @return true if the entity has all specified components, false otherwise
     */
    template<typename ... Components>
    constexpr bool has(Entity e, std::initializer_list<ComponentTypeID> variants) const {
        return has<Components ...>(e, std::span<const ComponentTypeID>(variants));
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
    template<typename T, std::invocable<Entity, typename Traits::template ComponentNormalized<T> >  Fn>
    constexpr bool group_entities(ComponentTypeID variant, Fn &&predicate) {
        auto mitr = _storage.find(Key{Traits::template component_type_id<T>, variant});
        if (mitr == _storage.end() ) return false;
        auto ct = Traits::template cast_to_component_pool_ptr<T>(mitr->second);

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

        auto new_pool_ptr = Traits::template create_pool<T>();
        auto new_pool = Traits::template cast_to_component_pool_ptr<T>(new_pool_ptr);

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
        ct->clear();    //clear content before destruction to prevent to call drop()
        mitr->second = std::move(new_pool_ptr);
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
    template<typename T, typename U, typename ... Vs>
    constexpr bool group_entities(ComponentTypeID variant_t  = {}, std::span<const ComponentTypeID> variant_uvs = {}) {
        return group_entities<T>(variant_t, [&](const Entity &e, const auto &){
            return has<U,Vs...>(e,variant_uvs);
        });
    }



    template<typename T, typename U, typename ... Vs>
    constexpr bool group_entities(ComponentTypeID variant_t, std::initializer_list<ComponentTypeID> variant_uvs ) {
        return optimize_pool<T, U, Vs...>(variant_t, std::span<const ComponentTypeID>(variant_uvs));
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

    ///creates component pool if needed
    /** @tparam Component Type of the component whose pool is to be created. This must be pure type,
     * qualifiers like const and reference are not allowed.
     *  @param variant component variant ID to differentiate multiple components of the same type
     *  @return Pointer to the component pool if created or already exists, nullptr otherwise
     */
    template<typename Component>
    PoolType<Component> *create_component_pool(ComponentTypeID variant = {}) {
        return create_component_if_needed<Component>(variant);
    }

    ///gets component pool if exists
    /** @tparam Component Type of the component whose pool is to be retrieved. This must be pure type,
     * qualifiers like const and reference are not allowed.
     *  @param variant component variant ID to differentiate multiple components of the same type
     *  @return Pointer to the component pool if exists, nullptr otherwise
     */
    template<typename Component>
     PoolType<Component> *get_component_pool(ComponentTypeID variant = {}) const {
        auto r = _storage.find(Key{Traits::template component_type_id<Component>, variant});
        if (r == _storage.end()) return nullptr;
        return Traits::template cast_to_component_pool_ptr<Component>(r->second);
    }

protected:
    Storage _storage;

    template<typename T>
    constexpr PoolPtr<T> create_component_if_needed(ComponentTypeID sub) {
        static_assert(!std::is_const_v<T> && !std::is_reference_v<T>,
        "Component type must be pure type without const or reference qualifiers");

        Key k{Traits::template component_type_id<T>, sub};
        auto iter = _storage.find(k);
        if (iter == _storage.end()) {
            return Traits::template cast_to_component_pool_ptr<T>(_storage.try_emplace(k, Traits::template create_pool<T>()).first->second);
        } else {
            return Traits::template cast_to_component_pool_ptr<T>(iter->second);
        }

    }

    template<typename T, typename ... Components>
    constexpr bool has_impl(Entity e, std::span<const ComponentTypeID> variants) const {
        if constexpr(sizeof...(Components) == 0) return true;
        else {
            if (variants.empty()) {
                if (!has<T>(e)) return false;            
            } else {
                if (!has<T>(e, variants.front())) return false;            
                variants = variants.subspan<1>();
            }
            return has_impl<Components...>(e, variants);
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
            if (!group_entities<T, Components...>(front, sub)) return false;            
            auto iter = std::copy(sub.begin(), sub.end(), var_tmp.begin());
            *iter = front;
            return optimize_rotate_2<N+1, cnt, Components..., T>(var_tmp);
        }


    }


};



}