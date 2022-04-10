#include "compute.h"
#include "../engine.h"

namespace hyperion::v2 {
ComputePipeline::ComputePipeline(Ref<Shader> &&shader)
    : EngineComponent(),
      m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline() = default;

void ComputePipeline::Create(Engine *engine)
{
    AssertThrow(m_shader != nullptr);
    m_shader->Init(engine);

    HYPERION_ASSERT_RESULT(m_wrapped.Create(
        engine->GetInstance()->GetDevice(),
        &m_shader->Get(),
        &engine->GetInstance()->GetDescriptorPool()
    ));

    m_is_created = true;
}

void ComputePipeline::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

void ComputePipeline::Dispatch(Engine *engine, CommandBuffer *command_buffer, size_t num_groups_x, size_t num_groups_y, size_t num_groups_z)
{
    m_wrapped.Bind(command_buffer->GetCommandBuffer());

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetInstance()->GetDevice(),
        command_buffer,
        &m_wrapped,
        {{.set = 1, .count = 1}}
    );

    m_wrapped.Dispatch(command_buffer->GetCommandBuffer(), num_groups_x, num_groups_y, num_groups_z);
}
} // namespace hyperion::v2