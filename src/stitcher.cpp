// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#include "stitcher.h"
#include <memory.h>
#include <iostream>
#include <cassert>
#include <map>
#include <set>
#include <unordered_set>
#include <deque>

Stitcher::Stitcher(MeshHierarchy<VertexData> *hierarchy, std::vector<float> &scalars, const std::vector<bool> &borders, const SDF &sdf) : scalars(scalars), hierarchy(hierarchy), vertsCount(hierarchy->getVertsCount()), isBorder(borders), sdf(&sdf) {}

void Stitcher::getConnectedComponents() {
    std::unordered_set<uint32_t> unexplored;
    for (uint32_t i = 0; i < vertsCount; ++i) {
        unexplored.insert(i);
    }
    std::vector<uint32_t> stack;
    uint32_t label = 0;
    connectedRegionPositiveOnlyCount = 0;
    while (!unexplored.empty()) {
        bool negative = false;
        uint32_t i = *unexplored.begin();
        unexplored.erase(i);
        stack.push_back(i);
        if (scalars[i] < 0) negative = true;
        while (!stack.empty()) {
            i = stack[stack.size()-1];
            stack.pop_back();
            for (auto n : hierarchy->getNeighborhood(i)) {
                uint32_t j = n.id;
                if (unexplored.count(j)) {
                    unexplored.erase(j);
                    stack.push_back(j);
                    if (scalars[j] < 0) negative = true;
                }
            }
        }
        label++;
        if (!negative) {
            connectedRegionPositiveOnlyCount++;
        }
    }
    connectedRegionCount = label - connectedRegionPositiveOnlyCount;
}

void Stitcher::floodRegion() {
    std::unordered_set<uint32_t> unexplored;
    for (uint32_t i = 0; i < vertsCount; ++i) {
        unexplored.insert(i);
    }
    std::vector<uint32_t> stack;
    uint32_t label = 0;
    positiveRegion = 0;
    negativeRegion = 0;
    while (!unexplored.empty()) {
        uint32_t i = *unexplored.begin();
        unexplored.erase(i);
        stack.push_back(i);
        regionID[i] = label;
        if (scalars[i] >= 0) {
            positiveRegion++;
        } else {
            negativeRegion++;
        }
        while (!stack.empty()) {
            i = stack[stack.size()-1];
            stack.pop_back();
            for (auto n : hierarchy->getNeighborhood(i)) {
                uint32_t j = n.id;
                if (unexplored.count(j) && ((scalars[i] >= 0 && scalars[j] >= 0) || (scalars[i] < 0 && scalars[j] < 0))) {
                    unexplored.erase(j);
                    stack.push_back(j);
                    regionID[j] = label;
                }
            }
        }
        label++;
    }
    positiveRegion -= connectedRegionPositiveOnlyCount;
}

void Stitcher::stitch(float period, bool holeClosing, bool showProgress) {
    regionID.resize(vertsCount);
    distances.resize(vertsCount);
    prevID.resize(vertsCount);
    nextHop.resize(vertsCount);
    ringNumber.resize(vertsCount);
    mask.resize(vertsCount);

    doHoleClosing = holeClosing;
    cleanIsolated();
    bool success = false;
    bool first = true;
    while (!success) {
        getConnectedComponents();
        floodRegion();
        if (first && showProgress) {
            first = false;
            std::cout << "Connected components having cycles: " << connectedRegionCount << std::endl;
            std::cout << "Regions: " << positiveRegion+negativeRegion << " (" << positiveRegion << " pos, " << negativeRegion << " neg)" << std::endl;
        }
        if (!connectedRegionCount) {
            break;
        }
        success = connectRegions(period, showProgress);
        if (!success) {
            return;
        }
    }
}

void Stitcher::repulse() {
    computeRingNumber();
    Sequential::For(0, vertsCount, [this](size_t i) {
        if (!isBorder[i] && ringNumber[i] >= 4) {
            scalars[i] = scalars[i] >= 0 ? 1e3 : -1e3;
        }
    });
}

void Stitcher::cleanIsolated() {
    computeRingNumber();
    for (uint32_t i = 0; i < vertsCount; ++i) {
        if (hierarchy->getNeighborhood(i).begin() != hierarchy->getNeighborhood(i).end()) {
            uint32_t j = hierarchy->getNeighborhood(i).begin()->id;
            bool sameSign = (scalars[i] >= 0 && scalars[j] >= 0) || (scalars[i] < 0 && scalars[j] < 0);
            if (ringNumber[i] == 0 && !isBorder[i] && !sameSign) {
                scalars[i] = scalars[j];
            }
        }
    }
}

bool Stitcher::connectRegions(float period, bool showProgress) {
    uint32_t parity = 0;
    float total = positiveRegion + negativeRegion - 2*connectedRegionCount;

    while (positiveRegion > connectedRegionCount || negativeRegion > connectedRegionCount) {
        bool positive = (++parity) % 2;
        if (positiveRegion == connectedRegionCount) positive = false;
        if (negativeRegion == connectedRegionCount) positive = true;
            
        bool success = connectRegion(period, positive);
        if (!success) {
            return false;
        }

        if (positive) {
            --positiveRegion;
        } else {
            --negativeRegion;
        }
        if (showProgress) {
            printProgress(1.0f-(positiveRegion+negativeRegion-2*connectedRegionCount)/total);
        }
    }
    return true;
}

