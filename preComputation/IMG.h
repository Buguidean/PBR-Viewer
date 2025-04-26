#pragma once

#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

struct CubemapData
{
    int width;
    int height;
    int channels;
    std::vector<float> data;
};

struct CubemapFace {
    glm::vec3 target;
    glm::vec3 up;
};

class IMG
{
public:

    IMG();
    ~IMG();

    bool loadCubemap(const std::string& inputCubemapDir);
    bool computeIrradianceMap(int outputSize, int numSamples);
    bool saveIrradianceMap(const std::string& outputPath);

private:

    glm::vec3 sampleCubemap(const glm::vec3& direction);
    glm::vec3 sampleCubemapBilinear(int faceIndex, float u, float v); 
    glm::vec3 getTexelColor(int faceIndex, int x, int y);
    bool directionToFaceUV(const glm::vec3& direction, int* faceIndex, float* u, float* v);

    std::shared_ptr<CubemapData> m_inputCubemap;
    std::shared_ptr<CubemapData> m_irradianceMap;
    std::vector<CubemapFace> m_cubemapFaces;

    int m_outputSize;
};