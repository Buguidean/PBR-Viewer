#include "IMG.h"
#include "MathUtils.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <sys/stat.h>
#include <omp.h>

// Include stb_image for image loading/saving
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

IMG::IMG()
    : m_outputSize(64)
{
    m_cubemapFaces = {
        {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)},  // Positive X
        {glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)}, // Negative X
        {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},   // Positive Y
        {glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)}, // Negative Y
        {glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)},  // Positive Z
        {glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)}  // Negative Z
    };
}

IMG::~IMG() {
}

bool IMG::loadCubemap(const std::string& inputCubemapDir) {
    const std::string faceNames[12] = {
        "posx", "negx", "posy", "negy", "posz", "negz", "right", "left", "top", "bottom", "back", "front"
    };
    
    m_inputCubemap = std::make_shared<CubemapData>();
    int faceWidth, faceHeight, faceChannels;
    bool firstFace = true;
    
    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        std::string facePath = inputCubemapDir + "/" + faceNames[faceIndex] + ".png";
        if (!fileExists(facePath)) {
            facePath = inputCubemapDir + "/" + faceNames[faceIndex + 6] + ".png";
            if (!fileExists(facePath)){
                std::cerr << "Could not find face file for: " << faceNames[faceIndex] << std::endl;
                return false;
            }
        }
        unsigned char* data = stbi_load(facePath.c_str(), &faceWidth, &faceHeight, &faceChannels, 0);
        if (!data) {
            std::cerr << "Failed to load face: " << facePath << std::endl;
            return false;
        }
        
        if (firstFace) {
            m_inputCubemap->width = faceWidth;
            m_inputCubemap->height = faceHeight;
            m_inputCubemap->channels = faceChannels;
            m_inputCubemap->data.resize(faceWidth * faceHeight * faceChannels * 6);
            firstFace = false;
        }
        
        int faceSize = faceWidth * faceHeight * faceChannels;
        int offset = faceSize * faceIndex;
        
        for (int i = 0; i < faceSize; i++) {
            m_inputCubemap->data[offset + i] = data[i] / 255.0f;
        }
        
        stbi_image_free(data);
        
        std::cout << "Loaded face: " << facePath << " (" << faceWidth << "x" << faceHeight 
                  << ", " << faceChannels << " channels)" << std::endl;
    }
    
    std::cout << "Loaded all 6 cubemap faces" << std::endl;
    return true;
}

bool IMG::computeIrradianceMap(int outputSize, int numSamples) {
    m_outputSize = outputSize;
    
    m_irradianceMap = std::make_shared<CubemapData>();
    m_irradianceMap->width = outputSize;
    m_irradianceMap->height = outputSize;
    m_irradianceMap->channels = 3;
    m_irradianceMap->data.resize(outputSize * outputSize * 6 * m_irradianceMap->channels, 0.0f);

    float PI = glm::pi<float>();
    
    #pragma omp parallel for collapse(3) schedule(dynamic)
    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        for (int y = 0; y < outputSize; y++) {
            for (int x = 0; x < outputSize; x++) {
                // [-1, 1]
                float u = (2.0f * x / outputSize) - 1.0f;
                float v = (2.0f * y / outputSize) - 1.0f;
                
                // Create a direction vector for this pixel
                glm::vec3 direction;
                switch (faceIndex) {
                    case 0: direction = glm::normalize(glm::vec3( 1.0f, -v, -u)); break; // +X
                    case 1: direction = glm::normalize(glm::vec3(-1.0f, -v,  u)); break; // -X
                    case 2: direction = glm::normalize(glm::vec3(  u, 1.0f,  v)); break; // +Y
                    case 3: direction = glm::normalize(glm::vec3(  u,-1.0f, -v)); break; // -Y
                    case 4: direction = glm::normalize(glm::vec3(  u, -v, 1.0f)); break; // +Z
                    case 5: direction = glm::normalize(glm::vec3( -u, -v,-1.0f)); break; // -Z
                }
                
                int pixelIndex = (faceIndex * outputSize * outputSize) + (y * outputSize) + x;
                glm::vec4 accumLocal(0.0f); // Local accumulator for each thread
                
                // Determine number of samples for this pixel
                int sampleId = 0;               
                int remainingSamples = numSamples;

                while (remainingSamples > 0) {                  
                    glm::vec2 hammersleyPoint = ::hammersley2D(sampleId, numSamples);
                    glm::vec3 L = hammersleyToDirection(hammersleyPoint.x, hammersleyPoint.y, direction);
                    
                    // Calculate NoL (Direction is the normal)
                    float NdotL = glm::dot(direction, L);
                    
                    // If the sample contributes (is in the hemisphere)
                    if (NdotL > 0.0f) {                      
                        // Sample the cubemap with bilinear filtering
                        glm::vec3 sampleColor = sampleCubemap(L);
                        
                        // Accumulate result (rgb) and weight (a)
                        accumLocal.x += sampleColor.r * NdotL;
                        accumLocal.y += sampleColor.g * NdotL;
                        accumLocal.z += sampleColor.b * NdotL;
                        accumLocal.w += NdotL;
                    }

                    ++sampleId;
                    --remainingSamples;
                }

                // Normalize the accumulated result
                if (accumLocal.w > 0.0f) {
                    accumLocal.x /= accumLocal.w;
                    accumLocal.y /= accumLocal.w;
                    accumLocal.z /= accumLocal.w;
                }
                
                int offset = (faceIndex * outputSize * outputSize + y * outputSize + x) * m_irradianceMap->channels;
                m_irradianceMap->data[offset + 0] = accumLocal.x;
                m_irradianceMap->data[offset + 1] = accumLocal.y;
                m_irradianceMap->data[offset + 2] = accumLocal.z;
            }
        }
    }
    
    return true;
}

