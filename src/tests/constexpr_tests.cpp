#include "../ecstl/ecstl.hpp"

using namespace ecstl;

struct PrimHash {
    constexpr std::size_t operator()(int x) const {return static_cast<std::size_t>(x);}
};


constexpr int test_open_hash() {
    ecstl::OpenHashMap<int, unique_ptr<int>, PrimHash> hh;
    for (int i = 0; i < 100; ++i) {
        hh.emplace(i, make_unique<int>(i*2+1));
    }
    for (int i = 0; i < 100;  ++i) {
        auto iter = hh.find(i);
        if (iter == hh.end()) return 1;
        if (*iter->second != i *2 + 1) return 2;
    }
    for (int i = 0; i < 100; i+=2) {
        hh.erase(i);
    }
    for (int i = 0; i < 100; i+=2) {
        auto iter = hh.find(i);
        if (iter != hh.end()) return 3;
    }
    for (int i = 1; i < 200; i+=2) {
        hh.emplace(i, make_unique<int>(i*3+1));
    }
    for (int i = 0; i < 100;  ++i) {
        auto iter = hh.find(i);
        if (i & 1) {
            if (iter == hh.end()) return 4;
            if (*iter->second != i *2 + 1) return 5;
        } else {
            if (iter != hh.end()) return 6;;
        }
    }
    for (int i = 101; i < 200;++i ) {
        auto iter = hh.find(i);
        if (i & 1) {
            if (iter == hh.end()) return 7;
            if (*iter->second != i *3 + 1) return 8;
        } else {
            if (iter != hh.end()) return 9;
        }
    }
    return 0;
}

static_assert(test_open_hash() == 0, "Failed");;

constexpr bool create_entity() {
    auto e1 = Entity::create_consteval();
    Registry rg;
    rg.set_entity_name(e1, "consteval");
    auto n = rg.get_entity_name(e1);
    return n == "consteval";
}

//static_assert(create_entity(), "Constexpr entity creation failed");

struct TestComponent {
    int foo;
};

constexpr Registry prepare_test_registry() {
    auto aaa = Entity(1, Entity::is_const_eval{});
    auto bbb = Entity(2, Entity::is_const_eval{});
    auto ccc = Entity(3, Entity::is_const_eval{});
    auto ddd = Entity(4, Entity::is_const_eval{});

    Registry rg;
    rg.set_entity_name(aaa, "aaa");
    rg.set_entity_name(bbb, "bbb");
    rg.set_entity_name(ccc, "ccc");
    rg.set_entity_name(ddd, "ddd");

    rg.set<TestComponent>(ddd,{55});
    rg.set<TestComponent>(bbb,{42});
    return rg;

}

constexpr int basic_view_test() {
 
    auto rg = prepare_test_registry();
    constexpr std::pair<std::string_view, int> results[] = {
        {"ddd", 55},
        {"bbb", 42},
    };

    auto res = std::begin(results);
    for (auto [_, n,t]:rg.view<ecstl::EntityName, TestComponent>()) {
        if (n != res->first) return 1;
        if (t.foo != res->second) return 2;
        ++res;
    }

    res = std::begin(results);
    for (auto [_, n,t]:const_cast<const Registry &>(rg).view<ecstl::EntityName, TestComponent>()) {
        if (n != res->first) return 3;
        if (t.foo != res->second) return 4;
        ++res;
    }
    return 0;

}

static_assert(basic_view_test() == 0);

constexpr int optimize_pool_test() {
 
    Registry rg = prepare_test_registry();

    rg.group<EntityName, TestComponent>();
    
    constexpr std::string_view results[] = {"aaa","bbb","ddd","ccc"};
    constexpr int results2[] = {42,55};

    auto res = std::begin(results);
    for (const auto &[_, n]: rg.all_of<EntityName>()) {
        if (n != *res) return 1;
        ++res;
    }

    auto res2 = std::begin(results2);
    for (const auto &[_, t]: rg.all_of<TestComponent>()) {
        if (t.foo != *res2) return 2;
        ++res2;
    }
    return 0;

}

static_assert(optimize_pool_test() == 0);


struct DropTestComponent {
    int *ptr = nullptr;
    constexpr void drop() {
        delete ptr;
    }
};

static_assert(is_droppable_by_method<DropTestComponent>);
static_assert(is_droppable<DropTestComponent>);

constexpr bool drop_test() {
    Registry r;
    auto e = Entity::create_consteval();
    r.set<DropTestComponent>(e, {new int(42)});
    r.set<DropTestComponent>(e, {new int(52)});
    r.remove<DropTestComponent>(e);
    r.set<DropTestComponent>(e, {new int(78)});
    //release in destructor
    //test fails if extra memory leaks
    return true;
}



static_assert(drop_test());



