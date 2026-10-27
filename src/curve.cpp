// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "curve.h"
#include "bvhsegment.h"
#include "subdivide.h"
#include <chrono>
#include <iostream>
#include <cstring>

Curve::Curve(const float *vertsPositions, uint32_t vertsCount, const uint32_t *triangleIndices, uint32_t triangleCount,
    float width, const SDF &sdf, const float *directions, bool resample, bool holeClosing, bool quiet) {

    auto startTotalTime = std::chrono::steady_clock::now();

    Subdivide *subdividedMesh = new Subdivide(vertsPositions, directions, triangleIndices, vertsCount, triangleCount, 0.5f*width, quiet);
    vertsPositions = subdividedMesh->getPositions();
    triangleIndices = subdividedMesh->getFaces();
    vertsCount = subdividedMesh->getVertCount();
    triangleCount = subdividedMesh->getFaceCount();

    std::vector<bool> borders(vertsCount, false);
    auto borderEdges = subdividedMesh->getBorderEdges();
    for (auto &edge : borderEdges) {
        borders[edge[0]] = true;
        borders[edge[1]] = true;
    }

    std::vector<float> direction(vertsCount);
    std::memcpy(direction.data(), subdividedMesh->getScalars(), vertsCount * sizeof(float));
    directions = direction.data();

    MeshHierarchy<VertexData> *hierarchy = new MeshHierarchy<VertexData>(vertsPositions, triangleIndices, vertsCount, triangleCount);
    delete subdividedMesh;

    Stripe stripe = Stripe(hierarchy, directions, borderEdges, borders, width, quiet);

    auto startTime = std::chrono::steady_clock::now();
    std::vector<float> scalars;
    stripe.optimize(scalars);
    hierarchy->dropLods();

    if (!quiet) std::cout << "Stripes generation: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;

    startTime = std::chrono::steady_clock::now();
    hierarchy->orderAdjacency(borders);
    Stitcher *stitcher = new Stitcher(hierarchy, scalars, borders, sdf);
    if (!quiet) std::cout << "Stitcher initialization (" << vertsCount << " Verts): " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;

    startTime = std::chrono::steady_clock::now();
    stitcher->stitch(2.0f*width, holeClosing, !quiet);

    if (!quiet) std::cout << "Stitching: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;

    startTime = std::chrono::steady_clock::now();
    if (!holeClosing) {
        stitcher->repulse();
    }
    delete stitcher;
    if (!quiet) std::cout << "Repulse: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;

    startTime = std::chrono::steady_clock::now();
    Isolines *isolines = new Isolines(hierarchy);
    isolines->extract(scalars.data());
    if (resample) {
        isolines->resample(0.5f*width);
    }
    cycles = isolines->getIsolines();
    delete isolines;

    std::vector<std::array<Vec3, 2>> border;
    for (const auto &edge : borderEdges) {
        border.push_back({ hierarchy->getVertexData()[edge[0]].position, hierarchy->getVertexData()[edge[1]].position });
    }
    delete hierarchy;
    snapCycles(sdf, border, width);
    uint32_t verts = 0;
    for (auto &c : cycles) verts += c.size();
    if (!quiet) std::cout << "Isolines (" << cycles.size() << " cycle, " << verts << " verts): "
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() << " ms" << std::endl;
    if (!quiet) std::cout << "==============\nTotal time (w/o IO): "
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTotalTime).count() << " ms" << std::endl;
}

void Curve::snapCycles(const SDF &sdf, const std::vector<std::array<Vec3, 2>> &border, float width) {
    if (!border.size()) return;
    BVHSegment bvh(border);
    for (auto &cycle : cycles) {
        Sequential::For(0, cycle.size(), [this, &sdf, &cycle, &bvh, width](size_t i) {
            Vec3 pos = cycle[i];
            float sdfVal = sdf.getVal(pos);
            float targetDist = width*0.5f;
            float margin = 0.3f*width;
            if (fabsf(sdfVal + targetDist) < margin) {
                Vec3 closest = bvh.closestPoint(pos);
                Vec3 dir = pos-closest;
                if (fabsf(dir.length()-targetDist) < margin) {
                    Vec3 newPos = closest + dir.normalize() * targetDist;
                    cycle[i] = newPos;
                }
            }
        });
    }
}

const std::vector<std::vector<Vec3>> &Curve::getCycles() const {
    return cycles;
}