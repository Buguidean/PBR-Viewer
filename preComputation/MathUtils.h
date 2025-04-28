#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdint.h>

// Van der Corput sequence with base 2 (Extracted from learnopengl)
inline float radicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// Hammersley point set for importance sampling (Extracted from learnopengl)
inline glm::vec2 hammersley2D(uint32_t i, uint32_t N) {
    return glm::vec2(float(i) / float(N), radicalInverse_VdC(i));
}

inline glm::vec3 hammersleyToDirection(float u, float v, const glm::vec3& normal) {
    float phi = 2.0f * glm::pi<float>() * u;
    
    // Cosine-weighted distribution
    float cosTheta = sqrt(1.0f - v);
    float sinTheta = sqrt(v);
    
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;
    
    glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
    
    glm::mat3 TBN(tangent, bitangent, normal);
    return TBN * glm::vec3(x, y, z);
}

inline glm::vec3 importanceSampleGGX(float u, float v, const glm::vec3& normal, float roughness){
    float s_roughness = roughness * roughness;
    float phi = 2.0f * glm::pi<float>() * u;

    // Roughness-weighted distribution
    float cosTheta = sqrt((1.0 - v) / (1.0 + (s_roughness * s_roughness - 1.0) * v));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;

    glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
    
    glm::mat3 TBN(tangent, bitangent, normal);
    return TBN * glm::vec3(x, y, z);
}