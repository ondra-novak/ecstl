#include "../ecstl/ecstl.hpp"
#include <iostream>

using namespace ecstl;

struct Greeting {
    std::string text;
};


int main() {
    Registry rg;
    Entity greeter = rg.create_entity();

    rg.set<Greeting>(greeter, {"Hello world"});

    for (const auto &[e, c1] : rg.view<Greeting>()) {
        std::cout << e << ": " << c1.text << std::endl;
    }
    return 0;
}