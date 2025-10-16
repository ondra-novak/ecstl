#include "../libecs/ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestComp {
    int foo;
};


static int flags = 3;

static void deleter(void *ptr, size_t sz) {
    (void)sz;
    struct TestComp *c = (struct TestComp *)ptr;
    flags &= ~c->foo;
    printf("Deleter called: cleaning %d result: %d\n", c->foo, flags);
}


int main(void) {
    ecs_registry_t *rg = ecs_create_registry();
    ecs_entity_t tester = ecs_create_entity(rg);
    ecs_component_t testComp = ecs_register_component(rg, "greeting",&deleter);


    struct TestComp v;
    v.foo = 1;

    ecs_store(rg, tester, testComp, &v, sizeof(v));
    
    v.foo = 2;

    ecs_store(rg, tester, testComp, &v, sizeof(v));

    ecs_destroy_registry(rg);

    return flags;

}
