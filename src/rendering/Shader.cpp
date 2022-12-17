#include "Shader.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

void ShaderGlobals::Create()
{
    auto *device = Engine::Get()->GetGPUDevice();

    scenes.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_maps.Create(device);
    env_probes.Create(device);
    env_grids.Create(device);
    immediate_draws.Create(device);
    entity_instance_batches.Create(device);
    cubemap_uniforms.Create(device, sizeof(CubemapUniforms));

    textures.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = Engine::Get()->GetGPUDevice();

    cubemap_uniforms.Destroy(device);

    env_probes.Destroy(device);
    env_grids.Destroy(device);

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_maps.Destroy(device);
    immediate_draws.Destroy(device);
    entity_instance_batches.Destroy(device);
}

struct RENDER_COMMAND(CreateShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;
    // Array<SubShader> subshaders;
    std::vector<SubShader> subshaders;

    RENDER_COMMAND(CreateShaderProgram)(
        const ShaderProgramRef &shader_program,
        const std::vector<SubShader> &subshaders
    ) : shader_program(shader_program),
        subshaders(subshaders)
    {
    }

    virtual ~RENDER_COMMAND(CreateShaderProgram)() = default;

    virtual Result operator()()
    {
        for (const SubShader &sub_shader : subshaders) {
            HYPERION_BUBBLE_ERRORS(shader_program->AttachShader(
                Engine::Get()->GetGPUInstance()->GetDevice(),
                sub_shader.type,
                sub_shader.spirv
            ));
        }

        return shader_program->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(DestroyShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;

    RENDER_COMMAND(DestroyShaderProgram)(const ShaderProgramRef &shader_program)
        : shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return shader_program->Destroy(Engine::Get()->GetGPUDevice());
    }
};

Shader::Shader(const std::vector<SubShader> &sub_shaders)
    : EngineComponentBase(),
      m_shader_program(ShaderProgramRef::Construct()),
      m_sub_shaders(sub_shaders)
{
}

Shader::Shader(const CompiledShader &compiled_shader)
    : EngineComponentBase(),
      m_shader_program(ShaderProgramRef::Construct())
{
    if (compiled_shader.IsValid()) {
        for (SizeType index = 0; index < compiled_shader.modules.Size(); index++) {
            const auto &byte_buffer = compiled_shader.modules[index];

            if (byte_buffer.Empty()) {
                continue;
            }

            m_sub_shaders.push_back(SubShader {
                .type = ShaderModule::Type(index),
                .spirv = {
                    .bytes = byte_buffer
                }
            });
        }
    }
}

Shader::~Shader()
{
    if (IsInitCalled()) {
        SetReady(false);

        PUSH_RENDER_COMMAND(DestroyShaderProgram, m_shader_program);
        
        HYP_SYNC_RENDER();
    }
}

void Shader::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    for (const auto &sub_shader : m_sub_shaders) {
        AssertThrowMsg(sub_shader.spirv.bytes.Any(), "Shader data missing");
    }

    PUSH_RENDER_COMMAND(
        CreateShaderProgram,
        m_shader_program,
        m_sub_shaders
    );

    SetReady(true);
}

} // namespace hyperion
