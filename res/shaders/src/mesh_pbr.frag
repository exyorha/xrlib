// Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2
#define PI 3.14159265359
#define EPSILON 0.0001

// Flag definitions for packed bits in tonemap uint
#define TONEMAP_MASK 0xFu         // First 4 bits reserved for tonemap enum (16 values)
#define TONEMAP_SHIFT 0u          // Shift amount for tonemap bits

#define RENDER_MODE_MASK 0xF0u    // Next 4 bits reserved for render mode enum (16 values)
#define RENDER_MODE_SHIFT 4u      // Shift amount for render mode bits

// Render modes (values after shift)
#define RENDER_MODE_UNLIT 0u
#define RENDER_MODE_BLINN_PHONG 1u
#define RENDER_MODE_PBR 2u

// Material texture flags
#define TEXTURE_BASE_COLOR_BIT     0x01u
#define TEXTURE_METALLIC_ROUGH_BIT 0x02u
#define TEXTURE_NORMAL_BIT         0x04u
#define TEXTURE_EMISSIVE_BIT       0x08u
#define TEXTURE_OCCLUSION_BIT      0x10u

// Remaining bits 8-31 available for additional flags

#include "tonemaps.glsl"    // Supported tonemapping functions (see xrlib::ETonemapOperator)

// Material UBO
layout(set = 0, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float normalScale;
    uint textureFlags;
    int alphaMode;
    vec2 padding;
} material;

// Scene Lighting UBO
    
// Main directional light
struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
};

// Point lights
struct PointLight {
    vec3 position;
    float range;
    vec3 color;
    float intensity;
};

// Spot lights
struct SpotLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    float innerCone;
    float outerCone;
};

// Tonemapping parameters with packed flags
struct Tonemapping {
    float exposure;
    float gamma;
    uint tonemap;      // Bits 0-3: tonemap operator, Bits 4-5: render mode, Bits 6-31: available
    float contrast;
    float saturation;
};

layout(set = 1, binding = 0) uniform SceneLighting {
    // Scene lighting
    DirectionalLight mainLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOTLIGHTS];

    // Scene ambient and active lights
    vec3 ambientColor;
    float ambientIntensity;
    uint activePointLights;
    uint activeSpotLights;

    Tonemapping tonemapping;
} scene;

// Texture samplers
layout(binding = 1) uniform sampler2D baseColorMap;
layout(binding = 2) uniform sampler2D metallicRoughnessMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D emissiveMap;
layout(binding = 5) uniform sampler2D occlusionMap;

// Input from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Output
layout(location = 0) out vec4 outColor;

// Optimized PBR functions
vec3 fresnelSchlickFast(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * exp2((-5.55473 * cosTheta - 6.98316) * cosTheta);
}

float DistributionGGXFast(float NdotH, float roughness) {
    float a2 = roughness * roughness;
    float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (PI * f * f);
}

float GeometrySmithFast(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return (NdotV * NdotL) / ((NdotV * (1.0 - k) + k) * (NdotL * (1.0 - k) + k));
}

// Unified light calculation struct
struct LightParams {
    vec3 lightDir;
    vec3 lightColor;
    float lightIntensity;
    vec3 viewDir;
    vec3 normal;
    vec3 baseColor;
    float metallic;
    float roughness;
    vec3 F0;
};

// Combined light calculation function
vec3 calculateLight(LightParams params, uint renderMode) {
    if (renderMode == RENDER_MODE_PBR) {
        vec3 H = normalize(params.viewDir + params.lightDir);
        float NdotL = max(dot(params.normal, params.lightDir), 0.0);
        float NdotH = max(dot(params.normal, H), 0.0);
        float NdotV = max(dot(params.normal, params.viewDir), 0.0);
        float VdotH = max(dot(params.viewDir, H), 0.0);

        vec3 F = fresnelSchlickFast(VdotH, params.F0);
        float D = DistributionGGXFast(NdotH, params.roughness);
        float G = GeometrySmithFast(NdotV, NdotL, params.roughness);

        vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - params.metallic);
        
        return (kD * params.baseColor / PI + specular) * params.lightColor * params.lightIntensity * NdotL;
    } else {
        // Blinn-Phong
        float diff = max(dot(params.normal, params.lightDir), 0.0);
        vec3 halfwayDir = normalize(params.lightDir + params.viewDir);
        float spec = pow(max(dot(params.normal, halfwayDir), 0.0), 32.0);
        return (diff + spec * 0.5) * params.lightColor * params.lightIntensity * params.baseColor;
    }
}

