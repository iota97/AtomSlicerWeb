// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "marchingtriangle.h"
#include "bvhsegment.h"
#include "subdividemesh.h"
#include "geodesicisolines.h"

GeodesicIsolines::GeodesicIsolines(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, const float *sourceSegments, uint32_t sourceSegmentsCount, float separation, float maxValue) {
    SubdivideMesh meshSubdivided(vertsPosition, triangles, vertsCount, trisCount, 0.75f*separation);
    MarchingTriangle isolineExtractor(meshSubdivided.getPositions(), meshSubdivided.getFaces(), meshSubdivided.getVertCount(), meshSubdivided.getFaceCount());
    std::vector<float> distances(meshSubdivided.getVertCount());
    std::vector<bool> masked(meshSubdivided.getVertCount(), false);

    std::vector<std::array<Vec3, 2>> segments;
    for (size_t i = 0; i < sourceSegmentsCount; ++i) {
        segments.push_back({Vec3(sourceSegments, 2*i+0), Vec3(sourceSegments, 2*i+1)});
    }

    float prevIsovalue = 0.0f;
    uint32_t orderId = 0;
    for (float isovalue = 0.5f*separation; isovalue < maxValue; isovalue += separation) {
        if (segments.empty()) break;
        BVHSegment bvh(segments);

        std::vector<float> scalars(distances.size());
        bool allBigger = true;
        for (size_t i = 0; i < scalars.size(); ++i) {
            Vec3 pos(meshSubdivided.getPositions(), i);
            Vec3 closest = bvh.closestPoint(pos);
            float distance = (pos-closest).length();
        
            distances[i] = distance + prevIsovalue;
            
            if (masked[i]) {
                distances[i] = prevIsovalue;
            }

            if (distances[i] < isovalue) {
                masked[i] = true;
            }

            scalars[i] = distances[i] - isovalue;
            if (scalars[i] < 0) {
                allBigger = false;
            }
        }
    
        if (!allBigger) {
            prevIsovalue = isovalue;
            isolineExtractor.extract(scalars.data());  
            isolineExtractor.resample(0.5f*separation);
            segments.clear();
            for (auto &line : isolineExtractor.getIsolines()) {
                lines.push_back(line);
                orders.push_back(orderId);
                for (size_t i = 0; i < line.size()-1; ++i) {
                    segments.push_back({line[i], line[i+1]});
                }
            }
            orderId++;
        }
    }
}

void GeodesicIsolines::saveLinesToPLY(const char *path) const {
    std::vector<Vec3> plyVerts;
    std::vector<uint32_t> plyOrder;
    std::vector<std::array<uint32_t, 2>> plyEdges;
    uint32_t curr = 0;
    uint32_t lineIdx = 0;
    for (auto &isoline : lines) {
        bool cycle = (isoline[0] == isoline.back());
        if (cycle) {
            for (uint32_t i = 0; i < isoline.size()-1; ++i) {
                plyVerts.push_back(isoline[i]);
                plyOrder.push_back(orders[lineIdx]);
            }
            for (uint32_t i = 0; i < isoline.size()-2; ++i) {
                plyEdges.push_back({curr+i, (curr+i+1)});
            }
            plyEdges.push_back({curr + uint32_t(isoline.size()-2), curr});
            curr += isoline.size() - 1;
        } else {
            for (uint32_t i = 0; i < isoline.size(); ++i) {
                plyVerts.push_back(isoline[i]);
                plyOrder.push_back(orders[lineIdx]);
            }
            for (uint32_t i = 0; i < isoline.size()-1; ++i) {
                plyEdges.push_back({curr+i, curr+i+1});
            }
            curr += isoline.size();
        }
        lineIdx++;
    }

    std::ofstream plyFile(path);
    plyFile << "ply\nformat ascii 1.0\nelement vertex " << plyVerts.size() << "\n"
            << "property float x\nproperty float y\nproperty float z\nproperty int order\n"
            << "element edge " << plyEdges.size() << "\nproperty int vertex1\nproperty int vertex2\n"
            << "end_header\n";
    
    for (uint32_t i = 0; i < plyVerts.size(); ++i) {
        plyFile << plyVerts[i].x << " " << plyVerts[i].y << " " << plyVerts[i].z << " " << plyOrder[i] << std::endl;
    }

    for (auto &e: plyEdges) {
        plyFile << e[0] << " " << e[1] << std::endl;
    }
}