uint32_t Stitcher::getRingNumber(uint32_t i) {
    uint32_t ring = 0;
    bool first = true, start = true, next = true, prev = true;
    for (auto n : hierarchy->getNeighborhood(i)) {
        uint32_t j = n.id;
        next = scalars[j] >= 0;
        if (first) {
            first = false;
            start = next;
            prev = next;
        } else {
            if (next != prev) ++ring;
            prev = next;
        }
        }
    if (prev != start) ++ring;
    return ring;
}

void Stitcher::computeRingNumber() {
    Sequential::For(0, vertsCount, [this](size_t i) {
        ringNumber[i] = getRingNumber(i);
    });
}

void Stitcher::computeMask(bool positives) {
    auto test = [positives](float f) { return positives ? f < 0 : f >= 0; };
    computeRingNumber();

    Sequential::For(0, vertsCount, [this, &test](size_t i) {
        if (!test(scalars[i]) || isBorder[i] || ringNumber[i] > 4) {
            mask[i] = 1;
            return;
        }

        if (ringNumber[i] == 4) {
            mask[i] = 1;
            uint32_t region = uint32_t(-1);
            for (auto n : hierarchy->getNeighborhood(i)) {
                uint32_t j = n.id;
                if (!test(scalars[j])) {
                    if (region == uint32_t(-1)) {
                        region = regionID[j];
                    } else if (region != regionID[j]) {
                        mask[i] = 0;
                        break;
                    }
                }
            }
        } else {
            mask[i] = 0;
        }
    });
}


bool Stitcher::connectRegion(float period, bool positives) {
    auto test = [positives](float f) { return positives ? f < 0 : f >= 0; };
    computeMask(positives);

    Sequential::For(0, vertsCount, [this, &test](size_t i) {
        if (test(scalars[i])) {
            distances[i][0][0] = std::numeric_limits<float>::infinity();
            distances[i][1][0] = std::numeric_limits<float>::infinity();
            distances[i][0][1] = std::numeric_limits<float>::infinity();
            distances[i][1][1] = std::numeric_limits<float>::infinity();
            prevID[i][0][0] = uint32_t(-1);
            prevID[i][1][0] = uint32_t(-1);
            prevID[i][0][1] = uint32_t(-1);
            prevID[i][1][1] = uint32_t(-1);
        } else {
            distances[i][0][0] = 0.0f;
            distances[i][1][0] = 0.0f;
            distances[i][0][1] = 0.0f;
            distances[i][1][1] = 0.0f;
            prevID[i][0][0] = regionID[i];
            prevID[i][1][0] = regionID[i];
            prevID[i][0][1] = regionID[i];
            prevID[i][1][1] = regionID[i];
        }
    });

    for (uint32_t iter = 0; iter < 4; ++iter) {
        auto found = Sequential::ForAny2(0, vertsCount, [this, iter, test](size_t i) {
            if (!mask[i]) {
                bool change = false;
                distances[i][0][1-iter%2] = distances[i][0][iter%2];
                distances[i][1][1-iter%2] = distances[i][1][iter%2];
                prevID[i][0][1-iter%2] = prevID[i][0][iter%2];
                prevID[i][1][1-iter%2] = prevID[i][1][iter%2];
                for (auto n : hierarchy->getNeighborhood(i)) {
                    uint32_t j = n.id;
                    Vec3 p_i = hierarchy->getVertexData()[i].position;
                    Vec3 p_j = hierarchy->getVertexData()[j].position;
                    float dist_ij = test(scalars[j]) ? (p_i-p_j).length() : 0.0f;
                    if (distances[i][0][1-iter%2] > distances[j][0][iter%2] + dist_ij) {
                        if (prevID[i][0][1-iter%2] != prevID[j][0][iter%2]) {
                            distances[i][1][1-iter%2] = distances[i][0][1-iter%2];
                            prevID[i][1][1-iter%2] = prevID[i][0][1-iter%2];
                            nextHop[i][1] = nextHop[i][0];
                        }
                        distances[i][0][1-iter%2] = distances[j][0][iter%2] + dist_ij;
                        prevID[i][0][1-iter%2] = prevID[j][0][iter%2];
                        nextHop[i][0] = j;
                        change = true;
                    } else if (prevID[i][0][1-iter%2] != prevID[j][0][iter%2] && distances[i][1][1-iter%2] > distances[j][0][iter%2] + dist_ij) {
                        distances[i][1][1-iter%2] = distances[j][0][iter%2] + dist_ij;
                        prevID[i][1] = prevID[j][0];
                        nextHop[i][1] = j;
                        change = true;
                    } else if (prevID[i][0][1-iter%2] != prevID[j][1][iter%2] && distances[i][1][1-iter%2] > distances[j][1][iter%2] + dist_ij) {
                        distances[i][1][1-iter%2] = distances[j][1][iter%2] + dist_ij;
                        prevID[i][1][1-iter%2] = prevID[j][1][iter%2];
                        nextHop[i][1] = j;
                        change = true;
                    }
                }
                return std::array<bool, 2>{!std::isinf(distances[i][1][1-iter%2]), change};
            }
            return std::array<bool, 2>{false, false};
        });

        if (!found[0]) {
            iter %= 2;
            if (!found[1]) {
                if (positives) {
                    positiveRegion = connectedRegionCount+1;
                    return false;
                } else {
                    negativeRegion = connectedRegionCount+1;
                    return false;
                }
            }
        }
    }

    uint32_t startId = Sequential::ArgMin(0, vertsCount, [this, test](size_t i) {
        return test(scalars[i]) ? distances[i][0][0] + distances[i][1][0] : std::numeric_limits<float>::infinity();
    });

    uint32_t curr = startId;
    while (test(scalars[curr])) {
        curr = nextHop[curr][0];
    } 
    uint32_t closeID = prevID[curr][0][0];

    pathIds.clear();
    curr = startId;
    while (test(scalars[curr])) {
        scalars[curr] = positives ? 1.0f : -1.0f;
        pathIds.push_back(curr);
        regionID[curr] = closeID;
        curr = nextHop[curr][0];
    } 
    scalars[curr] = positives ? 1.0f : -1.0f;

    curr = nextHop[startId][1];
    while (test(scalars[curr])) {
        scalars[curr] = positives ? 1.0f : -1.0f;
        pathIds.push_back(curr);
        regionID[curr] = closeID;
        curr = prevID[curr][0][0] != closeID ? nextHop[curr][0] : nextHop[curr][1];
    } 
    scalars[curr] = positives ? 1.0f : -1.0f;
    uint32_t farID = prevID[curr][0][0];

    Sequential::For(0, vertsCount, [this, farID, closeID](size_t i) {
        if (regionID[i] == farID) {
            regionID[i] = closeID;
        }
        distances[i][0][0] = std::numeric_limits<float>::infinity();
    });

    widthenStitch(period, positives, closeID);
    return true;
}

