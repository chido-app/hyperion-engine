#include "TemporalBlending.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::StorageImageDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::SamplerDescriptor;
using renderer::DescriptorKey;
using renderer::ImageRect;
using renderer::ShaderVec2;

struct RENDER_COMMAND(CreateTemporalBlendingImageOutputs) : RenderCommand
{
    TemporalBlending::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateTemporalBlendingImageOutputs)(TemporalBlending::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(Engine::Get()->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateTemporalBlendingDescriptors) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateTemporalBlendingDescriptors)(const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef &descriptor_set = descriptor_sets[frame_index];
            AssertThrow(descriptor_set.IsValid());
                
            HYPERION_ASSERT_RESULT(descriptor_set->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};


TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : TemporalBlending(
        extent,
        InternalFormat::RGBA8,
        technique,
        feedback,
        input_image_views
    )
{
}

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const Handle<Framebuffer> &input_framebuffer
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_framebuffer(input_framebuffer)
{
}

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_image_views(input_image_views)
{
}

TemporalBlending::~TemporalBlending() = default;

void TemporalBlending::Create()
{
    InitObject(m_input_framebuffer);

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();
}

void TemporalBlending::Destroy()
{
    m_perform_blending.Reset();

    for (auto &descriptor_set : m_descriptor_sets) {
        SafeRelease(std::move(descriptor_set));
    }

    for (auto &image_output : m_image_outputs) {
        SafeRelease(std::move(image_output.image));
        SafeRelease(std::move(image_output.image_view));
    }
}

void TemporalBlending::CreateImageOutputs()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto &image_output = m_image_outputs[frame_index];

        image_output.image = RenderObjects::Make<Image>(StorageImage(
            Extent3D(m_extent),
            InternalFormat::RGBA8,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            nullptr
        ));

        image_output.image_view = RenderObjects::Make<ImageView>();
    }

    PUSH_RENDER_COMMAND(CreateTemporalBlendingImageOutputs, m_image_outputs.Data());
}

void TemporalBlending::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = RenderObjects::Make<DescriptorSet>();

        if (m_input_framebuffer) {
            AssertThrowMsg(m_input_framebuffer->GetAttachmentUsages().size() != 0, "No attachment refs on input framebuffer!");
        }

        const ImageView *input_image_view = m_input_framebuffer ? m_input_framebuffer->GetAttachmentUsages()[0]->GetImageView() : m_input_image_views[frame_index];
        AssertThrow(input_image_view != nullptr);

        // input image (first pass just radiance image, second pass is prev image)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetElementSRV(0, input_image_view);

        // previous image (used for temporal blending)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetElementSRV(0, m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view);

        // velocity (used for temporal blending)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(2)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(3)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(4)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        // blurred output
        descriptor_set
            ->AddDescriptor<StorageImageDescriptor>(5)
            ->SetElementUAV(0, m_image_outputs[frame_index].image_view);

        // scene buffer - so we can reconstruct world positions
        descriptor_set
            ->AddDescriptor<DynamicStorageBufferDescriptor>(6)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->shader_globals->scenes.GetBuffers()[frame_index].get());

        // depth texture
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(7)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    PUSH_RENDER_COMMAND(CreateTemporalBlendingDescriptors, m_descriptor_sets);
}

void TemporalBlending::CreateComputePipelines()
{
    ShaderProps shader_props;

    switch (m_image_format) {
    case InternalFormat::RGBA8:
        shader_props.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_props.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_props.Set("OUTPUT_RGBA32F");
        break;
    }

    static const String feedback_strings[] = { "LOW", "MEDIUM", "HIGH" };

    shader_props.Set("TEMPORAL_BLEND_TECHNIQUE_" + String::ToString(UInt(m_technique)));
    shader_props.Set("FEEDBACK_" + feedback_strings[MathUtil::Min(UInt(m_feedback), std::size(feedback_strings) - 1)]);

    m_perform_blending = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(TemporalBlending), shader_props),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_perform_blending);
}

void TemporalBlending::Render(Frame *frame)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();
    
    m_image_outputs[frame->GetFrameIndex()].image->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image->GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> output_dimensions;
        ShaderVec2<UInt32> depth_texture_dimensions;
    } push_constants;

    push_constants.output_dimensions = Extent2D(extent);
    push_constants.depth_texture_dimensions = Extent2D(
        Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );

    m_perform_blending->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_perform_blending->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_perform_blending->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) }
    );

    m_perform_blending->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (extent.width + 7) / 8,
            (extent.height + 7) / 8,
            1
        }
    );

    // set it to be able to be used as texture2D for next pass, or outside of this
    m_image_outputs[frame->GetFrameIndex()].image->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2
