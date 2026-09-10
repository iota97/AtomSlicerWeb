// MIT License
// Copyright 2026 Giovanni Cocco and Inria

#ifdef __EMSCRIPTEN__
#include "slicer.h"
#include "mathutils.h"
#include "web.h"
#include "stlloader.h"
#include <thread>
#include <emscripten.h>
#include <mutex>
#include <condition_variable>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

enum class UI_STATE {
    Start,
    Slicing,
    Waiting,
    Done,
};

static StlLoader *mesh = nullptr;
static Slicer *slicer = nullptr;
static std::vector<float> waypointBuffer;
static std::vector<float> layerBuffer;
static std::vector<float> layerDirections;
static std::vector<bool> layerFromImage;
static UI_STATE uiState = UI_STATE::Start;
static float s_nozzleWidth;
static float s_waypointCount;
static float s_layerCount;
static std::mutex s_layerMutex;
static std::condition_variable s_layerCondition;
static bool s_layerResumed = false;
static uint32_t s_zigzagOffset = 0;
static float s_idleTime = 0;
static const std::vector<Toolpath::Mesh> *s_layerPtr;
static std::vector<std::vector<float>> s_layerDir;
static float s_meshScale;
static float s_imageOffsetX;
static float s_imageOffsetY;

std::vector<float> getLayerDirections(size_t i) {
    if (layerDirections.empty()) {
        return std::vector<float>();
    } else if (!layerFromImage[i]) {
        return std::vector<float>((*s_layerPtr)[i].vertices.size()/3, layerDirections[i]/180.0*M_PI);
    } else {
        return s_layerDir[i];
    }
}

void addAnisotropicSphere(std::vector<Vec3> &vertices, std::vector<Vec3> &normals, std::vector<float> &pointIdx, const Vec3 &center, const Vec3 &normal, float idx, float radius, float halfRadius, uint32_t resolution) {
    Vec3 n = normal.normalize();
    Vec3 ref = abs(n.x) < 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    Vec3 side = n.cross(ref).normalize();
    Vec3 up = n.cross(side).normalize();

    uint32_t rings = resolution / 2;

    for (uint32_t i = 0; i < rings; ++i) {
        float p0 = M_PI * i / rings;
        float p1 = M_PI * (i + 1) / rings;

        for (uint32_t j = 0; j < resolution; ++j) {
            float a0 = 2.0f * M_PI * j / resolution;
            float a1 = 2.0f * M_PI * (j + 1) / resolution;

            Vec3 a = center + n * (halfRadius * cos(p0)) + side * (radius * sin(p0) * cos(a0)) + up * (radius * sin(p0) * sin(a0));
            Vec3 b = center + n * (halfRadius * cos(p0)) + side * (radius * sin(p0) * cos(a1)) + up * (radius * sin(p0) * sin(a1));
            Vec3 c = center + n * (halfRadius * cos(p1)) + side * (radius * sin(p1) * cos(a1)) + up * (radius * sin(p1) * sin(a1));
            Vec3 d = center + n * (halfRadius * cos(p1)) + side * (radius * sin(p1) * cos(a0)) + up * (radius * sin(p1) * sin(a0));

            Vec3 normal1 = -(b - a).cross(c - a).normalize();
            Vec3 normal2 = -(c - a).cross(d - a).normalize();

            vertices.push_back(a);
            vertices.push_back(b);
            vertices.push_back(c);
            normals.push_back(normal1);
            normals.push_back(normal1);
            normals.push_back(normal1);
            pointIdx.push_back(idx);
            pointIdx.push_back(idx);
            pointIdx.push_back(idx);

            vertices.push_back(a);
            vertices.push_back(c);
            vertices.push_back(d);
            normals.push_back(normal2);
            normals.push_back(normal2);
            normals.push_back(normal2);
            pointIdx.push_back(idx);
            pointIdx.push_back(idx);
            pointIdx.push_back(idx);
        }
    }
}

