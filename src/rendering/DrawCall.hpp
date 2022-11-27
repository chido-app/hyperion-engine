#ifndef HYPERION_V2_DRAW_CALL_HPP
#define HYPERION_V2_DRAW_CALL_HPP

#include <core/Containers.hpp>
#include <Constants.hpp>
#include <core/HandleID.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <rendering/Buffers.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <unordered_map>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::GPUBuffer;

class Engine;
class Mesh;
class Material;
class Skeleton;

struct DrawCommandData;
class IndirectDrawState;

struct DrawCallID
{
    static_assert(sizeof(HandleID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    UInt64 value;

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(HandleID<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    DrawCallID(HandleID<Mesh> mesh_id, HandleID<Material> material_id)
        : value(mesh_id.Value() | (UInt64(material_id.Value()) << 32))
    {
    }

    bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    bool HasMaterial() const
        { return bool((UInt64(~0u) << 32) & value); }

    operator UInt64() const
        { return value; }

    UInt64 Value() const
        { return value; }
};

struct DrawCall
{
    static constexpr bool unique_per_material = !use_indexed_array_for_object_data;

    DrawCallID id;
    EntityBatchIndex batch_index;
    SizeType draw_command_index;

    HandleID<Material> material_id;
    HandleID<Skeleton> skeleton_id;

    UInt entity_id_count;
    HandleID<Entity> entity_ids[max_entities_per_instance_batch];

    UInt packed_data[4];

    Mesh *mesh;
};

struct DrawState
{
    Array<DrawCall> draw_calls;
    std::unordered_map<UInt64 /* DrawCallID */, Array<SizeType>> index_map;

    DrawState() = default;
    DrawState(const DrawState &other) = delete;
    DrawState &operator=(const DrawState &other) = delete;

    DrawState(DrawState &&other) noexcept;
    DrawState &operator=(DrawState &&other) noexcept;

    ~DrawState();

    void Push(EntityBatchIndex batch_index, DrawCallID id, IndirectDrawState &indirect_draw_state, const EntityDrawProxy &entity);
    DrawCall *TakeDrawCall(DrawCallID id);
    void Reset();
};

} // namespace hyperion::v2

#endif
