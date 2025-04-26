#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdint.h>

// Van der Corput sequence with base 2
inline float radicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// Hammersley point set for importance sampling
inline glm::vec2 hammersley2D(uint32_t i, uint32_t N) {
    return glm::vec2(float(i) / float(N), radicalInverse_VdC(i));
}

// Improved cosine-weighted hemisphere sampling for better quality
inline glm::vec3 hammersleyToDirection(float u, float v, const glm::vec3& normal) {
    // Spherical coordinates with improved cosine-weighted distribution
    float phi = 2.0f * glm::pi<float>() * u;
    
    // Cosine-weighted distribution
    float cosTheta = sqrt(1.0f - v);
    float sinTheta = sqrt(v);
    
    // Cartesian coordinates
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;
    
    // Create more precise tangent space basis
    glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
    
    // Create orthonormal basis matrix
    glm::mat3 TBN(tangent, bitangent, normal);
    
    // Transform to world space
    return TBN * glm::vec3(x, y, z);
}