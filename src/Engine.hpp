#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include <asset/Assets.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderList.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/render_components/Shadows.hpp>
#include <rendering/Resources.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DefaultFormats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>
#include <scene/World.hpp>

#include "GameThread.hpp"
#include "Threads.hpp"
#include "TaskSystem.hpp"

#include <core/ecs/ComponentRegistry.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <TaskSystem.hpp>

#include <util/EnumOptions.hpp>
#include <builders/shader_compiler/ShaderCompiler.hpp>

#include <Types.hpp>

#include <memory>
#include <mutex>
#include <stack>

#define HYP_FLUSH_RENDER_QUEUE(engine) \
    do { \
        (engine)->render_scheduler.FlushOrWait([](auto &fn) { \
            HYPERION_ASSERT_RESULT(fn(nullptr, 0)); \
        }); \
    } while (0)

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;

class Engine;

/* Current descriptor / attachment layout */

/*
 * | ====== Set 0 ====== | ====== Set 1 ====== | ====== Set 2 ====== | ====== Set 3 ====== | ====== Set 4 ====== |
 * | (UNUSED)            | GBuffer: textures[] | Scene data SSBO     | Material data SSBO  | Bindless textures   |
 * |                     | Gbuffer: depth      | empty               | Object data SSBO    | empty               |
 * |                     |                     | empty               | Skeleton data SSBO  | empty               |
 * |                     |                     | empty               | empty               | empty               |
 * |                     | Deferred result     | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Post effect 0       | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Shadow map          | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Image storage test  | empty               | empty               | empty               |
 */

struct DebugMarker {
    CommandBuffer     *command_buffer = nullptr;
    const char * const name           = "<Unnamed debug marker>";
    bool               is_ended       = false;

    DebugMarker(CommandBuffer *command_buffer, const char *marker_name)
        : command_buffer(command_buffer),
          name(marker_name)
    {
        if (command_buffer != nullptr) {
            command_buffer->DebugMarkerBegin(name);
        }
    }

    DebugMarker(const DebugMarker &other) = delete;
    DebugMarker &operator=(const DebugMarker &other) = delete;

    DebugMarker(DebugMarker &&other) noexcept = delete;
    DebugMarker &operator=(DebugMarker &&other) noexcept = delete;

    ~DebugMarker()
    {
        MarkEnd();
    }

    void MarkEnd()
    {
        if (is_ended) {
            return;
        }

        if (command_buffer != nullptr) {
            command_buffer->DebugMarkerEnd();
        }

        is_ended = true;
    }
};

class IndirectDrawState;



// struct RenderFunctor : public ScheduledFunction<renderer::Result, CommandBuffer * /* command_buffer */, UInt /* frame_index */>
// {
//     static constexpr UInt data_buffer_size = 256;

//     UByte data_buffer[data_buffer_size];

//     RenderFunctor() = default;

//     template <class Lambda>
//     RenderFunctor(Lambda &&lambda)
//         : ScheduledFunction(std::forward<Lambda>(lambda))
//     {
//     }

//     template <class DataStruct, class Lambda>
//     RenderFunctor(DataStruct &&data_struct, Lambda &&lambda)
//         : ScheduledFunction(std::forward<Lambda>(lambda))
//     {
//         static_assert(sizeof(data_struct) <= data_buffer_size, "DataStruct does not fit into buffer!");
//         static_assert(std::is_pod_v<DataStruct>, "DataStruct must be a POD object!");

//         Memory::Copy(&data_buffer[0], &data_struct, sizeof(data_struct));
//     }
// };

