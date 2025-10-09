#pragma once
#include "entity.hpp"
#include "component.hpp"
#include "registry.hpp"
#include "sparse_map.hpp"
#include <typeinfo>
#include <memory>
#include <unordered_map>


namespace ecstl {

template<typename K, typename V>
class SparseMapStorage: public SparseMap<K, V, HashOfKey<K>, std::equal_to<K> > {};
template<typename K, typename V>
class UnorderedMapStorage: public std::unordered_map<K, V, HashOfKey<K>, std::equal_to<K> > {};

template<typename T>
class ComponentPool: public GenericComponentPool<T, SparseMapStorage> {};

class Registry: public GenericRegistry<ComponentPool, UnorderedMapStorage>{};


}