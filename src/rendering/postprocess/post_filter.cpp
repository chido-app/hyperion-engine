#include "./post_filter.h"

#include "../environment.h"

namespace hyperion {

PostFilter::PostFilter(const std::shared_ptr<PostShader> &shader, BitFlags_t modifies_attachments)
    : m_shader(shader),
      m_modifies_attachments(modifies_attachments)
{
}

void PostFilter::SetUniforms(Camera *cam)
{
    m_shader->SetUniform("ViewMatrix", cam->GetViewMatrix());
    m_shader->SetUniform("ProjectionMatrix", cam->GetProjectionMatrix());
    m_shader->SetUniform("Resolution", Vector2(cam->GetWidth(), cam->GetHeight()));
    m_shader->SetUniform("CamFar", cam->GetFar());
    m_shader->SetUniform("CamNear", cam->GetNear());
}

void PostFilter::Begin(Camera *cam, const Framebuffer::FramebufferAttachments_t &attachments)
{
    for (int i = 0; i < attachments.size(); i++) {
        m_material.SetTexture(
            Framebuffer::default_texture_attributes[i].material_key,
            attachments[i]
        );
    }

    SetUniforms(cam);

    m_shader->ApplyTransforms(Transform(), cam);
    m_shader->ApplyMaterial(m_material);
    m_shader->Use();
}

void PostFilter::End(Camera *cam, Framebuffer *fbo, Framebuffer::FramebufferAttachments_t &attachments, bool copy_textures)
{
    m_shader->End();

    if (!copy_textures) {
        return;
    }

    if (!m_modifies_attachments) {
        return;
    }

    if (fbo == nullptr) {
        return;
    }

    for (int i = 0; i < attachments.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

        if (!ModifiesAttachment(attachment)) {
            continue;
        }

        soft_assert(attachments[i] != nullptr);
        soft_assert(Framebuffer::default_texture_attributes[i].is_volatile);

        fbo->Store(attachment, attachments[i]);
    }
}

} // namespace hyperion
