// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "stripe.h"
#include "subdivide.h"
#include <cstdlib>
#include <chrono>
#include <iostream>

static float s_period;

Stripe::Stripe(MeshHierarchy<VertexData> *hierarchy, const float *directions, const std::set<std::array<uint32_t, 2>> &borderEdges, const std::vector<bool> &borders, float width, bool quiet)
        : width(width), borderEdges(borderEdges), borders(borders), hierarchy(hierarchy), directionAngle(directions) {
    auto startTime = std::chrono::steady_clock::now();

    VertexData *vertexData = hierarchy->getVertexDataMut();
    for (size_t i = 0; i < hierarchy->getVertsCount(); ++i) {
        vertexData[i].direction[0] = Vec3(0);
    }

    for (auto &edge : borderEdges) {
        Vec3 dir = (vertexData[edge[0]].position - vertexData[edge[1]].position).normalize();
        vertexData[edge[0]].direction[0] += (dir.dot(vertexData[edge[0]].direction[0]) < 0 ? -1 : 1) * dir;
        vertexData[edge[1]].direction[0] += (dir.dot(vertexData[edge[1]].direction[0]) < 0 ? -1 : 1) * dir;
    }
    optimizeNearest();

    s_period = 2.0f*width;

    if (!quiet) std::cout << "Stripes initialization (" << hierarchy->getLodsCount() << " LODs): " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;
}

void Stripe::optimize(std::vector<float> &scalars) {
    hierarchy->optimize([this](uint32_t i, VertexData *vertexData) { init(i, vertexData); }, smoothLevel, restrict, prolong);
    auto vertexData = hierarchy->getVertexData();
    scalars.resize(hierarchy->getVertsCount());
    Sequential::For(0, hierarchy->getVertsCount(), [&](size_t i) {
        scalars[i] = cosf(vertexData[i].phase[0]);
    });
}

void Stripe::init(uint32_t i, VertexData *vertexData) {
    if (vertexData[i].phase[0] > 2.5f * width) {
        vertexData[i].direction[0] = Vec3(cosf(directionAngle[i]), sinf(directionAngle[i]), 0).orthonormalize(vertexData[i].normal);
    } else {
        if (hierarchy->getVertexData()[i].direction[0] == Vec3(0, 0, 0)) {
            vertexData[i].direction[0] = Vec3(1,0,0).orthonormalize(hierarchy->getVertexData()[i].normal);
        } else {
            vertexData[i].direction[0] = hierarchy->getVertexData()[i].direction[0].orthonormalize(hierarchy->getVertexData()[i].normal);
        }
    }

    vertexData[i].phase[0] = 0.0f;
    vertexData[i].isFrozen = borders[i];
}

void Stripe::smooth(uint32_t id, const MeshHierarchy<VertexData>::Neighborhood &neighborhood, VertexData *vertexData, uint32_t iteration) {
    uint32_t i = id;
    if (vertexData[i].isFrozen) {
        vertexData[i].phase[1-(iteration % 2)] = vertexData[i].phase[iteration % 2];
        return;
    }

    float x = 0.0f, y = 0.0f;
    for (auto &e : neighborhood) {
        uint32_t j = e.id;
        float w = e.weight;
        Vec3 p_i = vertexData[i].position;
        Vec3 p_j = vertexData[j].position;
        Vec3 p_ij = (p_i+p_j)*0.5f;

        Vec3 d_i = vertexData[i].direction[0];
        Vec3 d_j = vertexData[j].direction[0];
        float phi_j = vertexData[j].phase[(iteration % 2)];

        float a = 2.0f*float(M_PI)/s_period * (p_ij-p_i).dot(d_i);
        float b = 2.0f*float(M_PI)/s_period * (p_ij-p_j).dot(d_j) + phi_j;
        float angle = d_i.dot(d_j) > 0.0f ? b - a : -b - a;

        x += w*cosf(angle);
        y += w*sinf(angle);
    }
    vertexData[i].phase[1-(iteration % 2)] = atan2f(y, x);

}