bool IMG::generateMipMaps(int outputSize){
    if (outputSize < 32){
        return false;
    }

    int mipSize = outputSize;
    for (int i = 0 ; i < 5 ; i++){
        m_specularMap[i] = std::make_shared<CubemapData>();
        m_specularMap[i]->width = mipSize;
        m_specularMap[i]->height = mipSize;
        m_specularMap[i]->channels = 3;
        m_specularMap[i]->data.resize(mipSize * mipSize * 6 * m_specularMap[i]->channels, 0.0f);

        mipSize /= 2;
    }

    return true;
}

bool IMG::computeSpecularIBL(int outputSize, int numSamples) {
    m_outputSize = outputSize;
    if (!generateMipMaps(outputSize)){
        std::cerr<<"Output size too small for 5 mipmaps"<<std::endl;
        return false;
    }

    for (int mipLevel = 0; mipLevel < 5; mipLevel++) {

        int mipSize = outputSize >> mipLevel;
        float roughness = static_cast<float>(mipLevel) / 4.0f;
        
        #pragma omp parallel for collapse(3) schedule(dynamic)
        for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
            for (int y = 0; y < mipSize; y++) {
                for (int x = 0; x < mipSize; x++) {
                    // [-1, 1] range
                    float u = (2.0f * x / mipSize) - 1.0f;
                    float v = (2.0f * y / mipSize) - 1.0f;
                    
                    // (Direction is the normal)
                    glm::vec3 direction;
                    switch (faceIndex) {
                        case 0: direction = glm::normalize(glm::vec3( 1.0f, -v, -u)); break; // +X
                        case 1: direction = glm::normalize(glm::vec3(-1.0f, -v,  u)); break; // -X
                        case 2: direction = glm::normalize(glm::vec3(  u, 1.0f,  v)); break; // +Y
                        case 3: direction = glm::normalize(glm::vec3(  u,-1.0f, -v)); break; // -Y
                        case 4: direction = glm::normalize(glm::vec3(  u, -v, 1.0f)); break; // +Z
                        case 5: direction = glm::normalize(glm::vec3( -u, -v,-1.0f)); break; // -Z
                    }
                    
                    glm::vec4 accumLocal(0.0f);
                    
                    for (int sampleId = 0; sampleId < numSamples; sampleId++) {

                        glm::vec2 hammersleyPoint = ::hammersley2D(sampleId, numSamples);
                        glm::vec3 H = importanceSampleGGX(hammersleyPoint.x, hammersleyPoint.y, direction, roughness);
                        glm::vec3 L = glm::normalize(2.0f * glm::dot(direction, H) * H - direction);
                        
                        float NdotL = glm::dot(direction, L);
                        
                        if (NdotL > 0.0f) {
                            glm::vec3 sampleColor = sampleCubemap(L);
                            
                            accumLocal.x += sampleColor.r * NdotL;
                            accumLocal.y += sampleColor.g * NdotL;
                            accumLocal.z += sampleColor.b * NdotL;
                            accumLocal.w += NdotL;
                        }
                    }

                    // Normalize
                    if (accumLocal.w > 0.0f) {
                        accumLocal.x /= accumLocal.w;
                        accumLocal.y /= accumLocal.w;
                        accumLocal.z /= accumLocal.w;
                    }
                    
                    // Use correct mipSize for offset calculation
                    int offset = (faceIndex * mipSize * mipSize + y * mipSize + x) * m_specularMap[mipLevel]->channels;
                    m_specularMap[mipLevel]->data[offset + 0] = accumLocal.x;
                    m_specularMap[mipLevel]->data[offset + 1] = accumLocal.y;
                    m_specularMap[mipLevel]->data[offset + 2] = accumLocal.z;
                }
            }
        }
    }
    
    return true;
}

