// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "mathutils.h"
#include "stripe.h"
#include <vector>
#include <array>

class Isolines {
public:
    Isolines(const MeshHierarchy<VertexData> *hierarchy);

    void extract(const float *vertsScalars);
    std::vector<std::vector<Vec3>> getIsolines() const;
    void resample(float spacing);

private:
    std::vector<std::vector<Vec3>> cycles;

    const MeshHierarchy<VertexData> *hierarchy;
};