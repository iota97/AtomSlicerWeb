// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#pragma once

#include <functional>

namespace Sequential {

inline void For(size_t start, size_t stop, std::function<void(size_t)> foo) {
    for (size_t i = start; i < stop; ++i) {
        foo(i);
    }
}

inline std::array<bool, 2> ForAny2(size_t start, size_t stop, std::function<std::array<bool, 2>(size_t)> foo) {
    std::array<bool, 2> result{false, false};

    for (size_t i = start; i < stop; ++i) {
        auto val = foo(i);
        result[0] |= val[0];
        result[1] |= val[1];
    }

    return result;
}

inline size_t ArgMin(size_t start, size_t stop, std::function<float(size_t)> foo) {
    float val = std::numeric_limits<float>::infinity();
    size_t id = 0;

    for (size_t i = start; i < stop; ++i) {
        float newVal = foo(i);
        if (newVal < val) {
            val = newVal;
            id = i;
        }
    }

    return id;
}

}