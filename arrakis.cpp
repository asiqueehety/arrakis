// =============================================================================
//  ARRAKIS - Harvester Down: a timed desert rescue game
//  CSE 4102: Computer Graphics and Image Processing Laboratory
//
//  A high-fidelity real-time 3D simulation inspired by the DUNE universe.
//  ALL GEOMETRY IS PROCEDURALLY GENERATED FROM MATHEMATICAL PRIMITIVES.
//  NO EXTERNAL .OBJ FILES ARE LOADED.
//
//  FEATURING:
//    - Movie-Accurate Atreides Ornithopter:
//        8-blade dragonfly tandem flapping wings, faceted stealth cockpit canopy,
//        turbine jet intakes, glowing heat reheat thrusters, aerodynamic banking.
//    - Massive Industrial Spice Harvester:
//        Quad crawler tread units, front rotary harvesting drum, refinery superstructure,
//        bridge floodlights, exhaust funnels venting burning spice fumes.
//    - Movie-Realistic Spice Pressure Tanks:
//        Cylindrical pressure vessels with hemispherical domed heads, structural
//        cradles with diagonal trusses, pulsing illuminated spice level gauges, pipes.
//    - Continuous Sweeping Desert Sand Dunes:
//        Seamless 140x140 vertex heightfield terrain with barchan ridges, procedural
//        wind ripples, slope-dependent color stratification, and analytical normals.
//    - Atmospheric Wind & Blowing Sand Particle System:
//        Over 2,000 blowing sand particles streaming across dunes with wind gusts,
//        harvester crawler dust plumes, and ornithopter wing downwash dust swirls.
//    - Professional Lighting & Atmosphere:
//        Blinn-Phong + Half-Lambert sand wrap lighting, directional desert sun,
//        specular micro-sparkles, soft PCF shadow mapping, and warm desert distance fog.
//    - Cinematic Third-Person Chase Camera with smooth flight kinematics.
//
//  CONTROLS:
//    Enter           - Start / next mission
//    Mouse hover     - Steer aircraft and camera together (center stops turning)
//    W / S, A / D    - Forward / reverse, strafe left / right
//    Q / E           - Raise / lower terrain-following altitude
//    Left Shift      - Rechargeable boost
//    Space           - Hover, lower winch, rescue / unload at base
//    P / R           - Pause / retry mission
//    Arrow Keys      - Keyboard steering / camera elevation
//    C               - Toggle Camera View (Cinematic Chase / Cockpit / Overhead)
//    F               - Toggle Wireframe
//    F11             - Toggle Fullscreen
//    ESC             - Quit
// =============================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <string>
#include <fstream>
#include <cstdint>

// =============================================================================
// WINDOW & DISPLAY CONFIGURATION
// =============================================================================
int  g_scrW = 1280;
int  g_scrH = 720;
bool g_isFullscreen = false;
GLFWwindow* g_window = nullptr;

// =============================================================================
// CAMERA & FLIGHT STATE
// =============================================================================
bool  g_keys[1024]  = {};
bool  g_wireframe   = false;
int   g_camMode     = 0;     // 0: Cinematic Chase, 1: Cockpit/Close, 2: Overhead Tactical
float g_camOrbitPitch = 0.0f;

// Flight kinematics
glm::vec3 g_ornPos(0.0f, 18.0f, 0.0f);
glm::vec3 g_ornVel(0.0f);
float     g_ornYaw   = 0.0f;     // Heading in degrees
float     g_ornPitch = 0.0f;     // Pitch tilt in degrees
float     g_ornRoll  = 0.0f;     // Aerodynamic banking roll in degrees
float     g_ornSpeed = 0.0f;     // Current cruise velocity
bool      g_isBoosting = false;

// Wing kinematics (movie-accurate 8-wing tandem flutter)
float g_wingPhase = 0.0f;

// Harvester state
float g_harvX = -80.0f;
float g_harvZ = 15.0f;

// Dynamic Spice Tanks
struct SpiceTankInstance {
    glm::vec3 pos;
    float yaw;
    float scale;
};
std::vector<SpiceTankInstance> g_tanks;

// =============================================================================
// DUNE TERRAIN HEIGHTFIELD FUNCTION
// Continuous procedural analytical surface for seamless rolling dunes
// =============================================================================
float rawDuneHeight(float x, float z) {
    // Primary sweeping barchan dune swells
    float h1 = std::sin(x * 0.009f + z * 0.004f) * 14.0f;
    float h2 = std::cos(x * 0.004f - z * 0.012f) * 9.5f;

    // Asymmetric windward & leeward slipface ridges
    float r = std::sin(x * 0.028f + z * 0.016f);
    r = 1.0f - std::abs(r);
    float ridge = r * r * 5.0f;

    // Fine dunes & desert undulation
    float h3 = std::sin(x * 0.065f + z * 0.038f) * 1.8f;
    return h1 + h2 + ridge + h3 - 4.0f;
}

float getDuneHeight(float x, float z) {
    float height = rawDuneHeight(x, z);
    // Level the landing pad into the dunes; evacuation sites stay natural.
    static const glm::vec3 terraces[] = {{55, 55, rawDuneHeight(55, 55)}};
    for (const auto& center : terraces) {
        float radius = center.x == 55 ? 15.0f : 10.0f;
        float blend = 1.0f - glm::smoothstep(radius, radius + 10, glm::distance(glm::vec2(x, z), glm::vec2(center)));
        height = glm::mix(height, center.z, blend);
    }
    return height;
}

glm::vec3 getDuneNormal(float x, float z) {
    float eps = 0.4f;
    float hL = getDuneHeight(x - eps, z);
    float hR = getDuneHeight(x + eps, z);
    float hD = getDuneHeight(x, z - eps);
    float hU = getDuneHeight(x, z + eps);
    return glm::normalize(glm::vec3(-(hR - hL), 2.0f * eps, -(hU - hD)));
}

#include "arrakis_game.h"
#include "arrakis_tests.h"

// =============================================================================
// WIND & PARTICLE SYSTEM
// =============================================================================
const glm::vec3 WIND_DIR = glm::normalize(glm::vec3(-0.92f, -0.04f, 0.38f));
const float     WIND_BASE_SPEED = 18.0f;

struct SandParticle {
    glm::vec3 pos;
    glm::vec3 vel;
    float     life;
    float     maxLife;
    float     size;
    float     alpha;
};

const int MAX_PARTICLES = 2400;
std::vector<SandParticle> g_particles;

void initParticles() {
    g_particles.resize(MAX_PARTICLES);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> distPos(-120.0f, 120.0f);
    std::uniform_real_distribution<float> distY(0.5f, 25.0f);
    std::uniform_real_distribution<float> distLife(1.0f, 4.0f);
    std::uniform_real_distribution<float> distSpeed(0.8f, 1.4f);

    for (auto& p : g_particles) {
        p.pos = glm::vec3(distPos(rng), distY(rng), distPos(rng));
        p.vel = WIND_DIR * (WIND_BASE_SPEED * distSpeed(rng));
        p.maxLife = distLife(rng);
        p.life = distLife(rng);
        p.size = 0.18f + (rng() % 100) * 0.003f;
        p.alpha = 0.25f + (rng() % 100) * 0.0035f;
    }
}

void updateParticles(float dt, glm::vec3 centerPos, glm::vec3 harvPos) {
    float gust = 1.0f + 0.35f * std::sin((float)glfwGetTime() * 1.8f);
    static std::mt19937 rng(1984);
    std::uniform_real_distribution<float> distOffset(-100.0f, 100.0f);
    std::uniform_real_distribution<float> distY(0.2f, 32.0f);

    for (int i = 0; i < (int)g_particles.size(); ++i) {
        auto& p = g_particles[i];
        p.life -= dt;
        p.pos += p.vel * (gust * dt);

        // Ground collision & terrain hugging
        float groundY = getDuneHeight(p.pos.x, p.pos.z);
        if (p.pos.y < groundY + 0.2f) {
            p.pos.y = groundY + 0.2f;
            p.vel.y = std::abs(p.vel.y) * 0.5f + 0.8f;
        }

        // Respawn if life expired or drifted too far from camera/player
        float distToPlayer = glm::distance(glm::vec2(p.pos.x, p.pos.z), glm::vec2(centerPos.x, centerPos.z));
        if (p.life <= 0.0f || (distToPlayer > 160.0f && i % 4 != 0 && i % 3 != 0)) {
            p.life = 2.0f + (rng() % 100) * 0.02f;
            p.maxLife = p.life;

            // Half particles spawn upwind from player, some spawn behind harvester crawler treads
            if (i % 4 == 0 && wormAttackTime() < 12) {
                // Crawler dust plume
                glm::vec3 forward = harvesterForward(g_mission.harvesterYaw);
                glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
                p.pos = harvPos - forward * 4.0f + right * (-4.0f + (rng() % 80) * 0.1f)
                    + glm::vec3(0, 1.0f + (rng() % 30) * 0.1f, 0);
                p.vel = WIND_DIR * (WIND_BASE_SPEED * 0.5f) - forward * 2.0f + glm::vec3(0, 2.5f, 0);
                p.alpha = 0.55f;
                p.size = 0.45f;
            } else if (i % 3 == 0 && wormAttackTime() < 18) {
                glm::vec3 worm = wormPosition();
                float a = (rng() % 10000) * 0.0006283185f;
                float radius = 10 + (rng() % 1500) * 0.01f;
                p.pos = worm + glm::vec3(std::cos(a) * radius - (rng() % 2200) * 0.01f, 0, std::sin(a) * radius);
                p.pos.y = getDuneHeight(p.pos.x, p.pos.z) + 0.8f;
                p.vel = glm::vec3(std::cos(a) * 7, wormAttackTime() > 0 ? 10 : 2, std::sin(a) * 7) + WIND_DIR * 4.0f;
                p.size = wormAttackTime() > 0 ? 5.5f : 2.2f;
                p.alpha = 0.22f;
            } else if (i % 5 == 0 && g_keys[GLFW_KEY_SPACE]) {
                float a = (rng() % 1000) * 0.006283185f;
                p.pos = centerPos + glm::vec3(std::cos(a) * 4, -4, std::sin(a) * 4);
                p.vel = glm::vec3(std::cos(a) * 11, 1.5f, std::sin(a) * 11);
                p.size = 1.8f; p.alpha = 0.18f;
            } else {
                glm::vec3 upwind = -WIND_DIR * 90.0f;
                p.pos = centerPos + upwind + glm::vec3(distOffset(rng), distY(rng), distOffset(rng));
                float gy = getDuneHeight(p.pos.x, p.pos.z);
                if (p.pos.y < gy) p.pos.y = gy + 1.0f;
                p.vel = WIND_DIR * (WIND_BASE_SPEED * (0.85f + (rng() % 40) * 0.01f));
                p.alpha = 0.28f;
                p.size = 0.22f;
            }
        }
    }
}

// =============================================================================
// PROCEDURAL MESH DATA STRUCTURES & PRIMITIVES
// All shapes created mathematically without external asset files!
// =============================================================================
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Mesh {
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    GLsizei      indexCount = 0;
    GLenum       drawMode = GL_TRIANGLES;
};

