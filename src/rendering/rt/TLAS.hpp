#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <rendering/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include "BLAS.hpp"

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;

class TLAS : public EngineComponentWrapper<STUB_CLASS(TLAS), TopLevelAccelerationStructure>
{
public:
    TLAS();
    TLAS(const TLAS &other) = delete;
    TLAS &operator=(const TLAS &other) = delete;
    ~TLAS();

    void AddBLAS(Handle<BLAS> &&blas);

    void Init(Engine *engine);

    /*! \brief Update the TLAS on the RENDER thread, performing any
     * updates to the structure. This is also called for each BLAS underneath this.
     */
    void UpdateRender(Engine *engine, Frame *frame);

private:
    void PerformBLASUpdates();

    DynArray<Handle<BLAS>> m_blas;
    DynArray<Handle<BLAS>> m_blas_pending_addition;

    std::atomic_bool m_has_blas_updates { false };
    std::mutex m_blas_updates_mutex;
};

} // namespace hyperion::v2

#endif