size_t createToolpathTube(const std::vector<Toolpath::WayPoint> &toolpath, float radius, uint32_t resolution, std::vector<Vec3> &vertices, std::vector<Vec3> &normals, std::vector<float> &pointIdx) {
    vertices.clear();
    normals.clear();
    pointIdx.clear();
    float idx = 0;

    if (toolpath.size() < 2)
        return 0;

    const float halfRadius = radius * 0.5f;

    std::vector<Vec3> rings;

    size_t i = 0;

    while (i < toolpath.size()) {
        while (i < toolpath.size() && !toolpath[i].deposition)
            ++i;

        if (i >= toolpath.size())
            break;

        size_t start = i;

        while (i + 1 < toolpath.size() && toolpath[i + 1].deposition)
            ++i;

        size_t end = i;

        if (end > start) {
            rings.clear();

            for (size_t k = start; k <= end; ++k) {
                Vec3 tangent;

                if (k == start) {
                    tangent = (toolpath[k + 1].position - toolpath[k].position).normalize();
                } else if (k == end) {
                    tangent = (toolpath[k].position - toolpath[k - 1].position).normalize();
                } else {
                    tangent = (toolpath[k + 1].position - toolpath[k - 1].position).normalize();
                }

                Vec3 n = toolpath[k].normal.normalize();
                Vec3 side = tangent.cross(n).normalize();

                for (uint32_t j = 0; j < resolution; ++j) {
                    float a = 2.0f * M_PI * j / resolution;

                    Vec3 offset = n * (halfRadius * cos(a));
                    offset = offset + side * (radius * sin(a));

                    rings.push_back(toolpath[k].position - halfRadius*toolpath[k].normal + offset);
                }
            }

            float startIdx = idx;
            for (size_t k = 0; k < end - start; ++k) {
                for (uint32_t j = 0; j < resolution; ++j) {
                    uint32_t next = (j + 1) % resolution;

                    const Vec3 &a = rings[k * resolution + j];
                    const Vec3 &b = rings[k * resolution + next];
                    const Vec3 &c = rings[(k + 1) * resolution + next];
                    const Vec3 &d = rings[(k + 1) * resolution + j];

                    Vec3 normal1 = (b - a).cross(c - a).normalize();
                    Vec3 normal2 = (c - a).cross(d - a).normalize();

                    vertices.push_back(a);
                    vertices.push_back(b);
                    vertices.push_back(c);
                    normals.push_back(normal1);
                    normals.push_back(normal1);
                    normals.push_back(normal1);
                    pointIdx.push_back(idx);
                    pointIdx.push_back(idx);
                    pointIdx.push_back(idx);

                    vertices.push_back(a);
                    vertices.push_back(c);
                    vertices.push_back(d);
                    normals.push_back(normal2);
                    normals.push_back(normal2);
                    normals.push_back(normal2);
                    pointIdx.push_back(idx);
                    pointIdx.push_back(idx);
                    pointIdx.push_back(idx);
                }

                idx++;
            }

            addAnisotropicSphere(vertices, normals, pointIdx, toolpath[start].position - halfRadius*toolpath[start].normal, toolpath[start].normal, startIdx, radius, halfRadius, resolution);
            addAnisotropicSphere(vertices, normals, pointIdx, toolpath[end].position - halfRadius*toolpath[end].normal, toolpath[end].normal, idx-1, radius, halfRadius, resolution);
        }

        ++i;
    }

    return idx;
}

void setStatus(const std::string &status) {
    char *ptr = static_cast<char *>(std::malloc(status.size() + 1));
    std::memcpy(ptr, status.c_str(), status.size() + 1);

    MAIN_THREAD_ASYNC_EM_ASM({
        const status = UTF8ToString($0);
        document.getElementById('status').textContent = status;
        setProgress(0);

        _free($0);
    }, ptr);
}

