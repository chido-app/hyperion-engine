#include <rendering/DrawProxy.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WaitForRenderUpdatesToComplete(Engine *engine)
{
    HYP_FLUSH_RENDER_QUEUE(engine);
}

} // namespace hyperion::v2