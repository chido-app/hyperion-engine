#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;
layout(location=2) in vec3 v_light_direction;
layout(location=3) in vec3 v_camera_position;

layout(location=0) out vec4 color_output;

layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo_ping;
layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals_ping;
layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions_ping;

layout(set = 2, binding = 0) uniform sampler2D filter_0;

void main() {
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(gbuffer_albedo_ping, texcoord);
    normal = texture(gbuffer_normals_ping, texcoord);
    position = texture(gbuffer_positions_ping, texcoord);
    
    float NdotL = dot(normal.xyz, v_light_direction);
    
    vec4 t0 = texture(filter_0, texcoord);
    vec4 t1 = texture(filter_0, texcoord + vec2(0.001, 0.001));
    vec4 t2 = texture(filter_0, texcoord + vec2(0.001, -0.01));
    vec4 t3 = texture(filter_0, texcoord + vec2(-0.001, -0.001));
    vec4 t4 = texture(filter_0, texcoord + vec2(-0.001, 0.001));

    color_output = (t0 + t1 + t2 + t3 + t4) / 5.0;//vec4(vec3(max(NdotL, 0.025)) * albedo.rgb, 1.0);
}