void setError(const std::string &status) {
    char *ptr = static_cast<char *>(std::malloc(status.size() + 1));
    std::memcpy(ptr, status.c_str(), status.size() + 1);

    MAIN_THREAD_ASYNC_EM_ASM({
        const status = UTF8ToString($0);
        showError(status);
        _free($0);
    }, ptr);
}

void setUIState(UI_STATE state) {
    if (state == uiState) return;
    uiState = state;

    if (state == UI_STATE::Slicing) {
        MAIN_THREAD_ASYNC_EM_ASM({
            document.getElementById('perLayerSettings').style.display = "none";
            document.getElementById('settings').style.display = "";
            document.getElementById('sliceButton').disabled = true;
            document.getElementById('outputBox').style.display = "none";
            document.getElementById('warning').style.display = "none";
            document.getElementById('sliceButton').style.display = "";
            document.getElementById('resumeButton').style.display = "none"; 
            document.querySelectorAll('#settings input, #settings label, #settings select').forEach(element => {
                element.disabled = true;
            });
            document.getElementById('settings').classList.add('disabled');
            if (!document.getElementById("viewMode").querySelector('option[value="gcode"]')) {
                document.getElementById("viewMode").add(new Option("GCode", "gcode"));
            }
        });
    } else if (state == UI_STATE::Waiting) {
        MAIN_THREAD_ASYNC_EM_ASM({
            document.getElementById('perLayerSettings').style.display = "";
            document.getElementById('settings').style.display = "none";
            document.getElementById('viewMode').value = "layers";
            document.getElementById('viewerControls').style.display = "flex";
            document.getElementById("toolpathSlider").closest(".sliderControl").style.display = "none";
	        document.getElementById("layerSlider").closest(".sliderControl").style.display = "";
            document.getElementById('sliceButton').style.display = "none";
            document.getElementById('resumeButton').style.display = ""; 
            viewMode.querySelector('option[value="gcode"]')?.remove();
        });
    } else if (state == UI_STATE::Done) {
        MAIN_THREAD_ASYNC_EM_ASM({
            document.getElementById('sliceButton').disabled = false;
            document.getElementById('sliceButton').removeAttribute("data-progress");
            document.getElementById('sliceButton').style.setProperty("--progress", "0%");
            document.getElementById('outputBox').style.display = "flex";
            document.getElementById('warning').style.display = "block";
            document.getElementById('viewMode').value = "gcode";
            document.getElementById('viewerControls').style.display = "flex";
            document.getElementById("toolpathSlider").closest(".sliderControl").style.display = "";
	        document.getElementById("layerSlider").closest(".sliderControl").style.display = "none";
            document.querySelectorAll('#settings input, #settings label, #settings select').forEach(element => {
                element.disabled = false;
            });
            document.getElementById('settings').classList.remove('disabled');
        });
    }
}

void downloadFile(const char *fileName) {
    std::string cmd = std::string("downloadFile(\"") + std::string(fileName) + std::string("\")");
	emscripten_run_script(cmd.c_str());
}

void normalizeToolpathToMesh(std::vector<Vec3> &toolpath, const std::vector<Vec3> &mesh) {
    if (toolpath.empty() || mesh.empty()) return;

    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

    for (const Vec3 &p : mesh) {
        min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y); min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
    }

    Vec3 center{(max.x-min.x) * 0.5f, (max.y-min.y) * 0.5f, (max.z-min.z) * 0.5f};
    float maxSize = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
    float scale = maxSize > 0.0f ? 1.0f / maxSize : 1.0f;

    s_meshScale = scale;
    s_imageOffsetX =  0.5f*(1.0/scale - (max.x-min.x));
    s_imageOffsetY =  0.5f*(1.0/scale - (max.y-min.y));

    for (Vec3 &p : toolpath) {
        float x = p.x;
        float y = p.z;
        float z = -p.y;

        p.x = (x - center.x) * scale;
        p.y = (y - center.z) * scale;
        p.z = (z + center.y) * scale;
    }
}

