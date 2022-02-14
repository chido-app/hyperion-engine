#version 330 core

#define $PI 3.141592654

#include "../include/matrices.inc"
#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/lighting.inc"
#include "../include/parallax.inc"
#include "../include/tonemap.inc"

#if SHADOWS
#include "../include/shadows.inc"
#endif

in vec2 v_texcoord0;
uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform sampler2D PositionMap;
//uniform sampler2D NormalMap; // included in lighting.inc
uniform sampler2D DataMap;
uniform sampler2D TangentMap;
uniform sampler2D BitangentMap;

uniform sampler3D LightVolumeMap;

uniform sampler2D SSLightingMap;
uniform int HasSSLightingMap;

uniform vec3 CameraPosition;
uniform mat4 InverseViewProjMatrix;

#define $DIRECTIONAL_LIGHTING 1

vec3 decodeNormal(vec4 enc)
{
    vec2 fenc = enc.xy*4-2;
    float f = dot(fenc,fenc);
    float g = sqrt(1-f/4);
    vec3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
}

float SchlickFresnel2(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1.0/$PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1.0 / ($PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1.0 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1.0 / max(NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ), 0.001);
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

void main()
{
    vec4 albedo = texture(ColorMap, v_texcoord0);
    vec4 data = texture(DataMap, v_texcoord0);
    float depth = texture(DepthMap, v_texcoord0).r;

    float metallic = clamp(data.r, 0.05, 0.99);
    float roughness = clamp(data.g, 0.05, 0.99);
    float performLighting = data.a;
    float ao = 1.0;
    vec4 gi = vec4(0.0);
	vec3 irradiance = vec3(1.0);

    vec3 N = (texture(NormalMap, v_texcoord0).rgb * 2.0 - 1.0);
	vec3 tangent = texture(TangentMap, v_texcoord0).rgb * 2.0 - 1.0;
	vec3 bitangent = texture(BitangentMap, v_texcoord0).rgb * 2.0 - 1.0;
	mat3 tbn = mat3( normalize(tangent), normalize(bitangent), N );
	
    vec4 position = vec4(positionFromDepth(InverseViewProjMatrix, v_texcoord0, depth), 1.0);
    vec3 L = normalize(env_DirectionalLight.direction);
    vec3 V = normalize(CameraPosition-position.xyz);
	

    if (performLighting < 0.9) {
        output0 = vec4(albedo.rgb, 1.0);
        return;
    }

    if (HasSSLightingMap == 1) {
        vec4 ssl = texture(SSLightingMap, v_texcoord0);
        //gi = ssl.rgb;
        ao *= 1.0 - ssl.a;
    }

#if DIRECTIONAL_LIGHTING

	
	vec3 X = tangent;
	vec3 Y = bitangent;

    float NdotL = max(0.001, dot(N, L));
    float NdotV = max(0.001, dot(N, V));
    vec3 H = normalize(L + V);
    float NdotH = max(0.001, dot(N, H));
    float LdotH = max(0.001, dot(L, H));
    float VdotH = max(0.001, dot(V, H));
    float HdotV = max(0.001, dot(H, V));
	float LdotX = dot(L, X);
	float LdotY = dot(L, Y);
	float VdotX = dot(V, X);
	float VdotY = dot(V, Y);
	float HdotX = dot(H, X);
	float HdotY = dot(H, Y);

#if SHADOWS
    float shadowness = 0.0;
    int shadowSplit = getShadowMapSplit(distance(CameraPosition, position.xyz));

#if SHADOW_PCF
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            vec2 offset = poissonDisk[x * 4 + y] * $SHADOW_MAP_RADIUS;
            vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz + vec3(offset.x, offset.y, -offset.x));
            shadowness += getShadow(shadowSplit, shadowCoord, NdotL);
        }
    }

    shadowness /= 16.0;
#endif // !SHADOW_PCF

#if !SHADOW_PCF
    vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz);
    shadowness = getShadow(shadowSplit, shadowCoord, NdotL);
#endif // !SHADOW_PCF

    vec4 shadowColor = vec4(vec3(shadowness), 1.0);
    shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), position.xyz, CameraPosition, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif // SHADOWS

#if !SHADOWS
    float shadowness = 1.0;
    vec4 shadowColor = vec4(1.0);
#endif 

  vec4 blurredSpecularCubemap = vec4(0.0);
  vec4 specularCubemap = vec4(0.0);
  float roughnessMix = clamp(1.0 - exp(-(roughness / 1.0 * log(100.0))), 0.0, 1.0);

#if VCT_ENABLED
  vec4 vctSpec = VCTSpecular(position.xyz, N.xyz, CameraPosition, roughness);
  vec4 vctDiff = VCTDiffuse(position.xyz, N.xyz, CameraPosition, tangent, bitangent, roughness);
  specularCubemap += vctSpec;
  gi += vctDiff;
#endif // VCT_ENABLED

