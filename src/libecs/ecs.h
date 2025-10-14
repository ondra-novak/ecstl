#ifndef _LIBECS_FOR_C_HEADER_INCLUDED_324782301239103
#define _LIBECS_FOR_C_HEADER_INCLUDED_324782301239103

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif 

/// Opaque registry type
typedef struct _ecs_registry_type ecs_registry_t;
/// Entity type
typedef size_t ecs_entity_t;
/// Component type
typedef size_t ecs_component_t;
/// Deleter function type for component data
typedef void (*ecs_component_deleter_t)(void *data, size_t sz);


/// Create new registry
/** 
 * @return newly created registry, NULL on error
 */
ecs_registry_t * ecs_create_registry();
/// Destroy registry
/**
 * @param registry registry to destroy
 */
void ecs_destroy_registry(ecs_registry_t * registry);

/// Create new entity
/**
 * @param reg registry where to create the entity
 * @return newly created entity
 */
ecs_entity_t ecs_create_entity(ecs_registry_t * reg);
/// Create new entity with a name
/**
 * @param reg registry where to create the entity
 * @param name name of the entity (must be null-terminated string)
 * @return newly created entity
 */
ecs_entity_t ecs_create_named_entity(ecs_registry_t * reg, const char *name);
/// Destroy entity
/**
 * @param reg registry where the entity is stored
 * @param e entity to destroy
 * @note all component data associated with the entity is also destroyed. Deleters registered for components are called appropriately.
 */
void ecs_destroy_entity(ecs_registry_t * reg, ecs_entity_t e);

/// Get the name of an entity
/**
 * @param reg registry where the entity is stored
 * @param e entity whose name to get
 * @param buf buffer where to store the name (can be NULL)
 * @param bufsize size of the buffer (if buf is not NULL)
 * @return size of the name (not including terminating null character). If return value is >= bufsize, the name was truncated.
 * If the buffer is NULL, the return value is the size of the buffer needed to store the name (including terminating null character).
 * If the entity has no name, 0 is returned (the buffer is not modified in this case).
 */
size_t ecs_get_entity_name(ecs_registry_t * reg, ecs_entity_t e, char *buf, size_t bufsize);
/// Find entity by name
/**
 * @param reg registry where to search for the entity
 * @param name name of the entity to search for (must be null-terminated string)
 * @return entity with the given name, or 0 if no such entity exists
 */
ecs_entity_t ecs_find_entity_by_name(ecs_registry_t * reg, const char *name);


///Register new component
/**
 * @param name name of the component
 * @param deleter optional (can be NULL), specifies deleter which is called whenever the component is deleted
 * @return newly created component
 * @note returned component id is computed from the name using a hash function, so the same component name will always return the same component id.
 * If the component name was already registered, the existing component id is returned (the deleter is not changed in this case).
 */
ecs_component_t ecs_register_component(ecs_registry_t * reg, const char *name, ecs_component_deleter_t deleter);
/// Remove all components of given type
/**
 * @param reg registry where to remove the components
 * @param component component type to remove    
 * @note deleters registered for the component are called appropriately.
 */ 
void ecs_remove_all(ecs_registry_t * reg, ecs_component_t component);

/// Set component data for an entity
/**
 * @param reg registry where the entity is stored
 * @param entity entity for which to set the component data
 * @param component component type
 * @param data pointer to the component data (copied into the registry)
 * @param size size of the component data
 * @return 0 on success, non-zero on error
 * 
 * @note The size of the data must be the same as the size used when the component was first set for any entity.
 * If the component was never set for any entity before, any size is accepted.
 * If the entity already has component data of the given type, it is replaced (the deleter is called for the old data).
 * In case of size mistmatch, the function fails and returns -1
 */
int ecs_set(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component, const void *data, size_t size);
/// Get component data for an entity (const)
/**
 * @param reg registry where the entity is stored
 * @param entity entity for which to get the component data
 * @param component component type
 * @return pointer to the component data, or NULL if the entity does not have component of the given type
 * @note the returned pointer is valid as long as the component data is not modified or removed
 */
const void *ecs_get(const ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);
/// Get component data for an entity (mutable)
/**
 * @param reg registry where the entity is stored
 * @param entity entity for which to get the component data
 * @param component component type
 * @return pointer to the component data, or NULL if the entity does not have component of  the given type
 * @note the returned pointer is valid as long as the component data is not removed. If the component data is modified, the pointer may become invalid (e.g. if the component storage needs to reallocate memory)
 */
