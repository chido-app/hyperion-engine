#include "SSAO.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SSAOEffect::SSAOEffect()
    : PostProcessingEffect(
          stage,
          index,
          Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8
      )
{
}

SSAOEffect::~SSAOEffect() = default;

Handle<Shader> SSAOEffect::CreateShader(Engine *engine)
{
    return Handle<Shader>(new Shader(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/PostEffect.vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/SSAO.frag.spv")).ReadBytes()
            }}
        }
    ));
}

} // namespace hyperion::v2