size_t createLayersMesh(const std::vector<Toolpath::Mesh> &layers, std::vector<Vec3> &vertices, std::vector<Vec3> &normals, std::vector<float> &pointIdx) {
    vertices.clear();
    normals.clear();
    pointIdx.clear();

    for (size_t i = 0; i < layers.size(); ++i) {
        const std::vector<uint32_t> &tris = layers[i].triangles;
        const std::vector<float> &verts = layers[i].vertices;

        for (size_t t = 0; t < tris.size()/3; ++t) {
            size_t a = tris[3*t+0], b = tris[3*t+1], c = tris[3*t+2];
            Vec3 pa = Vec3(verts.data(), a), pb = Vec3(verts.data(), b), pc = Vec3(verts.data(), c);
            Vec3 n = triangleNormal(pa, pb, pc);
            vertices.push_back(pa);
            vertices.push_back(pb);
            vertices.push_back(pc);
            for (uint8_t j = 0; j < 3; ++j) {
                normals.push_back(n);
                pointIdx.push_back(i);
            }
        }
    }

    return vertices.size();
}

void updateToolpath() {
    auto &path = slicer->getToolpath();
    std::vector<Vec3> waypointNormal;
    std::vector<float> pointIdx;
    std::vector<Vec3> waypointPosition;
    s_waypointCount = createToolpathTube(path, 0.5f*s_nozzleWidth, 8, waypointPosition, waypointNormal, pointIdx);
    normalizeToolpathToMesh(waypointPosition, mesh->getVertices());
    waypointBuffer.clear();
    for (size_t i = 0; i < waypointPosition.size(); ++i) {
        waypointBuffer.push_back(waypointPosition[i].x);
        waypointBuffer.push_back(waypointPosition[i].y);
        waypointBuffer.push_back(waypointPosition[i].z);
        waypointBuffer.push_back(waypointNormal[i].x);
        waypointBuffer.push_back(waypointNormal[i].z);
        waypointBuffer.push_back(-waypointNormal[i].y);
        waypointBuffer.push_back(pointIdx[i]);
    }
    
    MAIN_THREAD_ASYNC_EM_ASM({
        getToolpath()
    });
}

void updateLayers(const std::vector<Toolpath::Mesh> &layers) {
    s_layerPtr = &layers;
    s_layerDir.resize(layers.size());
    for (auto &l : s_layerDir) {
        l.clear();
    }
    std::vector<Vec3> waypointNormal;
    std::vector<float> pointIdx;
    std::vector<Vec3> waypointPosition;
    createLayersMesh(layers, waypointPosition, waypointNormal, pointIdx);
    normalizeToolpathToMesh(waypointPosition, mesh->getVertices());
    layerBuffer.clear();

    s_layerCount = layers.size();
    layerDirections.resize(s_layerCount);
    layerFromImage.resize(s_layerCount);
    for (size_t i = 0; i < s_layerCount; ++i) {
        layerDirections[i] = ((i + s_zigzagOffset) % 4) * 45.0f;
        layerFromImage[i] = false;
    }

    for (size_t i = 0; i < waypointPosition.size(); ++i) {
        layerBuffer.push_back(waypointPosition[i].x);
        layerBuffer.push_back(waypointPosition[i].y);
        layerBuffer.push_back(waypointPosition[i].z);
        layerBuffer.push_back(waypointNormal[i].x);
        layerBuffer.push_back(waypointNormal[i].z);
        layerBuffer.push_back(-waypointNormal[i].y);
        layerBuffer.push_back(pointIdx[i]);
    }
    
    MAIN_THREAD_ASYNC_EM_ASM({
        getLayer()
    });
}

EM_JS(size_t, getlayersDirectionArrayLength, (), {
    return layersDirectionArray.length;
});

EM_JS(void, copylayersDirectionArrayFromJS, (float *dst), {
    for (let i = 0; i < layersDirectionArray.length; ++i) {
        HEAP32[(dst >> 2) + i] = layersDirectionArray[i];
    }
});

