// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "mesherosion.h"

#include "bvhsegment.h"
#include "bvhtriangle.h"
#include "subdividemesh.h"
#include <map>
#include <fstream>
#include <iostream>

MeshErosion::MeshErosion(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, float width, const std::vector<std::array<Vec3, 2>> &source) {
    BVHSegment bvh(source);
    SubdivideMesh meshSubdivided(vertsPosition, triangles, vertsCount, trisCount, width);

    vertsPosition = meshSubdivided.getPositions();
    triangles = meshSubdivided.getFaces();
    trisCount = meshSubdivided.getFaceCount();
    vertsCount = meshSubdivided.getVertCount();
    
    std::vector<float> vertsScalars(vertsCount);
    std::vector<Vec3> newVerts(vertsCount);
    for (uint32_t i = 0; i < vertsCount; ++i) {
        Vec3 pos(meshSubdivided.getPositions(), i);
        Vec3 closest = bvh.closestPoint(pos);
        float distance = (pos-closest).length();
        vertsScalars[i] = source.size() ? distance - width : 0.0f;
        newVerts[i] = Vec3(vertsPosition, i);
    }

    struct Edge {
        uint32_t v0, v1;
        Edge(uint32_t v0, uint32_t v1) : v0(v0 < v1 ? v0 : v1), v1(v0 < v1 ? v1 : v0) {}
        bool operator==(const Edge &o) const { return v0 == o.v0 && v1 == o.v1; }
        bool operator<(const Edge &o) const { if (v0 < o.v0) return true; if (o.v0 < v0) return false; return v1 < o.v1; }
    };
    std::map<Edge, uint32_t> edgeIds;

    for (uint32_t i = 0; i < trisCount; ++i) {
        uint32_t i0 = triangles[i*3+0], i1 = triangles[i*3+1], i2 = triangles[i*3+2];
        float v0 = vertsScalars[i0], v1 = vertsScalars[i1], v2 = vertsScalars[i2];
        Vec3 p0 = Vec3(vertsPosition, i0), p1 = Vec3 (vertsPosition, i1), p2 = Vec3 (vertsPosition, i2);

        switch (((v0 >= 0) << 2) | ((v1 >= 0) << 1) | ((v2 >= 0) << 0)) {
            case 0b111:
                tris.push_back({i0, i1, i2});
                break;
            case 0b001:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({edgeIds[{i1, i2}], i2, edgeIds[{i0, i2}]});
                break;
            case 0b010:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p2-v2/(v1-v2)*(p1-p2));
                }
                tris.push_back({edgeIds[{i0, i1}], i1, edgeIds[{i1, i2}]});
                break;
            case 0b100:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v0-v1)*(p0-p1));
                }
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p2-v2/(v0-v2)*(p0-p2));
                }
                tris.push_back({i0, edgeIds[{i0, i1}], edgeIds[{i0, i2}]});
                break;
            case 0b110:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({edgeIds[{i1, i2}], edgeIds[{i0, i2}], i0});
                tris.push_back({edgeIds[{i1, i2}], i0, i1});
                break;
            case 0b101:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({i0, edgeIds[{i0, i1}], i2});
                tris.push_back({edgeIds[{i0, i1}], edgeIds[{i1, i2}], i2});
                break;
            case 0b011:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                tris.push_back({edgeIds[{i0, i1}], i1, i2});
                tris.push_back({edgeIds[{i0, i1}], i2, edgeIds[{i0, i2}]});
                break;
            default:
                break;
        }
    }

    std::unordered_map<size_t, size_t> indexMap;
    for (const auto &tris : tris) {
        for (uint8_t i = 0; i < 3; ++i) {
            size_t oldIdx = tris[i];
            if (indexMap.find(oldIdx) == indexMap.end()) {
                indexMap[oldIdx] = verts.size();
                verts.push_back(newVerts[oldIdx]);
            }
        }
    }

    for (auto &tris : tris) {
        tris[0] = indexMap[tris[0]];
        tris[1] = indexMap[tris[1]];
        tris[2] = indexMap[tris[2]];
    }
}

void MeshErosion::saveMeshToPly(const char *path) {
    std::ofstream plyFile(path);
    plyFile << "ply\nformat ascii 1.0\nelement vertex " << verts.size() << "\n"
        << "property float x\nproperty float y\nproperty float z\n"
        << "element face " << tris.size() << "\nproperty list uchar uint vertex_indices\n"
        << "end_header\n";

    for (size_t i = 0; i < verts.size(); ++i) {
        plyFile << verts[i].x << " " << verts[i].y << " " << verts[i].z << std::endl;
    }

    for (size_t i = 0; i < tris.size(); ++i) {
        plyFile << "3 " << tris[i][0] << " " << tris[i][1] << " " << tris[i][2] << std::endl;
    }
}

