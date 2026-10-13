// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include <vector>
#include "mathutils.h"

class GeodesicIsolines {
public:
    GeodesicIsolines(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, const float *sourceSegments, uint32_t sourceSegmentsCount, float separation, float maxValue = std::numeric_limits<float>::infinity());
    void saveLinesToPLY(const char *path) const;
    const std::vector<std::vector<Vec3>> &getLines() const { return lines; };

private:
    std::vector<std::vector<Vec3>> lines;
    std::vector<uint32_t> orders;
};