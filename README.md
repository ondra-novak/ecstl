# Entity Component System for C++ 

A fast and simple Entity Component System (ECS) library for C++. This library is designed to be easy to use and integrate into your C++ projects, providing a flexible way to manage entities and their components.

Required C++20 or later.

## Key Features
* Template-based design for type safety and flexibility
* Constexpr and inline functions for performance
* Support C++20 ranges
* Component is struct or class
* Multiple component of same type with different variants (ready for scripting integration)
* constexpr tests


## Installation
You can include the header files directly in your project. Simply clone the repository and add the `src` directory to your include path.

## Example Usage


### Hello World Example
```cpp
#include <ecstl/ecstl.hpp>
#include <iostream>

using namespace ecstl;

// Define a simple component
struct Greeting {
    std::string text;
};

int main() {
    // Create a registry and an entity
    Registry rg;
    Entity greeter = rg.create_entity();

    // Assign a Greeting component to the entity
    rg.set<Greeting>(greeter, {"Hello world"});

    // Access and print the Greeting component
    for (const auto &[e, c1] : rg.view<Greeting>()) {
        std::cout << e << ": " << c1.text << std::endl;
    }
    return 0;
}
```

### Using views

```cpp

struct TestComponent {
    int foo;
};


int main() {

    /// Create a registry and some entities
    Registry db;
    auto aaa = db.create_entity("aaa");
    auto bbb = db.create_entity("bbb");
    auto ccc = db.create_entity("ccc");
    auto ddd = db.create_entity("ddd");

    ///Assign some components
    ///You can define variants of components using ComponentTypeID
    db.set<TestComponent>(ddd,example_component,{55});
    db.set<TestComponent>(ccc,{55});
    db.set<TestComponent>(bbb,example_component,{42});
 

    ///Example of creating a view
    for (const auto &q: db.view<EntityName, TestComponent>({ComponentTypeID{}, example_component})  ) {
        const auto &[e,a,b] = q;
        std::cout << e << "=" <<  a << ":" << b.foo << std::endl;
    }
}
```

### Registry API

Assume r =  Registry();

#### Entity Management

* **r.create_entity()** - Create a new entity
* **r.create_entity(std::string_view name)** - Create a new entity with a name
* **r.destroy_entity(Entity e)** - Destroy an entity and all its components

#### Component Management

* **r.set&lt;ComponentType&gt;(Entity e, ComponentType component_data)** - Assign a component to an entity
* **r.set&lt;ComponentType&gt;(Entity e, ComponentTypeID variant, ComponentType component_data)** - Assign a component to an entity with a specific variant
* **r.emplace&lt;ComponentType&gt;(Entity e, Args &&... args )** - Assign a component to an entity, constructing it in place
* **r.emplace&lt;ComponentType&gt;(Entity e, ComponentTypeID variant, Args &&... args)** - Assign a component to an entity with a specific variant, constructing it in place
* **r.get&lt;ComponentType&gt;(Entity e)** - Get a reference to a component of an entity. Both const and non-const versions are available.
* **r.get&lt;ComponentType&gt;(Entity e, ComponentTypeID variant)** - Get a reference to a component of an entity with a specific variant. Both const and non-const versions are available
* **r.remove&lt;ComponentType&gt;(Entity e)** - Remove a component from an entity
* **r.remove&lt;ComponentType&gt;(Entity e, ComponentTypeID variant)** - Remove a component with a specific variant from an entity
* **r.remove_all_of&lt;ComponentType&gt;()** - Remove all components of a specific type from all entities.
* **r.remove_all_of&lt;ComponentType&gt;(ComponentTypeID variant)** - Remove all components of a specific type and variant from all entities.

#### Queries and Iteration

* **r.all_of&lt;ComponentType&gt;()** - Get a range of all components of a specific type. Both const and non-const versions are available.
* **r.all_of&lt;ComponentType&gt;(ComponentTypeID variant)** - Get a range of all components of a specific type and variant. Both const and non-const versions are available.
* **r.view<ComponentTypes...>(std::initializer_list&lt;CompoinentTypeID&gt; variants = {})** - Create a view for iterating over entities with specific component types and optional variants.
* **r.has<ComponentTypes...>(Entity e, std::initializer_list&lt;CompoinentTypeID&gt; variants = {})** - Check if an entity contains all specified component types and optional variants.
* **r.group<ComponentTypes...>(std::initializer_list&lt;CompoinentTypeID&gt; variants = {})** - Create a group for iterating over entities with specific component types and optional variants. Groups are optimized for frequent access.
* **r.for_each_component(Entity e, Callback &)** - Iterate over all components of an entity, invoking the provided callback for each component.

#### Entity Naming

* **r.get_entity_name(Entity e)** - Get the name of an entity
* **r.set_entity_name(Entity e, std::string_view name)** - Set the name
* **r.find_entity_by_name(std::string_view name)** - Find an entity by its name. Returns optional<Entity>.

### Support for trivial components and destructive move

Components can be defined as trivial structs and a `drop` method can be implemented, which is called when the component is destroyed.
This reduces the need to declare a move constructor and destructor, allowing the struct to remain trivial even if it contains pointers that need to be released at the end of its lifetime. The resource release can be handled directly in the `drop()` method.

Note: this feature is optional. You can still define custom destructors and move constructors if needed and they will work as expected.

