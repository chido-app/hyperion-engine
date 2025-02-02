#ifndef HYP_NOISE_GLSL
#define HYP_NOISE_GLSL

#include "noise/NoiseSampling.inc"

float rand(vec2 n)
{ 
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float SimpleNoise(vec2 n)
{
    const vec2 d = vec2(0.0, 1.0);
    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

vec2 VogelDisk(int sample_index, int num_samples, float phi)
{
    const float golden_angle = 2.4;

    float r = sqrt(float(sample_index) + 0.5) / sqrt(float(num_samples));
    float theta = sample_index * golden_angle + phi;

    return vec2(r * cos(theta), r * sin(theta));
}

float InterleavedGradientNoise(vec2 position_screen)
{
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);

    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

// Get as random uint, modifies seed parameter
uint RandomUint(inout uint seed)
{
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
    return (float(RandomUint(seed) & 0x00FFFFFF) / float(0x01000000));
}

vec3 RandomInSphere(vec3 rand)
{
    float ang1 = (rand.x + 1.0) * HYP_FMATH_PI;
    float u = rand.y;
    float u2 = u * u;
    float sqrt1MinusU2 = sqrt(1.0 - u2);
    float x = sqrt1MinusU2 * cos(ang1);
    float y = sqrt1MinusU2 * sin(ang1);
    float z = u;
    return vec3(x, y, z);
}

vec3 RandomInHemisphere(vec3 rand, vec3 n)
{
    vec3 v = RandomInSphere(rand);
    return v * sign(dot(v, n));
}

vec3 RandomInHemisphereCosineWeighted(vec3 rand, vec3 n) {
    float r = rand.x * 0.5 + 0.5;
    float angle = (rand.y + 1.0) * HYP_FMATH_PI;
    float sr = sqrt(r);
    vec2 p = vec2(sr * cos(angle), sr * sin(angle));

    vec3 ph = vec3(p.xy, sqrt(1.0 - p*p));

    vec3 tangent = normalize(rand);
    vec3 bitangent = cross(tangent, n);
    tangent = cross(bitangent, n);

    return tangent * ph.x + bitangent * ph.y + n * ph.z;
}

// Generates a seed for a random number generator from 2 inputs plus a backoff
// https://github.com/nvpro-samples/optix_prime_baking/blob/332a886f1ac46c0b3eea9e89a59593470c755a0e/random.h
// https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_jitter_cam
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint InitRandomSeed(uint val0, uint val1)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    for (uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}


#endif