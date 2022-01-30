#version 430

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.08

in vec4 v_position;
in vec4 v_ndc;
in vec4 v_voxelPosition;
in vec4 v_positionCamspace;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/matrices.inc"
#include "include/frag_output.inc"
#include "include/depth.inc"
#include "include/lighting.inc"
#include "include/parallax.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif
uniform vec3 u_probePos;
uniform float u_scale;
uniform sampler3D tex[6];
layout(binding = 0, rgba32f) uniform image3D framebufferImage;
layout(binding = 1, rgba32f) uniform image3D framebufferImage1;
layout(binding = 2, rgba32f) uniform image3D framebufferImage2;
layout(binding = 3, rgba32f) uniform image3D framebufferImage3;
layout(binding = 4, rgba32f) uniform image3D framebufferImage4;
layout(binding = 5, rgba32f) uniform image3D framebufferImage5;

uniform mat4 WorldToVoxelTexMatrix;

vec4 voxelFetch(vec3 pos, vec3 dir, float lod)
{
	return textureLod(tex[0], pos, lod);
	vec4 sampleX =
		dir.x < 0.0
		? textureLod(tex[0], pos, lod)
		: textureLod(tex[1], pos, lod);
	
	vec4 sampleY =
		dir.y < 0.0
		? textureLod(tex[2], pos, lod)
		: textureLod(tex[3], pos, lod);
	
	vec4 sampleZ =
		dir.z < 0.0
		? textureLod(tex[4], pos, lod)
		: textureLod(tex[5], pos, lod);
	
	vec3 sampleWeights = abs(dir);
	float invSampleMag = 1.0 / (sampleWeights.x + sampleWeights.y + sampleWeights.z + .0001);
	sampleWeights *= invSampleMag;
	
	vec4 filtered = 
		sampleX * sampleWeights.x
		+ sampleY * sampleWeights.y
		+ sampleZ * sampleWeights.z;
		
		//if (dir.x < 0.0) {
		//	return textureLod(tex[0], pos, lod);
		//} else {
		//	return textureLod(tex[1], pos, lod);
		//}
		vec4 negX = textureLod(tex[0], pos, lod);
		vec4 posX = textureLod(tex[1], pos, lod);
		
		//if (dir.y < 0.0) {
		//	return textureLod(tex[2], pos, lod);
		//} else {
		///	return vec4(0.0);
		//}
		
		return mix(negX, posX, clamp(dir.x*0.5+0.5, 0.0, 1.0));
		
	
	return filtered;
}
// origin, dir, and maxDist are in texture space
// dir should be normalized
// coneRatio is the cone diameter to height ratio (2.0 for 90-degree cone)
vec4 voxelTraceCone(float minVoxelDiameter, vec3 origin, vec3 dir, float coneRatio, float maxDist)
{
	float minVoxelDiameterInv = 1.0/minVoxelDiameter;
	vec3 samplePos = origin;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(1.0, 1.0, 1.0, 1.0)*0.2;
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		vec3 samplePos = origin + dir * dist;
		vec4 sampleValue = voxelFetch(samplePos, -dir, sampleLOD);
		sampleValue = mix(sampleValue,fadeCol, clamp(dist/maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}
	return accum;
}
void main() 
{
	float voxelImageSize = 128.0;
	float halfVoxelImageSize = voxelImageSize * 0.5;

		
	vec4 lambert = vec4(0.0, 0.0, 0.0, 1.0);// = calcDirectionalLight(v_normals, env_DirectionalLight.direction);
	vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);// = calcDirectionalLightSpec(normalize(directionalLight.direction), u_specularFactor, normalize(v_normals.xyz), u_cameraPosition, v_position, C_albedo);
	vec4 imageColor = u_diffuseColor;
	//if (B_hasDiffuseMap > 0) {
	//	imageColor.rgb *= texture2D(T_diffuse, texcoord0).rgb;
	//}	
	vec4 coneTraceRes;
	float dist = distance(u_camerapos, v_position.xyz);
	float maxDist = voxelImageSize/u_scale;
	//if (dist < maxDist) {
		vec3 vPos = (vec3(v_voxelPosition.xyz * 0.2)-u_probePos+vec3(halfVoxelImageSize))/vec3(voxelImageSize);
		//vec4 voxelPosition = u_projMatrix * u_viewMatrix * vec4(v_position.xyz, 1.0);//WorldToVoxelTexMatrix * vec4(v_position.xyz, 1.0);
		//voxelPosition.xyz /= voxelPosition.w;
		//vec3 vPos = voxelPosition.xyz;
		
		vec3 eyeToFragment = normalize(v_position.xyz - u_camerapos);
		vec3 reflectionDir = reflect(eyeToFragment, normalize(v_normal.xyz));
		
		float rangeMin = 0.02;
		float rangeMax = 0.1;
		float level = clamp(dist/maxDist*0.5, rangeMin, rangeMax);
		vec4 reflection = voxelTraceCone(1.0/voxelImageSize, vPos, normalize(reflectionDir), 0.2, 20.0);
		coneTraceRes += voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz), 1.0, .3);
	    coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz+v_tangent), 1.0, .3);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz-v_tangent), 1.0, .3);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz+v_bitangent), 1.0, .3);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz-v_bitangent), 1.0, .3);
		specular += reflection;////mix(reflection, vec4(0.0), clamp(dist/maxDist, 0.0, 1.0));//+vec4(ambientLight.color.rgb, 1.0);
	//}
	
	output0 = vec4(reflection.rgb, 1.0);//(lambert)*imageColor + (specular) ;
	
}