```cpp

struct ComponentWithDrop {
    char *data = nullptr; 
    int a;
    float b;
    void drop() {delete [] data;}  //- called when component is removed, replaced or when registry is destroyed
}
```

## Signals

Communication between parts of the system that operate on different components is important. To support this, the library provides a convenience class implementing a signal–slot mechanism.

The headers `signals.hpp` and `signals_async.hpp` allow adding `SignalSlot<fn>` or `SharedSignalSlot<fn>` to your project. In async mode you can use `SignalSlot<fn, AsyncSignalDispatcher>`.

```cpp
SignalSlot<void(int)> slot;
auto con = connect(slot, [&](int v){ /*... some code...*/});

slot(42); //broadcast signal
```
Signal slots can be allocated statically, globally, or inside class instances. It is not recommended to place signal slots inside components. An asynchronous signal slot allows handlers to run in parallel using a thread pool, or — without a pool — to defer processing of all handlers until a later, convenient time (using AsyncSignalDispatcher<0>).


## Notes

### Qualifiers and non-const access to components in a const registry

All components are treated as a "payload" that the Registry only manages. A const-qualified get or find method may therefore return a non-const reference, because modifying the contents of a component does not affect the Registry's own state. When using locks, the Registry can be locked with a shared lock while component contents are still modified — the lock protects only the Registry object's internal data, not the user-managed payload.

If you need to enforce read-only access to components, specify the const qualifier for component types in methods such as get(), find(), view(), etc.

```cpp

for (auto &r: r.view<const C1, const C2, const C3>()) {...}
r.get<const C1>();
r.all_of<const C1>();
r.find<const C1>();
```

In other operations, qualifiers are not allowed and are generally ignored.


```cpp
r.set<const C1> ~= r.set<C1>
```

### Validity of views and iterators in various situations

- Changes in the registry do not invalidate a view as long as it is not being iterated. Exception: deleting the component pool for a type that is part of the iteration is undefined behavior for Registry configurations without shared pointers (the default).
- View iterators
    - remain valid for all const operations
    - remain valid when component contents is being modified
    - remain valid if components that are not currently being iterated are added or removed
    - **become invalid if a component that is part of the current iteration is added or removed**

**Recommendation:**

If you need to delete components that are currently being iterated, create a list of affected entities and perform deletions outside the iteration loop.

```cpp
std::vector<Entity> list;
for (auto [e,c1,c2]: r.view<C1,...>()) {
    if (need_remove_this(c1)) list.push_back(e);
}
for (auto e: list) r.remove<C1>(e);
```


## C Interface

A C interface is provided through a library libecsc. The C interface is limited compared to the C++ interface, but allows basic operations such as creating entities, assigning components, and querying components. Components are untyped, so you don't have
access to component variants. The component data are copied as binary blobs directly into components storage. You can define
deleter functions for components that need special handling when destroyed.

### Types
```c
/// Opaque registry type
typedef struct _ecs_registry_type ecs_registry_t;
/// Entity type
typedef size_t ecs_entity_t;
/// Component type
typedef size_t ecs_component_t;
/// Deleter function type for component data
typedef void (*ecs_component_deleter_t)(void *data, size_t sz);

```

### Create and destroy objects

```c
ecs_registry_t * ecs_create_registry(void);
void ecs_destroy_registry(ecs_registry_t * registry);
ecs_entity_t ecs_create_entity(ecs_registry_t * reg);
ecs_entity_t ecs_create_named_entity(ecs_registry_t * reg, const char *name);
void ecs_destroy_entity(ecs_registry_t * reg, ecs_entity_t e);
ecs_component_t ecs_register_component(ecs_registry_t * reg, const char *name, ecs_component_deleter_t deleter);
void ecs_unregister_component(ecs_registry_t * reg, ecs_component_t component);
```

### store and retrieve components

```c
int ecs_store(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component, const void *data, size_t size);
const void *ecs_retrieve(const ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);
const void *ecs_retrieve_mut(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);
void ecs_remove(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);
```

### query / enumeration 

Enumeration and querying is provided through a callback mechanism.

```c
typedef int (*callback)(ecs_entity_t, const void **data, void *context);
```
where:
* `entity` - the entity id
* `data` - pointer to array of component data pointers, you need to cast them to appropriate types. Count of pointers is equal to number of components requested in the query.
* `context` - user provided context pointer

```c
int ecs_view_iterate(ecs_registry_t * reg, int component_count, const ecs_component_t *components,
            int (*callback)(ecs_entity_t, const void **data, void *context), void *context);
int ecs_view_iterate_mut(ecs_registry_t * reg, int component_count, const ecs_component_t *components,
            int (*callback)(ecs_entity_t, void **data, void *context), void *context);
```

### grouping

Similar to C++ groups, you can create a group for frequently accessed components.

```c
int ecs_group(ecs_registry_t * reg, int component_count, const ecs_component_t *components);
```

### Miscellaneous

```c
int ecs_has(const ecs_registry_t *reg, ecs_entity_t entity, int component_count, const ecs_component_t *components);
size_t ecs_get_entity_name(ecs_registry_t * reg, ecs_entity_t e, char *buf, size_t bufsize);
ecs_entity_t ecs_find_entity_by_name(ecs_registry_t * reg, const char *name);
```



## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details


