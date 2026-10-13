// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once
#include "mathutils.h"
#include <vector>
#include <set>

class SubdivideMesh {
public:
    SubdivideMesh(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, float maxLength);
    
    const float *getPositions() const {
        return positions.data();
    }

    const uint32_t *getFaces() const {
        return triangles.data();
    }

    uint32_t getVertCount() const {
        return positions.size()/3;
    }

    uint32_t getFaceCount() const {
        return triangles.size()/3;
    }

private:
    std::vector<float> positions;
    std::vector<uint32_t> triangles;
};