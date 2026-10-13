// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "mathutils.h"
#include <map>
#include <vector>

std::set<std::array<uint32_t, 2>> getMeshBorder(const uint32_t *triangles, uint32_t trisCount) {
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
    }

    std::vector<uint8_t> edgeDegree(edgesID.size(), 0);
    for (size_t i = 0; i < trisCount; ++i) {
        uint32_t v0 = triangles[3 * i + 0], v1 = triangles[3 * i + 1], v2 = triangles[3 * i + 2];
        std::array<uint32_t, 2> e0{ std::min(v0, v1), std::max(v0, v1) };
        std::array<uint32_t, 2> e1{ std::min(v0, v2), std::max(v0, v2) };
        std::array<uint32_t, 2> e2{ std::min(v1, v2), std::max(v1, v2) };
        edgeDegree[edgesID[e0]]++; edgeDegree[edgesID[e1]]++; edgeDegree[edgesID[e2]]++; 
    }

    std::set<std::array<uint32_t, 2>> borderEdges;
    for (size_t i = 0; i < trisCount; ++i) {
        uint32_t v0 = triangles[3 * i + 0], v1 = triangles[3 * i + 1], v2 = triangles[3 * i + 2];
        std::array<uint32_t, 2> e0{ std::min(v0, v1), std::max(v0, v1) };
        std::array<uint32_t, 2> e1{ std::min(v0, v2), std::max(v0, v2) };
        std::array<uint32_t, 2> e2{ std::min(v1, v2), std::max(v1, v2) };

        if (edgeDegree[edgesID[e0]] == 1) borderEdges.insert(e0);
        if (edgeDegree[edgesID[e1]] == 1) borderEdges.insert(e1);
        if (edgeDegree[edgesID[e2]] == 1) borderEdges.insert(e2);
    }

    return borderEdges;
}