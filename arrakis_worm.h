#ifndef ARRAKIS_WORM_H
#define ARRAKIS_WORM_H

// Include after drawDesertRockFormation and before drawFullScene in arrakis.cpp.
// Initialize/clean up with the GL context current; draw in both scene passes.
#include <cmath>
#include <vector>

namespace arrakis_worm_detail {

static const float pi = 3.14159265358979323846f;
static const int bodyRows = 192;
static const int bodySlices = 96;
static Mesh body, lip, funnel, throat, teeth;
static std::vector<Vertex> bodyVertices;
static bool ready = false;
static bool poseValid = false;
static glm::vec3 lastCenter(0.0f);
static float lastAttack = 0.0f, lastTime = 0.0f;
static float lastHeading = 0.0f, lastProgress = 0.0f;

inline float smooth(float a, float b, float t) {
    float u = glm::clamp((t - a) / (b - a), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

inline void destroy(Mesh& mesh) {
    if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
    if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
    mesh = Mesh{};
}

inline void gridIndices(std::vector<unsigned int>& indices, int rows, int slices) {
    indices.reserve(rows * slices * 6);
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < slices; ++i) {
            unsigned int a = j * (slices + 1) + i;
            unsigned int b = a + 1, c = a + slices + 1, d = c + 1;
            indices.insert(indices.end(), {a, c, d, a, d, b});
        }
    }
}

inline void surfaceNormals(std::vector<Vertex>& v,
                           const std::vector<unsigned int>& indices, int slices) {
    for (Vertex& vertex : v) vertex.normal = glm::vec3(0.0f);
    for (size_t i = 0; i < indices.size(); i += 3) {
        Vertex& a = v[indices[i]];
        Vertex& b = v[indices[i + 1]];
        Vertex& c = v[indices[i + 2]];
        glm::vec3 n = glm::cross(b.pos - a.pos, c.pos - a.pos);
        a.normal += n; b.normal += n; c.normal += n;
    }
    // Weld the duplicated UV seam without welding separate tooth surfaces.
    for (size_t j = 0; j < v.size(); j += slices + 1) {
        glm::vec3 n = v[j].normal + v[j + slices].normal;
        v[j].normal = v[j + slices].normal = n;
    }
    for (Vertex& vertex : v) vertex.normal = glm::normalize(vertex.normal);
}

// Profile order follows the exterior up to the lip, then down the inner funnel.
// The same winding therefore gives outward skin and inward-facing mouth normals.
inline Mesh lathe(const std::vector<glm::vec2>& profile, float roughness, bool deepFloor = false) {
    const int slices = 160;
    std::vector<Vertex> v;
    std::vector<unsigned int> indices;
    v.reserve(profile.size() * (slices + 1));
    for (size_t j = 0; j < profile.size(); ++j) {
        for (int i = 0; i <= slices; ++i) {
            float u = float(i) / slices;
            float a = 2.0f * pi * u;
            float rough = roughness * std::sin(pi * float(j) / (profile.size() - 1))
                        * (0.55f * std::sin(11.0f * a + 0.7f * j)
                        + 0.30f * std::sin(23.0f * a - 0.9f * j)
                        + 0.15f * std::sin(47.0f * a + 1.1f * j));
            float r = profile[j].x + rough;
            v.push_back({glm::vec3(r * std::cos(a), profile[j].y, r * std::sin(a)),
                         glm::vec3(0.0f), glm::vec2(u, float(j) / (profile.size() - 1))});
        }
    }
    gridIndices(indices, int(profile.size()) - 1, slices);
    surfaceNormals(v, indices, slices);
    if (deepFloor) {
        // Recessed throat closure, never a cap across the mouth opening.
        unsigned int center = static_cast<unsigned int>(v.size());
        v.push_back({{0.0f, profile.back().y, 0.0f}, {0.0f,1.0f,0.0f}, {0.5f,0.5f}});
        for (int i = 0; i <= slices; ++i) {
            Vertex vertex = v[(profile.size() - 1) * (slices + 1) + i];
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.push_back(vertex);
            if (i < slices) indices.insert(indices.end(), {center, center + i + 2, center + i + 1});
        }
    }
    return uploadMesh(v, indices);
}

inline Mesh makeTeeth() {
    const int rows = 6, sides = 5, steps = 6;
    const int counts[rows] = {192, 184, 176, 168, 152, 128};
    const float radii[rows] = {13.1f, 11.3f, 9.4f, 7.7f, 6.0f, 4.5f};
    const float depths[rows] = {-0.8f, -2.6f, -4.5f, -6.3f, -8.2f, -10.1f};
    std::vector<Vertex> v;
    std::vector<unsigned int> indices;
    v.reserve(1000 * (steps * sides + 1));
    indices.reserve(1000 * ((steps - 1) * sides * 6 + sides * 3));
    for (int row = 0; row < rows; ++row) {
        for (int tooth = 0; tooth < counts[row]; ++tooth) {
            float seed = float(tooth * 17 + row * 131);
            float variation = 0.5f + 0.5f * std::sin(seed * 1.713f);
            float a = 2.0f * pi * (tooth + 0.37f * row
                    + 0.18f * std::sin(seed)) / counts[row];
            glm::vec3 radial(std::cos(a), 0.0f, std::sin(a));
            glm::vec3 sideways(-std::sin(a), 0.0f, std::cos(a));
            float reach = (radii[row] - 1.8f) * (0.55f + 0.17f * variation);
            float drop = 2.1f + 1.1f * variation;
            float curl = 0.20f + 0.30f * std::sin(seed * 0.91f);
            float width = (0.075f + 0.035f * variation) * (1.0f - 0.06f * row);
            unsigned int base = static_cast<unsigned int>(v.size());
            for (int k = 0; k < steps; ++k) {
                float t = float(k) / steps;
                glm::vec3 p = radial * (radii[row] - reach * t)
                            + sideways * (curl * t * t)
                            + glm::vec3(0.0f, depths[row] + 0.65f * std::sin(pi * t)
                                        - drop * t * t, 0.0f);
                glm::vec3 tangent = glm::normalize(-reach * radial + 2.0f * curl * t * sideways
                    + glm::vec3(0.0f, 0.65f * pi * std::cos(pi * t) - 2.0f * drop * t, 0.0f));
                glm::vec3 b = glm::normalize(glm::cross(tangent, sideways));
                glm::vec3 n = glm::cross(b, tangent);
                float radius = width * std::pow(1.0f - t, 0.85f);
                for (int s = 0; s < sides; ++s) {
                    float angle = 2.0f * pi * s / sides;
                    glm::vec3 normal = n * std::cos(angle) + b * std::sin(angle);
                    v.push_back({p + radius * normal, normal, glm::vec2(float(s) / sides, t)});
                }
            }
            for (int k = 0; k < steps - 1; ++k) {
                for (int s = 0; s < sides; ++s) {
                    unsigned int a0 = base + k * sides + s;
                    unsigned int b0 = base + k * sides + (s + 1) % sides;
                    indices.insert(indices.end(), {a0, b0, b0 + sides, a0, b0 + sides, a0 + sides});
                }
            }
            // One true apex, not a blunt, tiny capped cylinder.
            unsigned int tip = static_cast<unsigned int>(v.size());
            glm::vec3 tipPos = radial * (radii[row] - reach) + sideways * curl
                            + glm::vec3(0.0f, depths[row] - drop, 0.0f);
            glm::vec3 tipNormal = glm::normalize(-reach * radial + 2.0f * curl * sideways
                                 + glm::vec3(0.0f, -0.65f * pi - 2.0f * drop, 0.0f));
            v.push_back({tipPos, tipNormal, glm::vec2(0.5f, 1.0f)});
            for (int s = 0; s < sides; ++s) {
                unsigned int a0 = base + (steps - 1) * sides + s;
                unsigned int b0 = base + (steps - 1) * sides + (s + 1) % sides;
                indices.insert(indices.end(), {a0, b0, tip});
            }
        }
    }
    return uploadMesh(v, indices);
}

struct Pose {
    glm::vec3 head;
    float tilt;
    float aperture;
};

inline Pose pose(glm::vec3 center, float attack, float time,
                 float headingDegrees = 0.0f, float approachProgress = 0.0f) {
    float rise = smooth(0.0f, 6.0f, attack);
    float lunge = smooth(6.0f, 12.0f, attack);
    float retreat = smooth(13.5f, 18.0f, attack);
    float ground = getDuneHeight(center.x, center.z);
    float approachHeight = 10.0f + 4.0f * smooth(0.0f, 1.0f, approachProgress);
    float height = attack < 0.0f ? approachHeight : 14.0f + 22.0f * rise - 27.0f * lunge;
    height += 0.25f * std::sin(time * 1.6f) * rise * (1.0f - lunge);
    height = glm::mix(height, -24.0f, retreat);
    float reach = 12.0f * lunge * (1.0f - retreat);
    glm::vec3 head(center.x, ground + height, center.z + reach);
    float heading = glm::radians(headingDegrees);
    float targetY = getDuneHeight(center.x + 18.0f * std::sin(heading),
                                 center.z + 18.0f * std::cos(heading)) + 1.0f;
    // Deform in the local YZ plane; the draw root rotates local +Z into the heading.
    float aim = std::atan2(18.0f - reach, targetY - head.y);
    float tilt = glm::radians(70.0f) * (1.0f - rise)
               + aim * lunge * (1.0f - retreat);
    float swallow = smooth(12.0f, 13.5f, attack) * (1.0f - retreat);
    return {head, tilt, 1.0f - 0.18f * swallow};
}

inline glm::mat4 headMatrix(const Pose& p) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), p.head);
    m = glm::rotate(m, p.tilt, glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::scale(m, glm::vec3(p.aperture, 1.0f, p.aperture));
}

