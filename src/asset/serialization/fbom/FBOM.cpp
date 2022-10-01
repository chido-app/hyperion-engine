#include "FBOM.hpp"
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>
#include <asset/serialization/fbom/marshals/ShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/SubShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/MaterialMarshal.hpp>
#include <asset/serialization/fbom/marshals/TextureMarshal.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/serialization/fbom/marshals/SceneMarshal.hpp>
#include <asset/serialization/fbom/marshals/CameraMarshal.hpp>

namespace hyperion::v2::fbom {

FBOM::FBOM()
{
    // register loaders
    RegisterLoader<Entity>();
    RegisterLoader<Mesh>();
    RegisterLoader<Shader>();
    RegisterLoader<SubShader>();
    RegisterLoader<Material>();
    RegisterLoader<Texture>();
    RegisterLoader<Node>();
    RegisterLoader<Scene>();
    RegisterLoader<Camera>();
}

FBOM::~FBOM() = default;

} // namespace hyperion::v2::fbom