// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once

#include <vector>
#include <array>
#include "mathutils.h"

class MeshErosion {
public:
    MeshErosion(const float *vertsPosition, const uint32_t *triangles, uint32_t vertsCount, uint32_t trisCount, float width, const std::vector<std::array<Vec3, 2>> &source);
    void saveMeshToPly(const char *path);

    const std::vector<Vec3> &getVertices() const { return verts; }
    const std::vector<std::array<uint32_t, 3>> &getTris() const { return tris; }

private:
    std::vector<Vec3> verts;
    std::vector<std::array<uint32_t, 3>> tris;
};
