#include "ecs.h"

#include "../ecstl/ecstl.hpp"
#include <span>

using BinaryComponentView = std::span<char>;
using ConstBinaryComponentView = std::span<const char>;

namespace ecstl {

    template<typename K,  typename Hasher, typename Equal  >
    class IndexedFlatMap<K, BinaryComponentView, Hasher, Equal> {
    public:

    protected:

    };

}


using namespace ecstl;


