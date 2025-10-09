#pragma once
#include <cstdint>
#include <atomic>
#include <compare>


namespace ecstl {

/// An entity identifier
/** Entities are identified by a unique integer ID */
class Entity {
public:

    /// Default constructor creates a null entity
    constexpr Entity():_id(0) {}
    /// Construct an entity from a given ID
    /**
     * This constructor is mainly used internally to create entities with a specific ID.
     * It also ensures that the ID generator is updated to avoid ID collisions.
     * Useful for deserialization or cloning entities.
     */
    Entity(std::uint64_t id):_id(id) {
        auto r = _idgen.load();
        while (id > r && !_idgen.compare_exchange_weak(r, id));        
    }

    /// Create a new unique entity
    static Entity create()  {        
        return Entity(_idgen.fetch_add(1)+1);
    }

    constexpr bool operator==(const Entity &) const  = default;
    constexpr std::strong_ordering operator<=>(const Entity &) const  = default;

    /// Print the entity to an output stream
    template<typename IO>
    friend IO &operator<<(IO &io, const Entity &e)  {
        return (io << "#" << e._id);
    }

    friend std::size_t get_hash(const Entity &e) {
        std::hash<std::uint64_t> hasher;
        return hasher(e._id);
    }

protected:
    std::uint64_t _id;
    static std::atomic<std::uint64_t> _idgen;

};


inline std::atomic<std::uint64_t> Entity::_idgen;


}