void waitLayerFields(const std::vector<Toolpath::Mesh> &layers) {
    updateLayers(layers);
    setUIState(UI_STATE::Waiting);
    auto startTime = std::chrono::steady_clock::now();

    std::unique_lock<std::mutex> lock(s_layerMutex);
    s_layerCondition.wait(lock, [&] { 
        return s_layerResumed;
    });
    s_layerResumed = false;

    s_idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()/1000.0;

    setUIState(UI_STATE::Slicing);
}

struct ProfileParameters {
    float bedSizeX;
    float bedSizeY;
    float travelSpeed;
    float printingSpeed;
    float retractSpeed;
    float retractAmount;
    bool circularBed;
    bool relativeExtrusion;
};

ProfileParameters loadProfile(const std::string &profileName) {
    ProfileParameters params{};

    std::ifstream file("profiles/" + profileName + "/params.ini");
    std::string line;

    while (std::getline(file, line)) {
        auto separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);

        if (key == "BedSizeX")
            params.bedSizeX = std::stof(value);
        else if (key == "BedSizeY")
            params.bedSizeY = std::stof(value);
        else if (key == "TravelSpeed")
            params.travelSpeed = std::stof(value)*60;
        else if (key == "PrintingSpeed")
            params.printingSpeed = std::stof(value)*60;
        else if (key == "RetractSpeed")
            params.retractSpeed = std::stof(value)*60;
        else if (key == "RetractAmount")
            params.retractAmount = std::stof(value);
        else if (key == "CircularBed")
            params.circularBed = value == "true";
        else if (key == "RelativeExtrusion")
            params.relativeExtrusion = value == "true";
    }

    return params;
}

std::string getHeader(const std::string &profileName, float nozzleDiameter = 0.6f) {
    std::ifstream file("profiles/" + profileName + "/header.gcode");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string str = buffer.str();

    size_t pos = 0;
    std::string from = "<NOZZLE_DIAMETER>";
    std::string to =  std::to_string(nozzleDiameter);
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }

    return str;
}

