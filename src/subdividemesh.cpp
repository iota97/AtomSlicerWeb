// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "subdividemesh.h"
#include <map>
#include <array>
#include <iostream>
#include <cstring>

SubdivideMesh::SubdivideMesh(const float *vertsPosition, const uint32_t *trianglesID, uint32_t vertsCount, uint32_t trisCount, float maxLength) {
    positions.resize(3*vertsCount);
    triangles.resize(3*trisCount);
    std::memcpy(positions.data(), vertsPosition, vertsCount * 3 * sizeof(float));
    std::memcpy(triangles.data(), trianglesID, trisCount * 3 * sizeof(uint32_t));

    float maxEdgeLength = 0.0f;
    for (uint32_t i = 0; i < trisCount; ++i) {
        uint32_t v0 = triangles[3*i+0], v1 = triangles[3*i+1], v2 = triangles[3*i+2];
        maxEdgeLength = std::max(maxEdgeLength, (Vec3(vertsPosition, v0)-Vec3(vertsPosition, v1)).length());
        maxEdgeLength = std::max(maxEdgeLength, (Vec3(vertsPosition, v0)-Vec3(vertsPosition, v2)).length());
        maxEdgeLength = std::max(maxEdgeLength, (Vec3(vertsPosition, v1)-Vec3(vertsPosition, v2)).length());
    }

    while (maxEdgeLength > maxLength) {
        std::vector<uint32_t> newFaces(3*4*trisCount);
        std::map<std::array<uint32_t, 2>, uint32_t> edgesID;

        uint32_t id = 0;
        for (uint32_t i = 0; i < trisCount; ++i) {
            uint32_t v0 = triangles[3*i+0], v1 = triangles[3*i+1], v2 = triangles[3*i+2];
            std::array<uint32_t, 2> e0{std::min(v0, v1), std::max(v0, v1)};
            std::array<uint32_t, 2> e1{std::min(v0, v2), std::max(v0, v2)};
            std::array<uint32_t, 2> e2{std::min(v1, v2), std::max(v1, v2)};

            if (!edgesID.count(e0)) edgesID[e0] = id++;
            if (!edgesID.count(e1)) edgesID[e1] = id++;
            if (!edgesID.count(e2)) edgesID[e2] = id++;
            uint32_t v01 = vertsCount+edgesID[e0], v02 = vertsCount+edgesID[e1], v12 = vertsCount+edgesID[e2];

            newFaces[3*(4*i+0)+0] = v0; newFaces[3*(4*i+0)+1] = v01; newFaces[3*(4*i+0)+2] = v02;
            newFaces[3*(4*i+1)+0] = v2; newFaces[3*(4*i+1)+1] = v02; newFaces[3*(4*i+1)+2] = v12;
            newFaces[3*(4*i+2)+0] = v12; newFaces[3*(4*i+2)+1] = v01; newFaces[3*(4*i+2)+2] = v1;
            newFaces[3*(4*i+3)+0] = v02; newFaces[3*(4*i+3)+1] = v01; newFaces[3*(4*i+3)+2] = v12;
        }

        std::vector<float> newPositions(3*(vertsCount+edgesID.size()));
        maxEdgeLength = 0.0f;
        for (size_t i = 0; i < trisCount; ++i) {
            uint32_t v0 = triangles[3 * i + 0], v1 = triangles[3 * i + 1], v2 = triangles[3 * i + 2];
            std::array<uint32_t, 2> e0{ std::min(v0, v1), std::max(v0, v1) };
            std::array<uint32_t, 2> e1{ std::min(v0, v2), std::max(v0, v2) };
            std::array<uint32_t, 2> e2{ std::min(v1, v2), std::max(v1, v2) };
            uint32_t v01 = vertsCount + edgesID[e0], v02 = vertsCount + edgesID[e1], v12 = vertsCount + edgesID[e2];

            newPositions[3 * v0 + 0] = positions[3 * v0 + 0]; newPositions[3 * v0 + 1] = positions[3 * v0 + 1]; newPositions[3 * v0 + 2] = positions[3 * v0 + 2];
            newPositions[3 * v1 + 0] = positions[3 * v1 + 0]; newPositions[3 * v1 + 1] = positions[3 * v1 + 1]; newPositions[3 * v1 + 2] = positions[3 * v1 + 2];
            newPositions[3 * v2 + 0] = positions[3 * v2 + 0]; newPositions[3 * v2 + 1] = positions[3 * v2 + 1]; newPositions[3 * v2 + 2] = positions[3 * v2 + 2];

            Vec3 p01 = 0.5f * (Vec3(positions.data(), v0) + Vec3(positions.data(), v1));
            Vec3 p02 = 0.5f * (Vec3(positions.data(), v0) + Vec3(positions.data(), v2));
            Vec3 p12 = 0.5f * (Vec3(positions.data(), v1) + Vec3(positions.data(), v2));
            newPositions[3 * v01 + 0] = p01.x; newPositions[3 * v01 + 1] = p01.y; newPositions[3 * v01 + 2] = p01.z;
            newPositions[3 * v02 + 0] = p02.x; newPositions[3 * v02 + 1] = p02.y; newPositions[3 * v02 + 2] = p02.z;
            newPositions[3 * v12 + 0] = p12.x; newPositions[3 * v12 + 1] = p12.y; newPositions[3 * v12 + 2] = p12.z;

            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v0) - Vec3(newPositions.data(), v01)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v0) - Vec3(newPositions.data(), v02)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v01) - Vec3(newPositions.data(), v02)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v2) - Vec3(newPositions.data(), v02)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v2) - Vec3(newPositions.data(), v12)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v02) - Vec3(newPositions.data(), v12)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v12) - Vec3(newPositions.data(), v01)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v12) - Vec3(newPositions.data(), v1)).length());
            maxEdgeLength = std::max(maxEdgeLength, (Vec3(newPositions.data(), v01) - Vec3(newPositions.data(), v1)).length());
        }

        triangles = std::move(newFaces);
        positions = std::move(newPositions);
        trisCount = triangles.size()/3;
        vertsCount = positions.size()/3;
    }
}