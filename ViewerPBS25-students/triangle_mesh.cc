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
    // Clear and resize normals to match vertex count
    normals_.clear();
    normals_.resize(vertices_.size(), 0.0f); // Properly allocate space for all normals

    // Calculate number of vertices (each vertex has 3 components)
    size_t vertexCount = vertices_.size() / 3;

    // Create a list that maps each vertex to the faces it belongs to
    std::vector<std::vector<int>> facesPerVertex(vertexCount);

    // Compute normals per face
    std::vector<glm::vec3> faceNormals(faces_.size() / 3); // One normal per face, not per index

    // Process each face (triangle)
    for(size_t i = 0; i < faces_.size(); i += 3) {
        // Get vertex indices
        int idx1 = faces_[i];
        int idx2 = faces_[i+1];
        int idx3 = faces_[i+2];

        // Make sure indices are in bounds
        if(idx1 >= vertexCount || idx2 >= vertexCount || idx3 >= vertexCount) {
            std::cerr << "Invalid face index detected!" << std::endl;
            continue;
        }

        // Get vertex positions from their indices
        glm::vec3 v1(vertices_[3*idx1], vertices_[3*idx1+1], vertices_[3*idx1+2]);
        glm::vec3 v2(vertices_[3*idx2], vertices_[3*idx2+1], vertices_[3*idx2+2]);
        glm::vec3 v3(vertices_[3*idx3], vertices_[3*idx3+1], vertices_[3*idx3+2]);

        // Calculate edge vectors
        glm::vec3 e1 = v2 - v1;
        glm::vec3 e2 = v3 - v1;

        // Calculate face normal using cross product
        glm::vec3 normal = glm::cross(e1, e2);
        if(glm::length(normal) > 0.0001f) {
            normal = glm::normalize(normal);
        }

        // Store face normal
        int faceIdx = i / 3;
        faceNormals[faceIdx] = normal;

        // Map vertices to this face
        facesPerVertex[idx1].push_back(faceIdx);
        facesPerVertex[idx2].push_back(faceIdx);
        facesPerVertex[idx3].push_back(faceIdx);
    }

    // Compute vertex normals by averaging connected face normals
    for(size_t i = 0; i < vertexCount; ++i) {
        glm::vec3 vertexNormal(0.0f);

        // Sum up normals of all faces this vertex belongs to
        for(int faceIdx : facesPerVertex[i]) {
            vertexNormal += faceNormals[faceIdx];
        }

        // Normalize if not zero
        if(glm::length(vertexNormal) > 0.0001f) {
            vertexNormal = glm::normalize(vertexNormal);
        }

        // Store the normal in the normals vector
        normals_[3*i] = vertexNormal.x;
        normals_[3*i+1] = vertexNormal.y;
        normals_[3*i+2] = vertexNormal.z;
    }

    std::cout << "Normals computed! (" << normals_.size()/3 << " normals)" << std::endl;
}

}  // namespace data_representation