#if PROBE_ENABLED
  blurredSpecularCubemap = SampleEnvProbe(env_GlobalIrradianceCubemap, N, position.xyz, CameraPosition, tangent, bitangent);
  specularCubemap += SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent);
  //specularCubemap += mix(SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent), blurredSpecularCubemap, roughnessMix);
#if !SPHERICAL_HARMONICS_ENABLED
  //gi += EnvRemap(Irradiance(N));
#endif // !SPHERICAL_HARMONICS_ENABLED
#endif // PROBE_ENABLED

#if SPHERICAL_HARMONICS_ENABLED
  irradiance = SampleSphericalHarmonics(N);
#endif // SPHERICAL_HARMONICS_ENABLED

	float subsurface = 0.0;
	float specular = 0.5;
	float specularTint = 0.0;
	float anisotropic = 0.0;
	float sheen = 0.0;
	float sheenTint = 0.1;
	float clearcoat = 0.4;
	float clearcoatGloss = 1.0;
	

#define $DISNEY_BRDF 1
	
#if DISNEY_BRDF
    vec3 Cdlin = albedo.rgb;//mon2lin(albedo.rgb);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint) , Cdlin, metallic);
    vec3 Csheen = mix(vec3(1), Ctint, sheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel2(NdotL), FV = SchlickFresnel2(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH*LdotH*roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

    // specular
    float aspect = sqrt(1.0-anisotropic*0.9);
    float ax = max(.001, sqr(roughness)/aspect);
    float ay = max(.001, sqr(roughness)*aspect);
    float Ds = clamp(GTR2_aniso(NdotH, HdotX, HdotY, ax, ay), 0.0, 1.0);
    float FH = SchlickFresnel2(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, LdotX, LdotY, ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, VdotX, VdotY, ax, ay);

    // sheen
    vec3 Fsheen = FH * sheen * Csheen;

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss));
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
	
	
    vec2 AB = BRDFMap(NdotV, metallic);
    vec3 ibl = min(vec3(0.99), FH * AB.x + AB.y) * specularCubemap.rgb;
	
	vec3 ambient = Cdlin * irradiance * ao;

    vec3 result = (((1.0/$PI) * (mix(Fd, ss, subsurface) + (gi.rgb)))*ambient + Fsheen)
        * (1.0-metallic)
        + (Gs*Fs*Ds + .25*clearcoat*Gr*Fr*Dr) * ibl;
#endif

#if !DISNEY_BRDF
    vec3 Cdlin = mon2lin(albedo.rgb);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint) , Cdlin, metallic);
    vec3 Csheen = mix(vec3(1), Ctint, sheenTint);


    vec3 F0 = vec3(0.04);
    F0 = mix(vec3(1.0), F0, metallic);

    vec2 AB = BRDFMap(NdotV, metallic);

    //vec3 metallicSpec = mix(vec3(0.04), Cdlin, metallic);
    vec3 metallicDiff = mix(Cdlin, vec3(0.0), metallic);

    vec3 F = FresnelTerm(Cspec0, VdotH) * clamp(NdotL, 0.0, 1.0);
    float D = clamp(DistributionGGX(N, H, roughness), 0.0, 1.0);
    float G = clamp(SmithGGXSchlickVisibility(clamp(NdotL, 0.0, 1.0), clamp(NdotV, 0.0, 1.0), roughness), 0.0, 1.0);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= vec3(1.0 - metallic);

    vec3 reflectedLight = vec3(0.0, 0.0, 0.0);
    vec3 diffuseLight = vec3(0.0, 0.0, 0.0);
	
	
	float FH = SchlickFresnel2(LdotH);

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss));
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);


    float rim = mix(1.0 - roughnessMix * 1.0 * 0.9, 1.0, NdotV);
    vec3 specRef = vec3(/*(1.0 / max(rim, 0.0001)) * */ (F * G * D) + .25*clearcoat*Gr*Fr*Dr) * NdotL;
    reflectedLight += specRef;
	
	
    vec3 Fsheen = FH * sheen * Csheen;


    vec3 ibl = min(vec3(0.99), FresnelTerm(Cspec0, NdotV) * AB.x + AB.y);
    reflectedLight += ibl * specularCubemap.rgb;
	reflectedLight *= ao;

    vec3 diffRef = vec3((vec3(1.0) - F) * (1.0 / $PI) * NdotL) + Fsheen;
    diffRef += gi.rgb;
    diffuseLight += diffRef * (1.0 / $PI);
    diffuseLight *= metallicDiff;
	diffuseLight *= ao;

    vec3 result = (diffuseLight * irradiance) + reflectedLight * shadowColor.rgb;

#endif

#endif

    /*for (int i = 0; i < env_NumPointLights; i++) {
        result += ComputePointLight(
            env_PointLights[i],
            N,
            CameraPosition,
            position.xyz,
            albedo.rgb,
            0, // todo
            roughness,
            metallic
        );
    }*/
	
	result.rgb = tonemap(result.rgb);

    output0 = vec4(result, 1.0);
}
