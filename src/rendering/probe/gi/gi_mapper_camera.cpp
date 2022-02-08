#include "gi_mapper_camera.h"

#include "gi_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/gi/gi_voxel_shader.h"
#include "../../shaders/gi/gi_voxel_clear_shader.h"
#include "../../shaders/compute/blur_compute_shader.h"
#include "../../renderer.h"
#include "../../texture_3D.h"
#include "../../camera/ortho_camera.h"
#include "../../../math/math_util.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
GIMapperCamera::GIMapperCamera(const ProbeRegion &region)
    : Renderable(fbom::FBOMObjectType("GI_MAPPER_CAMERA"), RB_BUFFER),
      m_texture_id(0),
      m_region(region),
      m_camera(new OrthoCamera(-GIManager::voxel_map_size, GIManager::voxel_map_size, -GIManager::voxel_map_size, GIManager::voxel_map_size, 0, GIManager::voxel_map_size))//new PerspectiveCamera(90.0f, GIManager::voxel_map_size, GIManager::voxel_map_size, 0.01f, GIManager::voxel_map_size / GIManager::voxel_map_scale))
{
    m_texture.reset(new Texture3D(GIManager::voxel_map_size, GIManager::voxel_map_size, GIManager::voxel_map_size, nullptr));
    m_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);
    m_texture->SetFilter(CoreEngine::GLEnums::LINEAR, CoreEngine::GLEnums::LINEAR_MIPMAP_NEAREST);
    m_texture->SetFormat(CoreEngine::GLEnums::RGBA);
    m_texture->SetInternalFormat(CoreEngine::GLEnums::RGBA32F);

    m_clear_shader = ShaderManager::GetInstance()->GetShader<GIVoxelClearShader>(ShaderProperties());
    m_mipmap_shader = ShaderManager::GetInstance()->GetShader<BlurComputeShader>(
        ShaderProperties()
            .Define(std::string("DIRECTION_") + std::to_string(region.index), true)
    );
}

GIMapperCamera::~GIMapperCamera()
{
    delete m_camera;

    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
    }
}

void GIMapperCamera::Begin()
{
    if (!m_texture->IsUploaded()) {
        m_texture->Begin(false); // do not upload texture data
        // this ought to be refactored into a more reusable format
        glTexStorage3D(GL_TEXTURE_3D, GIManager::voxel_map_num_mipmaps, GL_RGBA32F, m_texture->GetWidth(), m_texture->GetHeight(), m_texture->GetLength());
        m_texture->End();
    }

    glBindImageTexture(0, m_texture->GetId(), 0, true, 0, GL_WRITE_ONLY, GL_RGBA32F);
    CatchGLErrors("Failed to bind image texture.");

}

void GIMapperCamera::End()
{
    glBindImageTexture(0, 0, 0, true, 0, GL_WRITE_ONLY, GL_RGBA32F);
}

void GIMapperCamera::Update(double dt)
{
    m_camera->SetTranslation(m_region.bounds.GetCenter());
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void GIMapperCamera::Render(Renderer *renderer, Camera *)
{
    Begin();

    // clear texture of previous frame's voxels
    m_clear_shader->Use();
    m_clear_shader->Dispatch(GIManager::voxel_map_size, GIManager::voxel_map_size, GIManager::voxel_map_size);
    m_clear_shader->End();

    renderer->RenderBucket(
        m_camera,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_shader.get(),
        false
    );

    End();

    m_mipmap_shader->SetUniform("srcTex", m_texture.get());

    for (int i = 1; i < GIManager::voxel_map_num_mipmaps; i++) {
        int mip_size = GIManager::voxel_map_size >> i;
        m_mipmap_shader->SetUniform("srcMipLevel", i - 1);
        glBindImageTexture(0, m_texture->GetId(), i, true, 0, GL_WRITE_ONLY, GL_RGBA32F);
        m_mipmap_shader->Use();
        m_mipmap_shader->Dispatch(mip_size, mip_size, mip_size);
        m_mipmap_shader->End();
    }

}

std::shared_ptr<Renderable> GIMapperCamera::CloneImpl()
{
    return std::make_shared<GIMapperCamera>(m_region);
}
} // namespace hyperion