Mesh uploadMesh(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices) {
    Mesh m;
    m.indexCount = (GLsizei)indices.size();

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glGenBuffers(1, &m.EBO);

    glBindVertexArray(m.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // layout 0: pos, 1: normal, 2: uv
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return m;
}

// 1. Primitive Cube (with separate normals per face)
Mesh createCube() {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;

    auto addFace = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 norm) {
        unsigned int base = (unsigned int)v.size();
        v.push_back({ p0, norm, {0.0f, 0.0f} });
        v.push_back({ p1, norm, {1.0f, 0.0f} });
        v.push_back({ p2, norm, {1.0f, 1.0f} });
        v.push_back({ p3, norm, {0.0f, 1.0f} });
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
    };

    // Front (+Z), Back (-Z), Left (-X), Right (+X), Top (+Y), Bottom (-Y)
    addFace({-0.5,-0.5, 0.5}, { 0.5,-0.5, 0.5}, { 0.5, 0.5, 0.5}, {-0.5, 0.5, 0.5}, { 0, 0, 1});
    addFace({ 0.5,-0.5,-0.5}, {-0.5,-0.5,-0.5}, {-0.5, 0.5,-0.5}, { 0.5, 0.5,-0.5}, { 0, 0,-1});
    addFace({-0.5,-0.5,-0.5}, {-0.5,-0.5, 0.5}, {-0.5, 0.5, 0.5}, {-0.5, 0.5,-0.5}, {-1, 0, 0});
    addFace({ 0.5,-0.5, 0.5}, { 0.5,-0.5,-0.5}, { 0.5, 0.5,-0.5}, { 0.5, 0.5, 0.5}, { 1, 0, 0});
    addFace({-0.5, 0.5, 0.5}, { 0.5, 0.5, 0.5}, { 0.5, 0.5,-0.5}, {-0.5, 0.5,-0.5}, { 0, 1, 0});
    addFace({-0.5,-0.5,-0.5}, { 0.5,-0.5,-0.5}, { 0.5,-0.5, 0.5}, {-0.5,-0.5, 0.5}, { 0,-1, 0});

    return uploadMesh(v, idx);
}

// 2. Primitive Cylinder (smooth body + capped ends)
Mesh createCylinder(int slices = 24) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;

    float r = 0.5f;
    float h = 1.0f;

    // Body
    for (int i = 0; i <= slices; ++i) {
        float theta = (float)i / (float)slices * glm::two_pi<float>();
        float c = std::cos(theta);
        float s = std::sin(theta);
        glm::vec3 norm(c, 0.0f, s);
        float u = (float)i / (float)slices;

        v.push_back({ glm::vec3(r * c, -0.5f * h, r * s), norm, glm::vec2(u, 0.0f) });
        v.push_back({ glm::vec3(r * c,  0.5f * h, r * s), norm, glm::vec2(u, 1.0f) });
    }

    for (int i = 0; i < slices; ++i) {
        unsigned int b = i * 2;
        idx.push_back(b); idx.push_back(b + 1); idx.push_back(b + 3);
        idx.push_back(b); idx.push_back(b + 3); idx.push_back(b + 2);
    }

    // Top cap (+Y)
    unsigned int topCenter = (unsigned int)v.size();
    v.push_back({ glm::vec3(0.0f, 0.5f * h, 0.0f), glm::vec3(0, 1, 0), glm::vec2(0.5f, 0.5f) });
    for (int i = 0; i <= slices; ++i) {
        float theta = (float)i / (float)slices * glm::two_pi<float>();
        v.push_back({ glm::vec3(r * std::cos(theta), 0.5f * h, r * std::sin(theta)), glm::vec3(0, 1, 0), glm::vec2(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)) });
    }
    for (int i = 0; i < slices; ++i) {
        idx.push_back(topCenter);
        idx.push_back(topCenter + 1 + i + 1);
        idx.push_back(topCenter + 1 + i);
    }

    // Bottom cap (-Y)
    unsigned int btmCenter = (unsigned int)v.size();
    v.push_back({ glm::vec3(0.0f, -0.5f * h, 0.0f), glm::vec3(0, -1, 0), glm::vec2(0.5f, 0.5f) });
    for (int i = 0; i <= slices; ++i) {
        float theta = (float)i / (float)slices * glm::two_pi<float>();
        v.push_back({ glm::vec3(r * std::cos(theta), -0.5f * h, r * std::sin(theta)), glm::vec3(0, -1, 0), glm::vec2(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)) });
    }
    for (int i = 0; i < slices; ++i) {
        idx.push_back(btmCenter);
        idx.push_back(btmCenter + 1 + i);
        idx.push_back(btmCenter + 1 + i + 1);
    }

    return uploadMesh(v, idx);
}

// 3. Primitive Sphere (smooth UV sphere)
Mesh createSphere(int lats = 16, int lons = 24) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;

    for (int i = 0; i <= lats; ++i) {
        float theta = (float)i / (float)lats * glm::pi<float>();
        float sinT = std::sin(theta);
        float cosT = std::cos(theta);

        for (int j = 0; j <= lons; ++j) {
            float phi = (float)j / (float)lons * glm::two_pi<float>();
            float sinP = std::sin(phi);
            float cosP = std::cos(phi);

            glm::vec3 norm(sinT * cosP, cosT, sinT * sinP);
            glm::vec3 pos = norm * 0.5f;
            glm::vec2 uv((float)j / (float)lons, (float)i / (float)lats);

            v.push_back({ pos, norm, uv });
        }
    }

    for (int i = 0; i < lats; ++i) {
        for (int j = 0; j < lons; ++j) {
            unsigned int first = (i * (lons + 1)) + j;
            unsigned int second = first + lons + 1;

            idx.push_back(first);
            idx.push_back(first + 1);
            idx.push_back(second);

            idx.push_back(second);
            idx.push_back(first + 1);
            idx.push_back(second + 1);
        }
    }

    return uploadMesh(v, idx);
}

// 4. Primitive Aerofoil Blade / Wedge (tapered insectoid wing)
Mesh createWingBlade() {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;

    // A slender tapered blade cross-section along X (span: 0 to 1), with chord along Z (-0.5 to 0.5)
    // Root chord = 1.0, Tip chord = 0.25, Root thickness = 0.08, Tip thickness = 0.02
    int spanSegs = 8;
    for (int i = 0; i <= spanSegs; ++i) {
        float t = (float)i / (float)spanSegs; // 0 (root) to 1 (tip)
        float x = t;
        float chord = glm::mix(1.0f, 0.22f, t);
        float thick = glm::mix(0.08f, 0.015f, t);

        // 4 points around aerofoil profile: leading edge, top crest, trailing edge, bottom crest
        glm::vec3 pLead(x, 0.0f, -chord * 0.45f);
        glm::vec3 pTop (x, thick * 0.5f, -chord * 0.1f);
        glm::vec3 pTrail(x, 0.0f, chord * 0.55f);
        glm::vec3 pBtm (x, -thick * 0.5f, -chord * 0.1f);

        v.push_back({ pLead,  glm::normalize(glm::vec3(0, 0, -1)), {t, 0.0f} });
        v.push_back({ pTop,   glm::normalize(glm::vec3(0, 1, 0)),  {t, 0.33f} });
        v.push_back({ pTrail, glm::normalize(glm::vec3(0, 0, 1)),  {t, 0.66f} });
        v.push_back({ pBtm,   glm::normalize(glm::vec3(0, -1, 0)), {t, 1.0f} });
    }

    for (int i = 0; i < spanSegs; ++i) {
        unsigned int b1 = i * 4;
        unsigned int b2 = (i + 1) * 4;
        for (int k = 0; k < 4; ++k) {
            int kNext = (k + 1) % 4;
            idx.push_back(b1 + k); idx.push_back(b2 + k); idx.push_back(b2 + kNext);
            idx.push_back(b1 + k); idx.push_back(b2 + kNext); idx.push_back(b1 + kNext);
        }
    }

    return uploadMesh(v, idx);
}

// 5. Continuous Rolling Sand Dune Terrain Heightfield Mesh
Mesh createDuneTerrain(int gridW = 140, int gridH = 140, float totalSize = 700.0f) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;
    v.reserve((gridW + 1) * (gridH + 1));
    idx.reserve(gridW * gridH * 6);

    float halfSize = totalSize * 0.5f;
    float step = totalSize / (float)gridW;

    for (int z = 0; z <= gridH; ++z) {
        float worldZ = -halfSize + z * step;
        for (int x = 0; x <= gridW; ++x) {
            float worldX = -halfSize + x * step;
            float worldY = getDuneHeight(worldX, worldZ);
            glm::vec3 norm = getDuneNormal(worldX, worldZ);
            glm::vec2 uv(worldX * 0.05f, worldZ * 0.05f);

            v.push_back({ glm::vec3(worldX, worldY, worldZ), norm, uv });
        }
    }

    for (int z = 0; z < gridH; ++z) {
        for (int x = 0; x < gridW; ++x) {
            unsigned int row1 = z * (gridW + 1);
            unsigned int row2 = (z + 1) * (gridW + 1);

            idx.push_back(row1 + x);
            idx.push_back(row2 + x);
            idx.push_back(row1 + x + 1);

            idx.push_back(row1 + x + 1);
            idx.push_back(row2 + x);
            idx.push_back(row2 + x + 1);
        }
    }

    return uploadMesh(v, idx);
}

// 6. Billboard Quad for blowing sand & particles
Mesh createQuad() {
    std::vector<Vertex> v = {
        { {-0.5f, -0.5f, 0.0f}, {0, 0, 1}, {0, 0} },
        { { 0.5f, -0.5f, 0.0f}, {0, 0, 1}, {1, 0} },
        { { 0.5f,  0.5f, 0.0f}, {0, 0, 1}, {1, 1} },
        { {-0.5f,  0.5f, 0.0f}, {0, 0, 1}, {0, 1} }
    };
    std::vector<unsigned int> idx = { 0, 1, 2, 0, 2, 3 };
    return uploadMesh(v, idx);
}

Mesh createRing() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    for (int i = 0; i <= 80; ++i) for (int j = 0; j <= 8; ++j) {
        float a = i * glm::two_pi<float>() / 80;
        float b = j * glm::two_pi<float>() / 8;
        glm::vec3 n(std::cos(a) * std::cos(b), std::sin(b), std::sin(a) * std::cos(b));
        vertices.push_back({glm::vec3(std::cos(a), 0, std::sin(a)) + n * 0.025f, n, {i / 80.0f, j / 8.0f}});
    }
    for (int i = 0; i < 80; ++i) for (int j = 0; j < 8; ++j) {
        unsigned int a = i * 9 + j, b = a + 9;
        indices.insert(indices.end(), {a, a + 1, b, b, a + 1, b + 1});
    }
    return uploadMesh(vertices, indices);
}

Mesh createErodedRock() {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;
    auto point = [](int lat, int lon) {
        float a = lat * glm::pi<float>() / 8, b = lon * glm::two_pi<float>() / 13;
        float erosion = 0.83f + 0.13f * std::sin(lon * 7.1f + lat * 2.3f) + 0.08f * std::cos(lat * 5.4f);
        return glm::vec3(std::sin(a) * std::cos(b) * erosion, std::cos(a), std::sin(a) * std::sin(b) * erosion) * 0.5f;
    };
    auto tri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::length(n) < 0.00001f) return;
        if (glm::dot(n, a + b + c) < 0) { std::swap(b, c); n = -n; }
        n = glm::normalize(n);
        unsigned int start = static_cast<unsigned int>(v.size());
        v.insert(v.end(), {{a, n, {0, 0}}, {b, n, {1, 0}}, {c, n, {1, 1}}});
        idx.insert(idx.end(), {start, start + 1, start + 2});
    };
    for (int i = 0; i < 8; ++i) for (int j = 0; j < 13; ++j) {
        tri(point(i, j), point(i + 1, j), point(i, j + 1));
        tri(point(i, j + 1), point(i + 1, j), point(i + 1, j + 1));
    }
    return uploadMesh(v, idx);
}