inline void deformBody(glm::vec3 center, const Pose& p) {
    glm::vec3 axis(0.0f, std::cos(p.tilt), std::sin(p.tilt));
    const float neckLength = 64.0f, join = 0.4f;
    glm::vec3 end = p.head - 1.4f * axis;
    // Constant-curvature neck: even a downward bite has a bend radius over 20 m.
    auto neckPoint = [&](float s) {
        float half = 0.5f * p.tilt * (1.0f - s);
        float mid = 0.5f * p.tilt * (1.0f + s);
        float sinc = half > 0.0001f ? std::sin(half) / half : 1.0f;
        float span = neckLength * (1.0f - s) * sinc;
        return end - glm::vec3(0.0f, span * std::cos(mid), span * std::sin(mid));
    };
    glm::vec3 p0(center.x, getDuneHeight(center.x, center.z) - 140.0f, center.z);
    glm::vec3 p3 = neckPoint(0.0f);
    float buriedLength = (p3.y - p0.y) / 3.0f;
    glm::vec3 p1 = p0 + glm::vec3(0.0f, buriedLength, 0.0f);
    glm::vec3 p2 = p3 - glm::vec3(0.0f, buriedLength, 0.0f);
    for (int j = 0; j <= bodyRows; ++j) {
        float t = float(j) / bodyRows;
        glm::vec3 c, tangent;
        if (t < join) {
            float s = t / join, u = 1.0f - s;
            c = u * u * u * p0 + 3.0f * u * u * s * p1
              + 3.0f * u * s * s * p2 + s * s * s * p3;
            tangent = glm::normalize(u * u * (p1 - p0)
                    + 2.0f * u * s * (p2 - p1) + s * s * (p3 - p2));
        } else {
            float s = (t - join) / (1.0f - join);
            c = neckPoint(s);
            tangent = glm::vec3(0.0f, std::cos(p.tilt * s), std::sin(p.tilt * s));
        }
        glm::vec3 across(0.0f, -tangent.z, tangent.y);
        float taper = 1.0f - smooth(0.85f, 1.0f, t);
        float ridge = 0.70f * std::pow(0.5f + 0.5f * std::cos(2.0f * pi * 32.0f * t), 3.0f);
        float radius = glm::mix(13.9f + ridge, 15.0f * p.aperture, 1.0f - taper);
        for (int i = 0; i <= bodySlices; ++i) {
            float a = 2.0f * pi * i / bodySlices;
            glm::vec3 radial = glm::vec3(std::cos(a), 0.0f, 0.0f) + std::sin(a) * across;
            float grain = taper * (0.11f * std::sin(13.0f * a + 9.0f * t)
                        + 0.07f * std::sin(29.0f * a - 17.0f * t));
            Vertex& vertex = bodyVertices[j * (bodySlices + 1) + i];
            vertex.pos = c + (radius + grain) * radial;
            vertex.uv = glm::vec2(float(i) / bodySlices, t * 32.0f);
        }
    }
    // Derivatives include the bend, taper and ridge slopes, not just radial normals.
    for (int j = 0; j <= bodyRows; ++j) {
        for (int i = 0; i < bodySlices; ++i) {
            int prev = (i + bodySlices - 1) % bodySlices;
            int next = (i + 1) % bodySlices;
            int lo = j == 0 ? 0 : j - 1, hi = j == bodyRows ? j : j + 1;
            glm::vec3 ds = bodyVertices[hi * (bodySlices + 1) + i].pos
                         - bodyVertices[lo * (bodySlices + 1) + i].pos;
            glm::vec3 da = bodyVertices[j * (bodySlices + 1) + next].pos
                         - bodyVertices[j * (bodySlices + 1) + prev].pos;
            bodyVertices[j * (bodySlices + 1) + i].normal = glm::normalize(glm::cross(ds, da));
        }
        bodyVertices[j * (bodySlices + 1) + bodySlices].normal = bodyVertices[j * (bodySlices + 1)].normal;
    }
}

} // namespace arrakis_worm_detail

