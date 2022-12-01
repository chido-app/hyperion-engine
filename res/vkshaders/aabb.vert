#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

#include "include/scene.inc"

#ifdef IMMEDIATE_MODE
    layout(std140, set = 0, binding = 0, row_major) readonly buffer ImmediateModeDraw
    {
        mat4 model_matrix;
        uint color_packed;
    };

    #define MODEL_MATRIX (model_matrix)
#else
    #define HYP_INSTANCING
    #include "include/object.inc"

    #define MODEL_MATRIX (object.model_matrix)
#endif

void main() {
    vec4 position = MODEL_MATRIX * vec4(a_position, 1.0);
    mat4 normal_matrix = transpose(inverse(MODEL_MATRIX));

    v_position = a_position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

    gl_Position = scene.projection * scene.view * position;
} 