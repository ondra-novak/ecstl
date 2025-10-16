#include "../libecs/ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Greeting {
    char text[50];
};

int print_view(ecs_entity_t e, const void **components, void *unused) {
    (void)unused;
    printf("#%zu = %s\n", e, ((const struct Greeting *)components[0])->text);
    return 0;
}

void deleter(void *ptr, size_t sz) {
    printf("Deleter called %p (%zu)\n", ptr, sz);
}

int main(void) {
    ecs_registry_t *rg = ecs_create_registry();
    ecs_entity_t greeter = ecs_create_entity(rg);
    ecs_component_t greeting = ecs_register_component(rg, "greeting",&deleter);

    struct Greeting val;
    strncpy(val.text,"Hello world!",sizeof(val.text));
    ecs_store(rg,greeter,greeting,&val,sizeof(val));
    ecs_view_iterate(rg, 1, &greeting, &print_view, NULL);
    ecs_destroy_registry(rg);

}