void Stripe::restrict(uint32_t id, const MeshHierarchy<VertexData>::Links &links, VertexData *vertexData, VertexData *vertexDataCoarse) {
    Vec3 direction(0, 0, 0);

    float x = 0.0f, y = 0.0f;
    float count = 0.0f;
    bool isFrozen = false;
    for (auto j : links) {
        Vec3 d_j = vertexData[j].direction[0];
        direction = direction + (d_j.dot(direction) > 0.0f ? d_j : -d_j);
    }
    direction = direction.orthonormalize(vertexDataCoarse[id].normal);
    vertexDataCoarse[id].direction[0] = direction;

    for (auto j : links) {
        Vec3 d_j = vertexData[j].direction[0];
        count++;
        if (vertexData[j].isFrozen) {
            isFrozen = true;
            Vec3 p_i = vertexDataCoarse[id].position;
            Vec3 p_j = vertexData[j].position;
            Vec3 d_i = vertexDataCoarse[id].direction[0];
            float phi_j = vertexData[j].phase[0];
            float b = 2.0f*float(M_PI)/s_period * (p_i-p_j).dot(d_j) + phi_j;
            float angle = d_i.dot(d_j) > 0.0f ? b: -b;

            x += cosf(angle);
            y += sinf(angle);
        }
    }

    vertexDataCoarse[id].phase[0] = atan2f(y, x);
    vertexDataCoarse[id].isFrozen = isFrozen;
}

void Stripe::prolong(uint32_t id, uint32_t coarse_id, VertexData *vertexData, VertexData *vertexDataCoarse) {
    if (vertexData[id].isFrozen)
        return;

    Vec3 p_i = vertexData[id].position;
    Vec3 p_j = vertexDataCoarse[coarse_id].position;

    Vec3 d_i = vertexData[id].direction[0];
    Vec3 d_j = vertexDataCoarse[coarse_id].direction[0];
    float phi_j = vertexDataCoarse[coarse_id].phase[0];

    float a = 2.0f*float(M_PI)/s_period * (p_j-p_i).dot(d_i);
    float b = phi_j;
    float angle = d_i.dot(d_j) > 0.0f ? b - a : -b - a;

    vertexData[id].phase[0] = angle;
}

void Stripe::optimizeNearest() {
    hierarchy->optimize([this](uint32_t i, VertexData* vertexData) { initNearest(i, vertexData); }, smoothNearest, restrictNearest, prolongNearest, 32);
}

void Stripe::initNearest(uint32_t id, VertexData* vertexData) {
    if (vertexData[id].direction[0].length() == 0) {
        vertexData[id].direction[0] = Vec3(0, 0, 0);
        vertexData[id].phase[0] = std::numeric_limits<float>::infinity();
    } else {
        vertexData[id].direction[0] = vertexData[id].direction[0].cross(vertexData[id].normal).normalize();
        vertexData[id].phase[0] = 0.0f;
    }
}

void Stripe::smoothNearest(uint32_t id, const MeshHierarchy<VertexData>::Neighborhood& neighborhood, VertexData* vertexData, uint32_t iteration) {
    vertexData[id].phase[1 - (iteration % 2)] = vertexData[id].phase[iteration % 2];
    vertexData[id].direction[1 - (iteration % 2)] = vertexData[id].direction[iteration % 2];

    for (auto& e : neighborhood) {
        float d_ij = (vertexData[id].position - vertexData[e.id].position).length();
        if (d_ij + vertexData[e.id].phase[iteration % 2] < vertexData[id].phase[1 - (iteration % 2)]) {
            vertexData[id].phase[1 - (iteration % 2)] = d_ij + vertexData[e.id].phase[iteration % 2];
            vertexData[id].direction[1 - (iteration % 2)] = vertexData[e.id].direction[iteration % 2].orthonormalize(vertexData[id].normal);
        }
    }
}

void Stripe::restrictNearest(uint32_t id, const MeshHierarchy<VertexData>::Links& links, VertexData* vertexData, VertexData* vertexDataCoarse) {
    vertexDataCoarse[id].direction[0] = Vec3(0, 0, 0);
    vertexDataCoarse[id].phase[0] = std::numeric_limits<float>::infinity();

    for (auto j : links) { 
        float d_ij = (vertexDataCoarse[id].position - vertexData[j].position).length();
        if (d_ij + vertexData[j].phase[0] < vertexDataCoarse[id].phase[0]) {
            vertexDataCoarse[id].phase[0] = d_ij + vertexData[j].phase[0];
            vertexDataCoarse[id].direction[0] = vertexData[j].direction[0].orthonormalize(vertexDataCoarse[id].normal);
        }
    }
}

