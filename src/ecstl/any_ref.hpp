#pragma once

#include "optional_ref.hpp"
#include "utils/class_ident.hpp"
#include <type_traits>


namespace ecstl {

class ConstAnyRef;

///Type-erased reference to any type
class AnyRef {
public:

    ///Create empty AnyRef
    constexpr AnyRef():_ref(nullptr), _type(class_hash<std::nullptr_t>) {}

    ///Create AnyRef from a Type (must not be AnyRef or ConstAnyRef)
    template<typename T>
    requires(!std::is_same_v<std::decay_t<T>, AnyRef>)
    constexpr AnyRef(T &val):_ref(&val),_type(class_hash<T>) {}
    ///Copy and move
    constexpr AnyRef(const AnyRef &other) = default;
    ///Move constructor
    constexpr AnyRef(AnyRef &&other) = default;
    ///Copy and move assignment
    constexpr AnyRef &operator=(const AnyRef &other) = default;
    ///Move assignment
    constexpr AnyRef &operator=(AnyRef &&other) = default;


    ///Check whether AnyRef contains a value
    constexpr explicit operator bool () const {return _ref != nullptr;}


    ///Check whether AnyRef holds a value of type T
    template<typename T>
    constexpr friend bool holds_alternative(const AnyRef &inst) {
        return inst._type == class_hash<T>;
    }

    ///Get the value of type T (throws std::bad_cast if type does not match)
    template<typename T>
    constexpr friend T &get(const AnyRef &inst) {
        if (!holds_alternative<T>(inst)) throw std::bad_cast();
        return *static_cast<T *>(inst._ref);
    }

    ///Get OptionalRef to type T (empty if type does not match)
    template<typename T>
    constexpr OptionalRef<T> get_if() const {
        if (holds_alternative<T>(*this)) return OptionalRef<T>(get<T>(*this));
        else return OptionalRef<T>();
    }

    

protected:
    void *_ref;
    std::size_t _type;

    friend class ConstAnyRef;
};


///Type-erased const reference to any type
class ConstAnyRef {
public:

    ///Create empty ConstAnyRef
    constexpr ConstAnyRef():_ref(nullptr), _type(class_hash<std::nullptr_t>) {}

    ///Create ConstAnyRef from AnyRef
    constexpr ConstAnyRef(const AnyRef &ref):_ref(ref._ref),_type(ref._type) {}

    ///Create ConstAnyRef from a Type (must not be AnyRef or ConstAnyRef)
    template<typename T>
    requires(!std::is_same_v<std::decay_t<T>, AnyRef> && !std::is_same_v<std::decay_t<T>, ConstAnyRef>)
    constexpr ConstAnyRef(const T &val):_ref(&val),_type(class_hash<T>) {}

    ///Copy and move
    ConstAnyRef(const ConstAnyRef &other) = default;
    ///Move constructor
    ConstAnyRef(ConstAnyRef &&other) = default;
    ///Copy and move assignment
    ConstAnyRef &operator=(const ConstAnyRef &other) = default;
    /// Move assignment
    ConstAnyRef &operator=(ConstAnyRef &&other) = default;

    ///Check whether ConstAnyRef contains a value
    explicit operator bool () const {return _ref != nullptr;}

    ///Check whether ConstAnyRef holds a value of type T
    template<typename T>
    friend bool holds_alternative(const ConstAnyRef &inst) {
        return inst._type == class_hash<T>;
    }

    ///Get the value of type T (throws std::bad_cast if type does not match)
    template<typename T>
    friend T &get(const ConstAnyRef &inst) {
        if (!holds_alternative<T>(inst)) throw std::bad_cast();
        return *static_cast<T *>(inst._ref);
    }

    ///Get OptionalRef to type T (empty if type does not match)
   template<typename T>
    OptionalRef<const T> get_if() const {
        if (holds_alternative<T>(*this)) return OptionalRef<const T>(get<T>(*this));
        else return OptionalRef<const T>();
    }


protected:
    const void *_ref;
    std::size_t _type;
};



}