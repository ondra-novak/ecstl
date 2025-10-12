#include "../ecstl/ecstl.hpp"

using namespace ecstl;

constexpr bool create_entity() {
    auto e1 = Entity::create_consteval();
    Registry rg;
    rg.set_entity_name(e1, "consteval");
    auto n = rg.get_entity_name(e1);
    return n == "consteval";
}

static_assert(create_entity(), "Constexpr entity creation failed");