void Stripe::prolongNearest(uint32_t id, uint32_t coarse_id, VertexData* vertexData, VertexData* vertexDataCoarse) {
    float d_ij = (vertexData[id].position - vertexDataCoarse[coarse_id].position).length();
    if (d_ij + vertexDataCoarse[coarse_id].phase[0] < vertexData[id].phase[0]) {
        vertexData[id].phase[0] = d_ij + vertexDataCoarse[coarse_id].phase[0];
        vertexData[id].direction[0] = vertexDataCoarse[coarse_id].direction[0].orthonormalize(vertexData[id].normal);
    }
}

void Stripe::smoothLevel(MeshHierarchy<VertexData> &level, uint32_t iterations) {
    VertexData *vertexData = level.getVertexDataMut();
    MeshHierarchy<VertexData>::Edge **adj = level.getAdjMatrix();
    float frequency = 2.0f*float(M_PI)/s_period;
    uint32_t vertsCount = level.getVertsCount();

    struct EdgeData { float weightedCosine, weightedSine, dotSign; };
    std::vector<EdgeData> edgeData(level.getEdgeCount());
    for (uint32_t i = 0; i < vertsCount; ++i) {
        if (vertexData[i].isFrozen) continue;
        Vec3 p_i = vertexData[i].position;
        Vec3 d_i = vertexData[i].direction[0];
        uint32_t offset = adj[i] - adj[0];
        for (const MeshHierarchy<VertexData>::Edge *e = adj[i]; e != adj[i+1]; ++e) {
            uint32_t j = e->id;
            float w = e->weight;
            Vec3 p_j = vertexData[j].position;
            Vec3 d_j = vertexData[j].direction[0];
            Vec3 p_ij = (p_i+p_j)*0.5f;
            float a = frequency * (p_ij-p_i).dot(d_i);
            float b = frequency * (p_ij-p_j).dot(d_j);
            float s = d_i.dot(d_j) > 0.0f ? 1.0f : -1.0f;
            float phaseShift = s*b - a;
            edgeData[offset++] = EdgeData{ w*cosf(phaseShift), w*sinf(phaseShift), s };
        }
    }

    std::vector<float> cosFront(vertsCount), sinFront(vertsCount), cosBack(vertsCount), sinBack(vertsCount);
    for (uint32_t i = 0; i < vertsCount; ++i) {
        cosFront[i] = cosBack[i] = cosf(vertexData[i].phase[0]);
        sinFront[i] = sinBack[i] = sinf(vertexData[i].phase[0]);
    }

    for (uint32_t it = 0; it < iterations; ++it) {
        for (uint32_t i = 0; i < vertsCount; ++i) {
            if (vertexData[i].isFrozen) continue;
            float x = 0.0f, y = 0.0f;
            uint32_t offset = adj[i] - adj[0];
            for (const MeshHierarchy<VertexData>::Edge *e = adj[i]; e != adj[i+1]; ++e) {
                auto &ed = edgeData[offset++];
                float cos_j = cosFront[e->id];
                float sin_j = sinFront[e->id];
                float cos_angle = ed.weightedCosine*cos_j - ed.dotSign*ed.weightedSine*sin_j;
                float sin_angle = ed.weightedSine*cos_j + ed.dotSign*ed.weightedCosine*sin_j;
                x += cos_angle;
                y += sin_angle;
            }

            float len2 = x*x + y*y;
            if (len2 > std::numeric_limits<float>::epsilon()) {
                float inv = 1.0f/sqrtf(len2);
                cosBack[i] = x*inv;
                sinBack[i] = y*inv;
            } else {
                cosBack[i] = 1.0f;
                sinBack[i] = 0.0f;
            }
        }
        std::swap(cosFront, cosBack);
        std::swap(sinFront, sinBack);
    }

    for (uint32_t i = 0; i < vertsCount; ++i) {
        if (!vertexData[i].isFrozen) vertexData[i].phase[0] = atan2f(sinFront[i], cosFront[i]);
    }
}