#pragma once
#include <cstdint>
#include <atomic>
#include <compare>
#include <source_location>
#include "hasher.hpp"


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
    constexpr Entity(std::uint64_t id):_id(id) {
        if (std::is_constant_evaluated()) return;
        auto r = _idgen.load();
        while (id > r && !_idgen.compare_exchange_weak(r, id));        
    }

    /// Create a new unique entity
    static Entity create()  {        
        return Entity(_idgen.fetch_add(1)+1);
    }

    static consteval Entity create_consteval(std::source_location loc = std::source_location::current()) {
        std::uint64_t idgen = 1 + get_hash(loc.file_name()) + loc.line() + (std::uint64_t(loc.column()) << 16);
        return Entity(idgen);
    }

    constexpr bool operator==(const Entity &) const  = default;
    constexpr std::strong_ordering operator<=>(const Entity &) const  = default;

    /// Print the entity to an output stream
    template<typename IO>
    friend IO &operator<<(IO &io, const Entity &e)  {
        return (io << "#" << e._id);
    }

    friend constexpr std::size_t get_hash(const Entity &e) {
        return e._id;
    }

protected:
    std::uint64_t _id;
    static std::atomic<std::uint64_t> _idgen;

};


inline std::atomic<std::uint64_t> Entity::_idgen;


}