void Stitcher::widthenStitch(float period, bool positives, uint32_t id) {
    std::deque<uint32_t> queue;
    std::unordered_set<uint32_t> toRetry;
    std::unordered_set<uint32_t> toRetryDeleted;
    std::unordered_set<uint32_t> explored;
    std::unordered_set<uint32_t> frontier;
    float width = 0.5f*period;

    for (auto i : pathIds) {
        Vec3 normal = hierarchy->getVertexData()[i].normal;
        Vec3 pos = hierarchy->getVertexData()[i].position;
        bool border = doHoleClosing && (sdf->getVal(pos) > -width && fabsf(sdf->getGrad(pos).dot(normal)) < 0.7);
        distances[i][0][0] = border ? width : 0.0f;
        for (auto n : hierarchy->getNeighborhood(i)) {
            uint32_t j = n.id;
            queue.push_back(j);
            explored.emplace(j);
            distances[j][0][0] = distances[i][0][0] + (pos-hierarchy->getVertexData()[j].position).length();
        }
    }
    while (!queue.empty()) {
        uint32_t i = queue.front();
        queue.pop_front();
        if (!isBorder[i]) {
            if (getRingNumber(i) == 2) {
                regionID[i] = id;
                scalars[i] = positives ? 1.0f : -1.0f;
                Vec3 pos = hierarchy->getVertexData()[i].position;
                for (auto n : hierarchy->getNeighborhood(i)) {
                    uint32_t j = n.id;
                    distances[j][0][0] = std::min(distances[j][0][0], distances[i][0][0] + (pos-hierarchy->getVertexData()[j].position).length());
                    if (distances[j][0][0] < 0.5f*width) {
                        if (!explored.count(j)) {
                            queue.push_back(j);
                            explored.emplace(j);
                        }
                    }
                    if (!isBorder[j] && !frontier.count(j)) {
                        frontier.emplace(j);
                    }
                }
            } else {
                toRetry.emplace(i);
            }
        }

        if (queue.empty()) {
            for (uint32_t j : toRetry) {
                if (getRingNumber(j) == 2) {
                    queue.push_back(j);
                    toRetryDeleted.insert(j);
                }
            }
            for (uint32_t j : toRetryDeleted) {
                toRetry.erase(j);
            }
            toRetryDeleted.clear();
        }
    }
    
    for (auto i : frontier) {
        if (getRingNumber(i) == 2) {
            float weigths = 0;
            float val = 0;
            for (auto n : hierarchy->getNeighborhood(i)) {
                val += n.weight*scalars[n.id];
                weigths += n.weight;
            }
            scalars[i] = weigths == 0 ? scalars[i] : val/weigths;

            for (auto n : hierarchy->getNeighborhood(i)) {
                uint32_t j = n.id;
                if ((scalars[i] >= 0 && scalars[j] >= 0) || (scalars[i] < 0 && scalars[j] < 0)) {
                    regionID[i] = regionID[j];
                    break;
                }
            }
        }
    }
}