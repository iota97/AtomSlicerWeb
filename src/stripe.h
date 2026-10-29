// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "hierarchy.h"
#include <array>
#include <set>
#include <cassert>
#include <memory>

struct VertexData : BaseVertexData {
    Vec3 direction[2];
    float phase[2];
    bool isFrozen;
};

class Stripe {
public:
    Stripe(MeshHierarchy<VertexData> *hierarchy, const float *directions, const std::set<std::array<uint32_t, 2>> &borderEdges, const std::vector<bool> &borders, float width, bool quiet = true);
    void optimize(std::vector<float> &scalars);

private:
    const float *directionAngle;
    const std::vector<bool> &borders;
    const std::set<std::array<uint32_t, 2>> &borderEdges;

    MeshHierarchy<VertexData> *hierarchy;
    float width = 0.0f;

    void init(uint32_t i, VertexData *vertexData);
    static void smoothLevel(MeshHierarchy<VertexData> &level, uint32_t iterations);
    static void smooth(uint32_t id, const MeshHierarchy<VertexData>::Neighborhood &neighborhood, VertexData *vertexData, uint32_t iteration);
    static void restrict(uint32_t id, const MeshHierarchy<VertexData>::Links &links, VertexData *vertexData, VertexData *vertexDataCoarse);
    static void prolong(uint32_t id, uint32_t coarse_id, VertexData *vertexData, VertexData *vertexDataCoarse);

    void optimizeNearest();
    void initNearest(uint32_t id, VertexData* vertexData);
    static void smoothNearest(uint32_t id, const MeshHierarchy<VertexData>::Neighborhood& neighborhood, VertexData* vertexData, uint32_t iteration);
    static void restrictNearest(uint32_t id, const MeshHierarchy<VertexData>::Links& links, VertexData* vertexData, VertexData* vertexDataCoarse);
    static void prolongNearest(uint32_t id, uint32_t coarse_id, VertexData* vertexData, VertexData* vertexDataCoarse);
};