using RenderFunctor = ScheduledFunction<renderer::Result, CommandBuffer * /* command_buffer */, UInt /* frame_index */>;

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    Engine(SystemSDL &, const char *app_name);
    ~Engine();
    
    Instance *GetInstance() const                                  { return m_instance.get(); }
    Device   *GetDevice() const                                    { return m_instance ? m_instance->GetDevice() : nullptr; }

    DeferredRenderer &GetDeferredRenderer()                        { return m_deferred_renderer; }
    const DeferredRenderer &GetDeferredRenderer() const            { return m_deferred_renderer; }

    RenderListContainer &GetRenderListContainer()                  { return m_render_list_container; }
    const RenderListContainer &GetRenderListContainer() const      { return m_render_list_container; }

    auto &GetRenderScheduler()                                     { return render_scheduler; }
    const auto &GetRenderScheduler() const                         { return render_scheduler; }

    auto &GetShaderData()                                          { return shader_globals; }
    const auto &GetShaderData() const                              { return shader_globals; }
    
    PlaceholderData &GetPlaceholderData()                          { return m_placeholder_data; }
    const PlaceholderData &GetPlaceholderData() const              { return m_placeholder_data; }
    
    ComponentRegistry<Entity> &GetComponentRegistry()              { return m_component_registry; }
    const ComponentRegistry<Entity> &GetComponentRegistry() const  { return m_component_registry; }
    
    World &GetWorld()                                              { return m_world; }
    const World &GetWorld() const                                  { return m_world; }

    Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Ref<RendererInstance> FindOrCreateRendererInstance(const RenderableAttributeSet &renderable_attributes);
    Ref<RendererInstance> AddRendererInstance(std::unique_ptr<RendererInstance> &&pipeline);

    template <class T>
    void SafeReleaseRenderResource(Ref<T> &&resource)
    {
        m_safe_deleter.SafeReleaseRenderResource(std::move(resource));
    }

    void Initialize();
    void Compile();

#if 0
    template <class Object>
    Ref<Object> MakeHandle(Object *object)
    {
        if constexpr (std::is_same_v<Object, Shader>) {
            Ref<Object> handle = resources.shaders.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Texture>) {
            Ref<Object> handle = resources.textures.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Material>) {
            Ref<Object> handle = resources.materials.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Light>) {
            Ref<Object> handle = resources.lights.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Entity>) {
            Ref<Object> handle = resources.entities.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Mesh>) {
            Ref<Object> handle = resources.meshes.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Skeleton>) {
            Ref<Object> handle = resources.skeletons.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Scene>) {
            Ref<Object> handle = resources.scenes.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, RenderPass>) {
            Ref<Object> handle = resources.render_passes.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Framebuffer>) {
            Ref<Object> handle = resources.framebuffers.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, RendererInstance>) {
            Ref<Object> handle = resources.renderer_instances.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, ComputePipeline>) {
            Ref<Object> handle = resources.compute_pipelines.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Blas>) {
            Ref<Object> handle = resources.blas.Add(object);
            handle->Init(this);
            return handle;
        } else if constexpr(std::is_same_v<Object, Camera>) {
            Ref<Object> handle = resources.cameras.Add(object);
            handle->Init(this);
            return handle;
        } else {
            static_assert(resolution_failure<Object>, "No handler defined for the given type");
        }
    }
#endif

    template <class Object>
    void Activate(Ref<Object> &object)
    {
        object->Init(this);
    }

    void PreFrameUpdate(Frame *frame);
    
    void RenderDeferred(Frame *frame);
    void RenderFinalPass(Frame *frame) const;

    ShaderGlobals           *shader_globals;

    EngineCallbacks          callbacks;
    Resources                resources;
    Assets                   assets;
    ShaderManager            shader_manager;
                             
    RenderState              render_state;
    
    std::atomic_bool         m_running{false};

    Scheduler<RenderFunctor> render_scheduler;

    GameThread game_thread;
    //TaskThread terrain_thread;
    TaskSystem task_system;

private:
    void ResetRenderState();
    void UpdateBuffersAndDescriptors(UInt frame_index);

    void PrepareFinalPass();

    void FindTextureFormatDefaults();
    
    std::unique_ptr<Instance>         m_instance;
    std::unique_ptr<RendererInstance> m_root_pipeline;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 16> m_texture_format_defaults;

    DeferredRenderer m_deferred_renderer;
    RenderListContainer m_render_list_container;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;

    FlatMap<RenderableAttributeSet, RendererInstance::ID> m_renderer_instance_mapping;

    ComponentRegistry<Entity> m_component_registry;

    PlaceholderData m_placeholder_data;

    SafeDeleter m_safe_deleter;

    World m_world;

    Ref<Mesh> m_full_screen_quad;
};

} // namespace hyperion::v2

#endif