// =============================================================================
// GLOBAL MESH ASSETS
// =============================================================================
Mesh g_meshCube;
Mesh g_meshCylinder;
Mesh g_meshSphere;
Mesh g_meshWing;
Mesh g_meshTerrain;
Mesh g_meshQuad;
Mesh g_meshRing;
Mesh g_meshRock;
unsigned int g_particleInstanceVBO = 0;
struct ParticleInstance { glm::vec4 posSize; glm::vec4 colorAlpha; };
std::vector<ParticleInstance> g_particleInstances;

// =============================================================================
// SHADERS & UNIFORMS
// =============================================================================

// 1. Comprehensive Cinematic Surface Shader (Blinn-Phong + Sand Wrap + Shadows + Fog)
const char* SCENE_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform vec3 wormCenter;
uniform float collapse;
uniform int isSand;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLight;

void main(){
    vec4 worldPos = model * vec4(aPos, 1.0);
    if(isSand == 1) {
        float d = length(worldPos.xz - wormCenter.xz);
        worldPos.y -= collapse * (1.0 - smoothstep(5.0, 48.0, d)) * 13.0;
    }
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNorm;
    TexCoord = aUV;
    FragPosLight = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
)";

const char* SCENE_FRAG = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLight;

out vec4 FragColor;

uniform vec3  objectColor;
uniform vec3  specularColor;
uniform float shininess;
uniform vec3  emissiveColor;
uniform int   isSand;

uniform vec3  lightDir;      // Normalized direction toward sun
uniform vec3  sunColor;
uniform vec3  ambientColor;
uniform vec3  viewPos;
uniform sampler2D shadowMap;

// Warm Desert Atmospheric Fog
uniform vec3  fogColor;
uniform float fogDensity;

float calcShadow(vec4 fragLight, vec3 norm){
    vec3 proj = fragLight.xyz / fragLight.w;
    proj = proj * 0.5 + 0.5;
    // Receiver-plane depth adjustment prevents PCF striping at grazing sunlight.
    vec3 dx = dFdx(proj), dy = dFdy(proj);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    vec2 depthSlope = vec2(0);
    if(abs(determinant) > 0.000000001)
        depthSlope = vec2(dx.z * dy.y - dy.z * dx.y, dx.x * dy.z - dy.x * dx.z) / determinant;
    if(proj.z > 1.0 || proj.z < 0.0 || any(lessThan(proj.xy, vec2(0))) || any(greaterThan(proj.xy, vec2(1)))) return 0.0;

    float bias = max(0.0035 * (1.0 - dot(norm, lightDir)), 0.0008);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // 5x5 PCF with a slope-corrected receiver depth at each sample.
    for(int x = -2; x <= 2; ++x){
        for(int y = -2; y <= 2; ++y){
            vec2 offset = vec2(x,y) * texelSize;
            float pcfDepth = texture(shadowMap, proj.xy + offset).r;
            shadow += (proj.z + dot(depthSlope, offset) - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 25.0;
}

void main(){
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);

    vec3 baseCol = objectColor;
    if(isSand == 2) {
        float grain = fract(sin(dot(FragPos, vec3(12.3, 42.8, 27.1))) * 43758.5453);
        float layers = sin(FragPos.y * 5.0 + sin(FragPos.x * 0.8));
        baseCol *= 0.88 + 0.12 * layers + 0.07 * grain;
    }

    // Procedural Dune Sand Micro-Shading
    if(isSand == 1){
        // Fine wind ripples aligned with wind angle
        float ripple1 = sin(FragPos.x * 2.2 + FragPos.z * 1.4) * 0.5 + 0.5;
        float ripple2 = sin(FragPos.x * 0.8 - FragPos.z * 1.9) * 0.5 + 0.5;
        float ripple = mix(ripple1, ripple2, 0.5);

        // Slope shading (crests are sun-bleached golden, valleys are deeper ochre)
        float slope = clamp(dot(N, vec3(0, 1, 0)), 0.0, 1.0);
        baseCol = mix(baseCol * 0.82, baseCol * 1.15, slope);
        float fade = 1.0 - smoothstep(25.0, 120.0, length(viewPos - FragPos));
        baseCol += vec3(0.035, 0.02, 0.008) * (ripple - 0.5) * fade;
        N = normalize(N + vec3(cos(FragPos.x * 2.2 + FragPos.z * 1.4) * 0.055, 0,
                               cos(FragPos.x * 2.2 + FragPos.z * 1.4) * 0.035) * fade);

        // Subtle quartz sand sparkle under direct sun
        float sparkle = pow(max(dot(N, H), 0.0), 120.0);
        float noise = fract(sin(dot(FragPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
        if(noise > 0.88) baseCol += vec3(0.35, 0.3, 0.22) * sparkle * fade;
    }

    // Ambient Term (warm bounce light from vast desert sand)
    vec3 ambient = ambientColor * baseCol;

    // Diffuse Term (Half-Lambert wrap lighting for velvety sand diffusion)
    float NdotL = dot(N, L);
    float diff = isSand == 1 ? max((NdotL + 0.35) / 1.35, 0.0) : max(NdotL, 0.0);
    vec3 diffuse = diff * sunColor * baseCol;

    // Specular Term (Blinn-Phong)
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * specularColor * sunColor;

    // Shadow Calculation
    float shadow = calcShadow(FragPosLight, N);

    vec3 litColor = ambient + (1.0 - shadow) * (diffuse + specular) + emissiveColor;

    // Exponential Squared Desert Distance Fog
    float dist = length(viewPos - FragPos);
    float fogFactor = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 mapped = pow(litColor / (litColor + vec3(0.8)), vec3(1.0 / 1.8));
    FragColor = vec4(mix(mapped, fogColor, fogFactor), 1.0);
}
)";

// 2. Shadow Depth Pass Shader
const char* SHADOW_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform vec3 wormCenter;
uniform float collapse;
uniform int isSand;
void main(){
    vec4 p = model * vec4(aPos, 1.0);
    if(isSand == 1) p.y -= collapse * (1.0 - smoothstep(5.0, 48.0, length(p.xz - wormCenter.xz))) * 13.0;
    gl_Position = lightSpaceMatrix * p;
}
)";
const char* SHADOW_FRAG = R"(
#version 330 core
void main(){}
)";

// 3. Sand Particles Shader (Soft Billboards)
const char* PARTICLE_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 instancePosSize;
layout(location=4) in vec4 instanceColorAlpha;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 camRight;
uniform vec3 camUp;

out vec2 TexCoord;
out vec4 ColorAlpha;

void main(){
    TexCoord = aUV;
    ColorAlpha = instanceColorAlpha;
    vec3 worldPos = instancePosSize.xyz + camRight * (aPos.x * instancePosSize.w) + camUp * (aPos.y * instancePosSize.w);
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

const char* PARTICLE_FRAG = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

in vec4 ColorAlpha;

void main(){
    float dist = length(TexCoord - vec2(0.5));
    if(dist > 0.5) discard;
    float soft = 1.0 - smoothstep(0.0, 0.5, dist);
    FragColor = vec4(ColorAlpha.rgb, soft * ColorAlpha.a);
}
)";

// 4. Clear Blue-Gold Desert Sky and Sun
const char* SKY_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
out vec3 RayDir;
uniform mat4 invViewProj;

void main(){
    vec4 farPoint = invViewProj * vec4(aPos.xy, 1.0, 1.0);
    vec4 nearPoint = invViewProj * vec4(aPos.xy, -1.0, 1.0);
    RayDir = farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w;
    gl_Position = vec4(aPos.xy, 0.9999, 1.0);
}
)";

const char* SKY_FRAG = R"(
#version 330 core
in vec3 RayDir;
out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 sunColor;

void main(){
    vec3 rd = normalize(RayDir);

    // No cloud noise: golden sand haze fades into a clean blue atmosphere.
    vec3 horizonDust = vec3(0.90, 0.74, 0.47);
    vec3 lowBlue = vec3(0.48, 0.69, 0.88);
    vec3 zenithBlue = vec3(0.10, 0.34, 0.72);
    float elevation = clamp(rd.y, 0.0, 1.0);
    vec3 skyBase = mix(horizonDust, lowBlue, smoothstep(0.0, 0.13, elevation));
    skyBase = mix(skyBase, zenithBlue, pow(elevation, 0.65));

    // Crisp warm sun disk surrounded by a restrained atmospheric halo.
    float sunDot = max(dot(rd, normalize(lightDir)), 0.0);
    float sunDisk = smoothstep(cos(radians(0.80)), cos(radians(0.58)), sunDot) * 1.9;
    float sunCorona = pow(sunDot, 180.0) * 0.30;
    float sunHaze = pow(sunDot, 12.0) * 0.10;

    vec3 finalSky = skyBase + (sunDisk + sunCorona + sunHaze) * sunColor;
    FragColor = vec4(finalSky, 1.0);
}
)";

// =============================================================================
// SHADER COMPILATION & MANAGEMENT
// =============================================================================
bool g_shaderOkay = true;
unsigned int compileShaderModule(GLenum type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; char log[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "[Shader Error]: " << log << std::endl;
        g_shaderOkay = false;
    }
    return s;
}

