#include "TLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

Tlas::Tlas()
    : EngineComponentWrapper()
{
}

Tlas::~Tlas()
{
    Teardown();
}

void Tlas::AddBlas(Handle<Blas> &&blas)
{
    if (blas == nullptr) {
        return;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(blas);
    }

    m_blas.push_back(std::move(blas));
}

void Tlas::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    std::vector<BottomLevelAccelerationStructure *> blas(m_blas.size());

    for (size_t i = 0; i < m_blas.size(); i++) {
        AssertThrow(m_blas[i] != nullptr);

        engine->InitObject(m_blas[i]);
        blas[i] = &m_blas[i]->Get();
    }

    EngineComponentWrapper::Create(
        engine,
        engine->GetInstance(),
        std::move(blas)
    );

    OnTeardown([this]() {
        m_blas.clear();

        EngineComponentWrapper::Destroy(GetEngine());
    });
}

} // namespace hyperion::v2