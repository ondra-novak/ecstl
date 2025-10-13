#pragma once
#include "entity.hpp"
#include "component.hpp"
#include "registry.hpp"
#include "indexed_flat_map.hpp"
#include <typeinfo>
#include <memory>
#include <unordered_map>


namespace ecstl {

template<typename K, typename V>
class IndexedFlatMapStorage: public IndexedFlatMap<K, V, HashOfKey<K>, std::equal_to<K> > {};
template<typename K, typename V>
class UnorderedMapStorage: public OpenHashMap<K, V, HashOfKey<K>, std::equal_to<K> > {};

template<typename T>
using ComponentPool = GenericComponentPool<T, IndexedFlatMapStorage>;
using Registry =  GenericRegistry<ComponentPool, UnorderedMapStorage>;


}