unsigned int buildProgram(const char* vs, const char* fs) {
    unsigned int v = compileShaderModule(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShaderModule(GL_FRAGMENT_SHADER, fs);
    unsigned int p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    int ok; char log[512];
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(p, 512, nullptr, log);
        std::cerr << "[Program Error]: " << log << std::endl;
        g_shaderOkay = false;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

unsigned int g_progScene;
unsigned int g_progShadow;
unsigned int g_progParticle;
unsigned int g_progSky;

// Shadow Map Framebuffer
const int SHADOW_RES = 2048;
unsigned int g_shadowFBO = 0;
unsigned int g_shadowTex = 0;

void setupShadowFramebuffer() {
    glGenFramebuffers(1, &g_shadowFBO);
    glGenTextures(1, &g_shadowTex);
    glBindTexture(GL_TEXTURE_2D, g_shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_RES, SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer is incomplete\n";
        g_shaderOkay = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Skybox fullscreen quad VAO
unsigned int g_skyVAO = 0;
unsigned int g_skyVBO = 0;
void setupSkyVAO() {
    float verts[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    glGenVertexArrays(1, &g_skyVAO);
    glGenBuffers(1, &g_skyVBO);
    glBindVertexArray(g_skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// =============================================================================
// RENDERING PIPELINE & DRAW CALL HELPERS
// =============================================================================
bool g_isShadowPass = false;

struct Material {
    glm::vec3 diffuse   = glm::vec3(0.6f);
    glm::vec3 specular  = glm::vec3(0.3f);
    float     shininess = 32.0f;
    glm::vec3 emissive  = glm::vec3(0.0f);
    int       isSand    = 0;
};

void drawMeshPrimitive(const Mesh& mesh, const glm::mat4& model, const Material& mat) {
    static const GLint shadowModel = glGetUniformLocation(g_progShadow, "model");
    static const GLint shadowSand = glGetUniformLocation(g_progShadow, "isSand");
    static const GLint modelLoc = glGetUniformLocation(g_progScene, "model");
    static const GLint colorLoc = glGetUniformLocation(g_progScene, "objectColor");
    static const GLint specLoc = glGetUniformLocation(g_progScene, "specularColor");
    static const GLint shineLoc = glGetUniformLocation(g_progScene, "shininess");
    static const GLint emitLoc = glGetUniformLocation(g_progScene, "emissiveColor");
    static const GLint sandLoc = glGetUniformLocation(g_progScene, "isSand");
    if (g_isShadowPass) {
        glUniformMatrix4fv(shadowModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(shadowSand, mat.isSand);
    } else {
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(colorLoc, 1, glm::value_ptr(mat.diffuse));
        glUniform3fv(specLoc, 1, glm::value_ptr(mat.specular));
        glUniform1f(shineLoc, mat.shininess);
        glUniform3fv(emitLoc, 1, glm::value_ptr(mat.emissive));
        glUniform1i(sandLoc, mat.isSand);
    }
    glBindVertexArray(mesh.VAO);
    glDrawElements(mesh.drawMode, mesh.indexCount, GL_UNSIGNED_INT, 0);
}

// Helper: transform primitive easily
void drawPart(const Mesh& mesh, glm::mat4 root, glm::vec3 t, glm::vec3 rDeg, glm::vec3 s, const Material& mat) {
    glm::mat4 m = root;
    m = glm::translate(m, t);
    if (rDeg.y != 0.0f) m = glm::rotate(m, glm::radians(rDeg.y), glm::vec3(0, 1, 0));
    if (rDeg.x != 0.0f) m = glm::rotate(m, glm::radians(rDeg.x), glm::vec3(1, 0, 0));
    if (rDeg.z != 0.0f) m = glm::rotate(m, glm::radians(rDeg.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, s);
    drawMeshPrimitive(mesh, m, mat);
}

// =============================================================================
// COLOR PALETTE (CINEMATIC DUNE)
// =============================================================================
const glm::vec3 COL_DUNE_SAND     (0.76f, 0.49f, 0.23f);
const glm::vec3 COL_DUNE_SHADOW   (0.55f, 0.36f, 0.18f);
const glm::vec3 COL_ATREIDES_HULL (0.22f, 0.23f, 0.22f); // Matte carbon stealth grey
const glm::vec3 COL_ATREIDES_DARK (0.13f, 0.14f, 0.13f);
const glm::vec3 COL_CANOPY_GLASS  (0.06f, 0.08f, 0.11f); // Dark tinted armored glass
const glm::vec3 COL_JET_NOZZLE    (0.08f, 0.08f, 0.08f);
const glm::vec3 COL_AFTERBURNER   (1.00f, 0.55f, 0.12f); // Fiery jet reheat
const glm::vec3 COL_SPICE_ORANGE  (0.96f, 0.42f, 0.04f); // Melange spice orange
const glm::vec3 COL_SPICE_GLOW    (1.00f, 0.50f, 0.08f);
const glm::vec3 COL_CRAWLER_TRACK (0.11f, 0.11f, 0.10f);
const glm::vec3 COL_HARV_CHASSIS  (0.42f, 0.34f, 0.25f);
const glm::vec3 COL_HARV_RUST     (0.48f, 0.24f, 0.12f);
const glm::vec3 COL_TANK_STEEL    (0.52f, 0.46f, 0.38f);
const glm::vec3 COL_ROCK_STRATA1  (0.42f, 0.28f, 0.17f);
const glm::vec3 COL_ROCK_STRATA2  (0.32f, 0.20f, 0.11f);

// =============================================================================
// 1. MOVIE-ACCURATE ATREIDES ORNITHOPTER
// 8-blade tandem dragonfly kinematics, faceted insectoid hull, glowing jets
// =============================================================================
void drawMovieOrnithopter(glm::mat4 root, float wingPhase, float roll, float pitch, bool boosting) {
    Material matHull;
    matHull.diffuse = COL_ATREIDES_HULL;
    matHull.specular = glm::vec3(0.4f);
    matHull.shininess = 64.0f;

    Material matDark;
    matDark.diffuse = COL_ATREIDES_DARK;
    matDark.specular = glm::vec3(0.3f);
    matDark.shininess = 32.0f;

    Material matCanopy;
    matCanopy.diffuse = COL_CANOPY_GLASS;
    matCanopy.specular = glm::vec3(0.9f);
    matCanopy.shininess = 128.0f;

    Material matFlame;
    matFlame.diffuse = COL_AFTERBURNER;
    matFlame.emissive = boosting ? glm::vec3(1.2f, 0.6f, 0.15f) : glm::vec3(0.6f, 0.25f, 0.05f);

    Material matJetNozzle;
    matJetNozzle.diffuse = COL_JET_NOZZLE;
    matJetNozzle.specular = glm::vec3(0.5f);
    matJetNozzle.shininess = 64.0f;

    Material matBlade;
    matBlade.diffuse = glm::vec3(0.18f, 0.19f, 0.18f);
    matBlade.specular = glm::vec3(0.7f);
    matBlade.shininess = 90.0f;

    // --- Fuselage Hull: Aerodynamic insectoid/dragonfly pod ---
    // Central pod
    drawPart(g_meshSphere, root, {0.0f, 0.0f, -0.4f}, {0, 0, 0}, {1.3f, 0.85f, 3.2f}, matHull);
    // Tapered forward nose
    drawPart(g_meshCylinder, root, {0.0f, -0.05f, -2.1f}, {85, 0, 0}, {0.7f, 1.4f, 0.45f}, matHull);
    // Forward sensor probe needle
    drawPart(g_meshCylinder, root, {0.0f, -0.1f, -3.1f}, {90, 0, 0}, {0.06f, 1.2f, 0.06f}, matDark);

    // Faceted stealth cockpit canopy (multiple tinted glass facets)
    drawPart(g_meshSphere, root, {0.0f, 0.32f, -1.2f}, {-10, 0, 0}, {0.85f, 0.65f, 1.6f}, matCanopy);
    // Canopy structural spine frame
    drawPart(g_meshCube, root, {0.0f, 0.58f, -1.1f}, {-10, 0, 0}, {0.12f, 0.18f, 1.5f}, matDark);

    // Twin dorsal intake cowls
    drawPart(g_meshCylinder, root, { 0.42f, 0.35f, -0.2f}, {82, 0, 0}, {0.35f, 1.2f, 0.28f}, matHull);
    drawPart(g_meshCylinder, root, {-0.42f, 0.35f, -0.2f}, {82, 0, 0}, {0.35f, 1.2f, 0.28f}, matHull);

    // --- Heavy Shoulder Wing-Gimbals (mounting nacelles) ---
    drawPart(g_meshCylinder, root, { 0.72f, 0.1f, -0.3f}, {0, 0, 90}, {0.45f, 0.65f, 0.45f}, matDark);
    drawPart(g_meshCylinder, root, {-0.72f, 0.1f, -0.3f}, {0, 0, 90}, {0.45f, 0.65f, 0.45f}, matDark);
    drawPart(g_meshCylinder, root, { 0.68f, 0.05f, 0.5f}, {0, 0, 90}, {0.42f, 0.62f, 0.42f}, matDark);
    drawPart(g_meshCylinder, root, {-0.68f, 0.05f, 0.5f}, {0, 0, 90}, {0.42f, 0.62f, 0.42f}, matDark);

    // --- Twin Jet Turbines & Glowing Heat Reheat Exhaust ---
    drawPart(g_meshCylinder, root, { 0.52f, -0.15f, 0.9f}, {90, 0, 0}, {0.42f, 1.6f, 0.42f}, matHull);
    drawPart(g_meshCylinder, root, {-0.52f, -0.15f, 0.9f}, {90, 0, 0}, {0.42f, 1.6f, 0.42f}, matHull);
    // Exhaust nozzles
    drawPart(g_meshCylinder, root, { 0.52f, -0.15f, 1.72f}, {90, 0, 0}, {0.36f, 0.25f, 0.36f}, matJetNozzle);
    drawPart(g_meshCylinder, root, {-0.52f, -0.15f, 1.72f}, {90, 0, 0}, {0.36f, 0.25f, 0.36f}, matJetNozzle);
    // Reheat thrust flame
    float flameLen = boosting ? 1.4f : 0.6f;
    drawPart(g_meshCylinder, root, { 0.52f, -0.15f, 1.8f + flameLen * 0.5f}, {90, 0, 0}, {0.24f, flameLen, 0.24f}, matFlame);
    drawPart(g_meshCylinder, root, {-0.52f, -0.15f, 1.8f + flameLen * 0.5f}, {90, 0, 0}, {0.24f, flameLen, 0.24f}, matFlame);

    // --- Elongated Dragonfly Tail Boom & Canted Stabilizers ---
    drawPart(g_meshCylinder, root, {0.0f, 0.0f, 2.4f}, {90, 0, 0}, {0.32f, 2.6f, 0.28f}, matHull);
    drawPart(g_meshCylinder, root, {0.0f, 0.05f, 4.4f}, {90, 0, 0}, {0.18f, 2.2f, 0.16f}, matDark);
    // Stinger tip
    drawPart(g_meshCylinder, root, {0.0f, 0.05f, 5.7f}, {90, 0, 0}, {0.08f, 1.1f, 0.08f}, matDark);

    // Canted twin V-Tail rudders
    drawPart(g_meshCube, root, { 0.5f, 0.55f, 4.2f}, {0, 0, 38}, {0.08f, 0.9f, 0.65f}, matHull);
    drawPart(g_meshCube, root, {-0.5f, 0.55f, 4.2f}, {0, 0, -38}, {0.08f, 0.9f, 0.65f}, matHull);
    // Ventral landing skids
    drawPart(g_meshCube, root, { 0.45f, -0.6f, -0.4f}, {0, 0, 0}, {0.08f, 0.45f, 2.2f}, matDark);
    drawPart(g_meshCube, root, {-0.45f, -0.6f, -0.4f}, {0, 0, 0}, {0.08f, 0.45f, 2.2f}, matDark);

    // =========================================================================
    // TANDEM DRAGONFLY WINGS (8 BLADES TOTAL — 4 PER SIDE)
    // Dynamic out-of-phase movie oscillation:
    // Upper pair & lower pair flutter in anti-phase with realistic pitch twist!
    // =========================================================================
    float flapAmp = 34.0f; // degrees of flapping
    float flap1 = std::sin(wingPhase) * flapAmp;
    float flap2 = std::sin(wingPhase + glm::pi<float>() * 0.75f) * flapAmp; // anti-phase flutter

    // 4 wings on starboard (+X) and 4 wings on port (-X)
    struct WingDef {
        int side;       // +1: right, -1: left
        glm::vec3 pivot;// gimbal attachment position
        float flapAng;  // current flap sweep
        float sweepBack;// wing sweep angle in degrees
        float dihedral; // resting vertical tilt
    };

    WingDef wings[8] = {
        // Forward Upper Pair
        {  1, {  0.95f,  0.22f, -0.35f },  flap1, 14.0f,  12.0f },
        { -1, { -0.95f,  0.22f, -0.35f },  flap1, 14.0f,  12.0f },
        // Forward Lower Pair (fluttering anti-phase)
        {  1, {  0.95f, -0.05f, -0.25f },  flap2, 18.0f, -10.0f },
        { -1, { -0.95f, -0.05f, -0.25f },  flap2, 18.0f, -10.0f },
        // Aft Upper Pair
        {  1, {  0.90f,  0.18f,  0.50f }, -flap2, 28.0f,   8.0f },
        { -1, { -0.90f,  0.18f,  0.50f }, -flap2, 28.0f,   8.0f },
        // Aft Lower Pair
        {  1, {  0.90f, -0.08f,  0.60f }, -flap1, 32.0f, -12.0f },
        { -1, { -0.90f, -0.08f,  0.60f }, -flap1, 32.0f, -12.0f },
    };

    for (const auto& w : wings) {
        glm::mat4 wm = root;
        wm = glm::translate(wm, w.pivot);

        // Flapping rotation around longitudinal/roll axis
        wm = glm::rotate(wm, glm::radians(w.side * (w.dihedral + w.flapAng)), glm::vec3(0, 0, 1));
        // Backward sweep
        wm = glm::rotate(wm, glm::radians(w.side * w.sweepBack), glm::vec3(0, 1, 0));
        // Pitch twist during stroke
        float pitchTwist = std::cos(wingPhase) * 8.0f * (float)w.side;
        wm = glm::rotate(wm, glm::radians(pitchTwist), glm::vec3(1, 0, 0));

        // Mirroring reverses winding; render both sides of thin aerofoils.
        wm = glm::scale(wm, glm::vec3(w.side * 5.2f, 1.0f, 1.0f));

        glDisable(GL_CULL_FACE);
        drawMeshPrimitive(g_meshWing, wm, matBlade);
        glEnable(GL_CULL_FACE);
    }
}

// =============================================================================
// 2. MASSIVE INDUSTRIAL SPICE HARVESTER
// Quad caterpillar treads, rotary crusher scoop, multi-deck refinery
// =============================================================================
void drawSpiceHarvester(glm::mat4 root, float crawlerAnim) {
    Material matChassis; matChassis.diffuse = COL_HARV_CHASSIS; matChassis.specular = glm::vec3(0.2f); matChassis.shininess = 20.0f;
    Material matTread;   matTread.diffuse   = COL_CRAWLER_TRACK; matTread.specular = glm::vec3(0.15f); matTread.shininess = 10.0f;
    Material matRust;    matRust.diffuse    = COL_HARV_RUST;    matRust.specular = glm::vec3(0.2f); matRust.shininess = 16.0f;
    Material matSpice;   matSpice.diffuse   = COL_SPICE_ORANGE; matSpice.emissive = glm::vec3(0.8f, 0.35f, 0.05f);
    Material matGlass;   matGlass.diffuse   = glm::vec3(0.1f, 0.2f, 0.25f); matGlass.specular = glm::vec3(0.8f); matGlass.shininess = 90.0f;
    Material matLight;   matLight.diffuse   = glm::vec3(1.0f, 0.95f, 0.7f); matLight.emissive = glm::vec3(1.2f, 1.1f, 0.8f);

    // Main heavy crawler body / hopper
    drawPart(g_meshCube, root, {0.0f, 2.6f, 0.0f}, {0, 0, 0}, {9.5f, 3.8f, 6.2f}, matChassis);
    // Upper refinery deck
    drawPart(g_meshCube, root, {0.0f, 4.8f, -0.4f}, {0, 0, 0}, {8.2f, 1.2f, 5.0f}, matChassis);

    // --- Quad Giant Caterpillar Track Units (Front-L, Front-R, Rear-L, Rear-R) ---
    float trackOffsetsX[2] = { -4.6f, 4.6f };
    float trackOffsetsZ[2] = { -2.4f, 2.4f };
    for (float ox : trackOffsetsX) {
        for (float oz : trackOffsetsZ) {
            glm::mat4 tm = glm::translate(root, glm::vec3(ox, 0.95f, oz));
            // Track pontoon housing
            drawPart(g_meshCube, tm, {0, 0, 0}, {0, 0, 0}, {1.8f, 1.7f, 3.8f}, matTread);
            // Drive sprockets & road wheels
            for (float wz = -1.4f; wz <= 1.4f; wz += 0.93f) {
                drawPart(g_meshCylinder, tm, {ox > 0 ? 0.95f : -0.95f, -0.2f, wz}, {0, 0, 90}, {1.2f, 0.35f, 1.2f}, matChassis);
                drawPart(g_meshCube, tm, {ox > 0 ? 1.15f : -1.15f, -0.2f, wz}, {crawlerAnim * 80, 0, 0}, {0.04f, 0.95f, 0.12f}, matRust);
            }
            for (int pad = 0; pad < 10; ++pad) {
                float a = pad * glm::two_pi<float>() / 10 + crawlerAnim * 0.7f;
                float angle = -glm::degrees(std::atan2(0.85f * std::cos(a), -1.8f * std::sin(a)));
                drawPart(g_meshCube, tm, {0, 0.85f * std::sin(a), 1.8f * std::cos(a)}, {angle, 0, 0}, {1.9f, 0.12f, 0.38f}, matRust);
            }
        }
    }

    // --- Forward Harvesting Cutter Drum / Scoop ---
    // Massive rotating cylinder with teeth
    drawPart(g_meshCylinder, root, {0.0f, 1.1f, -4.2f}, {crawlerAnim * 70, 0, 90}, {2.2f, 8.8f, 2.2f}, matRust);
    Material matDoor; matDoor.diffuse = {0.06f, 0.07f, 0.06f};
    drawPart(g_meshCube, root, {4.78f, 1.8f, 1.2f}, {0, 0, 0}, {0.12f, 2.4f, 1.5f}, matDoor);
    drawPart(g_meshCube, root, {5.5f, 0.45f, 1.2f}, {0, 0, -16}, {2, 0.15f, 1.7f}, matChassis);
    // Glowing spice intake suction scoop inside drum
    drawPart(g_meshCube, root, {0.0f, 0.9f, -4.8f}, {15, 0, 0}, {8.2f, 0.6f, 1.4f}, matSpice);

    // Side intake separator funnels
    drawPart(g_meshCylinder, root, { 4.8f, 2.2f, -1.8f}, {0, 0, -22}, {1.2f, 2.4f, 1.2f}, matRust);
    drawPart(g_meshCylinder, root, {-4.8f, 2.2f, -1.8f}, {0, 0,  22}, {1.2f, 2.4f, 1.2f}, matRust);

    // --- Overhead Bridge Observation Deck ---
    drawPart(g_meshCube, root, {2.8f, 5.8f, -2.2f}, {0, 0, 0}, {2.6f, 1.4f, 2.0f}, matChassis);
    // Command bridge panoramic viewports
    drawPart(g_meshCube, root, {2.8f, 5.9f, -3.25f}, {0, 0, 0}, {2.4f, 0.65f, 0.2f}, matGlass);
    // Forward floodlights illuminating desert sand
    drawPart(g_meshSphere, root, { 1.8f, 5.2f, -3.3f}, {0, 0, 0}, {0.35f, 0.35f, 0.35f}, matLight);
    drawPart(g_meshSphere, root, { 3.8f, 5.2f, -3.3f}, {0, 0, 0}, {0.35f, 0.35f, 0.35f}, matLight);

    // --- Industrial Exhaust Stacks & Spice Smoke Vents ---
    drawPart(g_meshCylinder, root, { 1.6f, 6.2f, 1.8f}, {0, 0, 0}, {0.75f, 2.6f, 0.75f}, matChassis);
    drawPart(g_meshCylinder, root, {-1.6f, 6.2f, 1.8f}, {0, 0, 0}, {0.75f, 2.6f, 0.75f}, matChassis);
    // Stack glowing spice vapor vent rims
    drawPart(g_meshCylinder, root, { 1.6f, 7.55f, 1.8f}, {0, 0, 0}, {0.85f, 0.2f, 0.85f}, matSpice);
    drawPart(g_meshCylinder, root, {-1.6f, 7.55f, 1.8f}, {0, 0, 0}, {0.85f, 0.2f, 0.85f}, matSpice);
}

// =============================================================================
// 3. MOVIE-REALISTIC SPICE PRESSURE TANKS
// Heavy pressure cylinder, hemispherical heads, diagonal transport cradle, glowing spice level
// =============================================================================
void drawMovieSpiceTank(glm::mat4 root) {
    Material matCradle; matCradle.diffuse = glm::vec3(0.24f, 0.23f, 0.22f); matCradle.specular = glm::vec3(0.3f); matCradle.shininess = 32.0f;
    Material matVessel; matVessel.diffuse = COL_TANK_STEEL; matVessel.specular = glm::vec3(0.6f); matVessel.shininess = 64.0f;
    Material matSeam;   matSeam.diffuse   = glm::vec3(0.18f, 0.17f, 0.16f); matSeam.specular = glm::vec3(0.2f); matSeam.shininess = 20.0f;
    Material matSpice;  matSpice.diffuse  = COL_SPICE_ORANGE; matSpice.emissive = glm::vec3(1.2f, 0.55f, 0.08f);

    // --- Heavy Structural Transport Cradle with Diagonal Trusses ---
    // Ground skid beams
    drawPart(g_meshCube, root, { 1.35f, 0.18f, 0.0f}, {0, 0, 0}, {0.35f, 0.35f, 4.4f}, matCradle);
    drawPart(g_meshCube, root, {-1.35f, 0.18f, 0.0f}, {0, 0, 0}, {0.35f, 0.35f, 4.4f}, matCradle);
    // Cross supports
    drawPart(g_meshCube, root, {0.0f, 0.25f,  1.6f}, {0, 0, 0}, {3.0f, 0.25f, 0.35f}, matCradle);
    drawPart(g_meshCube, root, {0.0f, 0.25f, -1.6f}, {0, 0, 0}, {3.0f, 0.25f, 0.35f}, matCradle);
    // Cradle vertical stanchions & saddles
    for (float z : { -1.3f, 1.3f }) {
        drawPart(g_meshCube, root, { 1.3f, 1.1f, z}, {0, 0, -22}, {0.28f, 1.8f, 0.35f}, matCradle);
        drawPart(g_meshCube, root, {-1.3f, 1.1f, z}, {0, 0,  22}, {0.28f, 1.8f, 0.35f}, matCradle);
    }

    // --- Horizontal Cylindrical Pressure Vessel ---
    // Central cylinder (diameter: 2.2, length: 3.2 along Z)
    drawPart(g_meshCylinder, root, {0.0f, 1.55f, 0.0f}, {90, 0, 0}, {2.2f, 3.2f, 2.2f}, matVessel);
    // Hemispherical domed heads (front and rear caps)
    drawPart(g_meshSphere, root, {0.0f, 1.55f,  1.6f}, {0, 0, 0}, {2.2f, 2.2f, 1.3f}, matVessel);
    drawPart(g_meshSphere, root, {0.0f, 1.55f, -1.6f}, {0, 0, 0}, {2.2f, 2.2f, 1.3f}, matVessel);

    // Reinforcing stiffener weld bands
    drawPart(g_meshCylinder, root, {0.0f, 1.55f,  0.8f}, {90, 0, 0}, {2.28f, 0.18f, 2.28f}, matSeam);
    drawPart(g_meshCylinder, root, {0.0f, 1.55f, -0.8f}, {90, 0, 0}, {2.28f, 0.18f, 2.28f}, matSeam);

    // Top sealed loading hatch & relief valve
    drawPart(g_meshCylinder, root, {0.0f, 2.72f, 0.0f}, {0, 0, 0}, {0.65f, 0.25f, 0.65f}, matSeam);
    drawPart(g_meshSphere,   root, {0.0f, 2.92f, 0.0f}, {0, 0, 0}, {0.4f, 0.25f, 0.4f}, matSpice);

    // Glowing Vertical Spice Level Sight Tube (illuminated Melange spice indicator)
    drawPart(g_meshCube, root, { 1.15f, 1.55f, 0.0f}, {0, 0, 0}, {0.08f, 1.5f, 0.18f}, matSpice);
}

// =============================================================================
// 4. REALISTIC DESERT ROCK YARDANG FORMATIONS
// Wind-carved stratified sandstone crags with sediment bands
// =============================================================================
void drawDesertRockFormation(glm::mat4 root, int variant) {
    Material matStrata1; matStrata1.diffuse = COL_ROCK_STRATA1; matStrata1.specular = glm::vec3(0.15f); matStrata1.shininess = 16.0f;
    Material matStrata2; matStrata2.diffuse = COL_ROCK_STRATA2; matStrata2.specular = glm::vec3(0.12f); matStrata2.shininess = 12.0f;

    matStrata1.isSand = matStrata2.isSand = 2;
    drawPart(g_meshRock, root, {0, 2.2f, 0}, {8, float(variant * 19), -5}, {8, 7, 6}, matStrata1);
    drawPart(g_meshRock, root, {1, 5, -1}, {0, 55, 10}, {5, 5, 4}, matStrata2);
    drawPart(g_meshRock, root, {-3, 0.8f, 2}, {5, -40, 12}, {4, 3, 3}, matStrata2);
    drawPart(g_meshRock, root, {3.2f, 0.3f, 2.2f}, {15, 70, 20}, {2.2f, 1.6f, 2.1f}, matStrata1);
}

#include "arrakis_worm.h"
#include "arrakis_hud.h"

void drawRescueWorld(float time) {
    Material cyan; cyan.diffuse = {0.05f, 0.5f, 0.55f}; cyan.emissive = {0.03f, 0.6f, 0.65f};
    Material amber; amber.diffuse = {0.9f, 0.5f, 0.1f}; amber.emissive = {0.6f, 0.25f, 0.03f};
    Material dark; dark.diffuse = {0.16f, 0.19f, 0.19f};
    Material cloth; cloth.diffuse = {0.30f, 0.24f, 0.17f}; cloth.specular = {0.03f, 0.03f, 0.03f};
    Material visor; visor.diffuse = {0.08f, 0.20f, 0.27f}; visor.specular = {0.8f, 0.8f, 0.8f}; visor.shininess = 90;
    glm::mat4 base = glm::translate(glm::mat4(1), g_mission.base);
    drawPart(g_meshCylinder, base, {0, 0.25f, 0}, {0, 0, 0}, {24, 0.5f, 24}, dark);
    drawPart(g_meshRing, base, {0, 0.55f, 0}, {0, 0, 0}, {11, 1, 11}, cyan);
    drawPart(g_meshCube, base, {0, 0.56f, 0}, {0, 0, 0}, {1.2f, 0.05f, 10}, cyan);
    drawPart(g_meshCube, base, {0, 0.56f, 0}, {0, 0, 0}, {10, 0.05f, 1.2f}, cyan);
    for (int i = 0; i < 4; ++i) {
        float a = i * glm::half_pi<float>();
        drawPart(g_meshCylinder, base, {std::cos(a) * 13, 2, std::sin(a) * 13}, {0, 0, 0}, {0.2f, 4, 0.2f}, dark);
        drawPart(g_meshSphere, base, {std::cos(a) * 13, 4.1f, std::sin(a) * 13}, {0, 0, 0}, {0.65f, 0.65f, 0.65f}, cyan);
    }
    for (int groupIndex = 0; groupIndex < static_cast<int>(g_mission.groups.size()); ++groupIndex) {
        const auto& group = g_mission.groups[groupIndex];
        if (!group.released) continue;
        int waiting = 0;
        for (const auto& c : g_mission.crew)
            if (c.group == groupIndex && (c.state == CrewState::Running || c.state == CrewState::Waiting)) ++waiting;
        if (waiting == 0) continue;
        glm::vec3 p = group.center;
        p.y = getDuneHeight(p.x, p.z) + 0.2f;
        glm::mat4 ring = glm::translate(glm::mat4(1), p);
        glm::vec3 normal = getDuneNormal(p.x, p.z), axis = glm::cross(glm::vec3(0, 1, 0), normal);
        if (glm::length(axis) > 0.001f) ring = glm::rotate(ring, std::acos(glm::clamp(normal.y, -1.0f, 1.0f)), glm::normalize(axis));
        drawPart(g_meshRing, ring, {0, 0, 0}, {0, 0, 0}, {4.5f, 1, 4.5f}, amber);
        drawPart(g_meshCylinder, glm::translate(glm::mat4(1), p), {4, 2, 0}, {0, 0, 0}, {0.12f, 4, 0.12f}, dark);
        drawPart(g_meshSphere, glm::translate(glm::mat4(1), p), {4, 4.1f, 0}, {0, 0, 0}, {0.5f, 0.5f, 0.5f}, amber);
    }
    for (int i = 0; i < int(g_mission.crew.size()); ++i) {
        const auto& c = g_mission.crew[i];
        if (c.state != CrewState::Waiting && c.state != CrewState::Running) continue;
        glm::vec3 crewPosition = c.pos;
        if (i == g_mission.target && g_mission.pickup > 0)
            crewPosition.y += (g_ornPos.y - crewPosition.y - 1.8f) * glm::smoothstep(0.0f, 1.0f, g_mission.pickup);
        glm::mat4 root = glm::translate(glm::mat4(1), crewPosition);
        float stride = c.state == CrewState::Running ? std::sin(time * 11 + i) * 24 : std::sin(time * 2 + i) * 4;
        drawPart(g_meshSphere, root, {0, 1.05f, 0}, {0, 0, 0}, {0.55f, 0.85f, 0.4f}, cloth);
        drawPart(g_meshSphere, root, {0, 1.65f, 0}, {0, 0, 0}, {0.43f, 0.46f, 0.43f}, cloth);
        drawPart(g_meshSphere, root, {0, 1.67f, 0.17f}, {0, 0, 0}, {0.32f, 0.15f, 0.10f}, visor);
        for (int side : {-1, 1}) {
            drawPart(g_meshCylinder, root, {side * 0.14f, 0.4f, 0}, {side * stride, 0, 0}, {0.17f, 0.8f, 0.17f}, cloth);
            drawPart(g_meshCylinder, root, {side * 0.34f, 1.1f, 0}, {-side * stride, 0, side * 14.0f}, {0.15f, 0.7f, 0.15f}, cloth);
        }
        drawPart(g_meshSphere, root, {0, 2.25f, 0}, {0, 0, 0}, {0.18f, 0.18f, 0.18f}, cyan);
    }
    if (g_mission.pickup > 0 && g_mission.target >= 0) {
        glm::vec3 end = g_mission.crew[g_mission.target].pos + glm::vec3(0, 1, 0);
        end.y += (g_ornPos.y - end.y - 0.8f) * glm::smoothstep(0.0f, 1.0f, g_mission.pickup);
        glm::vec3 start = g_ornPos - glm::vec3(0, 0.7f, 0);
        glm::vec3 dir = end - start;
        float length = glm::length(dir);
        glm::mat4 rope = glm::translate(glm::mat4(1), (start + end) * 0.5f);
        glm::vec3 axis = glm::cross(glm::vec3(0, 1, 0), dir / length);
        if (glm::length(axis) > 0.001f) rope = glm::rotate(rope, std::acos(glm::clamp(dir.y / length, -1.0f, 1.0f)), glm::normalize(axis));
        else if (dir.y < 0) rope = glm::rotate(rope, glm::pi<float>(), glm::vec3(1, 0, 0));
        drawPart(g_meshCylinder, rope, {0, 0, 0}, {0, 0, 0}, {0.055f, length, 0.055f}, cyan);
    }
    drawSandworm(wormPosition(), wormAttackTime(), time, g_mission.wormYaw,
                 glm::clamp(g_mission.elapsed / g_mission.breachAt, 0.0f, 1.0f));
}

// =============================================================================
// MASTER SCENE DRAW CALL
// Executed twice per frame: once for the shadow map, once for the main view
// =============================================================================
void drawFullScene(float curTime) {
    // 1. Vast Continuous Sand Dune Landscape
    Material matSand;
    matSand.diffuse = COL_DUNE_SAND;
    matSand.specular = glm::vec3(0.25f);
    matSand.shininess = 16.0f;
    matSand.isSand = 1;
    drawMeshPrimitive(g_meshTerrain, glm::mat4(1.0f), matSand);

    // 2. Movie-Accurate Atreides Ornithopter
    if (!g_mission.planeLost) {
        glm::mat4 om = glm::translate(glm::mat4(1.0f), g_ornPos);
        om = glm::rotate(om, glm::radians(g_ornYaw), glm::vec3(0, 1, 0));
        om = glm::rotate(om, glm::radians(g_ornPitch), glm::vec3(1, 0, 0));
        om = glm::rotate(om, glm::radians(g_ornRoll), glm::vec3(0, 0, 1));
        drawMovieOrnithopter(om, g_wingPhase, g_ornRoll, g_ornPitch, g_isBoosting);
    }

    // 3. Massive Industrial Spice Harvester (hugging the dune surface!)
    if (wormAttackTime() < 12) {
        float hy = getDuneHeight(g_harvX, g_harvZ);
        float sink = glm::smoothstep(6.0f, 12.0f, wormAttackTime());
        hy -= sink * 20;
        glm::mat4 hm = glm::translate(glm::mat4(1.0f), glm::vec3(g_harvX, hy, g_harvZ));
        hm = glm::rotate(hm, glm::radians(g_mission.harvesterYaw), glm::vec3(0, 1, 0));
        // Apply dune slope in the crawler's own forward/right axes.
        glm::vec3 hnorm = getDuneNormal(g_harvX, g_harvZ);
        glm::vec3 forward = harvesterForward(g_mission.harvesterYaw);
        glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
        hm = glm::rotate(hm, std::atan2(-glm::dot(hnorm, forward), hnorm.y), glm::vec3(1, 0, 0));
        hm = glm::rotate(hm, std::atan2(-glm::dot(hnorm, right), hnorm.y), glm::vec3(0, 0, 1));
        hm = glm::rotate(hm, sink * 0.8f, glm::vec3(1, 0, 0));
        drawSpiceHarvester(hm, g_mission.harvesterTravel);
    }

    // 4. Dynamic Spice Tanks (deployed behind Harvester onto dune surface)
    for (const auto& tank : g_tanks) {
        if (wormAttackTime() > 12) continue;
        float ty = getDuneHeight(tank.pos.x, tank.pos.z);
        glm::mat4 tm = glm::translate(glm::mat4(1.0f), glm::vec3(tank.pos.x, ty, tank.pos.z));
        tm = glm::rotate(tm, glm::radians(tank.yaw), glm::vec3(0, 1, 0));
        tm = glm::scale(tm, glm::vec3(tank.scale));
        drawMovieSpiceTank(tm);
    }

    // 5. Desert Rock Yardang Crags (at natural elevated points across the desert)
    for (int i = 0; i < 8; ++i) {
        float rx = g_rockSites[i].x;
        float rz = g_rockSites[i].y;
        float ry = getDuneHeight(rx, rz) - 0.5f;
        glm::mat4 rm = glm::translate(glm::mat4(1.0f), glm::vec3(rx, ry, rz));
        rm = glm::rotate(rm, glm::radians((float)(i * 45)), glm::vec3(0, 1, 0));
        drawDesertRockFormation(rm, i);
    }
    drawRescueWorld(curTime);
}

// =============================================================================
// GLFW CALLBACKS
// =============================================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    if (w > 0 && h > 0) {
        g_scrW = w;
        g_scrH = h;
        glViewport(0, 0, w, h);
    }
}

void centerMouse(GLFWwindow* win) {
    int width, height;
    glfwGetWindowSize(win, &width, &height);
    g_mouseHover = glm::vec2(0);
    g_mouseTurnRate = 0;
    glfwSetCursorPos(win, width * 0.5, height * 0.5);
}

void cursor_position_callback(GLFWwindow* win, double x, double y) {
    if (g_mission.phase != MissionPhase::Flying || g_mission.paused
        || !glfwGetWindowAttrib(win, GLFW_FOCUSED)) {
        g_mouseHover = glm::vec2(0);
        g_mouseTurnRate = 0;
        return;
    }
    // Cursor coordinates are logical window pixels, not framebuffer pixels.
    int width, height;
    glfwGetWindowSize(win, &width, &height);
    setMouseHover(x, y, width, height);
}

void key_callback(GLFWwindow* win, int key, int, int action, int) {
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS)   g_keys[key] = true;
        if (action == GLFW_RELEASE) g_keys[key] = false;
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, true);
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        g_wireframe = !g_wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);
    }
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        g_camMode = (g_camMode + 1) % 3;
    }
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
        if (g_mission.phase == MissionPhase::Title) resetMission(true);
        else if (g_mission.phase == MissionPhase::Debrief) resetMission(true, true);
        centerMouse(win);
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) { resetMission(true); centerMouse(win); }
    if (key == GLFW_KEY_P && action == GLFW_PRESS && g_mission.phase == MissionPhase::Flying) {
        g_mission.paused = !g_mission.paused;
        centerMouse(win);
    }
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        g_isFullscreen = !g_isFullscreen;
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (g_isFullscreen) {
            glfwSetWindowMonitor(win, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(win, nullptr, 100, 100, 1280, 720, 0);
        }
        centerMouse(win);
    }
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
bool captureScreenshot(const std::string& path) {
    std::vector<unsigned char> pixels(static_cast<size_t>(g_scrW) * g_scrH * 4);
    glReadPixels(0, 0, g_scrW, g_scrH, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto word = [&](uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (i * 8)) & 255));
    };
    out.put('B'); out.put('M'); word(static_cast<uint32_t>(54 + pixels.size()), 4);
    word(0, 4); word(54, 4); word(40, 4); word(g_scrW, 4); word(g_scrH, 4);
    word(1, 2); word(32, 2); word(0, 4); word(static_cast<uint32_t>(pixels.size()), 4);
    word(0, 4); word(0, 4); word(0, 4); word(0, 4);
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return out.good();
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--test-game") return runMissionTests();
    bool smokeTest = argc > 1 && std::string(argv[1]) == "--smoke-test";
    bool breachTest = argc > 1 && std::string(argv[1]) == "--smoke-breach";
    bool mouseTest = argc > 1 && std::string(argv[1]) == "--smoke-mouse";
    bool pursuitTest = argc > 1 && std::string(argv[1]) == "--smoke-pursuit";
    bool renderTest = smokeTest || breachTest || mouseTest || pursuitTest;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    glfwWindowHint(GLFW_SAMPLES, 4);
    // Windowed startup leaves the desktop accessible; F11 enters fullscreen.
    g_window = glfwCreateWindow(g_scrW, g_scrH,
        "ARRAKIS | Harvester Down - Desert Rescue",
        nullptr, nullptr);

    if (!g_window) {
        std::cerr << "Window creation failed, falling back to windowed mode..." << std::endl;
        g_isFullscreen = false;
        g_scrW = 1280; g_scrH = 720;
        g_window = glfwCreateWindow(g_scrW, g_scrH, "ARRAKIS", nullptr, nullptr);
        if (!g_window) { glfwTerminate(); return -1; }
    }

    glfwMakeContextCurrent(g_window);
    glfwGetFramebufferSize(g_window, &g_scrW, &g_scrH);
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetKeyCallback(g_window, key_callback);
    glfwSetCursorPosCallback(g_window, cursor_position_callback);
    glfwSetCursorEnterCallback(g_window, [](GLFWwindow*, int) {
        g_mouseHover = glm::vec2(0);
        g_mouseTurnRate = 0;
    });
    glfwSetWindowSizeCallback(g_window, [](GLFWwindow* win, int, int) { centerMouse(win); });
    glfwSetWindowFocusCallback(g_window, [](GLFWwindow*, int focused) {
        if (!focused) {
            std::fill(std::begin(g_keys), std::end(g_keys), false);
            g_mouseHover = glm::vec2(0);
            g_mouseTurnRate = 0;
            if (g_mission.phase == MissionPhase::Flying) g_mission.paused = true;
        }
    });
    glfwSwapInterval(1); // Enable VSync for buttery smooth frames

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "========================================================\n";
    std::cout << " ARRAKIS / Harvester Down / Desert Rescue\n";
    std::cout << " GPU: " << glGetString(GL_RENDERER) << "\n";
    std::cout << " OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << " Resolution: " << g_scrW << "x" << g_scrH << "\n";
    std::cout << "========================================================\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Build procedural geometry
    g_meshCube     = createCube();
    g_meshCylinder = createCylinder(24);
    g_meshSphere   = createSphere(16, 24);
    g_meshWing     = createWingBlade();
    g_meshTerrain  = createDuneTerrain(260, 260, 900.0f);
    g_meshQuad     = createQuad();
    g_meshRing     = createRing();
    g_meshRock     = createErodedRock();
    initSandwormMeshes();

    // Compile shader programs
    g_progScene    = buildProgram(SCENE_VERT, SCENE_FRAG);
    g_progShadow   = buildProgram(SHADOW_VERT, SHADOW_FRAG);
    g_progParticle = buildProgram(PARTICLE_VERT, PARTICLE_FRAG);
    g_progSky      = buildProgram(SKY_VERT, SKY_FRAG);

    setupShadowFramebuffer();
    setupSkyVAO();
    initRescueHud();
    if (!g_shaderOkay || !arrakis_hud_detail::resources().program) {
        glfwDestroyWindow(g_window); glfwTerminate(); return 1;
    }
    initParticles();
    glGenBuffers(1, &g_particleInstanceVBO);
    glBindVertexArray(g_meshQuad.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_particleInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(ParticleInstance), nullptr, GL_STREAM_DRAW);
    for (int i = 0; i < 2; ++i) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(3 + i, 1);
    }
    glBindVertexArray(0);
    g_particleInstances.reserve(MAX_PARTICLES);
    resetMission(renderTest);
    centerMouse(g_window);
    if (breachTest || pursuitTest) {
        float targetTime = breachTest ? g_mission.breachAt + 7 : 110.0f;
        int steps = static_cast<int>(std::ceil(targetTime * 120));
        for (int i = 0; i < steps; ++i) updateMission(targetTime / steps);
        g_ornPos = harvesterPosition() + glm::vec3(35, 16, 40);
        g_mission.altitude = 16;
    }

    // Environment Lighting Constants
    glm::vec3 sunDir = glm::normalize(glm::vec3(-0.35f, 0.12f, -1.0f));
    glm::vec3 sunColor(1.15f, 1.04f, 0.85f);
    glm::vec3 ambientColor(0.34f, 0.38f, 0.44f);
    glm::vec3 fogColor(0.90f, 0.74f, 0.47f);
    float     fogDensity = 0.0022f;

    float prevTime = (float)glfwGetTime();

    // Camera damping state
    glm::vec3 camPosSmoothed = g_ornPos + glm::vec3(0, 26, 42);
    glm::vec3 camTargetSmoothed = g_ornPos;
    float simulationTime = g_mission.elapsed;
    float fps = 60;
    int frameCount = 0;
    int cameraRevision = g_missionRevision;
    bool renderFailed = false;
    double smokeStarted = glfwGetTime();

    // =========================================================================
    // MAIN INTERACTIVE RENDER LOOP
    // =========================================================================
    while (!glfwWindowShouldClose(g_window)) {
        if (glfwGetWindowAttrib(g_window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            prevTime = static_cast<float>(glfwGetTime());
            continue;
        }
        float curTime = (float)glfwGetTime();
        float dt = curTime - prevTime;
        prevTime = curTime;
        if (dt > 0.1f) dt = 0.1f; // Clamp delta time to avoid physics explosion
        float realDt = dt;

        glfwPollEvents();
        if (renderTest) {
            dt = 1.0f / 60; g_mission.paused = false;
            g_mouseHover = glm::vec2(0);
            if (mouseTest) {
                int width, height;
                glfwGetWindowSize(g_window, &width, &height);
                setMouseHover(width * (frameCount < 60 ? 0.85 : 0.15), height * 0.5, width, height);
                g_keys[GLFW_KEY_W] = true;
                g_camMode = (frameCount / 40) % 3;
            }
        }
        fps = glm::mix(fps, 1.0f / std::max(realDt, 0.0001f), 0.03f);
        // Bounded substeps keep winching and flight stable at low frame rates.
        if (!g_mission.paused) {
            int steps = std::max(1, static_cast<int>(std::ceil(dt / (1.0f / 120))));
            for (int i = 0; i < steps; ++i) updateMission(dt / steps);
            // Let the swallowing/retreat animation finish behind the results UI.
            if (g_mission.phase == MissionPhase::Debrief) g_mission.elapsed += dt;
            simulationTime += dt;
            if (g_mission.phase == MissionPhase::Title) g_wingPhase += 18 * dt;
            updateParticles(dt, g_ornPos, harvesterPosition());
        }
        curTime = simulationTime;

        // ---------------------------------------------------------------------
        // 4. CAMERA SYSTEM (CINEMATIC CHASE / COCKPIT / OVERHEAD)
        // ---------------------------------------------------------------------
        if (cameraRevision != g_missionRevision) {
            cameraRevision = g_missionRevision;
            camPosSmoothed = g_ornPos + glm::vec3(0, 14, 38);
            camTargetSmoothed = g_ornPos;
        }
        if (!g_mission.paused) {
            if (g_keys[GLFW_KEY_UP])   g_camOrbitPitch = std::min(g_camOrbitPitch + 40.0f * dt, 45.0f);
            if (g_keys[GLFW_KEY_DOWN]) g_camOrbitPitch = std::max(g_camOrbitPitch - 40.0f * dt, -10.0f);
        }

        glm::vec3 camTarget = g_ornPos;
        glm::vec3 camOffset = flightCameraOffset(g_camMode);
        glm::vec3 camDesiredPos = g_ornPos + camOffset;
        if (breachTest) {
            camTarget = harvesterPosition() + glm::vec3(0, 14, 0);
            camDesiredPos = harvesterPosition() + glm::vec3(65, 48, 80);
        } else if (pursuitTest) {
            camTarget = (harvesterPosition() + wormPosition()) * 0.5f + glm::vec3(0, 6, 0);
            camDesiredPos = camTarget + glm::vec3(65, 30, 155);
        }

        // Camera terrain collision safety
        float camGroundY = getDuneHeight(camDesiredPos.x, camDesiredPos.z) + 1.5f;
        if (camDesiredPos.y < camGroundY) camDesiredPos.y = camGroundY;

        // Smooth translation/elevation, but share the aircraft's exact heading.
        // Smoothing world-space camera X/Z independently would expose its side.
        camPosSmoothed.y = glm::mix(camPosSmoothed.y, camDesiredPos.y, 1.0f - std::exp(-9.0f * dt));
        camTargetSmoothed = glm::mix(camTargetSmoothed, camTarget, 1.0f - std::exp(-12.0f * dt));
        camPosSmoothed.x = camTargetSmoothed.x + camOffset.x;
        camPosSmoothed.z = camTargetSmoothed.z + camOffset.z;
        if (breachTest || pursuitTest) {
            camPosSmoothed.x = camDesiredPos.x;
            camPosSmoothed.z = camDesiredPos.z;
        }
        camPosSmoothed.y = std::max(camPosSmoothed.y, getDuneHeight(camPosSmoothed.x, camPosSmoothed.z) + 2);
        if (mouseTest) {
            glm::vec3 viewForward = camTargetSmoothed - camPosSmoothed;
            viewForward.y = 0;
            if (glm::dot(glm::normalize(viewForward), flightForward()) < 0.9999f) {
                std::cerr << "Camera heading diverged from aircraft\n";
                renderFailed = true; glfwSetWindowShouldClose(g_window, true);
            }
        }

        glm::mat4 view = glm::lookAt(camPosSmoothed, camTargetSmoothed, glm::vec3(0, 1, 0));
        float aspect = (float)g_scrW / (float)g_scrH;
        glm::mat4 proj = glm::perspective(glm::radians(58.0f), aspect, 0.4f, 1200.0f);

        // ---------------------------------------------------------------------
        // 5. SHADOW MAP PASS (PASS 1)
        // ---------------------------------------------------------------------
        g_isShadowPass = true;
        glViewport(0, 0, SHADOW_RES, SHADOW_RES);
        glBindFramebuffer(GL_FRAMEBUFFER, g_shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Follow player area with orthographic shadow frustum
        glm::vec3 shadowCenter = g_ornPos;
        shadowCenter.y = getDuneHeight(shadowCenter.x, shadowCenter.z);
        float shadowBoxSize = 90.0f;
        glm::mat4 lightProj = glm::ortho(-shadowBoxSize, shadowBoxSize, -shadowBoxSize, shadowBoxSize, 2.0f, 320.0f);
        glm::vec3 lightCamPos = shadowCenter + sunDir * 120.0f;
        glm::mat4 lightView = glm::lookAt(lightCamPos, shadowCenter, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProj * lightView;

        glUseProgram(g_progShadow);
        glUniformMatrix4fv(glGetUniformLocation(g_progShadow, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glm::vec3 wormCenter = wormPosition();
        float collapse = glm::smoothstep(7.0f, 14.0f, wormAttackTime());
        glUniform3fv(glGetUniformLocation(g_progShadow, "wormCenter"), 1, glm::value_ptr(wormCenter));
        glUniform1f(glGetUniformLocation(g_progShadow, "collapse"), collapse);

        // Slope-scale bias via polygon offset to eliminate shadow acne
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.5f, 4.0f);
        drawFullScene(curTime);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ---------------------------------------------------------------------
        // 6. MAIN SCENE RENDER PASS (PASS 2)
        // ---------------------------------------------------------------------
        g_isShadowPass = false;
        glViewport(0, 0, g_scrW, g_scrH);
        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Step A: Atmospheric Desert Skybox ---
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glUseProgram(g_progSky);
        glUniform3fv(glGetUniformLocation(g_progSky, "lightDir"), 1, glm::value_ptr(sunDir));
        glUniform3fv(glGetUniformLocation(g_progSky, "sunColor"), 1, glm::value_ptr(sunColor));
        glm::mat4 invViewProj = glm::inverse(proj * view);
        glUniformMatrix4fv(glGetUniformLocation(g_progSky, "invViewProj"), 1, GL_FALSE, glm::value_ptr(invViewProj));

        glBindVertexArray(g_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        // --- Step B: 3D Scene Geometry with Blinn-Phong + Shadows + Fog ---
        glUseProgram(g_progScene);
        glUniformMatrix4fv(glGetUniformLocation(g_progScene, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_progScene, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(g_progScene, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glUniform3fv(glGetUniformLocation(g_progScene, "lightDir"), 1, glm::value_ptr(sunDir));
        glUniform3fv(glGetUniformLocation(g_progScene, "sunColor"), 1, glm::value_ptr(sunColor));
        glUniform3fv(glGetUniformLocation(g_progScene, "ambientColor"), 1, glm::value_ptr(ambientColor));
        glUniform3fv(glGetUniformLocation(g_progScene, "viewPos"), 1, glm::value_ptr(camPosSmoothed));
        glUniform3fv(glGetUniformLocation(g_progScene, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f (glGetUniformLocation(g_progScene, "fogDensity"), fogDensity);
        glUniform3fv(glGetUniformLocation(g_progScene, "wormCenter"), 1, glm::value_ptr(wormCenter));
        glUniform1f(glGetUniformLocation(g_progScene, "collapse"), collapse);

        // Bind shadow depth texture to texture slot 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_shadowTex);
        glUniform1i(glGetUniformLocation(g_progScene, "shadowMap"), 0);

        drawFullScene(curTime);

        // --- Step C: Wind & Blowing Sand Particle System ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Don't write depth for soft dust billboards
        glDisable(GL_CULL_FACE);

        glUseProgram(g_progParticle);
        glUniformMatrix4fv(glGetUniformLocation(g_progParticle, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_progParticle, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

        // Extract camera right & up vectors for billboard orientation
        glm::vec3 camRight(view[0][0], view[1][0], view[2][0]);
        glm::vec3 camUp   (view[0][1], view[1][1], view[2][1]);
        glUniform3fv(glGetUniformLocation(g_progParticle, "camRight"), 1, glm::value_ptr(camRight));
        glUniform3fv(glGetUniformLocation(g_progParticle, "camUp"), 1, glm::value_ptr(camUp));

        g_particleInstances.clear();
        for (const auto& p : g_particles) {
            float fade = glm::clamp(p.life / 0.5f, 0.0f, 1.0f) * glm::clamp((p.maxLife - p.life) / 0.3f, 0.0f, 1.0f);
            g_particleInstances.push_back({glm::vec4(p.pos, p.size), glm::vec4(COL_DUNE_SAND, p.alpha * fade)});
        }
        std::sort(g_particleInstances.begin(), g_particleInstances.end(), [&](const ParticleInstance& a, const ParticleInstance& b) {
            glm::vec3 da = glm::vec3(a.posSize) - camPosSmoothed, db = glm::vec3(b.posSize) - camPosSmoothed;
            return glm::dot(da, da) > glm::dot(db, db);
        });
        glBindBuffer(GL_ARRAY_BUFFER, g_particleInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, g_particleInstances.size() * sizeof(ParticleInstance), g_particleInstances.data(), GL_STREAM_DRAW);
        glBindVertexArray(g_meshQuad.VAO);
        glDrawElementsInstanced(GL_TRIANGLES, g_meshQuad.indexCount, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(g_particleInstances.size()));

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        RescueHudState hud;
        hud.width = g_scrW; hud.height = g_scrH;
        hud.rescued = g_mission.rescued; hud.aboard = g_mission.aboard;
        hud.lost = g_mission.lost; hud.best = g_mission.best; hud.wave = g_mission.wave;
        hud.inside = 0;
        for (const auto& c : g_mission.crew) {
            if (c.state == CrewState::Inside) ++hud.inside;
            if (c.state == CrewState::Running || c.state == CrewState::Waiting) {
                ++hud.waiting;
                hud.crewMarkers.push_back({c.pos.x, c.pos.z});
            }
        }
        hud.missionDuration = g_mission.breachAt;
        hud.pursuit = glm::clamp(g_mission.elapsed / g_mission.breachAt, 0.0f, 1.0f);
        hud.wormDistance = wormGap();
        hud.extractionRemaining = std::max(0.0f, 60 - wormAttackTime());
        hud.releasedGroups = 0;
        for (const auto& group : g_mission.groups) if (group.released) ++hud.releasedGroups;
        hud.remaining = std::max(0.0f, g_mission.breachAt - g_mission.elapsed);
        hud.altitude = g_ornPos.y - getDuneHeight(g_ornPos.x, g_ornPos.z);
        hud.speed = g_ornSpeed; hud.boost = g_mission.boost;
        hud.pickupProgress = g_mission.pickup; hud.unloadProgress = g_mission.unload;
        hud.fps = fps; hud.prompt = g_mission.prompt;
        hud.heading = g_ornYaw;
        hud.title = g_mission.phase == MissionPhase::Title;
        hud.ended = g_mission.phase == MissionPhase::Debrief; hud.paused = g_mission.paused;
        hud.player = {g_ornPos.x, g_ornPos.z}; hud.base = {g_mission.base.x, g_mission.base.z};
        hud.harvester = {g_harvX, g_harvZ}; hud.worm = {wormCenter.x, wormCenter.z};
        glm::vec3 pickup = nearestCrewPosition();
        hud.pickup = {pickup.x, pickup.z};
        hud.attackTime = wormAttackTime();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        drawRescueHud(hud);
        glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);

        // ---------------------------------------------------------------------
        // 7. BUFFER SWAP & EVENT POLLING
        // ---------------------------------------------------------------------
        if (renderTest) {
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL error in frame " << frameCount << ": " << error << "\n";
                renderFailed = true; glfwSetWindowShouldClose(g_window, true);
            }
            if (frameCount == 119 && argc > 2 && !captureScreenshot(argv[2])) {
                std::cerr << "Screenshot write failed\n"; renderFailed = true;
            }
        }
        glfwSwapBuffers(g_window);
        if (renderTest && ++frameCount >= 120) glfwSetWindowShouldClose(g_window, true);
    }

    // Cleanup resources
    glDeleteProgram(g_progScene);
    glDeleteProgram(g_progShadow);
    glDeleteProgram(g_progParticle);
    glDeleteProgram(g_progSky);
    glDeleteFramebuffers(1, &g_shadowFBO);
    glDeleteTextures(1, &g_shadowTex);
    glDeleteBuffers(1, &g_particleInstanceVBO);
    glDeleteBuffers(1, &g_skyVBO);
    glDeleteVertexArrays(1, &g_skyVAO);
    cleanupSandwormMeshes();
    cleanupRescueHud();
    for (Mesh* m : {&g_meshCube, &g_meshCylinder, &g_meshSphere, &g_meshWing, &g_meshTerrain, &g_meshQuad, &g_meshRing, &g_meshRock}) {
        glDeleteVertexArrays(1, &m->VAO);
        glDeleteBuffers(1, &m->VBO);
        glDeleteBuffers(1, &m->EBO);
    }

    glfwDestroyWindow(g_window);
    if (renderTest) std::cout << "Render smoke test: " << (renderFailed ? "FAILED" : "PASS")
        << " / " << frameCount << " frames / " << (glfwGetTime() - smokeStarted) << " seconds\n";
    glfwTerminate();
    return renderFailed ? 1 : 0;
}