std::string getFooter(const std::string &profileName) {
    std::ifstream file("profiles/" + profileName + "/footer.gcode");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> getProfiles() {
    std::vector<std::string> profiles;
    for (const auto& entry : std::filesystem::directory_iterator("profiles")) {
        if (entry.is_directory()) {
            profiles.push_back(entry.path().filename().string());
        }
    }
    return profiles;
}

void toolpathToGCodeProfile(const char *path, const char *profile) {
    float width = s_nozzleWidth;
    auto params = loadProfile(profile);
    auto header = getHeader(profile, width);
    auto footer = getFooter(profile);

    float height = 0.5f*width;
    float totalE = 0.0f;
    auto &waypoints = slicer->getToolpath();

    float minX = std::numeric_limits<float>::infinity(), minY = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity(), maxY = -std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    for (const auto &point : waypoints) {
        minX = std::min(minX, point.position.x);
        minY = std::min(minY, point.position.y);
        maxX = std::max(maxX, point.position.x);
        maxY = std::max(maxY, point.position.y);
        minZ = std::min(minZ, point.position.z);
    }

    for (auto &p : waypoints) {
        p.position -= Vec3(0.5f*(maxX+minX), 0.5f*(maxY+minY), minZ - height);
        if (!params.circularBed) {
            p.position += 0.5f * Vec3(params.bedSizeX, params.bedSizeY, 0);
        }
    }

    std::ofstream gcode(path);
    gcode << std::fixed;

    auto pos = header.find("G29 A X0 Y0 I256 J256");
    if (pos != std::string::npos) {
        Vec3 origin = params.circularBed ? Vec3(0) : Vec3(0.5f*(maxX+minX), 0.5f*(maxY+minY), 0);
        Vec3 diagonal = Vec3(maxX-minX, maxY-minY, 0);

        maxX = origin.x + diagonal.x * 0.5f;
        maxY = origin.y + diagonal.y * 0.5f;
        minX = origin.x - diagonal.x * 0.5f;
        minY = origin.y - diagonal.y * 0.5f;

        header.replace(
        header.find("G29 A X0 Y0 I256 J256"),
            strlen("G29 A X0 Y0 I256 J256"),
            "G29 A X" + std::to_string(minX) +
            " Y" + std::to_string(minY) +
            " I" + std::to_string(maxX - minX) +
            " J" + std::to_string(maxY - minY)
        );
    }
    
    gcode << header << "\n";
    bool isFanOn = false, needPrime = true;
    for (size_t i = 0; i < waypoints.size(); ++i) {
        if (needPrime && waypoints[i].deposition) {
            float e = params.retractAmount;
            totalE += e;
            if (!params.relativeExtrusion) {
                e = totalE;
            }
            gcode << "G1 E" << std::setprecision(2) << e << " F" << std::setprecision(0) << params.retractSpeed << "\n";
            needPrime = false;
        } else if (i > 0 && !needPrime && !waypoints[i].deposition) {
            size_t j = i;
            float dist = 0;
            while (j < waypoints.size() && !waypoints[j].deposition) {
                dist += (waypoints[j].position-waypoints[j-1].position).length();
                if (dist > 3.0f*width) {
                    float e = -params.retractAmount;
                    totalE += e;
                    if (!params.relativeExtrusion) {
                        e = totalE;
                    }
                    gcode << "G1 E" << std::setprecision(2) << e << " F" << std::setprecision(0) << params.retractSpeed << "\n";
                    needPrime = true;
                    break;
                }
                ++j;
            }
        }

        float e = 0, f = params.travelSpeed;
        if (i > 0 && waypoints[i].deposition) {
            float crossSection = 0.25f*M_PI*1.75*1.75;
            float distance = (waypoints[i].position-waypoints[i-1].position).length();
            e = distance*width*height/crossSection;
            totalE += e;
            f = params.printingSpeed;
        }
        if (!params.relativeExtrusion) {
            e = totalE;
        }
        gcode << "G1 X" << std::setprecision(8) << waypoints[i].position.x << " Y" << waypoints[i].position.y << " Z" << waypoints[i].position.z <<
                " E" << e << " F" << std::setprecision(0) << f << "\n";

        if (isFanOn && waypoints[i].position.z < 4*height) {
            gcode << "M106 S0\n";
            isFanOn = false;
        } else if (!isFanOn && waypoints[i].position.z > 4*height) {
            gcode << "M106 S255\n";
            isFanOn = true;
        }
    }

    gcode << "M106 S0\n";
    gcode << footer << std::endl;
}

extern "C" {
    void loadSTL(char *fileName) {
        delete mesh;
		mesh = new StlLoader(fileName);
        emscripten_run_script("getMesh()");
    }

    char *getProfileNames() {
        auto profiles = getProfiles();
        std::string result;
        for (const auto &profile : profiles) {
            if (result != "") {
                result += "/";
            }
            result += profile;
        }

        char *output = (char *) malloc(result.size() + 1);
        std::copy(result.begin(), result.end(), output);
        output[result.size()] = '\0';

        return output;
    }

    float *getTriangles() {
        return (float *) mesh->getTriangleSoup().data();
    }

    uint32_t getTrianglesCount() {
        return mesh->getTriangleSoup().size();
    }

    float *getToolpath() {
        return waypointBuffer.data();
    }

    uint32_t getToolpathCount() {
        return waypointBuffer.size() / 7;
    }

    float getWaypointCount() {
        return s_waypointCount;
    }

    float *getLayer() {
        return layerBuffer.data();
    }

    uint32_t getLayerVertexCount() {
        return layerBuffer.size() / 7;
    }

    float getLayerCount() {
        return s_layerCount;
    }
 
	void slice(char *fileName, float nozzleWidth, float topAngle, float bottomAngle, float numberOfCover, float collisionAngle,
               uint32_t zigzagOffset, bool upCollisionCheck, bool holeClosing, int infillType, int optimizeObjective, bool emscriptenWait) {
        if (uiState == UI_STATE::Slicing) return;
        setUIState(UI_STATE::Slicing);
		auto startTimeTotal = std::chrono::steady_clock::now();
		auto startTime = std::chrono::steady_clock::now();
        s_idleTime = 0;
        layerDirections.clear();
		std::cout << "STL Loading..." << std::endl;
        setStatus("Loading STL...");

		delete mesh;
		mesh = new StlLoader(fileName);
        free(fileName);
		std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()/1000.0 << " s" << std::endl;

		float layerHeightRateo = 0.5f; // This one need more testing.
		uint32_t hardCodedField = 0; // No really useful to expose.
        s_zigzagOffset = zigzagOffset;

		Infill::Type infill = Infill::Type(infillType);
		NormalField::Objective objective = NormalField::Objective(optimizeObjective);
        s_nozzleWidth = nozzleWidth;

		std::thread thread([=]() {
			delete slicer;
			slicer = new Slicer(mesh->getTriangleSoup(), nozzleWidth, layerHeightRateo*nozzleWidth, topAngle, bottomAngle, infill, objective, hardCodedField, numberOfCover, zigzagOffset, collisionAngle, upCollisionCheck, holeClosing, emscriptenWait);
			float time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTimeTotal).count()/1000.0;
            std::cout << "\nTotal time: " << time - s_idleTime << " s" << std::endl;
            setStatus(std::string("Slicing Completed in ") + std::to_string(int(time-s_idleTime)) + std::string(" Seconds."));
            setUIState(UI_STATE::Done);
            updateToolpath();
            updateLayers(slicer->getLayers());
		});
		thread.detach();
	}

    void setLayerDirection(size_t i, float dir) {
        layerDirections[i] = dir;
    }

    float getLayerDirection(size_t i) {
        return layerDirections[i]; 
    }

    void setLayerFromImage(size_t i, bool status) {
        layerFromImage[i] = status;
    }

    bool getLayerFromImage(size_t i) {
        return layerFromImage[i]; 
    }

    void saveProfile(char *fileName, char *profile) {
		if (!slicer || uiState != UI_STATE::Done) return;

        std::string filename(fileName);

        if (std::string(profile) == "ratrig3") {
            slicer->saveToGCode3Axis(filename.c_str());
        } else if (std::string(profile) == "ratrig5") {
            slicer->saveToGCode(filename.c_str());
        } else if (std::string(profile) == "meshply") {
            filename = "toolpath.ply";
            slicer->saveToPLY(filename.c_str());
        } else {
            toolpathToGCodeProfile(filename.c_str(), profile);
        }

        downloadFile(filename.c_str());
        free(fileName);
    }

    void resume() {
        std::unique_lock<std::mutex> lock(s_layerMutex);
        s_layerResumed = true;
        s_layerCondition.notify_one();
    }

    void loadLayerImage(char *path, size_t i) {
        int32_t texelCountX, texelCountY, channels;
        unsigned char *image = stbi_load(path, &texelCountX, &texelCountY, &channels, 1);
        if (!image) {
            std::cout << "Failed to load image: " << path << std::endl;
            return;
        }

        auto &directions = s_layerDir[i];
        const auto &positions = (*s_layerPtr)[i].vertices;
        directions.resize((*s_layerPtr)[i].vertices.size());

        for (size_t j = 0; j < directions.size(); ++j) {
            float posX = positions[3*j+0], posY = positions[3*j+1];
            int32_t texelU = std::min(std::max(int32_t((posX + s_imageOffsetX)*s_meshScale * texelCountX), 0), texelCountX-1);
            int32_t texelV = std::min(std::max(int32_t((1.0f - (posY + s_imageOffsetY)*s_meshScale) * texelCountY), 0), texelCountY-1);
            float angle = image[texelV*texelCountX + texelU]/255.0f*M_PI;
            directions[j] = angle;
        }
        setLayerFromImage(i, true);
        stbi_image_free(image);
    }
}
#endif