glm::vec3 IMG::sampleCubemap(const glm::vec3& direction) {
    int faceIndex;
    float u, v;
    
    if (!directionToFaceUV(direction, &faceIndex, &u, &v)) {
        return glm::vec3(0.0f);
    }

    return sampleCubemapBilinear(faceIndex, u, v);
}

glm::vec3 IMG::sampleCubemapBilinear(int faceIndex, float u, float v) {
    // Get the four nearest texels
    float sx = u * m_inputCubemap->width;
    float sy = v * m_inputCubemap->height;
    
    int x1 = static_cast<int>(std::floor(sx));
    int y1 = static_cast<int>(std::floor(sy));
    int x2 = x1 + 1;
    int y2 = y1 + 1;
    
    // Clamp to valid range
    x1 = std::max(0, std::min(m_inputCubemap->width - 1, x1));
    y1 = std::max(0, std::min(m_inputCubemap->height - 1, y1));
    x2 = std::max(0, std::min(m_inputCubemap->width - 1, x2));
    y2 = std::max(0, std::min(m_inputCubemap->height - 1, y2));
    
    // Calculate fractional parts for interpolation
    float fx = sx - x1;
    float fy = sy - y1;
    
    // Get colors from the four nearest texels
    glm::vec3 c11 = getTexelColor(faceIndex, x1, y1);
    glm::vec3 c21 = getTexelColor(faceIndex, x2, y1);
    glm::vec3 c12 = getTexelColor(faceIndex, x1, y2);
    glm::vec3 c22 = getTexelColor(faceIndex, x2, y2);
    
    // Bilinear interpolation
    glm::vec3 c1 = c11 * (1.0f - fx) + c21 * fx;
    glm::vec3 c2 = c12 * (1.0f - fx) + c22 * fx;
    glm::vec3 c  = c1 * (1.0f - fy) + c2 * fy;
    
    return c;
}

glm::vec3 IMG::getTexelColor(int faceIndex, int x, int y) {
    int offset = (faceIndex * m_inputCubemap->width * m_inputCubemap->height + 
                 y * m_inputCubemap->width + x) * m_inputCubemap->channels;
    
    glm::vec3 color;
    color.r = m_inputCubemap->data[offset + 0];
    color.g = m_inputCubemap->data[offset + 1];
    color.b = m_inputCubemap->data[offset + 2];
    
    return color;
}