MeshErosion::MeshErosion(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, const SDF &sdf, const SDF &sdfOriginal, const std::vector<std::array<Vec3, 3>> &meshTriangles) {
    std::vector<float> vertsScalars(vertsCount);
    std::vector<Vec3> newVerts(vertsCount);

    BVHTriangle bvhTris(meshTriangles);
    Vec3 minAABB, maxAABB;
    bvhTris.getAABB(minAABB, maxAABB);

    for (uint32_t i = 0; i < vertsCount; ++i) {
        Vec3 point(vertsPosition, i);
        float sdfVal = sdfOriginal.getValTrilinear(point);

        if (sdfVal < -2*sdf.getCellSize()) {
            vertsScalars[i] = -sdf.getValTrilinear(point);
        } else {
            const uint32_t raysCount = 5;
            Vec3 pos = point + minAABB;
            Vec3 closest = bvhTris.closestPoint(pos);
            Vec3 delta = pos - closest;
            uint32_t outside = 0;
            uint32_t seed = i;
            for (uint32_t j = 0; j < raysCount; ++j) {
                outside += bvhTris.intersectionCount(pos, randomDirectionFromUint(seed)) % 2;
                seed = randomUintFromUint(seed);
            }
            float sign = outside > raysCount/2 ? -1 : 1;
            vertsScalars[i] = -sign * delta.length();
        }
        newVerts[i] = Vec3(vertsPosition, i);
    }

    struct Edge {
        uint32_t v0, v1;
        Edge(uint32_t v0, uint32_t v1) : v0(v0 < v1 ? v0 : v1), v1(v0 < v1 ? v1 : v0) {}
        bool operator==(const Edge &o) const { return v0 == o.v0 && v1 == o.v1; }
        bool operator<(const Edge &o) const { if (v0 < o.v0) return true; if (o.v0 < v0) return false; return v1 < o.v1; }
    };
    std::map<Edge, uint32_t> edgeIds;

    for (uint32_t i = 0; i < trisCount; ++i) {
        uint32_t i0 = triangles[i*3+0], i1 = triangles[i*3+1], i2 = triangles[i*3+2];
        float v0 = vertsScalars[i0], v1 = vertsScalars[i1], v2 = vertsScalars[i2];
        Vec3 p0 = Vec3(vertsPosition, i0), p1 = Vec3 (vertsPosition, i1), p2 = Vec3 (vertsPosition, i2);

        switch (((v0 >= 0) << 2) | ((v1 >= 0) << 1) | ((v2 >= 0) << 0)) {
            case 0b111:
                tris.push_back({i0, i1, i2});
                break;
            case 0b001:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({edgeIds[{i1, i2}], i2, edgeIds[{i0, i2}]});
                break;
            case 0b010:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p2-v2/(v1-v2)*(p1-p2));
                }
                tris.push_back({edgeIds[{i0, i1}], i1, edgeIds[{i1, i2}]});
                break;
            case 0b100:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v0-v1)*(p0-p1));
                }
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p2-v2/(v0-v2)*(p0-p2));
                }
                tris.push_back({i0, edgeIds[{i0, i1}], edgeIds[{i0, i2}]});
                break;
            case 0b110:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({edgeIds[{i1, i2}], edgeIds[{i0, i2}], i0});
                tris.push_back({edgeIds[{i1, i2}], i0, i1});
                break;
            case 0b101:
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                if (!edgeIds.count({i1, i2})) {
                    edgeIds[{i1, i2}] = newVerts.size();
                    newVerts.push_back(p1-v1/(v2-v1)*(p2-p1));
                }
                tris.push_back({i0, edgeIds[{i0, i1}], i2});
                tris.push_back({edgeIds[{i0, i1}], edgeIds[{i1, i2}], i2});
                break;
            case 0b011:
                if (!edgeIds.count({i0, i2})) {
                    edgeIds[{i0, i2}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v2-v0)*(p2-p0));
                }
                if (!edgeIds.count({i0, i1})) {
                    edgeIds[{i0, i1}] = newVerts.size();
                    newVerts.push_back(p0-v0/(v1-v0)*(p1-p0));
                }
                tris.push_back({edgeIds[{i0, i1}], i1, i2});
                tris.push_back({edgeIds[{i0, i1}], i2, edgeIds[{i0, i2}]});
                break;
            default:
                break;
        }
    }

    std::unordered_map<size_t, size_t> indexMap;
    for (const auto &tris : tris) {
        for (uint8_t i = 0; i < 3; ++i) {
            size_t oldIdx = tris[i];
            if (indexMap.find(oldIdx) == indexMap.end()) {
                indexMap[oldIdx] = verts.size();
                verts.push_back(newVerts[oldIdx]);
            }
        }
    }

    for (auto &tris : tris) {
        tris[0] = indexMap[tris[0]];
        tris[1] = indexMap[tris[1]];
        tris[2] = indexMap[tris[2]];
    }
}
