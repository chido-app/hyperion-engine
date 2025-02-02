#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

#define HYP_NO_CUBEMAP

layout(set = 0, binding = 17) uniform sampler sampler_nearest;
#define HYP_SAMPLER_NEAREST sampler_nearest

layout(set = 0, binding = 18) uniform sampler sampler_linear;
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../../include/defines.inc"
#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/object.inc"
#include "../../include/scene.inc"

#include "../../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"
#include "../../include/rt/payload.inc"

#include "../../include/rt/probe/probe_uniforms.inc"

layout(std140, set = 0, binding = 9) uniform ProbeSystem {
    ProbeSystemUniforms probe_system;
};

layout(set = 0, binding = 16) uniform texture2D shadow_maps[];

layout(std140, set = 0, binding = 15, row_major) readonly buffer ShadowShaderData
{
    ShadowMap shadow_map_data[HYP_MAX_SHADOW_MAPS];
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#undef HYP_NO_CUBEMAP

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

struct PackedVertex
{
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float texcoord_s;
    float texcoord_t;
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(buffer_reference, scalar) readonly buffer PackedVertexBuffer { float vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uvec3 indices[]; };

layout(std140, set = 0, binding = 4) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

layout(std140, set = 0, binding = 5) readonly buffer MaterialBuffer
{
    Material materials[];
};

layout(std140, set = 0, binding = 6) readonly buffer EntityBuffer
{
    Object entities[];
};

layout(std140, set = 0, binding = 7) readonly buffer LightShaderData
{
    Light lights[];
};


#define HYP_GET_LIGHT(index) \
    lights[probe_system.light_indices[(index / 4)][index % 4]]

// for RT, all textures are bindless
layout(set = 2, binding = 0) uniform sampler2D textures[];

void main()
{

    MeshDescription mesh_description = mesh_descriptions[gl_InstanceCustomIndexEXT];
    PackedVertexBuffer vertex_buffer = PackedVertexBuffer(mesh_description.vertex_buffer_address);
    IndexBuffer index_buffer = IndexBuffer(mesh_description.index_buffer_address);

    uvec3 index = uvec3(
        index_buffer.indices[gl_PrimitiveID]
    );
    // payload.color = vec3(greaterThan(index, ivec3(mesh_description.num_vertices)));
    // return;
    
    Vertex v0;
    {
        const uint offset = 8 * index[0];
        v0.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v0.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v0.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }

    Vertex v1;
    {
        const uint offset = 8 * index[1];
        v1.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v1.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v1.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    Vertex v2;
    {
        const uint offset = 8 * index[2];
        v2.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v2.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v2.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    // payload.color = vec3(v0.position);
    // return;

    v0.position = (gl_ObjectToWorldEXT * vec4(v0.position, 1.0)).xyz;
    v1.position = (gl_ObjectToWorldEXT * vec4(v1.position, 1.0)).xyz;
    v2.position = (gl_ObjectToWorldEXT * vec4(v2.position, 1.0)).xyz;

    // Interpolate normal
    const vec3 barycentric_coords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 normal = normalize((gl_ObjectToWorldEXT * vec4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const vec2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const vec3 position = v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z;
    
    vec4 material_color = vec4(1.0);

    const uint32_t material_index = mesh_description.material_index;
    const Material material = materials[material_index];

    const uint32_t entity_index = mesh_description.entity_index;
    const Object entity = entities[entity_index];
    
    material_color = material.albedo;

    if (HAS_TEXTURE(material, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(material, MATERIAL_TEXTURE_ALBEDO_map, texcoord);
        
        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MATERIAL_TEXTURE_METALNESS_MAP)) {
        float metalness_sample = SAMPLE_TEXTURE(material, MATERIAL_TEXTURE_METALNESS_MAP, texcoord).r;
        
        metalness = metalness_sample;
    }

    const float ambient = 0.005;

    vec3 indirect_lighting = material_color.rgb * (1.0 - metalness) * vec3(ambient);  

    vec3 direct_lighting = vec3(0.0);

    for (uint light_index = 0; light_index < probe_system.num_bound_lights; light_index++) {
        const Light light = HYP_GET_LIGHT(light_index);

        const vec3 L = CalculateLightDirection(light, position);
        const float NdotL = max(dot(normal, L), 0.00001);
        
        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(position.xyz, light.position_intensity.xyz, light.radius)
            : 1.0;

        vec3 local_light = vec3(NdotL) * UINT_TO_VEC4(light.color_encoded).rgb;

        local_light *= light.position_intensity.w * attenuation;

        
        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index == probe_system.shadow_map_index) {
            local_light *= GetShadowStandard(probe_system.shadow_map_index, position.xyz, vec2(0.0), NdotL);
        }

        direct_lighting += local_light;
    }

    payload.color = indirect_lighting + (material_color.rgb * direct_lighting);
    payload.distance = gl_RayTminEXT + gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);
}
