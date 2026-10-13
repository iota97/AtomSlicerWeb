// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "marchingtriangle.h"
#include <cassert>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <algorithm>

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

MarchingTriangle::MarchingTriangle(const float *vertsPositions, const uint32_t *triangleIndices, uint32_t vertsCount, uint32_t triangleCount) :
    vertsPositions(vertsPositions), triangleIndices(triangleIndices), vertsCount(vertsCount), triangleCount(triangleCount) {}

std::vector<std::vector<Vec3>> MarchingTriangle::getIsolines() const {
    return isolines;
}

void MarchingTriangle::extract(const float *vertsScalars) {
    std::unordered_multimap<Edge, Edge> edgeMap;
    for (uint32_t i = 0; i < triangleCount; ++i) {
        uint32_t i0 = triangleIndices[i*3+0], i1 = triangleIndices[i*3+1], i2 = triangleIndices[i*3+2];
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

    std::vector<Edge> isoline;
    std::vector<Vec3> isolineVerts;
    isolines.clear();
    while (!edgeMap.empty()) {
        auto current = edgeMap.begin();
        Edge first = current->first;
        Edge second = current->second;
        bool reversed = false;
        while (true) {
            isoline.push_back(current->first);
            bool found = false;
            for (auto[itr, rangeEnd] = edgeMap.equal_range(current->second); itr != rangeEnd; ++itr) {
                if (itr->second != current->first) {
                    current = itr;
                    found = true;
                    break;
                }
            }

            if (found && current->first == first) {
                isoline.push_back(current->first);
                break;
            }

            if (!found) {
                isoline.push_back(current->second);
                if (reversed) break;
                reversed = true;
                for (auto[itr, rangeEnd] = edgeMap.equal_range(first); itr != rangeEnd; ++itr) {
                    if (itr->second != second) {
                        current = itr;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
                reverse(isoline.begin(), isoline.end());
            }
        }

        for (auto &e : isoline) {
            edgeMap.erase(e);
        }

        for (uint32_t i = 0; i < isoline.size(); ++i) {
            uint32_t i0 = isoline[i].v0, i1 = isoline[i].v1;
            Vec3 p0(vertsPositions, i0), p1(vertsPositions, i1);
            float v0 = vertsScalars[i0] ? vertsScalars[i0] : 1e-8, v1 = vertsScalars[i1] ? vertsScalars[i1] : 1e-8;
            isolineVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
        }
        isolines.push_back(isolineVerts);
        isoline.clear();
        isolineVerts.clear();
    }
}

void MarchingTriangle::resample(float spacing) {
    std::vector<std::vector<Vec3>> newIsolines;
    for (auto &isoline : isolines) {
        if (isoline.empty()) continue;
        std::vector<Vec3> newIsoline;
        newIsoline.push_back(isoline[0]);
        for (size_t i = 1; i < isoline.size(); ++i) {
            float segDistance = (isoline[i]-newIsoline.back()).length();
            if (segDistance >= spacing) {
                float t = spacing/segDistance;
                Vec3 pos = t*isoline[i] + (1-t)*isoline[i-1];
                newIsoline.push_back(pos);
            }
        }
        newIsoline.push_back(isoline.back());
        newIsolines.emplace_back(std::move(newIsoline));
    }
    isolines = std::move(newIsolines);
}

void MarchingTriangle::saveToPLY(const char *path) const {
    std::vector<Vec3> plyVerts;
    std::vector<std::array<uint32_t, 2>> plyEdges;
    uint32_t curr = 0;
    for (auto &isoline : isolines) {
        bool cycle = (isoline[0] == isoline.back());
        if (cycle) {
            for (uint32_t i = 0; i < isoline.size()-1; ++i) {
                plyVerts.push_back(isoline[i]);
            }
            for (uint32_t i = 0; i < isoline.size()-2; ++i) {
                plyEdges.push_back({curr+i, (curr+i+1)});
            }
            plyEdges.push_back({curr + uint32_t(isoline.size()-2), curr});
            curr += isoline.size() - 1;
        } else {
            for (uint32_t i = 0; i < isoline.size(); ++i) {
                plyVerts.push_back(isoline[i]);
            }
            for (uint32_t i = 0; i < isoline.size()-1; ++i) {
                plyEdges.push_back({curr+i, curr+i+1});
            }
            curr += isoline.size();
        }
    }

    std::ofstream plyFile(path);
    plyFile << "ply\nformat ascii 1.0\nelement vertex " << plyVerts.size() << "\n"
            << "property float x\nproperty float y\nproperty float z\n"
            << "element edge " << plyEdges.size() << "\nproperty int vertex1\nproperty int vertex2\n"
            << "end_header\n";
    
    for (uint32_t i = 0; i < plyVerts.size(); ++i) {
        plyFile << plyVerts[i].x << " " << plyVerts[i].y << " " << plyVerts[i].z << std::endl;
    }

    for (auto &e: plyEdges) {
        plyFile << e[0] << " " << e[1] << std::endl;
    }
}