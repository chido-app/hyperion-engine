#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/SubShaderMarshal.hpp>
#include <rendering/Shader.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Shader> : public FBOMObjectMarshalerBase<Shader>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("ShaderInstance");
    }

    virtual FBOMResult Serialize(const Shader &in_object, FBOMObject &out) const override
    {
        const auto name_string = in_object.GetCompiledShader().name.LookupString();

        out.SetProperty("name", FBOMString(), name_string.Size(), name_string.Data());

        auto properties_array = in_object.GetCompiledShader().properties.ToArray();

        out.SetProperty("properties.size", FBOMUnsignedInt(), UInt(properties_array.Size()));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".name",
                FBOMString(),
                item.name.Size(),
                item.name.Data()
            );
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        ANSIString name_string;

        if (auto err = in.GetProperty("name").ReadString(name_string)) {
            return err;
        }

        const Name name = CreateNameFromDynamicString(name_string);

        ShaderProps properties;

        UInt num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (UInt i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            String property_name;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property_name)) {
                continue;
            }

            properties.Set(property_name);
        }

        Handle<Shader> shader = Engine::Get()->GetShaderManager().GetOrCreate(name, properties);

        if (!shader.IsValid()) {
            DebugLog(
                LogType::Error,
                "Failed to deserialize Shader instance: The referenced compiled shader is not valid.\n"
                "\tName: %s\n"
                "\tProperties: %s\n",
                name_string.Data(),
                properties.ToString().Data()
            );

            return { FBOMResult::FBOM_ERR, "Invalid compiled shader" };
        }

        out_object = UniquePtr<Handle<Shader>>::Construct(std::move(shader));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif