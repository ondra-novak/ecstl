#pragma once
#include <memory>


namespace ecstl {

#if __cpp_lib_constexpr_memory >= 202202L
using std::make_unique;
using std::unique_ptr;
#else

struct DefaultDeleter {
    template<typename X>
    constexpr void operator()(X *x) const {delete x;}
};
template <typename T, typename Deleter = DefaultDeleter>
class unique_ptr {
public:
    using pointer = T*;
    using element_type = T;
    using deleter_type = Deleter;   
    using reference = T&;
    using const_reference = const T&;
    using rvalue_reference = T&&;
    using const_rvalue_reference = const T&&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_extent_t<T>;

    constexpr unique_ptr() noexcept : _ptr(nullptr), _deleter() {}
    constexpr unique_ptr(std::nullptr_t) noexcept : _ptr(nullptr), _deleter() {}
    constexpr explicit unique_ptr(pointer p) noexcept : _ptr(p), _deleter() {}
    constexpr unique_ptr(pointer p, const deleter_type& d) noexcept : _ptr(p), _deleter(d) {}
    constexpr unique_ptr(pointer p, deleter_type&& d) noexcept : _ptr(p), _deleter(std::move(d)) {}   
    constexpr unique_ptr(const unique_ptr&) = delete;
    constexpr unique_ptr(unique_ptr&& u) noexcept : _ptr(u.release()), _deleter(std::forward<deleter_type>(u.get_deleter())) {}



    template <typename U, typename E>
    requires (std::is_convertible_v<U*, T*> && std::is_convertible_v<E, Deleter>)
    constexpr unique_ptr(unique_ptr<U, E>&& u) noexcept : _ptr(u.release()), _deleter(std::forward<E>(u.get_deleter())) {}
    constexpr ~unique_ptr() { reset(); }  
    constexpr unique_ptr& operator=(const unique_ptr&) = delete;
    constexpr unique_ptr& operator=(unique_ptr&& u) noexcept {
        reset(u.release());
        _deleter = std::forward<deleter_type>(u.get_deleter());
        return *this;   
    }
    template <typename U, typename E>
    requires (std::is_convertible_v<U*, T*> && std::is_convertible_v<E, Deleter>)
    constexpr unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        _deleter = std::forward<E>(u.get_deleter());
        return *this;
    }
    constexpr unique_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }
    constexpr reference operator*() const { return *get(); }
    constexpr pointer operator->() const noexcept { return get(); }
    constexpr pointer get() const noexcept { return _ptr; }
    constexpr deleter_type& get_deleter() noexcept { return _deleter; }
    constexpr const deleter_type& get_deleter() const noexcept { return _deleter; }
    constexpr explicit operator bool() const noexcept { return get() != nullptr; }
    constexpr pointer release() noexcept {    
        pointer old_ptr = _ptr;
        _ptr = nullptr;
        return old_ptr;
    }
    constexpr void reset(pointer p = pointer()) noexcept {
        pointer old_ptr = _ptr;
        _ptr = p;
        if (old_ptr) {
            _deleter(old_ptr);  
        }   
    }
    constexpr void swap(unique_ptr& u) noexcept {
        std::swap(_ptr, u._ptr);
        std::swap(_deleter, u._deleter);
    }
private:
    pointer _ptr;
    [[no_unique_address]] deleter_type _deleter;
};
template <typename T, typename... Args>
constexpr unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));   
}
#endif
}