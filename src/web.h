#ifdef __EMSCRIPTEN__
#pragma once
#include <emscripten.h>
#include "toolpath.h"

void setStatus(const std::string &status);
void setError(const std::string &status);
void waitLayerFields(const std::vector<Toolpath::Mesh> &layers);
std::vector<float> getLayerDirections(size_t i);

#endif