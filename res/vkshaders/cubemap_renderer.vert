#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_multiview : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=7) out flat vec3 v_camera_position;
layout(location=8) out mat3 v_tbn_matrix;
layout(location=11) out flat uint v_object_index;
layout(location=12) out flat vec3 v_env_probe_extent;
layout(location=13) out flat uint v_cube_face_index;
layout(location=14) out vec2 v_cube_face_uv;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;
layout (location = 6) in vec4 a_bone_weights;
layout (location = 7) in vec4 a_bone_indices;

#include "include/scene.inc"

#define HYP_INSTANCING
#include "include/object.inc"
#include "include/env_probe.inc"

#define HYP_ENABLE_SKINNING

// struct EnvProbeMatrices
// {
//     mat4 projection_matrices[6];
//     mat4 view_matrices[6];
// };

// layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 20, row_major) readonly buffer EnvProbeMatricesBuffer
// {
//     EnvProbeMatrices env_probe_uniforms[];
// };

layout(std430, set = HYP_DESCRIPTOR_SET_SCENE, binding = 3, row_major) readonly buffer EnvProbeBuffer
{
    EnvProbe current_env_probe;
};

#ifdef HYP_ENABLE_SKINNING
#include "include/Skeleton.glsl"
#endif

void main() 
{
    vec4 position;
    mat4 normal_matrix;

    if (object.bucket == HYP_OBJECT_BUCKET_SKYBOX) {
        position = vec4((a_position * 150.0) + scene.camera_position.xyz, 1.0);
        normal_matrix = transpose(inverse(object.model_matrix));
    } else {
#ifdef HYP_ENABLE_SKINNING
        if (bool(object.flags & ENTITY_GPU_FLAG_HAS_SKELETON)) {
            mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

            position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
        } else {
#endif
            position = object.model_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix));
#ifdef HYP_ENABLE_SKINNING
        }
#endif
    }

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_camera_position = scene.camera_position.xyz;

    v_tangent = normalize(normal_matrix * vec4(a_tangent, 0.0)).xyz;
    v_bitangent = normalize(normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);

    mat4 projection_matrix = scene.projection;
    mat4 view_matrix = current_env_probe.face_view_matrices[gl_ViewIndex];

    v_object_index = OBJECT_INDEX;

    v_env_probe_extent = current_env_probe.aabb_max.xyz - current_env_probe.aabb_min.xyz;

    gl_Position = projection_matrix * view_matrix * position;

    v_cube_face_index = gl_ViewIndex;
    v_cube_face_uv = gl_Position.xy * 0.5 + 0.5;
}
