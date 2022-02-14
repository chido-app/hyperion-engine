#include "material.h"

namespace hyperion {

const Material::MaterialParameterTable_t Material::default_parameters({
    std::make_pair(MATERIAL_PARAMETER_ROUGHNESS, 0.3f),
    std::make_pair(MATERIAL_PARAMETER_METALNESS, 0.7f),
    std::make_pair(MATERIAL_PARAMETER_EMISSIVENESS, 0.0f)
});

MaterialParameter::MaterialParameter()
    : type(MaterialParameter_None),
      values({ 0.0f })
{
}

MaterialParameter::MaterialParameter(const float value)
    : type(MaterialParameter_Float),
      values({ value, 0.0f, 0.0f, 0.0f })
{
}

MaterialParameter::MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype)
    : type(paramtype)
{
    for (size_t i = 0; i < values.size(); i++) {
        if (i < nvalues) {
            values[i] = data[i];
        } else {
            values[i] = 0.0f;
        }
    }
}

MaterialParameter::MaterialParameter(const MaterialParameter &other)
    : type(other.type),
      values(other.values)
{
}

Material::Material()
    : fbom::FBOMLoadable(fbom::FBOMObjectType("MATERIAL")),
      diffuse_color(Vector4(1.0))
{
    for (size_t i = 0; i < default_parameters.Size(); i++) {
        auto pair = default_parameters.KeyValueAt(i);

        if (default_parameters.Has(pair.first)) {
            m_params.Set(pair.first, pair.second);
        }
    }
}

Material::Material(const Material &other)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("MATERIAL")),
      m_params(other.m_params),
      alpha_blended(other.alpha_blended),
      depth_test(other.depth_test),
      depth_write(other.depth_write),
      diffuse_color(other.diffuse_color),
      textures(other.textures)
{
}

void Material::SetParameter(MaterialParameterKey key, float value)
{
    float values[] = { value };
    m_params.Set(key, MaterialParameter(values, 1, MaterialParameter_Float));
}

void Material::SetParameter(MaterialParameterKey key, int value)
{
    float values[] = { (float)value };
    m_params.Set(key, MaterialParameter(values, 1, MaterialParameter_Int));
}

void Material::SetParameter(MaterialParameterKey key, const Vector2 &value)
{
    float values[] = { value.x, value.y };
    m_params.Set(key, MaterialParameter(values, 2, MaterialParameter_Vector2));
}

void Material::SetParameter(MaterialParameterKey key, const Vector3 &value)
{
    float values[] = { value.x, value.y, value.z };
    m_params.Set(key, MaterialParameter(values, 3, MaterialParameter_Vector3));
}

void Material::SetParameter(MaterialParameterKey key, const Vector4 &value)
{
    float values[] = { value.x, value.y, value.z, value.w };
    m_params.Set(key, MaterialParameter(values, 4, MaterialParameter_Vector4));
}

void Material::SetTexture(const std::string &name, const std::shared_ptr<Texture> &texture)
{
    textures[name] = texture;
}

std::shared_ptr<Texture> Material::GetTexture(const std::string &name) const
{
    const auto it = textures.find(name);

    if (it != textures.end()) {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<Loadable> Material::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Material> Material::CloneImpl()
{
    return std::make_shared<Material>(*this);
}

} // namespace hyperion
