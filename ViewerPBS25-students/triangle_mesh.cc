// Author: Imanol Munoz-Pandiella 2023 based on Marc Comino 2020

#include <triangle_mesh.h>

#include <algorithm>
#include <limits>

#include <iostream>
#include <glm/geometric.hpp>

namespace data_representation {

TriangleMesh::TriangleMesh() { Clear(); }

void TriangleMesh::Clear() {
    vertices_.clear();
    faces_.clear();
    normals_.clear();
    texCoords_.clear();

    min_ = glm::vec3(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
    max_ = glm::vec3(std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest());
}

void TriangleMesh::computeNormals()
{
    normals_.clear();
    normals_.resize(vertices_.size(), 0.0f);
    size_t vertexCount = vertices_.size() / 3;

    std::vector<std::vector<int>> facesPerVertex(vertexCount);
    std::vector<glm::vec3> faceNormals(faces_.size() / 3);

    for(size_t i = 0; i < faces_.size(); i += 3) {
        // Get vertex indices
        int idx1 = faces_[i];
        int idx2 = faces_[i+1];
        int idx3 = faces_[i+2];

        // Get vertex positions from their indices
        glm::vec3 v1(vertices_[3*idx1], vertices_[3*idx1+1], vertices_[3*idx1+2]);
        glm::vec3 v2(vertices_[3*idx2], vertices_[3*idx2+1], vertices_[3*idx2+2]);
        glm::vec3 v3(vertices_[3*idx3], vertices_[3*idx3+1], vertices_[3*idx3+2]);

        glm::vec3 e1 = v2 - v1;
        glm::vec3 e2 = v3 - v1;

        glm::vec3 normal = glm::cross(e1, e2);
        normal = glm::normalize(normal);

        int faceIdx = i / 3;
        faceNormals[faceIdx] = normal;

        facesPerVertex[idx1].push_back(faceIdx);
        facesPerVertex[idx2].push_back(faceIdx);
        facesPerVertex[idx3].push_back(faceIdx);
    }

    // Compute vertex normals
    for(size_t i = 0; i < vertexCount; ++i) {
        glm::vec3 vertexNormal(0.0f);

        for(int faceIdx : facesPerVertex[i]) {
            vertexNormal += faceNormals[faceIdx];
        }
        vertexNormal = glm::normalize(vertexNormal);

        normals_[3*i] = vertexNormal.x;
        normals_[3*i+1] = vertexNormal.y;
        normals_[3*i+2] = vertexNormal.z;
    }

    std::cout << "Normals computed!" << std::endl;
}

}  // namespace data_representation
