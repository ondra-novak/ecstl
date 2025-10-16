#include "../libecs/ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestComponent {
    int foo;
};


struct CallbackCtx {
    int equal;
    int count;
};

static int view_callback(ecs_entity_t e, const void **components, void *ctx) {
    (void)e;
    struct CallbackCtx *context = (struct CallbackCtx *)ctx;
    const struct TestComponent *c1 = (const struct TestComponent *)components[0]; 
    const struct TestComponent *c2 = (const struct TestComponent *)components[1];
    context->count++;
    if (c1->foo == c2->foo) context->equal++;
    return 0;
}

int main(void) {
    ecs_registry_t *rg = ecs_create_registry();
    ecs_component_t c1 = ecs_register_component(rg, "c1",NULL);
    ecs_component_t c2 = ecs_register_component(rg, "c2",NULL);

    for (int i = 0; i < 100; ++i) {
        struct TestComponent c;
        c.foo = i;

        ecs_entity_t e = ecs_create_entity(rg);
        
        if (i % 4 == 3) ecs_store(rg, e, c1, &c, sizeof(c));
        if (i % 3 == 2) ecs_store(rg, e, c2, &c, sizeof(c));
    }


    ecs_component_t cset[2] = {c1,c2};
    struct CallbackCtx ctx = {0,0};
    ecs_view_iterate(rg, 2, cset, &view_callback, &ctx);
    printf("%d,%d\n", ctx.count, ctx.equal);
    ecs_group(rg, 2, cset);
    return ctx.count == ctx.equal;
}
