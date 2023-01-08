#include "FXAA.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

FXAAEffect::FXAAEffect()
    : PostProcessingEffect(stage, index)
{
}

FXAAEffect::~FXAAEffect() = default;

Handle<Shader> FXAAEffect::CreateShader()
{
    return Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(FXAA));
}

} // namespace hyperion::v2