// IBL Approximations
// Spherical Harmonics approximation for ambient diffuse
vec3 getAmbientSH(vec3 normal) {

    // Base coefficients modulated by scene ambient
    vec3 L00 = scene.ambientColor * vec3(0.15, 0.13, 0.11);  // Ambient base
    
    // Side light influenced by main light color
    vec3 L1_1 = scene.mainLight.color * vec3(0.25, 0.15, 0.1);  
    
    // Top light (sky contribution)
    vec3 L10 = scene.ambientColor * vec3(0.075, 0.075, 0.1);
    
    // Front light influenced by main light
    vec3 L11 = scene.mainLight.color * vec3(0.15, 0.1, 0.05);   
    
    // Modulate all coefficients by scene ambient intensity
    L00 *= scene.ambientIntensity;
    L1_1 *= scene.mainLight.intensity * 0.5;
    L10 *= scene.ambientIntensity;
    L11 *= scene.mainLight.intensity * 0.5;
    
    // Spherical harmonics evaluation
    return L00 + 
           L1_1 * normal.x +
           L10 * normal.y +
           L11 * normal.z;
}

// Approximated specular IBL
vec3 getSpecularIBL(vec3 reflectDir, float roughness, vec3 F0) {
    float NoV = max(dot(reflectDir, vec3(0, 1, 0)), 0.0);
    vec3 fresnel = fresnelSchlickFast(NoV, F0);
    
    // Reduce the base reflection colors
    vec3 horizonColor = mix(scene.mainLight.color, vec3(0.3, 0.2, 0.15), 0.4) * scene.mainLight.intensity;
    vec3 skyColor = scene.ambientColor * vec3(0.1, 0.12, 0.15) * scene.ambientIntensity;
    
    vec3 gradientColor = mix(horizonColor, skyColor, pow(reflectDir.y * 0.5 + 0.5, 1.5));
    
    float smoothness = 1.0 - roughness;
    float reflectionStrength = smoothness * smoothness; 
    
    return gradientColor * fresnel * reflectionStrength;
}

// Main IBL approximation function
vec3 approximateIBL(vec3 normal, vec3 viewDir, vec3 baseColor, float metallic, float roughness) {
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Get diffuse and specular components
    vec3 diffuseIBL = getAmbientSH(normal) * baseColor * (1.0 - metallic);
    
    // Add roughness-based attenuation to specular
    float specularAttenuation = (1.0 - roughness) * (1.0 - roughness);
    vec3 specularIBL = getSpecularIBL(reflectDir, roughness, F0) * specularAttenuation;
    
    // Scale the diffuse contribution higher relative to specular
    return diffuseIBL * 1.5 + specularIBL * 0.5;
}

vec3 calculateAmbient(vec3 baseColor, vec3 normal, vec3 viewDir, float metallic, float roughness, float ao, uint renderMode) {
    if (renderMode == RENDER_MODE_PBR) {
       // vec3 F0 = mix(vec3(0.04), baseColor, metallic);
       // vec3 kS = fresnelSchlickFast(max(dot(normal, viewDir), 0.0), F0);
       // vec3 kD = (1.0 - kS) * (1.0 - metallic);
       // return scene.ambientColor * scene.ambientIntensity * (kD * baseColor + kS * mix(vec3(0.04), baseColor, metallic)) * ao;
        vec3 iblColor = approximateIBL(normal, viewDir, baseColor, metallic, roughness);
        return iblColor * ao * scene.ambientIntensity;
    }

    return scene.ambientColor * scene.ambientIntensity * baseColor;
}

