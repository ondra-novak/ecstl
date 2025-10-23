#pragma once

#include "utils/any_ref.hpp"
#include <concepts>
#include <string_view>
#include "utils/type_name.hpp"


namespace ecstl {

///Holds ID for given component
/**
 * ComponentTypeID can be created from:
 * - uint64_t - direct numeric ID
 * - string_view - hash of the string_view is used as ID
 */
struct ComponentTypeID {
    size_t _value = 0;

    constexpr ComponentTypeID() = default;
    constexpr explicit ComponentTypeID(size_t v) : _value(v) {}
    constexpr ComponentTypeID(std::string_view name)
        : _value(calc_hash(name)) {}

    constexpr auto operator<=>(const ComponentTypeID&) const = default;

    constexpr uint64_t get_id() const {return _value;}

    constexpr ComponentTypeID operator+(const ComponentTypeID &other) const {
        return ComponentTypeID(_value + 0x9e3779b9 + (other._value << 6) + (other._value > 2));
    }

private:
    // constexpr FNV-1a 64-bit
    static constexpr size_t calc_hash(std::string_view s) {
        hash<std::string_view> hasher;
        return hasher(s);
    }
};

///Checks whether T has component_type as const static member
template<typename T>
concept has_component_type_as_const = requires {
    {T::component_type} -> std::convertible_to<ComponentTypeID>;
};

template<typename T>
concept is_droppable_by_method = requires(T &obj) {
    {obj.drop()};
};



template<is_droppable_by_method T>
constexpr void drop(T &v) {
    v.drop();
}


template<typename T>
concept is_droppable = requires(T &t) {
    {drop(t)};
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
    constexpr virtual ~IComponentPool() {}
    /// Get the type of the component
    constexpr virtual const ComponentTypeID &get_type() const = 0;
    /// Erase the component data for a given entity
    constexpr virtual void erase(Entity e)  = 0;
    /// Count of components
    constexpr virtual size_t size() const = 0;
    /// Retrieve entity as AnyRef if exists.
    constexpr virtual AnyRef entity(Entity e) = 0;
    /// Retrieve entity as ConstAnyRef if exists.
    constexpr virtual ConstAnyRef entity(Entity e) const = 0;
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

    ///inicialize default
    constexpr GenericComponentPool()=default;

    constexpr ~GenericComponentPool() {
        if constexpr(is_droppable<T>) {
            for (auto [k,v]: *this) {
                drop(v);
            }
        }
    }

    ///inicialize with database instance
    template<typename DB>
    constexpr GenericComponentPool(DB &) {}   //we don't need database

    virtual const ComponentTypeID &get_type() const {
        return component_type_id<T>;
    }
    virtual constexpr void erase(Entity e)  {
        if constexpr(is_droppable<T>) {
            auto iter = Super::find(e);
            if (iter == Super::end()) return;
            drop(iter->second);
        } 
        Super::erase(e);
    }

    virtual constexpr  size_t size() const {
        return Super::size();
    }
    virtual AnyRef entity(Entity e) {
        auto iter = Super::find(e);
        if (iter == Super::end()) return AnyRef{};
        else return AnyRef(iter->second);
    }
    virtual ConstAnyRef entity(Entity e) const {
        auto iter = Super::find(e);
        if (iter == Super::end()) return ConstAnyRef{};
        else return ConstAnyRef(iter->second);
    }
};




}