inline void cleanupSandwormMeshes() {
    using namespace arrakis_worm_detail;
    destroy(body); destroy(lip); destroy(funnel); destroy(throat); destroy(teeth);
    bodyVertices.clear();
    ready = poseValid = false;
}

inline void initSandwormMeshes() {
    using namespace arrakis_worm_detail;
    if (ready) return;
    bodyVertices.resize((bodyRows + 1) * (bodySlices + 1));
    deformBody(glm::vec3(0.0f), pose(glm::vec3(0.0f), 6.0f, 0.0f));
    std::vector<unsigned int> indices;
    gridIndices(indices, bodyRows, bodySlices);
    body = uploadMesh(bodyVertices, indices);
    GLint previousBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, body.VBO);
    glBufferData(GL_ARRAY_BUFFER, bodyVertices.size() * sizeof(Vertex), bodyVertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));

    lip = lathe({{15.0f,-1.4f}, {15.6f,-0.9f}, {15.9f,-0.2f}, {15.5f,0.5f},
                 {14.8f,0.8f}, {14.0f,0.5f}, {13.4f,-0.1f}, {13.1f,-0.8f}}, 0.16f);
    funnel = lathe({{13.1f,-0.8f}, {12.8f,-1.3f}, {12.0f,-2.0f}, {11.3f,-2.6f},
                    {10.5f,-3.5f}, {9.4f,-4.5f}, {8.6f,-5.4f}, {7.7f,-6.3f},
                    {6.8f,-7.3f}, {6.0f,-8.2f}, {5.2f,-9.2f}, {4.5f,-10.1f},
                    {3.6f,-11.4f}, {2.9f,-13.0f}}, 0.08f);
    throat = lathe({{2.9f,-13.0f}, {2.6f,-15.0f}, {2.2f,-19.0f},
                    {1.7f,-24.0f}, {0.8f,-27.0f}}, 0.0f, true);
    teeth = makeTeeth();
    ready = true;
    poseValid = false;
}

