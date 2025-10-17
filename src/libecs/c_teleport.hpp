#pragma once
#include "ecs.h"
#include "../ecstl/ecstl.hpp"

namespace ecstl {


    Registry *cast_from_c(ecs_registry_t * reg) {
        return reinterpret_cast<Registry *>(reg);
    }
    const Registry *cast_from_c(const ecs_registry_t * reg) {
        return reinterpret_cast<const Registry *>(reg);
    }
    ecs_registry_t *cast_to_c(Registry * reg) {
        return reinterpret_cast<ecs_registry_t *>(reg);
    }     
    const ecs_registry_t *cast_to_c(const Registry * reg) {
        return reinterpret_cast<const ecs_registry_t *>(reg);
    }     

    

}