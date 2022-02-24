#include "particle_shader.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "../util/shader_preprocessor.h"

namespace hyperion {
ParticleShader::ParticleShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/particle.vert");
    const std::string fs_path("shaders/particle.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );
}

void ParticleShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);
}

void ParticleShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
}
} // namespace hyperion
