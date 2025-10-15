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
* **r.find&lt;ComponentType&gt;(Entity e)** - Find a component of an entity. Returns an iterator or end() if not found. Both const and non-const versions are available.
* **r.find&lt;ComponentType&gt;(Entity e, ComponentTypeID variant)** - Find a component of an entity. Returns an iterator or end() if not found. Both const and non-const versions are available.
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

```cpp

struct ComponentWithDrop {
    char *data = nullptr; 
    int a;
    float b;
    void drop() {delete data;}  //- called when component is removed, replaced or when registry is destroyed
}
```

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
