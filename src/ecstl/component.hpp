#pragma once

#include "any_ref.hpp"
#include <concepts>
#include <string_view>
#include "type_name.hpp"


namespace ecstl {

///Holds ID for given component
/**
 * ComponentTypeID can be created from:
 * - uint64_t - direct numeric ID
 * - string_view - hash of the string_view is used as ID
 */
struct ComponentTypeID {
    uint64_t _value = 0;

    constexpr ComponentTypeID() = default;
    constexpr explicit ComponentTypeID(uint64_t v) : _value(v) {}
    constexpr ComponentTypeID(std::string_view name)
        : _value(hash(name)) {}

    constexpr auto operator<=>(const ComponentTypeID&) const = default;

    constexpr uint64_t get_id() const {return _value;}

    constexpr ComponentTypeID operator+(const ComponentTypeID &other) const {
        return ComponentTypeID(_value + 0x9e3779b9 + (other._value << 6) + (other._value > 2));
    }

private:
    // constexpr FNV-1a 64-bit
    static constexpr uint64_t hash(std::string_view s) {
        uint64_t h = 1469598103934665603ull;
        for (char c : s)
            h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
        return h;
    }
};

///Checks whether T has component_type as const static member
template<typename T>
concept has_component_type_as_const = requires {
    {T::component_type} -> std::convertible_to<ComponentTypeID>;
};

template<typename T>
constexpr bool always_false = false;

///Traits to get component type ID from type T
template<typename T>
struct ComponentTraits {
    static constexpr auto id =  ComponentTypeID(type_name<T>);    
};

///Specialization for types having component_type as const static member
template<has_component_type_as_const T>
struct ComponentTraits<T> {
    static constexpr auto id =  ComponentTypeID(T::component_type);    
};

///Get component type ID for type T (constexpr)
template<typename T>
constexpr auto component_type_id = ComponentTraits<T>::id;



///Interface for component storage
class IComponentPool {
public:

    struct VTable {
        void (*destructor)(IComponentPool *);
        void (*erase)(IComponentPool *, Entity);
        size_t (*size)(const IComponentPool *);
        AnyRef (*entity)(IComponentPool *, Entity);
        ConstAnyRef (*entity_const)(const IComponentPool *,Entity);
    };

    const VTable *_vptr = nullptr;
    
    
    constexpr IComponentPool(const VTable *vptr):_vptr(vptr) {}
    /// Erase the component data for a given entity
    constexpr void erase(Entity e)  {_vptr->erase(this,e);}
    /// Count of components
    constexpr size_t size() const {return _vptr->size(this);}
    /// Retrieve entity as AnyRef if exists.
    constexpr AnyRef entity(Entity e) {return _vptr->entity(this,e);}
    /// Retrieve entity as ConstAnyRef if exists.
    constexpr ConstAnyRef entity(Entity e) const {return _vptr->entity_const(this,e);}
    /// destroy this class (use deleter)
    constexpr void destroy() {
        _vptr->destructor(this);
    }
};




/// Generic component pool using given storage
/**
 * T is the component type
 * Storage is a template with two parameters (key and value) that provides
 * standard map interface (insert, find, erase, begin, end, size)
 */
template<typename T, template<class,class> class Storage>
class GenericComponentPool : public IComponentPool, public Storage<Entity, T>  {
public:
    using Super = Storage<Entity, T>;

    static constexpr GenericComponentPool *super(IComponentPool *me);
    static constexpr const GenericComponentPool *super(const IComponentPool *me);

    static constexpr VTable vtable = {
        [](IComponentPool *me){
            GenericComponentPool *p = super(me);
            delete p;
        },
        [](IComponentPool *me, Entity e){super(me)->erase(e);},
        [](const IComponentPool *me){return super(me)->size();},
        [](IComponentPool *me, Entity e){return super(me)->entity(e);},
        [](const IComponentPool *me, Entity e){return super(me)->entity(e);},
    };

    ///inicialize default
    constexpr GenericComponentPool() : IComponentPool(&vtable) {}

    ///inicialize with database instance
    template<typename DB>
    constexpr GenericComponentPool(DB &) {}   //we don't need database

    const ComponentTypeID &get_type() const {
        return component_type_id<T>;
    }
    void erase(Entity e)  {
        Super::erase(e);
    }

    size_t size() const {
        return Super::size();
    }
    AnyRef entity(Entity e) {
        auto iter = Super::find(e);
        if (iter == Super::end()) return AnyRef{};
        else return AnyRef(iter->second);
    }
    ConstAnyRef entity(Entity e) const {
        auto iter = Super::find(e);
        if (iter == Super::end()) return ConstAnyRef{};
        else return ConstAnyRef(iter->second);
    }
};

template <typename T, template<class,class> class Storage>
inline constexpr GenericComponentPool<T, Storage> *GenericComponentPool<T, Storage>::super(IComponentPool *me)
{
    {return static_cast<GenericComponentPool<T, Storage> *>(me);}
}

template <typename T, template<class,class> class Storage>
inline constexpr const GenericComponentPool<T, Storage> *GenericComponentPool<T, Storage>::super(const IComponentPool *me)
{
    {return static_cast<const GenericComponentPool<T, Storage> *>(me);}
}

}