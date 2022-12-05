#include "Framebuffer.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

struct RENDER_COMMAND(CreateRenderPass) : RenderCommand
{
    renderer::RenderPass *render_pass;

    RENDER_COMMAND(CreateRenderPass)(renderer::RenderPass *render_pass)
        : render_pass(render_pass)
    {
    }

    virtual Result operator()()
    {
        return render_pass->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(DestroyRenderPass) : RenderCommand
{
    renderer::RenderPass *render_pass;

    RENDER_COMMAND(DestroyRenderPass)(renderer::RenderPass *render_pass)
        : render_pass(render_pass)
    {
    }

    virtual Result operator()()
    {
        return render_pass->Destroy(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateFramebuffer) : RenderCommand
{
    renderer::FramebufferObject *framebuffer;
    renderer::RenderPass *render_pass;

    RENDER_COMMAND(CreateFramebuffer)(renderer::FramebufferObject *framebuffer, renderer::RenderPass *render_pass)
        : framebuffer(framebuffer),
          render_pass(render_pass)
    {
    }

    virtual Result operator()()
    {
        return framebuffer->Create(Engine::Get()->GetGPUDevice(), render_pass);
    }
};

struct RENDER_COMMAND(DestroyFramebuffer) : RenderCommand
{
    renderer::FramebufferObject *framebuffer;

    RENDER_COMMAND(DestroyFramebuffer)(renderer::FramebufferObject *framebuffer)
        : framebuffer(framebuffer)
    {
    }

    virtual Result operator()()
    {
        return framebuffer->Destroy(Engine::Get()->GetGPUDevice());
    }
};


Framebuffer2::Framebuffer2(
    Extent2D extent,
    renderer::RenderPassStage stage,
    renderer::RenderPass::Mode render_pass_mode,
    UInt num_multiview_layers
) : Framebuffer2(Extent3D(extent), stage, render_pass_mode)
{
}

Framebuffer2::Framebuffer2(
    Extent3D extent,
    renderer::RenderPassStage stage,
    renderer::RenderPass::Mode render_pass_mode,
    UInt num_multiview_layers
) : EngineComponentBase(),
    m_framebuffers { renderer::FramebufferObject(extent), renderer::FramebufferObject(extent) },
    m_render_pass(stage, render_pass_mode, num_multiview_layers)
{
}

Framebuffer2::~Framebuffer2()
{
    Teardown();
}

void Framebuffer2::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    RenderCommands::Push<RENDER_COMMAND(CreateRenderPass)>(&m_render_pass);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        RenderCommands::Push<RENDER_COMMAND(CreateFramebuffer)>(&m_framebuffers[frame_index], &m_render_pass);
    }

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            RenderCommands::Push<RENDER_COMMAND(DestroyFramebuffer)>(&m_framebuffers[frame_index]);
        }

        RenderCommands::Push<RENDER_COMMAND(DestroyRenderPass)>(&m_render_pass);

        HYP_SYNC_RENDER();
    });
}

void Framebuffer2::AddAttachmentRef(AttachmentRef *attachment_ref)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index].AddAttachmentRef(attachment_ref);
    }

    m_render_pass.AddAttachmentRef(attachment_ref);
}

void Framebuffer2::RemoveAttachmentRef(const Attachment *attachment)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index].RemoveAttachmentRef(attachment);
    }

    m_render_pass.RemoveAttachmentRef(attachment);
}

void Framebuffer2::BeginCapture(UInt frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass.Begin(command_buffer, &m_framebuffers[frame_index]);
}

void Framebuffer2::EndCapture(UInt frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass.End(command_buffer);
}

} // namespace hyperion::v2
