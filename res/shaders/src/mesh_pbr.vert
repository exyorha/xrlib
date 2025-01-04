// Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#extension GL_EXT_multiview : require

// Push constants - view/projection matrices for both eyes
layout(push_constant) uniform PushConsts {
    mat4 eyeVPs[2];
} pushConsts;

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord0;
layout(location = 4) in vec2 inTexCoord1;
layout(location = 5) in vec3 inColor0;
layout(location = 6) in ivec4 inJoints;
layout(location = 7) in vec4 inWeights;

// Instance data (model matrix columns)
layout(location = 8) in vec4 inModelMatrix_col0;
layout(location = 9) in vec4 inModelMatrix_col1;
layout(location = 10) in vec4 inModelMatrix_col2;
layout(location = 11) in vec4 inModelMatrix_col3;

// Output to fragment shader
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;

void main() {
    // Reconstruct model matrix from columns
    mat4 modelMatrix = mat4(
        inModelMatrix_col0,
        inModelMatrix_col1,
        inModelMatrix_col2,
        inModelMatrix_col3
    );

    // Calculate normal matrix (transpose of inverse of upper 3x3 model matrix)
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    // Transform vertex position to world space
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    // Transform normal to world space
    outNormal = normalize(normalMatrix * inNormal);

    // Transform tangent and calculate bitangent
    vec3 tangent = normalize(normalMatrix * inTangent.xyz);
    outTangent = tangent;
    
    // Calculate bitangent (using tangent.w for handedness)
    outBitangent = cross(outNormal, tangent) * inTangent.w;

    // Pass through texture coordinates
    outUV = inTexCoord0;

    // Final position in clip space (using eye VPs from push constants)
    gl_Position = pushConsts.eyeVPs[gl_ViewIndex] * worldPos;
}