bool IMG::directionToFaceUV(const glm::vec3& direction, int* faceIndex, float* u, float* v) {
    // Find the dominant axis
    float absX = std::abs(direction.x);
    float absY = std::abs(direction.y);
    float absZ = std::abs(direction.z);
    
    bool isXPositive = direction.x > 0;
    bool isYPositive = direction.y > 0;
    bool isZPositive = direction.z > 0;
    
    // Use the largest component to determine the face
    if (absX >= absY && absX >= absZ) {
        *faceIndex = isXPositive ? 0 : 1; // +X or -X
        *u = isXPositive ? -direction.z : direction.z;
        *v = -direction.y;
        *u = (*u / absX + 1.0f) * 0.5f;
        *v = (*v / absX + 1.0f) * 0.5f;
    } else if (absY >= absX && absY >= absZ) {
        *faceIndex = isYPositive ? 2 : 3; // +Y or -Y
        *u = direction.x;
        *v = isYPositive ? direction.z : -direction.z;
        *u = (*u / absY + 1.0f) * 0.5f;
        *v = (*v / absY + 1.0f) * 0.5f;
    } else {
        *faceIndex = isZPositive ? 4 : 5; // +Z or -Z
        *u = isZPositive ? direction.x : -direction.x;
        *v = -direction.y;
        *u = (*u / absZ + 1.0f) * 0.5f;
        *v = (*v / absZ + 1.0f) * 0.5f;
    }
    
    return true;
}

bool IMG::saveIrradianceMap(const std::string& outputPath) {
    if (!m_irradianceMap) {
        std::cerr << "No irradiance map to save" << std::endl;
        return false;
    }
    
    // For simplicity, we'll save each face as a separate file
    const std::string faceNames[6] = {
        "right", "left", "top", "bottom", "back", "front"
    };
    
    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        std::string facePath = outputPath + faceNames[faceIndex] + ".png";
        std::vector<unsigned char> faceData(m_outputSize * m_outputSize * 4);
        for (int y = 0; y < m_outputSize; y++) {
            for (int x = 0; x < m_outputSize; x++) {
                int srcOffset = (faceIndex * m_outputSize * m_outputSize + y * m_outputSize + x) * m_irradianceMap->channels;
                int dstOffset = (y * m_outputSize + x) * 4; // Always use RGBA
                
                for (int c = 0; c < 3; c++) { // Process RGB
                    float value = m_irradianceMap->data[srcOffset + c];
                    faceData[dstOffset + c] = static_cast<unsigned char>(255.0f * std::min(1.0f, std::max(0.0f, value)));
                }
                
                // Always set alpha to fully opaque
                faceData[dstOffset + 3] = 255;
            }
        }
        
        // Save the face image as PNG
        if (!stbi_write_png(facePath.c_str(), m_outputSize, m_outputSize, 4, 
                          faceData.data(), m_outputSize * 4)) {
            std::cerr << "Failed to save face: " << facePath << std::endl;
            return false;
        }
        
        std::cout << "Saved face: " << facePath << std::endl;
    }
    
    return true;
}

bool IMG::saveSpecularIBL(const std::string& outputPath){
    if (!m_specularMap[0]) {
        std::cerr << "No specular map to save" << std::endl;
        return false;
    }
    
    // For simplicity, we'll save each face as a separate file
    const std::string faceNames[6] = {
        "right", "left", "top", "bottom", "back", "front"
    };
    
    for (int mipLevel = 0 ; mipLevel < 5 ; mipLevel++){
        int mipSize = m_outputSize >> mipLevel;
        std::string mip = std::to_string(mipLevel);
        for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
            std::string facePath = outputPath + mip + "_" + faceNames[faceIndex] + ".png";
            std::vector<unsigned char> faceData(mipSize * mipSize * 4);
            for (int y = 0; y < mipSize; y++) {
                for (int x = 0; x < mipSize; x++) {
                    int srcOffset = (faceIndex * mipSize * mipSize + y * mipSize + x) * m_specularMap[mipLevel]->channels;
                    int dstOffset = (y * mipSize + x) * 4; // Always use RGBA
                    
                    for (int c = 0; c < 3; c++) { // Process RGB
                        float value = m_specularMap[mipLevel]->data[srcOffset + c];
                        faceData[dstOffset + c] = static_cast<unsigned char>(255.0f * std::min(1.0f, std::max(0.0f, value)));
                    }
                    
                    // Always set alpha to fully opaque
                    faceData[dstOffset + 3] = 255;
                }
            }
            
            // Save the face image as PNG
            if (!stbi_write_png(facePath.c_str(), mipSize, mipSize, 4, 
                            faceData.data(), mipSize * 4)) {
                std::cerr << "Failed to save face: " << facePath << std::endl;
                return false;
            }
            
            std::cout << "Saved face: " << facePath << std::endl;
        }
    }
    
    return true;
}