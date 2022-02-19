#include "deferred_rendering.h"
#include "../../environment.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
DeferredRenderingShader::DeferredRenderingShader(const ShaderProperties &properties)
    : PostShader(properties)
{
    const std::string fs_path("shaders/filters/deferred.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]",
            Environment::possion_disk[i]);
    }
}

void DeferredRenderingShader::ApplyMaterial(const Material &mat)
{
    PostShader::ApplyMaterial(mat);

    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            const std::string i_str = std::to_string(i);

            if (auto shadow_map = env->GetShadowMap(i)) {
                shadow_map->Prepare();

                SetUniform("u_shadowMap[" + i_str + "]", shadow_map.get());
            }

            SetUniform("u_shadowMatrix[" + i_str + "]", env->GetShadowMatrix(i));
            SetUniform("u_shadowSplit[" + i_str + "]", (float)env->GetShadowSplit(i));
        }
    }

    env->BindLights(this);


    for (int i = 0; i < env->GetProbeManager()->NumProbes(); i++) {
        env->GetProbeManager()->GetProbe(i)->Bind(this);
    }


    if (!env->GetProbeManager()->EnvMapEnabled()) {
        if (auto cubemap = env->GetGlobalCubemap()) {
            cubemap->Prepare();

            SetUniform("env_GlobalCubemap", cubemap.get());
        }
    }
}

void DeferredRenderingShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    Matrix4 inverse_proj_view(camera->GetViewMatrix());
    inverse_proj_view *= camera->GetProjectionMatrix();
    inverse_proj_view.Invert();

    // TODO: move into general shader calculations
    SetUniform("InverseViewProjMatrix", inverse_proj_view);
}
} // namespace hyperion
