#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <memory>
#include <utility>
#include <tuple>

namespace hyperion {
namespace renderer {

struct MeshBindingDescription
{
    UInt32 binding;
    UInt32 stride;
    VkVertexInputRate input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(UInt32 binding, UInt32 stride, VkVertexInputRate input_rate)
        : binding(binding), stride(stride), input_rate(input_rate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->input_rate;
        return bind;
    }
};


struct QueueFamilyIndices
{
    using Index = UInt32;
    
    std::optional<Index> graphics_family;
    std::optional<Index> transfer_family;
    std::optional<Index> present_family;
    std::optional<Index> compute_family;

    bool IsComplete() const {
        return graphics_family.has_value()
            && transfer_family.has_value()
            && present_family.has_value()
            && compute_family.has_value();
    }
};

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};


struct PackedVertex
{
    Float32 position_x,
        position_y,
        position_z,
        normal_x,
        normal_y,
        normal_z,
        texcoord0_x,
        texcoord0_y;
};

static_assert(sizeof(PackedVertex) == sizeof(Float32) * 8);

struct alignas(16) MeshDescription
{
    UInt64 vertex_buffer_address;
    UInt64 index_buffer_address;
    
    UInt32 entity_index;
    UInt32 material_index;
    UInt32 num_indices;
    UInt32 num_vertices;
};

using PackedIndex = UInt32;

using ImageSubResourceFlagBits = UInt;

enum ImageSubResourceFlags : ImageSubResourceFlagBits
{
    IMAGE_SUB_RESOURCE_FLAGS_NONE    = 0,
    IMAGE_SUB_RESOURCE_FLAGS_COLOR   = 1 << 0,
    IMAGE_SUB_RESOURCE_FLAGS_DEPTH   = 1 << 1,
    IMAGE_SUB_RESOURCE_FLAGS_STENCIL = 1 << 2
};

/* images */
struct ImageSubResource
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    UInt32 base_array_layer = 0;
    UInt32 base_mip_level = 0;
    UInt32 num_layers = 1;
    UInt32 num_levels = 1;

    bool operator==(const ImageSubResource &other) const
    {
        return flags == other.flags
            && base_array_layer == other.base_array_layer
            && num_layers == other.num_layers
            && base_mip_level == other.base_mip_level
            && num_levels == other.num_levels;
    }
};

template<class ...Args>
class PerFrameData
{
    struct FrameDataWrapper
    {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(UInt32 num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    HYP_FORCE_INLINE UInt NumFrames() const
        { return m_num_frames; }

    HYP_FORCE_INLINE FrameDataWrapper &operator[](UInt32 index)
        { return m_data[index]; }

    HYP_FORCE_INLINE const FrameDataWrapper &operator[](UInt32 index) const
        { return m_data[index]; }

    HYP_FORCE_INLINE FrameDataWrapper &At(UInt32 index)
        { return m_data[index]; }

    HYP_FORCE_INLINE const FrameDataWrapper &At(UInt32 index) const
        { return m_data[index]; }

    HYP_FORCE_INLINE void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    UInt m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

struct IndirectDrawCommand
{
    // native vk object
    VkDrawIndexedIndirectCommand command;
};

static_assert(std::is_standard_layout_v<IndirectDrawCommand>, "IndirectDrawCommand must be POD");
static_assert(sizeof(IndirectDrawCommand) == 20, "Verify size of struct in shader");

} // namespace renderer
} // namespace hyperion

template <>
struct ::std::hash<hyperion::renderer::ImageSubResource>
{
    size_t operator()(const hyperion::renderer::ImageSubResource &sub_resource) const
    {
        hyperion::HashCode hc;
        hc.Add(sub_resource.flags);
        hc.Add(sub_resource.base_array_layer);
        hc.Add(sub_resource.num_layers);
        hc.Add(sub_resource.base_mip_level);
        hc.Add(sub_resource.num_levels);

        return hc.Value();
    }
};

#endif //HYPERION_RENDERER_STRUCTS_H
