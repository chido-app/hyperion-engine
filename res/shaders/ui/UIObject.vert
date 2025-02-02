#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) out vec3 v_position;
layout (location = 1) out vec3 v_screen_space_position;
layout (location = 2) out vec2 v_texcoord0;
layout (location = 3) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;

#include "../include/scene.inc"

#define HYP_INSTANCING
#include "../include/object.inc"

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    vec4 ndc_position = camera.projection * camera.view * position;

    v_position = position.xyz;
    v_screen_space_position = vec3(ndc_position.xy * 0.5 + 0.5, ndc_position.z);
    v_texcoord0 = a_texcoord0;

    v_object_index = OBJECT_INDEX;

    gl_Position = ndc_position;

} 