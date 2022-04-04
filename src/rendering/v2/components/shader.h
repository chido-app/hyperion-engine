#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"
#include "bindless.h"

#include <rendering/backend/renderer_shader.h>
#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>
#include <util/heap_array.h>
#include <util/range.h>

#include <memory>

namespace hyperion::v2 {

using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;

struct alignas(256) ObjectShaderData {
    Matrix4 model_matrix;
};

HYP_PACK_BEGIN
struct TextureRef {
    uint32_t index;
    uint32_t used;
}
HYP_PACK_END

struct alignas(256) MaterialShaderData {
    static constexpr size_t max_bound_textures = 8;

    Vector4 albedo;

    float metalness;
    float roughness;
    float subsurface;
    float specular;

    float specular_tint;
    float anisotropic;
    float sheen;
    float sheen_tint;

    float clearcoat;
    float clearcoat_gloss;
    float emissiveness;
    float _padding;

    uint32_t uv_flip_s;
    uint32_t uv_flip_t;
    float uv_scale;
    float parallax_height;
    
    TextureRef texture_index[max_bound_textures];
};

static_assert(sizeof(MaterialShaderData) == 256);

struct alignas(256) SceneShaderData {
    Matrix4 view;
    Matrix4 projection;
    Vector4 camera_position;
    Vector4 light_direction;
};

template <class StructType>
class BufferRangeUpdater {
protected:
    template <class BufferContainer>
    void PerformUpdate(Device *device, BufferContainer &buffer_container, size_t buffer_index, StructType *ptr)
    {
        auto &dirty = m_dirty[buffer_index];

        if (!dirty.GetEnd()) {
            return;
        }

        AssertThrow(dirty.GetEnd() > dirty.GetStart());

        buffer_container[buffer_index]->Copy(
            device,
            dirty.GetStart()    * sizeof(StructType),
            dirty.GetDistance() * sizeof(StructType),
            ptr + dirty.GetStart()
        );

        dirty = { 0, 0 };
    }

    void MarkDirty(size_t index)
    {
        for (size_t i = 0; i < m_dirty.size(); i++) {
            m_dirty[i] = {
                MathUtil::Min(m_dirty[i].GetStart(), index),
                MathUtil::Max(m_dirty[i].GetEnd(), index + 1)
            };
        }
    }

    std::vector<Range<size_t>> m_dirty;
};

template <class Buffer, class StructType, size_t Size>
class ShaderData : public BufferRangeUpdater<StructType> {
public:
    ShaderData(size_t num_buffers)
    {
        m_buffers.resize(num_buffers);
        this->m_dirty.resize(num_buffers);

        for (size_t i = 0; i < num_buffers; i++) {
            m_buffers[i] = std::make_unique<Buffer>();
            this->m_dirty[i] = {0, Size};
        }
    }

    ShaderData(const ShaderData &other) = delete;
    ShaderData &operator=(const ShaderData &other) = delete;
    ~ShaderData() = default;

    inline const auto &GetBuffers() const { return m_buffers; }

    inline StructType &Get(size_t index) { return m_objects[index]; }
    inline const StructType &Get(size_t index) const { return m_objects[index]; }
    
    void Create(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            m_buffers[i]->Create(device, m_objects.ByteSize());
        }
    }

    void Destroy(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            m_buffers[i]->Destroy(device);
        }
    }

    void UpdateBuffer(Device *device, size_t buffer_index)
    {
        this->PerformUpdate(device, m_buffers, buffer_index, m_objects.Data());
    }

    void Set(size_t index, StructType &&value)
    {
        AssertThrowMsg(index < m_objects.Size(), "Cannot set shader data out of bounds");

        m_objects[index] = std::move(value);

        this->MarkDirty(index);
    }
    
private:
    std::vector<std::unique_ptr<Buffer>> m_buffers;
    HeapArray<StructType, Size> m_objects;
};

struct ShaderGlobals {
    /* max number of materials, based on size in mb */
    static constexpr size_t max_materials = (1ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
    static constexpr size_t max_materials_bytes = max_materials * sizeof(MaterialShaderData);
    /* max number of objects, based on size in mb */
    static constexpr size_t max_objects = (8ull * 1024ull * 1024ull) / sizeof(ObjectShaderData);
    static constexpr size_t max_objects_bytes = max_materials * sizeof(ObjectShaderData);
    /* max number of scenes (cameras, essentially), based on size in kb */
    static constexpr size_t max_scenes = (16ull * 1024ull) / sizeof(SceneShaderData);
    static constexpr size_t max_scenes_bytes = max_scenes * sizeof(SceneShaderData);

    ShaderGlobals(size_t num_buffers)
        : scenes(num_buffers),
          objects(num_buffers),
          materials(num_buffers)
    {
    }

    ShaderData<UniformBuffer, SceneShaderData, max_scenes>        scenes;
    ShaderData<StorageBuffer, ObjectShaderData, max_objects>      objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials>  materials;
    BindlessStorage                                               textures;
};

struct SubShader {
    ShaderModule::Type type;
    ShaderObject       spirv;
};

class Shader : public EngineComponent<renderer::ShaderProgram> {
public:
    explicit Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::vector<SubShader> m_sub_shaders;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

