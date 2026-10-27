// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "stripe.h"
#include "stitcher.h"
#include "isolines.h"
#include "sdf.h"

class Curve {
public:
    Curve(const float *vertsPositions, uint32_t vertsCount, const uint32_t *triangleIndices, uint32_t triangleCount,
        float width, const SDF& sdf, const float *directions = nullptr, bool resample = false, bool holeClosing = false, bool quiet = false);

    const std::vector<std::vector<Vec3>> &getCycles() const;

private:
    void snapCycles(const SDF &sdf, const std::vector<std::array<Vec3, 2>> &border, float width);

    std::vector<std::vector<Vec3>> cycles;
};