inline void drawSandworm(glm::vec3 center, float attackTime, float time,
                         float headingDegrees = 0.0f, float approachProgress = 0.0f) {
    using namespace arrakis_worm_detail;
    if (!ready || !std::isfinite(attackTime) || !std::isfinite(time)
        || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
        || !std::isfinite(headingDegrees) || !std::isfinite(approachProgress)
        || attackTime >= 18.0f) return;
    approachProgress = glm::clamp(approachProgress, 0.0f, 1.0f);
    Pose p = pose(center, attackTime, time, headingDegrees, approachProgress);
    // Both render passes use the same pose. Only the flexible skin needs streaming.
    if (!poseValid || center.x != lastCenter.x || center.y != lastCenter.y || center.z != lastCenter.z
        || attackTime != lastAttack || time != lastTime
        || headingDegrees != lastHeading || approachProgress != lastProgress) {
        deformBody(center, p);
        GLint previousBuffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, body.VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bodyVertices.size() * sizeof(Vertex), bodyVertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
        lastCenter = center; lastAttack = attackTime; lastTime = time;
        lastHeading = headingDegrees; lastProgress = approachProgress;
        poseValid = true;
    }
    Material skin;
    skin.diffuse = glm::vec3(0.43f, 0.27f, 0.12f);
    skin.specular = glm::vec3(0.055f, 0.04f, 0.025f);
    skin.shininess = 8.0f;
    Material rim = skin;
    rim.diffuse = glm::vec3(0.49f, 0.31f, 0.14f);
    Material inside = skin;
    inside.diffuse = glm::vec3(0.15f, 0.075f, 0.029f);
    inside.specular = glm::vec3(0.018f);
    Material dark = inside;
    dark.diffuse = glm::vec3(0.009f, 0.004f, 0.002f);
    dark.specular = glm::vec3(0.0f);
    Material enamel = skin;
    enamel.diffuse = glm::vec3(0.58f, 0.43f, 0.24f);
    enamel.specular = glm::vec3(0.07f);
    enamel.shininess = 18.0f;
    glm::mat4 root = glm::translate(glm::mat4(1.0f), center);
    root = glm::rotate(root, glm::radians(headingDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    root = glm::translate(root, -center);
    glm::mat4 head = root * headMatrix(p);
    drawMeshPrimitive(body, root, skin);
    drawMeshPrimitive(lip, head, rim);
    drawMeshPrimitive(funnel, head, inside);
    drawMeshPrimitive(throat, head, dark);
    drawMeshPrimitive(teeth, head, enamel);
    float wake = 1.0f - smooth(0.0f, 6.0f, attackTime);
    if (wake > 0.001f) {
        Material sand = skin;
        sand.diffuse = glm::vec3(0.68f, 0.46f, 0.22f);
        sand.specular = glm::vec3(0.015f);
        for (int i = 0; i < 4; ++i) {
            glm::vec3 local(center.x, center.y, center.z - 14.0f - 14.0f * i);
            glm::vec3 world(root * glm::vec4(local, 1.0f));
            local.y = getDuneHeight(world.x, world.z) - 0.8f;
            glm::mat4 mound = glm::translate(root, local);
            mound = glm::scale(mound, glm::vec3((12.0f - i) * wake,
                                (3.0f + 0.35f * std::sin(time * 2.0f - i)) * wake,
                                10.0f * wake));
            drawMeshPrimitive(g_meshSphere, mound, sand);
        }
    }
}

#endif // ARRAKIS_WORM_H
