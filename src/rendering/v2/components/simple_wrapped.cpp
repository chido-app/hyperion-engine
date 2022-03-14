#include "simple_wrapped.h"
#include "../engine.h"

namespace hyperion::v2 {

using renderer::Device;

class Engine;

Device *GetEngineDevice(Engine *engine)
{
    return engine->GetInstance()->GetDevice();
}

} // namespace hyperion::v2