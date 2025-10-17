#include "../ecstl/ecstl.hpp"
#include <functional>
#include <iostream>
#include <ranges>

template ecstl::Entity ecstl::GenericRegistry<ecstl::ComponentPool, ecstl::IndexedFlatMapStorage>::create_entity(std::string_view);
template auto ecstl::GenericRegistry<ecstl::ComponentPool, ecstl::IndexedFlatMapStorage>::all_of<ecstl::EntityName>();
template auto ecstl::GenericRegistry<ecstl::ComponentPool, ecstl::IndexedFlatMapStorage>::all_of<ecstl::EntityName>() const;

struct TestComponent {
    int foo;
};

constexpr auto example_component = ecstl::ComponentTypeID("example_component");


void testOpenHash() {
    ecstl::OpenHashMap<int, int> hh;
    for (int i = 0; i < 100; ++i) {
        hh.emplace(i, i*2+1);
    }
    for (int i = 0; i < 100;  ++i) {
        auto iter = hh.find(i);
        if (iter == hh.end()) abort();
        if (iter->second != i *2 + 1) abort();
    }
    for (int i = 0; i < 100; i+=2) {
        hh.erase(i);
    }
    for (int i = 0; i < 100; i+=2) {
        auto iter = hh.find(i);
        if (iter != hh.end()) abort();
    }
    for (int i = 1; i < 200; i+=2) {
        hh.emplace(i, i*3+1);
    }
    for (int i = 0; i < 100;  ++i) {
        auto iter = hh.find(i);
        if (i & 1) {
            if (iter == hh.end()) abort();
            if (iter->second != i *2 + 1) abort();
        } else {
            if (iter != hh.end()) abort();
        }
    }
    for (int i = 101; i < 200;++i ) {
        auto iter = hh.find(i);
        if (i & 1) {
            if (iter == hh.end()) abort();
            if (iter->second != i *3 + 1) abort();
        } else {
            if (iter != hh.end()) abort();
        }
    }

}

int main() {
    testOpenHash();

    ecstl::Registry db;
    auto aaa = db.create_entity("aaa");
    auto bbb = db.create_entity("bbb");
    auto ccc = db.create_entity("ccc");
    auto ddd = db.create_entity("ddd");

    db.set<TestComponent>(ddd,example_component,{55});
    db.set<TestComponent>(ccc,{55});
    db.set<TestComponent>(bbb,example_component,{42});
 


    db.for_each_component(aaa, [](ecstl::AnyRef ref){
        ref.get_if<ecstl::EntityName>().and_then([](const auto &x){std::cout << "Name: " << x << std::endl;;});
    });
    for (const auto &c: db.all_of<ecstl::EntityName>()) {
        std::cout << c.first << " = " << c.second << std::endl;
    }

    for (auto [e,a,b]: db.view<ecstl::EntityName, TestComponent>({ecstl::ComponentTypeID{}, example_component})  ) {
        std::cout << e << "=" <<  a << ":" << b.foo << std::endl;
    }

    std::cout << db.has<TestComponent, ecstl::EntityName>(bbb, {example_component}) << std::endl;
    std::cout << db.has<TestComponent, ecstl::EntityName>(aaa, {example_component}) << std::endl;

    db.group<ecstl::EntityName, TestComponent>({{}, example_component});

    for (const auto &c: db.all_of<ecstl::EntityName>()) {
        std::cout << c.first << " = " << c.second << std::endl;
    }

    for (const auto &c: db.all_of<TestComponent>(example_component)) {
        std::cout << c.first << " = " << c.second.foo << std::endl;
    }


}