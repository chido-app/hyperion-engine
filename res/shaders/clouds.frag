#version 330 core

in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord0;

uniform float m_GlobalTime;
uniform sampler2D m_CloudMap;
uniform vec4 m_CloudColor;

const float timeScale = 0.005;
const float cloudScale = 100.0;
const float skyCover = 0.5;
const float softness = 0.4;
const float brightness = 1.6;
const int noiseOctaves = 4;

#define $FOG_START 100.0
#define $FOG_END 700.0

#include "include/frag_output.inc"

float saturate(float num)
{
  return clamp(num, 0.0, 1.0);
}

float noise(vec2 uv)
{
  return texture(m_CloudMap, uv * cloudScale).r;
}

vec2 rotate(vec2 uv)
{
  uv = uv + noise(uv*0.2) * 0.005;
  // float rot = 0.5;
  // float sinRot=sin(rot);
  // float cosRot=cos(rot);
  
  // optimized
  float sinRot = 0.479426;
  float cosRot = 0.877583;
  
  mat2 rotMat = mat2(cosRot, -sinRot, sinRot, cosRot);
  return uv * rotMat;
}

float fbm (vec2 uv)
{
  // const float rot = 1.57;
  // float sinRot=sin(rot);
  // float cosRot=cos(rot);
  
  // optimized
  float sinRot = 0.999999;
  float cosRot = 0.000796;
  
  float f = 0.0;
  float total = 0.0;
  float mul = 0.5;
  mat2 rotMat = mat2(cosRot, -sinRot, sinRot, cosRot);
    
  for(int i = 0; i < noiseOctaves; i++) {
    f += noise(uv + m_GlobalTime * 0.00015 * timeScale * (1.0 - mul)) * mul;
    total += mul;
    uv *= 3.0;
    uv = rotate(uv);
    mul *= 0.5;
  }
  return f / total;
}

void main() 
{

  float dist = gl_FragCoord.z / gl_FragCoord.w; 
  vec4 fogColor = vec4(1.0, 1.0, 1.0, 0.0);
  float fogFactor = ($FOG_END - dist) / ($FOG_END - $FOG_START);
  fogFactor = saturate(fogFactor);

  float color1 =  fbm(v_texcoord0 - vec2(0.5 + m_GlobalTime * 0.04 * timeScale));
  float color2 = fbm(v_texcoord0 - vec2(10.5 + m_GlobalTime * 0.02 * timeScale));

            
  float clouds1 = smoothstep(1.0 - skyCover, min((1.0 - skyCover) + softness * 2.0, 1.0), color1);
  float clouds2 = smoothstep(1.0 - skyCover, min((1.0 - skyCover) + softness, 1.0), color2);

  float cloudsFormComb = saturate(clouds1 + clouds2);

  float cloudCol = saturate(saturate(1.0 - color1 * 0.2) * brightness);
  vec4 clouds1Color = vec4(cloudCol, cloudCol, cloudCol, 1.0);
  vec4 clouds2Color = mix(clouds1Color, vec4(0.0, 0.0, 0.0, 0.0), 0.25);


  clouds1Color = mix(fogColor, clouds1Color, fogFactor);
  clouds2Color = mix(fogColor, clouds2Color, fogFactor);

  vec4 cloudColComb = mix(clouds1Color, clouds2Color, saturate(clouds2 - clouds1));
  cloudColComb = mix(vec4(1.0, 1.0, 1.0, 0.0), cloudColComb, cloudsFormComb);// * m_CloudColor;

  
  output0 = cloudColComb;
  // if (output0.a < 0.1) {
  //   discard;
  // }
  output1 = vec4(v_normal * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(0.0);
  output4 = vec4(0.0);
}

