
#pragma once
#include<type_traits>
#include <optional>


namespace ecstl {

    template<typename T>
    class Ref;

 



    template<typename T>
    class Ref {
    public:
        constexpr Ref() = default;
        constexpr Ref(std::nullopt_t):_ref(nullptr) {}
        constexpr Ref(T &val):_ref(&val) {};

        constexpr operator bool() const { return _ref != nullptr;}
        constexpr T *operator->() const {return _ref;}
        constexpr T &operator*() const {return *_ref;}
        
        constexpr T &value() const {return *_ref;}
        template<typename Q>
        requires(std::is_convertible_v<T &, Q &>)
        constexpr Q &value_or(Q &alt) const {            
            return _ref?static_cast<Q &>(*_ref):alt;
        }
        constexpr bool has_value() const {return _ref != nullptr;}

        template< class F >
        constexpr auto and_then( F&& f ) {
                using RetVal = std::remove_cvref_t<std::invoke_result_t<F, T&> >;
            if (*this) {
                return std::forward<F>(f)(value());
            } else if constexpr(!std::is_void_v<RetVal>){ 
                return RetVal{};
            }
        }

        template< class F >
        constexpr auto or_else( F&& f ) {
            using RetVal = std::remove_cvref_t<std::invoke_result_t<F, T&> >;
            if (!*this) {
                return std::forward<F>(f)(value());
            } else { 
                return RetVal(this->value());
            }
        }

        





    protected:
        T *_ref = nullptr;
    };


    template<typename T>
    class Ref<Ref<T> >: public Ref<T> {
    public:
        using Ref<T>::Ref;
    };
}