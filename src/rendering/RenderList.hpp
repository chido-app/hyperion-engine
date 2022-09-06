#ifndef HYPERION_V2_RENDER_LIST_H
#define HYPERION_V2_RENDER_LIST_H

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/Handle.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/DefaultFormats.hpp>
#include <scene/Scene.hpp>
#include <Types.hpp>

#include <mutex>
#include <array>

namespace hyperion::v2 {

class Engine;

class RenderListContainer
{
public:
    static const FixedArray<TextureFormatDefault, num_gbuffer_textures> gbuffer_textures;
    
    class RenderListBucket
    {
        friend class RenderListContainer;

        Bucket                                bucket{BUCKET_OPAQUE};
        Handle<RenderPass>                    render_pass;
        DynArray<Handle<Framebuffer>>         framebuffers;
        DynArray<std::unique_ptr<Attachment>> attachments;
        DynArray<Handle<RendererInstance>>    renderer_instances;
        DynArray<Handle<RendererInstance>>    renderer_instances_pending_addition;
        AtomicVar<bool>                       renderer_instances_changed;
        std::mutex                            renderer_instances_mutex;

    public:
        RenderListBucket();
        ~RenderListBucket();

        Bucket GetBucket() const { return bucket; }
        void SetBucket(Bucket bucket) { this->bucket = bucket; }
        
        Handle<RenderPass> &GetRenderPass() { return render_pass; }
        const Handle<RenderPass> &GetRenderPass() const { return render_pass; }
        
        DynArray<Handle<Framebuffer>> &GetFramebuffers() { return framebuffers; }
        const DynArray<Handle<Framebuffer>> &GetFramebuffers() const { return framebuffers; }

        DynArray<Handle<RendererInstance>> &GetRendererInstances() { return renderer_instances; }
        const DynArray<Handle<RendererInstance>> &GetRendererInstances() const { return renderer_instances; }

        bool IsRenderableBucket() const
        {
            return bucket == Bucket::BUCKET_OPAQUE
                || bucket == Bucket::BUCKET_TRANSLUCENT
                || bucket == Bucket::BUCKET_SKYBOX
                || bucket == Bucket::BUCKET_PARTICLE;
        }

        void AddRendererInstance(Handle<RendererInstance> &&renderer_instance);
        void AddPendingRendererInstances(Engine *engine);
        void AddFramebuffersToPipelines();
        void AddFramebuffersToPipeline(Handle<RendererInstance> &pipeline);
        void CreateRenderPass(Engine *engine);
        void CreateFramebuffers(Engine *engine);
        void Destroy(Engine *engine);
    };

    RenderListContainer();
    RenderListContainer(const RenderListContainer &other) = delete;
    RenderListContainer &operator=(const RenderListContainer &other) = delete;
    ~RenderListContainer() = default;

    auto &GetBuckets()
        { return m_buckets; }

    const auto &GetBuckets() const
        { return m_buckets; }

    RenderListBucket &Get(Bucket bucket)
        { return m_buckets[int(bucket)]; }

    const RenderListBucket &Get(Bucket bucket) const
        { return m_buckets[int(bucket)]; }

    RenderListBucket &operator[](Bucket bucket)
        { return Get(bucket); }

    const RenderListBucket &operator[](Bucket bucket) const
        { return Get(bucket); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void AddPendingRendererInstances(Engine *engine);
    void AddFramebuffersToPipelines(Engine *engine);

private:
    FixedArray<RenderListBucket, Bucket::BUCKET_MAX> m_buckets;
};

} // namespace hyperion::v2

#endif