const void *ecs_get_mut(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);
/// Remove component data for an entity
/** 
 * @param reg registry where the entity is stored
 * @param entity entity for which to remove the component data
 * @param component component type
 * @note if the entity does not have component of the given type, nothing happens
 * @note the deleter registered for the component is called appropriately
 */
void ecs_remove(ecs_registry_t * reg, ecs_entity_t entity, ecs_component_t component);

/// Maximum number of components in a view
/** 
 * @note changing this value requires recompilation of the library and all code using it (the backend is templated on this value).
 */
#define ECS_MAX_COMPONENT_COUNT_IN_VIEW 128

/// Create a view for iterating over entities having given components
/**
 * @param reg registry where to create the view
 * @param component_count number of components in the view (must be > 0 and <= ECS_MAX_COMPONENT_COUNT_IN_VIEW)
 * @param components array of component types (of length component_count)
 * @param callback callback function which is called for each entity in the view. The callback receives the entity and an array of pointers to the component data (of length component_count).
 * If the component data is mutable (see ecs_view_iterate_mut), the pointers point to mutable data, otherwise they point to const data.
 * @param context user-defined context pointer which is passed to the callback function as is
 * @return 0 on success, non-zero if iteration was aborted by the callback (the callback returned non-zero value).
 * @note if component_count is 0 or greater than ECS_MAX_COMPONENT_COUNT_IN_VIEW, the function fails and returns -1
 * @note if any of the components was not registered, it is considered as having no entities, so the callback is never called and the function returns 0
 */

int ecs_view_iterate(ecs_registry_t * reg, int component_count, const ecs_component_t *components,
            int (*callback)(ecs_entity_t, const void **data, void *context), void *context);

/// Create a view for iterating over entities having given components (mutable component data)
/**
 * @param reg registry where to create the view
 * @param component_count number of components in the view (must be > 0 and <= ECS_MAX_COMPONENT_COUNT_IN_VIEW)
 * @param components array of component types (of length component_count)
 * @param callback callback function which is called for each entity in the view. The callback receives the entity and an array of pointers to the component data (of length component_count).
 * If the component data is mutable (see ecs_view_iterate_mut), the pointers point to mutable data, otherwise   they point to const data.   
 * If the callback returns non-zero value, the iteration is aborted and the function returns this value.
 *  @param context user-defined context pointer which is passed to the callback function as is
 * @return 0 on success, non-zero if iteration was aborted by the callback (the callback returned non-zero value).
 * @note if component_count is 0 or greater than ECS_MAX_COMPONENT_COUNT_IN_VIEW, the function fails and returns -1
 * @note if any of the components was not registered, it is considered as having no entities, so the callback is never called and the function returns 0
 * @note if multiple mutable components are requested, and they are the same component, the behavior is undefined (the callback may receive multiple pointers to the same data)
 */
int ecs_view_iterate_mut(ecs_registry_t * reg, int component_count, const ecs_component_t *components,
            int (*callback)(ecs_entity_t, void **data, void *context), void *context);

/// Create a group for fast iteration over entities having given components
/**
 * Groups are sort of optimalization, which allow faster iteration over views. It reorganizes the storage of components
 * so iteration over the group is as faster. However any changes to the entities (adding or removing components, creating or destroying entities) 
 * can disrupt the group, so it should be used only when the set of entities is mostly static.
 * 
 * @param reg registry where to create the group
 * @param component_count number of components in the group (must be > 0 and <= ECS_MAX_COMPONENT_COUNT_IN_VIEW)
 * @param components array of component types (of length component_count)
 * @retval 1 if the group was created successfully,
 * @retval 0 no optimization was possible (there is no intersection of entities having all the given components)
 * @retval-1 error, e.g. component_count is 0 or greater than ECS_MAX_COMPONENT_COUNT_IN_VIEW
 */
int ecs_group(ecs_registry_t * reg, int component_count, const ecs_component_t *components);

/// Lock the registry for writing (exclusive access)
void lock_ecs_registry(ecs_registry_t * reg);
/// Unlock the registry for writing
void unlock_ecs_registry(ecs_registry_t * reg);
/// Lock the registry for reading (shared access)
void lock_ecs_registry_shared(ecs_registry_t * reg);
/// Unlock the registry for reading
void unlock_ecs_registry_shared(ecs_registry_t * reg);


#ifdef __cplusplus
}
#endif 


#endif