// =============================================================================
//  ARRAKIS — Cinematic Dynamic Dune Experience
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
//    W / S           - Accelerate / Decelerate
//    A / D           - Bank & Turn Left / Right (aerodynamic roll)
//    Q / E           - Ascend / Descend (pitch tilt)
//    Left Shift      - Afterburner Jet Boost (high speed cruise)
//    Space           - Airbrake / Hover Mode
//    Arrow Keys      - Orbit Camera around Ornithopter
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

// =============================================================================
// WINDOW & DISPLAY CONFIGURATION
// =============================================================================
int  g_scrW = 1920;
int  g_scrH = 1080;
bool g_isFullscreen = true;
GLFWwindow* g_window = nullptr;

// =============================================================================
// CAMERA & FLIGHT STATE
// =============================================================================
bool  g_keys[1024]  = {};
bool  g_wireframe   = false;
int   g_camMode     = 0;     // 0: Cinematic Chase, 1: Cockpit/Close, 2: Overhead Tactical
float g_camOrbitYaw = 0.0f;
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
float g_harvSpeed = 3.2f;

// Dynamic Spice Tanks
struct SpiceTankInstance {
    glm::vec3 pos;
    float yaw;
    float scale;
};
std::vector<SpiceTankInstance> g_tanks;
float g_lastTankDist = -80.0f;

