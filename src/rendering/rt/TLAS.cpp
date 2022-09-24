#include "TLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

TLAS::TLAS()
    : EngineComponentWrapper()
{
}

TLAS::~TLAS()
{
    Teardown();
}

void TLAS::AddBLAS(Handle<BLAS> &&blas)
{
    if (!blas) {
        return;
    }

    if (IsInitCalled()) {
        if (!GetEngine()->InitObject(blas)) {
            // the blas could not be initialzied. not valid?
            return;
        }
    }

    std::lock_guard guard(m_blas_updates_mutex);

    m_blas_pending_addition.PushBack(std::move(blas));
    m_has_blas_updates.store(true);
}

void TLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    // add all pending to be added to the list
    if (m_has_blas_updates.load()) {
        std::lock_guard guard(m_blas_updates_mutex);

        m_blas.Concat(std::move(m_blas_pending_addition));

        m_has_blas_updates.store(false);
    }

    std::vector<BottomLevelAccelerationStructure *> blas(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        AssertThrow(m_blas[i] != nullptr);
        AssertThrow(engine->InitObject(m_blas[i]));

        blas[i] = &m_blas[i]->Get();
    }

    EngineComponentWrapper::Create(
        engine,
        engine->GetInstance(),
        std::move(blas)
    );

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        m_blas.Clear();

        if (m_has_blas_updates.load()) {
            m_blas_updates_mutex.lock();
            m_blas_pending_addition.Clear();
            m_blas_updates_mutex.unlock();
            m_has_blas_updates.store(false);
        }

        EngineComponentWrapper::Destroy(GetEngine());
    });
}

void TLAS::PerformBLASUpdates()
{
    std::lock_guard guard(m_blas_updates_mutex);

    while (m_blas_pending_addition.Any()) {
        auto blas = m_blas_pending_addition.PopFront();
        AssertThrow(blas);

        m_wrapped.AddBLAS(&blas->Get());

        m_blas.PushBack(std::move(blas));
    }

    m_has_blas_updates.store(false);
}

void TLAS::UpdateRender(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_has_blas_updates.load()) {
        PerformBLASUpdates();
    }
    
    for (auto &blas : m_blas) {
        AssertThrow(blas != nullptr);

        blas->UpdateRender(engine, frame);
    }

    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}

} // namespace hyperion::v2