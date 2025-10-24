#include "registry.hpp"

namespace ecstl {


struct RegistryTraitsForSharedPtrs : DefaultRegistryTraits{

    using PoolSmartPtr = std::shared_ptr<IComponentPool>;

    template<typename T>
    using ComponentPoolPtr =  std::conditional_t<std::is_const_v<T>, 
        std::shared_ptr<const ComponentPool<T> > , std::shared_ptr<ComponentPool<T> > >;

    template<typename T>
    static ComponentPoolPtr<T> cast_to_component_pool_ptr(const PoolSmartPtr &ptr) {
        return std::static_pointer_cast<typename ComponentPoolPtr<T>::element_type >(ptr);
    }
    template<typename T>
    static PoolSmartPtr create_pool() {
        return std::make_shared<ComponentPool<T> >();
    }   
};

///Implements registr with using shared pointers
/**
 * - views are retained even if registry is destroyed
 * - group() doesn't affect current view.
 * - this is core class which allows to implement MT safe registry with RW locks
 */
using RegistrySharedPtr = GenericRegistry<RegistryTraitsForSharedPtrs>;


}