// =============================================================================
// DUNE TERRAIN HEIGHTFIELD FUNCTION
// Continuous procedural analytical surface for seamless rolling dunes
// =============================================================================
float getDuneHeight(float x, float z) {
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

glm::vec3 getDuneNormal(float x, float z) {
    float eps = 0.4f;
    float hL = getDuneHeight(x - eps, z);
    float hR = getDuneHeight(x + eps, z);
    float hD = getDuneHeight(x, z - eps);
    float hU = getDuneHeight(x, z + eps);
    return glm::normalize(glm::vec3(-(hR - hL), 2.0f * eps, -(hU - hD)));
}

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
    std::mt19937 rng((unsigned int)(glfwGetTime() * 1000.0f));
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
        if (p.life <= 0.0f || distToPlayer > 130.0f) {
            p.life = 2.0f + (rng() % 100) * 0.02f;
            p.maxLife = p.life;

            // Half particles spawn upwind from player, some spawn behind harvester crawler treads
            if (i % 4 == 0) {
                // Crawler dust plume
                p.pos = harvPos + glm::vec3(-4.0f + (rng() % 80) * 0.1f, 1.0f + (rng() % 30) * 0.1f, -1.0f + (rng() % 60) * 0.1f);
                p.vel = WIND_DIR * (WIND_BASE_SPEED * 0.9f) + glm::vec3(0.0f, 2.5f, 0.0f);
                p.alpha = 0.55f;
                p.size = 0.45f;
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
        idx.push_back(topCenter + 1 + i);
        idx.push_back(topCenter + 1 + i + 1);
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
        idx.push_back(btmCenter + 1 + i + 1);
        idx.push_back(btmCenter + 1 + i);
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
            idx.push_back(second);
            idx.push_back(first + 1);

            idx.push_back(second);
            idx.push_back(second + 1);
            idx.push_back(first + 1);
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

// =============================================================================
// GLOBAL MESH ASSETS
// =============================================================================
Mesh g_meshCube;
Mesh g_meshCylinder;
Mesh g_meshSphere;
Mesh g_meshWing;
Mesh g_meshTerrain;
Mesh g_meshQuad;

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

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLight;

void main(){
    vec4 worldPos = model * vec4(aPos, 1.0);
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
    if(proj.z > 1.0) return 0.0;

    float bias = max(0.0035 * (1.0 - dot(norm, lightDir)), 0.0008);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // 3x3 PCF filter for soft shadow edges
    for(int x = -1; x <= 1; ++x){
        for(int y = -1; y <= 1; ++y){
            float pcfDepth = texture(shadowMap, proj.xy + vec2(x,y) * texelSize).r;
            shadow += (proj.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main(){
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);

    vec3 baseCol = objectColor;

    // Procedural Dune Sand Micro-Shading
    if(isSand == 1){
        // Fine wind ripples aligned with wind angle
        float ripple1 = sin(FragPos.x * 2.2 + FragPos.z * 1.4) * 0.5 + 0.5;
        float ripple2 = sin(FragPos.x * 0.8 - FragPos.z * 1.9) * 0.5 + 0.5;
        float ripple = mix(ripple1, ripple2, 0.5);

        // Slope shading (crests are sun-bleached golden, valleys are deeper ochre)
        float slope = clamp(dot(N, vec3(0, 1, 0)), 0.0, 1.0);
        baseCol = mix(baseCol * 0.82, baseCol * 1.15, slope);
        baseCol += vec3(0.035, 0.02, 0.008) * (ripple - 0.5);

        // Subtle quartz sand sparkle under direct sun
        float sparkle = pow(max(dot(N, H), 0.0), 120.0);
        float noise = fract(sin(dot(FragPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
        if(noise > 0.88) baseCol += vec3(0.35, 0.3, 0.22) * sparkle;
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

    vec3 finalColor = mix(litColor, fogColor, fogFactor);
    FragColor = vec4(finalColor, 1.0);
}
)";

// 2. Shadow Depth Pass Shader
const char* SHADOW_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
void main(){
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
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

uniform mat4 view;
uniform mat4 projection;
uniform vec3 particlePos;
uniform float particleSize;
uniform vec3 camRight;
uniform vec3 camUp;

out vec2 TexCoord;

void main(){
    TexCoord = aUV;
    vec3 worldPos = particlePos + camRight * (aPos.x * particleSize) + camUp * (aPos.y * particleSize);
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

const char* PARTICLE_FRAG = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform float particleAlpha;
uniform vec3  particleColor;

void main(){
    float dist = length(TexCoord - vec2(0.5));
    if(dist > 0.5) discard;
    float soft = smoothstep(0.5, 0.0, dist);
    FragColor = vec4(particleColor, soft * particleAlpha);
}
)";

// 4. Cinematic Skybox Shader (Arrakis Amber & Sun Corona)
const char* SKY_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
out vec3 RayDir;
uniform mat4 invViewProj;

void main(){
    RayDir = aPos;
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

    // Warm desert atmospheric gradient: horizon haze to deep amber zenith
    vec3 horizonDust = vec3(0.88, 0.58, 0.28);
    vec3 zenithAmber = vec3(0.55, 0.26, 0.08);
    vec3 skyBase = mix(horizonDust, zenithAmber, pow(clamp(rd.y + 0.15, 0.0, 1.0), 0.75));

    // Blinding Arrakis Sun Disk + Atmospheric Corona
    float sunDot = max(dot(rd, normalize(lightDir)), 0.0);
    float sunDisk   = pow(sunDot, 1200.0) * 3.5;
    float sunCorona = pow(sunDot, 16.0)   * 0.65;
    float sunHaze   = pow(sunDot, 3.0)    * 0.25;

    vec3 finalSky = skyBase + (sunDisk + sunCorona + sunHaze) * sunColor;
    FragColor = vec4(finalSky, 1.0);
}
)";

// =============================================================================
// SHADER COMPILATION & MANAGEMENT
// =============================================================================
unsigned int compileShaderModule(GLenum type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; char log[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "[Shader Error]: " << log << std::endl;
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Skybox fullscreen quad VAO
unsigned int g_skyVAO = 0;
void setupSkyVAO() {
    float verts[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    unsigned int vbo;
    glGenVertexArrays(1, &g_skyVAO);
    glGenBuffers(1, &vbo);
    glBindVertexArray(g_skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
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
    if (g_isShadowPass) {
        glUniformMatrix4fv(glGetUniformLocation(g_progShadow, "model"), 1, GL_FALSE, glm::value_ptr(model));
    } else {
        glUniformMatrix4fv(glGetUniformLocation(g_progScene, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(g_progScene, "objectColor"), 1, glm::value_ptr(mat.diffuse));
        glUniform3fv(glGetUniformLocation(g_progScene, "specularColor"), 1, glm::value_ptr(mat.specular));
        glUniform1f (glGetUniformLocation(g_progScene, "shininess"), mat.shininess);
        glUniform3fv(glGetUniformLocation(g_progScene, "emissiveColor"), 1, glm::value_ptr(mat.emissive));
        glUniform1i (glGetUniformLocation(g_progScene, "isSand"), mat.isSand);
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
const glm::vec3 COL_DUNE_SAND     (0.85f, 0.62f, 0.35f);
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

        // Mirrored scale for port side
        wm = glm::scale(wm, glm::vec3(w.side * 5.2f, 1.0f, 1.0f));

        drawMeshPrimitive(g_meshWing, wm, matBlade);
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
            }
        }
    }

    // --- Forward Harvesting Cutter Drum / Scoop ---
    // Massive rotating cylinder with teeth
    drawPart(g_meshCylinder, root, {0.0f, 1.1f, -4.2f}, {0, 0, 90}, {2.2f, 8.8f, 2.2f}, matRust);
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

    // Multi-tier angular boulder stack simulating wind erosion yardangs
    drawPart(g_meshCube, root, { 0.0f, 2.2f,  0.0f}, {12,  25,  -8}, {5.2f, 4.4f, 4.8f}, matStrata1);
    drawPart(g_meshCube, root, { 0.6f, 5.2f, -0.4f}, {-8,  55,  14}, {3.6f, 3.2f, 3.4f}, matStrata2);
    drawPart(g_meshCube, root, { 0.2f, 7.4f, -0.8f}, {22, -15,  10}, {2.2f, 2.4f, 2.1f}, matStrata1);
    drawPart(g_meshCube, root, {-1.8f, 1.2f,  1.4f}, { 5, -40,  12}, {3.2f, 2.4f, 2.8f}, matStrata2);
    drawPart(g_meshCube, root, { 2.2f, 0.8f, -1.6f}, {14,  30, -18}, {2.8f, 1.6f, 2.6f}, matStrata1);

    // Talus boulder debris around base
    drawPart(g_meshCube, root, {-2.8f, 0.4f, -2.0f}, {30, 45, 10}, {1.4f, 0.8f, 1.2f}, matStrata2);
    drawPart(g_meshCube, root, { 3.2f, 0.3f,  2.2f}, {15, 70, 20}, {1.2f, 0.6f, 1.1f}, matStrata1);
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
    {
        glm::mat4 om = glm::translate(glm::mat4(1.0f), g_ornPos);
        om = glm::rotate(om, glm::radians(g_ornYaw), glm::vec3(0, 1, 0));
        om = glm::rotate(om, glm::radians(g_ornPitch), glm::vec3(1, 0, 0));
        om = glm::rotate(om, glm::radians(g_ornRoll), glm::vec3(0, 0, 1));
        drawMovieOrnithopter(om, g_wingPhase, g_ornRoll, g_ornPitch, g_isBoosting);
    }

    // 3. Massive Industrial Spice Harvester (hugging the dune surface!)
    {
        float hy = getDuneHeight(g_harvX, g_harvZ);
        glm::mat4 hm = glm::translate(glm::mat4(1.0f), glm::vec3(g_harvX, hy, g_harvZ));
        // Slight tilt matching dune slope
        glm::vec3 hnorm = getDuneNormal(g_harvX, g_harvZ);
        hm = glm::rotate(hm, -hnorm.x * 0.4f, glm::vec3(0, 0, 1));
        drawSpiceHarvester(hm, curTime);
    }

    // 4. Dynamic Spice Tanks (deployed behind Harvester onto dune surface)
    for (const auto& tank : g_tanks) {
        float ty = getDuneHeight(tank.pos.x, tank.pos.z);
        glm::mat4 tm = glm::translate(glm::mat4(1.0f), glm::vec3(tank.pos.x, ty, tank.pos.z));
        tm = glm::rotate(tm, glm::radians(tank.yaw), glm::vec3(0, 1, 0));
        tm = glm::scale(tm, glm::vec3(tank.scale));
        drawMovieSpiceTank(tm);
    }

    // 5. Desert Rock Yardang Crags (at natural elevated points across the desert)
    static const glm::vec3 rockLocations[] = {
        {  45.0f, 0.0f, -65.0f },
        { -55.0f, 0.0f,  40.0f },
        {  90.0f, 0.0f,  30.0f },
        { -90.0f, 0.0f, -70.0f },
        {  15.0f, 0.0f,  95.0f },
        { -25.0f, 0.0f, -115.0f },
        { 120.0f, 0.0f, -25.0f },
        { -130.0f, 0.0f,  85.0f }
    };
    for (int i = 0; i < 8; ++i) {
        float rx = rockLocations[i].x;
        float rz = rockLocations[i].z;
        float ry = getDuneHeight(rx, rz) - 0.5f;
        glm::mat4 rm = glm::translate(glm::mat4(1.0f), glm::vec3(rx, ry, rz));
        rm = glm::rotate(rm, glm::radians((float)(i * 45)), glm::vec3(0, 1, 0));
        drawDesertRockFormation(rm, i);
    }
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
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        g_isFullscreen = !g_isFullscreen;
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (g_isFullscreen) {
            glfwSetWindowMonitor(win, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(win, nullptr, 100, 100, 1280, 720, 0);
        }
    }
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    g_scrW = mode->width;
    g_scrH = mode->height;

    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    // Launch fullscreen
    g_window = glfwCreateWindow(g_scrW, g_scrH,
        "ARRAKIS — Cinematic Dynamic Dune Simulation | CSE 4102 Project",
        monitor, nullptr);

    if (!g_window) {
        std::cerr << "Window creation failed, falling back to windowed mode..." << std::endl;
        g_isFullscreen = false;
        g_scrW = 1280; g_scrH = 720;
        g_window = glfwCreateWindow(g_scrW, g_scrH, "ARRAKIS", nullptr, nullptr);
        if (!g_window) { glfwTerminate(); return -1; }
    }

    glfwMakeContextCurrent(g_window);
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetKeyCallback(g_window, key_callback);
    glfwSwapInterval(1); // Enable VSync for buttery smooth frames

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "========================================================\n";
    std::cout << " ARRAKIS — Cinematic Dynamic Dune Scene\n";
    std::cout << " GPU: " << glGetString(GL_RENDERER) << "\n";
    std::cout << " OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << " Resolution: " << g_scrW << "x" << g_scrH << "\n";
    std::cout << "========================================================\n";

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Build procedural geometry
    g_meshCube     = createCube();
    g_meshCylinder = createCylinder(24);
    g_meshSphere   = createSphere(16, 24);
    g_meshWing     = createWingBlade();
    g_meshTerrain  = createDuneTerrain(140, 140, 700.0f);
    g_meshQuad     = createQuad();

    // Compile shader programs
    g_progScene    = buildProgram(SCENE_VERT, SCENE_FRAG);
    g_progShadow   = buildProgram(SHADOW_VERT, SHADOW_FRAG);
    g_progParticle = buildProgram(PARTICLE_VERT, PARTICLE_FRAG);
    g_progSky      = buildProgram(SKY_VERT, SKY_FRAG);

    setupShadowFramebuffer();
    setupSkyVAO();
    initParticles();

    // Initial deployment of a few spice tanks in the distance
    g_tanks.push_back({ glm::vec3(-55.0f, 0.0f, 15.0f), 25.0f, 1.0f });
    g_tanks.push_back({ glm::vec3(-35.0f, 0.0f, 18.0f), -15.0f, 1.0f });
    g_tanks.push_back({ glm::vec3(-15.0f, 0.0f, 12.0f), 10.0f, 1.0f });

    // Environment Lighting Constants
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.65f, 1.15f, 0.45f)); // Blinding high sun
    glm::vec3 sunColor(1.0f, 0.94f, 0.82f);
    glm::vec3 ambientColor(0.28f, 0.22f, 0.16f); // Warm desert ambient bounce
    glm::vec3 fogColor(0.85f, 0.58f, 0.28f);     // Rich desert dust horizon
    float     fogDensity = 0.0055f;

    float prevTime = (float)glfwGetTime();

    // Camera damping state
    glm::vec3 camPosSmoothed = g_ornPos + glm::vec3(0, 10, 25);
    glm::vec3 camTargetSmoothed = g_ornPos;

    // =========================================================================
    // MAIN INTERACTIVE RENDER LOOP
    // =========================================================================
    while (!glfwWindowShouldClose(g_window)) {
        float curTime = (float)glfwGetTime();
        float dt = curTime - prevTime;
        prevTime = curTime;
        if (dt > 0.1f) dt = 0.1f; // Clamp delta time to avoid physics explosion

        // ---------------------------------------------------------------------
        // 1. ORNITHOPTER FLIGHT SIMULATION & KINEMATICS
        // ---------------------------------------------------------------------
        g_isBoosting = g_keys[GLFW_KEY_LEFT_SHIFT];
        bool isBraking  = g_keys[GLFW_KEY_SPACE];

        float maxSpeed = g_isBoosting ? 38.0f : 20.0f;
        float accelRate = g_isBoosting ? 24.0f : 12.0f;

        if (g_keys[GLFW_KEY_W]) {
            g_ornSpeed = std::min(g_ornSpeed + accelRate * dt, maxSpeed);
        } else if (g_keys[GLFW_KEY_S]) {
            g_ornSpeed = std::max(g_ornSpeed - accelRate * 1.5f * dt, -8.0f);
        } else {
            // Drag deceleration
            if (g_ornSpeed > 0.0f) g_ornSpeed = std::max(0.0f, g_ornSpeed - 6.0f * dt);
            if (g_ornSpeed < 0.0f) g_ornSpeed = std::min(0.0f, g_ornSpeed + 6.0f * dt);
        }
        if (isBraking) {
            g_ornSpeed *= (1.0f - 3.5f * dt);
        }

        // Turning & Aerodynamic Banking Roll
        float turnRate = 65.0f * dt;
        float targetRoll = 0.0f;
        if (g_keys[GLFW_KEY_A]) {
            g_ornYaw += turnRate;
            targetRoll = 32.0f; // Bank left
        } else if (g_keys[GLFW_KEY_D]) {
            g_ornYaw -= turnRate;
            targetRoll = -32.0f; // Bank right
        }
        g_ornRoll = glm::mix(g_ornRoll, targetRoll, 7.0f * dt);

        // Pitch & Altitude Controls
        float targetPitch = 0.0f;
        if (g_keys[GLFW_KEY_Q]) {
            g_ornPos.y += 12.0f * dt;
            targetPitch = -15.0f; // Nose up
        }
        if (g_keys[GLFW_KEY_E]) {
            g_ornPos.y -= 12.0f * dt;
            targetPitch = 15.0f;  // Nose down
        }
        g_ornPitch = glm::mix(g_ornPitch, targetPitch, 6.0f * dt);

        // Advance position according to heading
        float yawRad = glm::radians(g_ornYaw);
        glm::vec3 fwdDir(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
        g_ornPos += fwdDir * (g_ornSpeed * dt);

        // Terrain collision avoidance for ornithopter
        float minFlightY = getDuneHeight(g_ornPos.x, g_ornPos.z) + 3.0f;
        if (g_ornPos.y < minFlightY) g_ornPos.y = minFlightY;

        // Wing Flutter Frequency (scales dynamically with flight speed and boost!)
        float flapSpeed = g_isBoosting ? 38.0f : (16.0f + (g_ornSpeed / maxSpeed) * 16.0f);
        g_wingPhase += flapSpeed * dt;

        // ---------------------------------------------------------------------
        // 2. SPICE HARVESTER MOVEMENT & DYNAMIC SPICE TANK SPAWNING
        // ---------------------------------------------------------------------
        g_harvX += g_harvSpeed * dt;
        if (g_harvX > 160.0f) {
            g_harvX = -160.0f;
            g_lastTankDist = -160.0f;
        }

        // Spawn spice tanks as harvester advances across the dunes
        if (g_harvX - g_lastTankDist > 24.0f) {
            SpiceTankInstance st;
            st.pos = glm::vec3(g_harvX - 8.0f, 0.0f, g_harvZ + (-2.0f + (rand() % 40) * 0.1f));
            st.yaw = (float)(rand() % 360);
            st.scale = 1.0f;
            g_tanks.push_back(st);
            g_lastTankDist = g_harvX;

            if (g_tanks.size() > 24) {
                g_tanks.erase(g_tanks.begin());
            }
        }

        // ---------------------------------------------------------------------
        // 3. WIND & PARTICLES UPDATE
        // ---------------------------------------------------------------------
        float harvGroundY = getDuneHeight(g_harvX, g_harvZ);
        glm::vec3 harvWorldPos(g_harvX, harvGroundY, g_harvZ);
        updateParticles(dt, g_ornPos, harvWorldPos);

        // ---------------------------------------------------------------------
        // 4. CAMERA SYSTEM (CINEMATIC CHASE / COCKPIT / OVERHEAD)
        // ---------------------------------------------------------------------
        if (g_keys[GLFW_KEY_LEFT])  g_camOrbitYaw -= 55.0f * dt;
        if (g_keys[GLFW_KEY_RIGHT]) g_camOrbitYaw += 55.0f * dt;
        if (g_keys[GLFW_KEY_UP])    g_camOrbitPitch = std::min(g_camOrbitPitch + 40.0f * dt, 50.0f);
        if (g_keys[GLFW_KEY_DOWN])  g_camOrbitPitch = std::max(g_camOrbitPitch - 40.0f * dt, -20.0f);

        glm::vec3 camTarget = g_ornPos;
        glm::vec3 camDesiredPos;

        if (g_camMode == 0) {
            // Cinematic Third-Person Chase Camera: floating above & behind ornithopter
            float camDist = 24.0f;
            float totalYaw = g_ornYaw + 180.0f + g_camOrbitYaw;
            float totalPitch = 24.0f + g_camOrbitPitch;
            float cyr = glm::radians(totalYaw);
            float cpr = glm::radians(totalPitch);

            camDesiredPos = g_ornPos + glm::vec3(
                camDist * std::cos(cpr) * std::sin(cyr),
                camDist * std::sin(cpr),
                camDist * std::cos(cpr) * std::cos(cyr)
            );
        } else if (g_camMode == 1) {
            // Cockpit / Close Chase View
            float camDist = 7.5f;
            float totalYaw = g_ornYaw + 180.0f + g_camOrbitYaw;
            float totalPitch = 12.0f + g_camOrbitPitch;
            float cyr = glm::radians(totalYaw);
            float cpr = glm::radians(totalPitch);

            camDesiredPos = g_ornPos + glm::vec3(
                camDist * std::cos(cpr) * std::sin(cyr),
                2.2f + camDist * std::sin(cpr),
                camDist * std::cos(cpr) * std::cos(cyr)
            );
        } else {
            // High Overhead Tactical View (Surveying desert & harvester below)
            camDesiredPos = g_ornPos + glm::vec3(0.0f, 65.0f, 15.0f);
        }

        // Camera terrain collision safety
        float camGroundY = getDuneHeight(camDesiredPos.x, camDesiredPos.z) + 1.5f;
        if (camDesiredPos.y < camGroundY) camDesiredPos.y = camGroundY;

        // Smooth spring damping
        camPosSmoothed = glm::mix(camPosSmoothed, camDesiredPos, 9.0f * dt);
        camTargetSmoothed = glm::mix(camTargetSmoothed, camTarget, 12.0f * dt);

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

        glm::vec3 sandCol = COL_DUNE_SAND * 1.15f;
        glUniform3fv(glGetUniformLocation(g_progParticle, "particleColor"), 1, glm::value_ptr(sandCol));

        glBindVertexArray(g_meshQuad.VAO);
        for (const auto& p : g_particles) {
            glUniform3fv(glGetUniformLocation(g_progParticle, "particlePos"), 1, glm::value_ptr(p.pos));
            glUniform1f (glGetUniformLocation(g_progParticle, "particleSize"), p.size);
            glUniform1f (glGetUniformLocation(g_progParticle, "particleAlpha"), p.alpha);
            glDrawElements(GL_TRIANGLES, g_meshQuad.indexCount, GL_UNSIGNED_INT, 0);
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        // ---------------------------------------------------------------------
        // 7. BUFFER SWAP & EVENT POLLING
        // ---------------------------------------------------------------------
        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    // Cleanup resources
    glDeleteProgram(g_progScene);
    glDeleteProgram(g_progShadow);
    glDeleteProgram(g_progParticle);
    glDeleteProgram(g_progSky);
    glDeleteFramebuffers(1, &g_shadowFBO);
    glDeleteTextures(1, &g_shadowTex);

    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}