void main() {
    // Extract flags from tonemap uint - get both tonemap and render mode
    uint tonemapOp = (scene.tonemapping.tonemap & TONEMAP_MASK) >> TONEMAP_SHIFT;
    uint renderMode = (scene.tonemapping.tonemap & RENDER_MODE_MASK) >> RENDER_MODE_SHIFT;

    // Always sample base color
    vec4 baseColor = material.baseColorFactor;
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    float ao = 1.0;

    if ((material.textureFlags & TEXTURE_BASE_COLOR_BIT) != 0u) {
        baseColor *= texture(baseColorMap, inUV);
        if (material.alphaMode == 1 && baseColor.a < material.alphaCutoff) {
            discard;
        }
    }

    // Early exit for unlit mode
    if (renderMode == RENDER_MODE_UNLIT) {
        outColor = baseColor;
        return;
    }

    // Always calculate normal
    vec3 N = normalize(inNormal);
    if ((material.textureFlags & TEXTURE_NORMAL_BIT) != 0u) {
        vec3 tangentNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;
        tangentNormal.xy *= material.normalScale;
        mat3 TBN = mat3(normalize(inTangent), normalize(inBitangent), N);
        N = normalize(TBN * tangentNormal);
    }

    // Sample PBR textures if in PBR mode
    if (renderMode == RENDER_MODE_PBR) {
        if ((material.textureFlags & TEXTURE_METALLIC_ROUGH_BIT) != 0u) {
            vec4 metallicRoughness = texture(metallicRoughnessMap, inUV);
            metallic *= metallicRoughness.b;
            roughness *= metallicRoughness.g;
        }

        if ((material.textureFlags & TEXTURE_OCCLUSION_BIT) != 0u) {
            ao = texture(occlusionMap, inUV).r;
        }
    }

    vec3 V = normalize(-inWorldPos);
    vec3 Lo = vec3(0.0);

    // Fresnel reflectance at normal incidence
    // 0.04 is average for most dielectric mats (SIGGRAPH 2013 - Real shading in Unreal Engine 4)
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
    vec3 F0 = renderMode == RENDER_MODE_PBR ? mix(vec3(0.04), baseColor.rgb, metallic) : vec3(0.0); 

    // Setup base light parameters
    LightParams lightParams;
    lightParams.viewDir = V;
    lightParams.normal = N;
    lightParams.baseColor = baseColor.rgb;
    lightParams.metallic = metallic;
    lightParams.roughness = roughness;
    lightParams.F0 = F0;

    // Main directional light
    lightParams.lightDir = normalize(-scene.mainLight.direction);
    lightParams.lightColor = scene.mainLight.color;
    lightParams.lightIntensity = scene.mainLight.intensity;
    Lo += calculateLight(lightParams, renderMode);

    // Point lights
    for (uint i = 0; i < scene.activePointLights && i < MAX_POINT_LIGHTS; i++) {
        vec3 L = scene.pointLights[i].position - inWorldPos;
        float dist2 = dot(L, L);
        lightParams.lightDir = L * inversesqrt(dist2);
        float atten = clamp(1.0 - dist2/(scene.pointLights[i].range * scene.pointLights[i].range), 0.0, 1.0);
        
        lightParams.lightColor = scene.pointLights[i].color;
        lightParams.lightIntensity = scene.pointLights[i].intensity * atten;
        Lo += calculateLight(lightParams, renderMode);
    }

    // Spot lights
    for (uint i = 0; i < scene.activeSpotLights && i < MAX_SPOTLIGHTS; i++) {
        vec3 L = scene.spotLights[i].position - inWorldPos;
        float dist2 = dot(L, L);
        L *= inversesqrt(dist2);
        float theta = dot(L, normalize(-scene.spotLights[i].direction));
        
        if (theta > scene.spotLights[i].outerCone) {
            float epsilon = scene.spotLights[i].innerCone - scene.spotLights[i].outerCone;
            float intensity = clamp((theta - scene.spotLights[i].outerCone) / epsilon, 0.0, 1.0);
            float atten = clamp(1.0 - dist2/(scene.spotLights[i].range * scene.spotLights[i].range), 0.0, 1.0);
            
            lightParams.lightDir = L;
            lightParams.lightColor = scene.spotLights[i].color;
            lightParams.lightIntensity = scene.spotLights[i].intensity * atten * intensity;
            Lo += calculateLight(lightParams, renderMode);
        }
    }

    // Calculate ambient and combine
    vec3 color = calculateAmbient(baseColor.rgb, N, V, metallic, roughness, ao, renderMode) + Lo;

    // Tonemap based on extracted operator
    TonemapParams tonemapParams = TonemapParams(scene.tonemapping.exposure, scene.tonemapping.gamma);
    color = tonemapOp == 1 ? tonemapReinhard(color, tonemapParams) :
            tonemapOp == 2 ? tonemapACES(color, tonemapParams) :
            tonemapOp == 3 ? tonemapKHRNeutral(color, tonemapParams) :
            tonemapOp == 4 ? tonemapUncharted2(color, tonemapParams) :
            color;

    color = gammaCorrect(color, scene.tonemapping.gamma);

    // Optional emissive
    if ((material.textureFlags & TEXTURE_EMISSIVE_BIT) != 0u) {
        color += texture(emissiveMap, inUV).rgb * material.emissiveFactor.rgb;
    }

    outColor = vec4(color, baseColor.a);
}