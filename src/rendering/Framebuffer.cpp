#include "Framebuffer.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

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

Framebuffer::Framebuffer(
    Extent2D extent,
    Handle<RenderPass> &&render_pass
) : Framebuffer(Extent3D(extent), std::move(render_pass))
{
}

Framebuffer::Framebuffer(
    Extent3D extent,
    Handle<RenderPass> &&render_pass
) : EngineComponentBase(),
    m_framebuffer(extent),
    m_render_pass(std::move(render_pass))
{
}

Framebuffer::~Framebuffer()
{
    Teardown();
}

void Framebuffer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    InitObject(m_render_pass);

    RenderCommands::Push<RENDER_COMMAND(CreateFramebuffer)>(&m_framebuffer, &m_render_pass->GetRenderPass());

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        RenderCommands::Push<RENDER_COMMAND(DestroyFramebuffer)>(&m_framebuffer);
        
        HYP_SYNC_RENDER();
    });
}

void Framebuffer::AddAttachmentRef(AttachmentRef *attachment_ref)
{
    m_framebuffer.AddAttachmentRef(attachment_ref);    
}

void Framebuffer::RemoveAttachmentRef(const Attachment *attachment)
{
    m_framebuffer.RemoveAttachmentRef(attachment);
}

void Framebuffer::BeginCapture(CommandBuffer *command_buffer)
{
    m_render_pass->GetRenderPass().Begin(command_buffer, &m_framebuffer);
}

void Framebuffer::EndCapture(CommandBuffer *command_buffer)
{
    m_render_pass->GetRenderPass().End(command_buffer);
}

} // namespace hyperion::v2
