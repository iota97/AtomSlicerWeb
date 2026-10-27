// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "isolines.h"
#include <cassert>
#include <unordered_map>
#include <bitset>
#include <fstream>
#include <iostream>

struct Edge {
    uint32_t v0, v1;
    Edge(uint32_t v0, uint32_t v1) : v0(v0), v1(v1) {}
    bool operator==(const Edge &o) const { return v0 == o.v0 && v1 == o.v1; }
    bool operator!=(const Edge &o) const { return v0 != o.v0 || v1 != o.v1; }
};

template <>
struct std::hash<Edge> {
    std::size_t operator()(const Edge& k) const {
        return (std::hash<uint32_t>()(k.v0)^(std::hash<uint32_t>()(k.v1) << 1)) >> 1;
    }
};

Isolines::Isolines(const MeshHierarchy<VertexData> *hierarchy) : hierarchy(hierarchy) {}

std::vector<std::vector<Vec3>> Isolines::getIsolines() const {
    return cycles;
}

void Isolines::extract(const float *vertsScalars) {
    std::unordered_multimap<Edge, Edge> edgeMap;
    for (uint32_t i = 0; i < hierarchy->getTrisCount(); ++i) {
        uint32_t i0 = hierarchy->getTris()[i*3+0], i1 = hierarchy->getTris()[i*3+1], i2 = hierarchy->getTris()[i*3+2];
        float v0 = vertsScalars[i0], v1 = vertsScalars[i1], v2 = vertsScalars[i2];
        uint8_t mask = ((v0 >= 0) << 2) | ((v1 >= 0) << 1) | ((v2 >= 0) << 0);
        switch (mask) {
            case 0b001:
            case 0b110:
                edgeMap.insert({{std::min(i0, i2), std::max(i0, i2)}, {std::min(i1, i2), std::max(i1, i2)}});
                edgeMap.insert({{std::min(i1, i2), std::max(i1, i2)}, {std::min(i0, i2), std::max(i0, i2)}});
                break;
            case 0b010:
            case 0b101:
                edgeMap.insert({{std::min(i0, i1), std::max(i0, i1)}, {std::min(i1, i2), std::max(i1, i2)}});
                edgeMap.insert({{std::min(i1, i2), std::max(i1, i2)}, {std::min(i0, i1), std::max(i0, i1)}});
                break;
            case 0b011:
            case 0b100:
                edgeMap.insert({{std::min(i0, i1), std::max(i0, i1)}, {std::min(i0, i2), std::max(i0, i2)}});
                edgeMap.insert({{std::min(i0, i2), std::max(i0, i2)}, {std::min(i0, i1), std::max(i0, i1)}});
                break;
            default:
                break;
        }
    }

    std::vector<Edge> cycle;
    std::vector<Vec3> cycleVerts;
    cycles.clear();
    while (true) {
        if (edgeMap.empty())
            break;

        auto current = edgeMap.begin();
        Edge first = current->first;
        while (true) {
            cycle.push_back(current->first);
            for (auto[itr, rangeEnd] = edgeMap.equal_range(current->second); itr != rangeEnd; ++itr) {
                if (itr->second != current->first) {
                    current = itr;
                    break;
                }
            }

            if (current->first == first) {
                break;
            }
        }
        for (auto &e : cycle) {
            edgeMap.erase(e);
        }

        for (uint32_t i = 0; i < cycle.size(); ++i) {
            uint32_t i0 = cycle[i].v0, i1 = cycle[i].v1;
            Vec3 p0 = hierarchy->getVertexData()[i0].position;
            Vec3 p1 = hierarchy->getVertexData()[i1].position;
            float v0 = vertsScalars[i0] ? vertsScalars[i0] : 1e-8, v1 = vertsScalars[i1] ? vertsScalars[i1] : 1e-8;
            cycleVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
        }
        cycles.push_back(cycleVerts);
        cycle.clear();
        cycleVerts.clear();
    }
}

struct GridCell {
    int32_t x, y, z;
    bool operator==(const GridCell &o) const { return x == o.x && y == o.y && z == o.z; };
};
template <>
struct std::hash<GridCell> {
    size_t operator()(const GridCell& k) const {
        return ((std::hash<int32_t>()(k.x)
                ^ (hash<int32_t>()(k.y) << 1)) >> 1)
                ^ (hash<int32_t>()(k.z) << 1);
    }
};

void Isolines::resample(float spacing) {
    std::vector<std::vector<Vec3>> newCycles;
    for (auto &cycle : cycles) {
        if (cycle.size() < 2) continue;
        std::vector<Vec3> newCycle;
        newCycle.push_back(cycle[1]);
        for (size_t i = 1; i < cycle.size(); ++i) {
            float segDistance = (cycle[i]-newCycle.back()).length();
            if (segDistance >= spacing) {
                float t = spacing/segDistance;
                Vec3 pos = t*cycle[i] + (1-t)*cycle[i-1];
                newCycle.push_back(pos);
            }
        }
        newCycles.emplace_back(std::move(newCycle));
    }
    cycles = std::move(newCycles);
}
