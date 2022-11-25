#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/lib/CMemory.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion::renderer {

class Device;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;

Engine *GetEngine();

renderer::Device *GetEngineDevice(Engine *engine);

} // namespace hyperion::v2

#endif