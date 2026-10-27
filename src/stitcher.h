// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "stripe.h"
#include "sdf.h"

class Stitcher {
public:
    Stitcher(MeshHierarchy<VertexData> *hierarchy, std::vector<float> &scalars, const std::vector<bool> &borders, const SDF &sdf);
    
    void stitch(float period, bool holeClosing, bool showProgress = false);
    void repulse();

private:
    void floodRegion();
    void cleanIsolated();
    bool connectRegions(float period, bool showProgress);
    void computeRingNumber();
    uint32_t getRingNumber(uint32_t i);
    bool connectRegion(float period, bool positives);
    void computeMask(bool positives);
    void getConnectedComponents();
    void widthenStitch(float period, bool positives, uint32_t id);

    std::vector<float> &scalars;
    MeshHierarchy<VertexData> *hierarchy;
    std::vector<uint32_t> regionID;
    std::vector<uint32_t> ringNumber;
    std::vector<bool> mask;
    const std::vector<bool> &isBorder;
    std::vector<std::array<std::array<float, 2>, 2>> distances;
    std::vector<std::array<std::array<uint32_t, 2>, 2>> prevID;
    std::vector<std::array<uint32_t, 2>> nextHop;
    std::vector<uint32_t> pathIds;

    uint32_t vertsCount;
    uint32_t positiveRegion;
    uint32_t negativeRegion;
    uint32_t connectedRegionCount;
    uint32_t connectedRegionPositiveOnlyCount;

    const SDF *sdf = nullptr;
    bool doHoleClosing = false;
};