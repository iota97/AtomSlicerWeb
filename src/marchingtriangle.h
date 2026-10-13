// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "mathutils.h"
#include <vector>
#include <array>

class MarchingTriangle {
public:
    MarchingTriangle(const float *vertsPositions, const uint32_t *triangleIndices, uint32_t vertsCount, uint32_t triangleCount);

    void extract(const float *vertsScalars);
    void resample(float spacing);

    std::vector<std::vector<Vec3>> getIsolines() const;
    void saveToPLY(const char *path) const;

private:
    std::vector<std::vector<Vec3>> isolines;
    const float *vertsPositions;
    const uint32_t *triangleIndices;
    uint32_t vertsCount;
    uint32_t triangleCount;      
};