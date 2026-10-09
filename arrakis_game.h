#pragma once

enum class MissionPhase { Title, Flying, Debrief };
enum class CrewState { Inside, Running, Waiting, Aboard, Safe, Lost };
struct Evacuee {
    glm::vec3 pos{0};
    glm::vec3 destination{0};
    CrewState state = CrewState::Inside;
    float release = 0;
    int group = 0, slot = 0;
};
struct EvacuationGroup {
    int count = 0;
    float release = 0;
    glm::vec3 center{0};
    bool released = false;
};
struct RescueMission {
    MissionPhase phase = MissionPhase::Title;
    bool paused = false;
    bool planeLost = false;
    bool boostLocked = false;
    int wave = 1, rescued = 0, aboard = 0, lost = 0, best = 0;
    float elapsed = 0, breachAt = 210, altitude = 8, boost = 1;
    float harvesterYaw = 0, harvesterTravel = 0, wormYaw = 90;
    float pickup = 0, unload = 0;
    int target = -1;
    std::string prompt = "";
    glm::vec3 base{55, 0, 55};
    std::vector<Evacuee> crew;
    std::vector<EvacuationGroup> groups;
};
RescueMission g_mission;
int g_missionRevision = 0;
glm::vec2 g_mouseHover(0);
float g_mouseTurnRate = 0;
constexpr int RESCUE_CAPACITY = 8;
const glm::vec2 g_rockSites[] = {{45, -65}, {-55, 40}, {90, 30}, {-90, -70},
                              {15, 95}, {-25, -115}, {120, -25}, {-130, 85}};

float horizontalDistance(glm::vec3 a, glm::vec3 b) {
    return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}
glm::vec3 harvesterPosition() {
    return {g_harvX, getDuneHeight(g_harvX, g_harvZ), g_harvZ};
}
glm::vec3 harvesterRoute(float time) {
    float x = -40 + 0.62f * time + 15 * std::sin(time * 0.045f);
    float z = -55 + 22 * std::sin(time * 0.032f) + 9 * std::sin(time * 0.071f);
    return {x, getDuneHeight(x, z), z};
}
float harvesterRouteYaw(float time) {
    float dx = 0.62f + 0.675f * std::cos(time * 0.045f);
    float dz = 0.704f * std::cos(time * 0.032f) + 0.639f * std::cos(time * 0.071f);
    return glm::degrees(std::atan2(-dx, -dz));
}
glm::vec3 harvesterForward(float yaw) {
    float angle = glm::radians(yaw);
    return {-std::sin(angle), 0, -std::cos(angle)};
}
void updateHarvester(float elapsed) {
    float time = glm::clamp(elapsed, 0.0f, g_mission.breachAt);
    glm::vec3 previous = harvesterPosition(), next = harvesterRoute(time);
    g_mission.harvesterTravel += horizontalDistance(previous, next);
    g_harvX = next.x; g_harvZ = next.z;
    g_mission.harvesterYaw = harvesterRouteYaw(time);
}
float wormGap() {
    float progress = glm::clamp(g_mission.elapsed / g_mission.breachAt, 0.0f, 1.0f);
    return glm::mix(220.0f, 18.0f, progress);
}
glm::vec3 wormPosition() {
    glm::vec3 p = harvesterPosition();
    p.x -= wormGap();
    p.y = getDuneHeight(p.x, p.z);
    return p;
}
float wormAttackTime() { return g_mission.elapsed - g_mission.breachAt; }

glm::vec3 nearestCrewPosition() {
    float nearest = 10000;
    glm::vec3 point = harvesterPosition();
    for (const auto& c : g_mission.crew) {
        if (c.state != CrewState::Waiting && c.state != CrewState::Running) continue;
        float distance = horizontalDistance(g_ornPos, c.pos);
        if (distance < nearest) { nearest = distance; point = c.pos; }
    }
    return point;
}

void setMouseHover(double x, double y, int width, int height) {
    g_mouseHover = glm::vec2(0);
    if (width <= 0 || height <= 0 || !std::isfinite(x) || !std::isfinite(y)
        || x < 0 || y < 0 || x > width || y > height) return;
    auto deadzone = [](float value) {
        return std::copysign(std::max(0.0f, (std::abs(value) - 0.12f) / 0.88f), value);
    };
    g_mouseHover = {deadzone(static_cast<float>(2 * x / width - 1)),
                    deadzone(static_cast<float>(2 * y / height - 1))};
}

glm::vec3 flightForward() {
    float yaw = glm::radians(g_ornYaw);
    return {-std::sin(yaw), 0, -std::cos(yaw)};
}

float flightCameraPitch() {
    return glm::clamp(12 - g_mouseHover.y * 22 + g_camOrbitPitch, 6.0f, 65.0f);
}

glm::vec3 flightCameraOffset(int mode) {
    glm::vec3 behind = -flightForward();
    if (mode == 2) return behind * 26.0f + glm::vec3(0, 100, 0);
    float distance = mode == 1 ? 20.0f : 40.0f;
    float pitch = glm::radians(flightCameraPitch());
    return behind * (distance * std::cos(pitch))
        + glm::vec3(0, distance * std::sin(pitch) + (mode == 1 ? 2.2f : 0.0f), 0);
}

void resetMission(bool start, bool next = false) {
    ++g_missionRevision;
    int best = g_mission.best;
    int wave = next ? g_mission.wave + 1 : g_mission.wave;
    g_mission = RescueMission{};
    g_mission.best = best;
    g_mission.wave = wave;
    g_mission.breachAt = std::max(150.0f, 210.0f - (wave - 1) * 10.0f);
    g_mission.phase = start ? MissionPhase::Flying : MissionPhase::Title;
    glm::vec3 harvester = harvesterRoute(0);
    g_harvX = harvester.x; g_harvZ = harvester.z;
    g_mission.harvesterYaw = harvesterRouteYaw(0);
    g_mission.base.y = getDuneHeight(g_mission.base.x, g_mission.base.z);
    g_ornPos = g_mission.base + glm::vec3(0, 8, 0);
    g_ornVel = glm::vec3(0);
    g_ornYaw = 0;
    g_mouseHover = glm::vec2(0);
    g_mouseTurnRate = 0;
    g_ornSpeed = g_ornPitch = g_ornRoll = 0;
    g_camOrbitPitch = 0;
    int remaining = 36;
    while (remaining > 0) {
        int group = static_cast<int>(g_mission.groups.size());
        int count = std::min(remaining, 1 + group % 4);
        float release = 6.0f + group * 7.0f;
        g_mission.groups.push_back({count, release, harvester, false});
        for (int slot = 0; slot < count; ++slot) {
            Evacuee c;
            c.release = release; c.group = group; c.slot = slot;
            c.pos = c.destination = harvester;
            g_mission.crew.push_back(c);
        }
        remaining -= count;
    }
    g_tanks.clear();
    g_tanks.push_back({{-12, 0, -26}, 20, 1});
    g_tanks.push_back({{-19, 0, -32}, -15, 1});
    g_tanks.push_back({{-26, 0, -23}, 10, 1});
}

void finishMission() {
    g_mission.phase = MissionPhase::Debrief;
    g_mission.best = std::max(g_mission.best, g_mission.rescued);
    g_mission.pickup = g_mission.unload = 0;
    g_ornVel = glm::vec3(0);
}

void updateMission(float dt) {
    auto& m = g_mission;
    if (m.phase != MissionPhase::Flying || m.paused) return;
    m.elapsed += dt;
    updateHarvester(m.elapsed);
    float attack = wormAttackTime();
    const bool winch = g_keys[GLFW_KEY_SPACE];
    float steer = glm::clamp(g_mouseHover.x + float(g_keys[GLFW_KEY_RIGHT]) - float(g_keys[GLFW_KEY_LEFT]), -1.0f, 1.0f);
    float turnRate = -steer * 80;
    float turnDecay = std::exp(-dt * 8);
    // Integrate the smoothed turn analytically so steering does not depend on FPS.
    g_ornYaw = std::remainder(g_ornYaw + turnRate * dt
        + (g_mouseTurnRate - turnRate) * (1 - turnDecay) / 8, 360.0f);
    g_mouseTurnRate = glm::mix(turnRate, g_mouseTurnRate, turnDecay);
    g_ornRoll = glm::mix(g_ornRoll, g_mouseTurnRate * 0.28f, 1 - std::exp(-dt * 6));
    glm::vec3 forward = flightForward();
    glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
    glm::vec3 input = right * (float(g_keys[GLFW_KEY_D]) - float(g_keys[GLFW_KEY_A]))
        + forward * (float(g_keys[GLFW_KEY_W]) - float(g_keys[GLFW_KEY_S]));
    if (glm::length(input) > 0) input = glm::normalize(input);
    if (!g_keys[GLFW_KEY_LEFT_SHIFT]) m.boostLocked = false;
    if (m.boost <= 0.04f) m.boostLocked = true;
    g_isBoosting = g_keys[GLFW_KEY_LEFT_SHIFT] && !m.boostLocked && !winch && glm::length(input) > 0;
    m.boost = glm::clamp(m.boost + dt * (g_isBoosting ? -0.22f : 0.14f), 0.0f, 1.0f);
    glm::vec3 desired = input * (g_isBoosting ? 48.0f : 28.0f);
    if (winch) desired *= 0.10f;
    g_ornVel = glm::mix(g_ornVel, desired, 1.0f - std::exp(-dt * (winch ? 12.0f : 5.5f)));
    g_ornPos += g_ornVel * dt;
    g_ornPos.x = glm::clamp(g_ornPos.x, -160.0f, 160.0f);
    g_ornPos.z = glm::clamp(g_ornPos.z, -160.0f, 160.0f);
    g_ornSpeed = glm::length(g_ornVel);
    m.altitude = glm::clamp(m.altitude + (float(g_keys[GLFW_KEY_Q]) - float(g_keys[GLFW_KEY_E])) * 15 * dt, 4.0f, 55.0f);
    if (winch) m.altitude = glm::mix(m.altitude, 6.0f, 1.0f - std::exp(-dt * 4));
    float ground = getDuneHeight(g_ornPos.x, g_ornPos.z);
    float clearance = m.altitude;
    for (const auto& rock : g_rockSites) {
        float distance = glm::distance(glm::vec2(g_ornPos.x, g_ornPos.z), rock);
        float avoid = 1 - glm::smoothstep(8.0f, 17.0f, distance);
        clearance = std::max(clearance, 13 * avoid);
    }
    if (attack < 11 && std::abs(g_ornPos.x - g_harvX) < 8 && std::abs(g_ornPos.z - g_harvZ) < 7)
        clearance = std::max(clearance, 12.0f);
    g_ornPos.y = glm::mix(g_ornPos.y, ground + clearance, 1.0f - std::exp(-dt * 8));
    g_ornPos.y = std::max(g_ornPos.y, ground + 3.0f);
    g_ornPitch = glm::mix(g_ornPitch, -g_ornSpeed * 0.3f, 1.0f - std::exp(-dt * 6));
    g_wingPhase += (g_isBoosting ? 36 : 23) * dt;

    // Drop each batch at its scheduled world-space point, independent of FPS.
    for (int i = 0; i < static_cast<int>(m.groups.size()); ++i) {
        auto& group = m.groups[i];
        if (group.released || m.elapsed < group.release) continue;
        glm::vec3 origin = harvesterRoute(group.release);
        glm::vec3 forward = harvesterForward(harvesterRouteYaw(group.release));
        glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
        glm::vec3 exit = origin + right * 6.1f - forward * 1.2f;
        group.center = exit + right * 14.0f;
        group.center.y = getDuneHeight(group.center.x, group.center.z);
        group.released = true;
        for (auto& c : m.crew) if (c.group == i && c.state == CrewState::Inside) {
            c.state = CrewState::Running;
            c.pos = exit;
            c.destination = group.center + forward * (1.7f * (c.slot - (group.count - 1) * 0.5f))
                + right * (1.3f * (c.slot % 2));
            c.destination.y = getDuneHeight(c.destination.x, c.destination.z);
        }
    }
    int waiting = 0;
    glm::vec3 worm = wormPosition();
    glm::vec3 harvForward = harvesterForward(m.harvesterYaw);
    glm::vec3 harvRight = glm::cross(harvForward, glm::vec3(0, 1, 0));
    for (auto& c : m.crew) {
        if (c.state == CrewState::Inside)
            c.pos = harvesterPosition() + harvRight * 6.1f - harvForward * 1.2f;
        if (c.state == CrewState::Running) {
            glm::vec3 d = c.destination - c.pos; d.y = 0;
            float distance = glm::length(d);
            float walkDt = std::min(dt, std::max(0.0f, m.elapsed - c.release));
            if (distance <= walkDt * 3.8f + 0.00001f) {
                c.pos = c.destination;
                c.state = CrewState::Waiting;
            } else c.pos += d / distance * (walkDt * 3.8f);
        }
        c.pos.y = getDuneHeight(c.pos.x, c.pos.z);
        bool outside = c.state == CrewState::Running || c.state == CrewState::Waiting;
        float dangerRadius = attack < 0 ? 16.0f : (attack >= 12 ? 44.0f : 32.0f);
        bool activeWorm = (attack < 0 && m.elapsed > 8) || (attack > 3 && attack < 18);
        if ((c.state == CrewState::Inside && attack >= 12)
            || (outside && activeWorm && horizontalDistance(c.pos, worm) < dangerRadius)) {
            c.state = CrewState::Lost; ++m.lost;
        }
        if (c.state == CrewState::Waiting || c.state == CrewState::Running || c.state == CrewState::Inside) ++waiting;
    }

    m.prompt = attack < 0 ? "HARVESTER FLEEING / RESCUE AMBER GROUPS" : "WORM BREACH / CLEAR THE HARVESTER";
    bool low = g_ornPos.y - ground <= 11.0f;
    bool steady = g_ornSpeed < 6;
    bool atBase = horizontalDistance(g_ornPos, m.base) < 14;
    if (m.aboard == RESCUE_CAPACITY) m.prompt = "CABIN FULL / RETURN TO CYAN BASE";
    if (attack >= 12) m.prompt = m.aboard > 0 ? "HARVESTER LOST / DELIVER SURVIVORS TO BASE" : "HARVESTER LOST / RESCUE REMAINING GROUPS";
    if (atBase && m.aboard > 0) {
        m.prompt = "HOLD SPACE / UNLOAD SURVIVORS";
        if (winch && low && steady) {
            m.unload += dt / 1.2f;
            if (m.unload >= 1) {
                for (auto& c : m.crew) if (c.state == CrewState::Aboard) c.state = CrewState::Safe;
                m.rescued += m.aboard; m.aboard = 0; m.unload = 0;
                m.best = std::max(m.best, m.rescued);
            }
        } else m.unload = 0;
    } else m.unload = 0;
    int nearest = -1;
    float nearestDistance = 12;
    for (int i = 0; i < int(m.crew.size()); ++i) {
        auto& c = m.crew[i];
        if (c.state != CrewState::Running && c.state != CrewState::Waiting) continue;
        float d = horizontalDistance(g_ornPos, c.pos);
        if (d < nearestDistance) { nearestDistance = d; nearest = i; }
    }
    if (nearest != m.target) m.pickup = 0;
    m.target = nearest;
    if (nearest >= 0 && m.aboard < RESCUE_CAPACITY) {
        m.prompt = !low ? "TOO HIGH / HOLD SPACE TO LOWER WINCH" : "HOLD SPACE / HOVER TO WINCH CREW";
        if (winch && low && steady) {
            m.pickup += dt / 0.42f;
            if (m.pickup >= 1) {
                m.crew[nearest].state = CrewState::Aboard;
                ++m.aboard; --waiting; m.pickup = 0; m.target = -1;
            }
        } else m.pickup = 0;
    } else m.pickup = 0;
    bool approachHit = attack < 0 && m.elapsed > 8 && horizontalDistance(g_ornPos, worm) < 18 && g_ornPos.y < worm.y + 22;
    bool breachHit = attack > 3 && attack < 15 && horizontalDistance(g_ornPos, worm) < 24 && g_ornPos.y < worm.y + 40;
    if (approachHit || breachHit) {
        for (auto& c : m.crew) if (c.state != CrewState::Safe && c.state != CrewState::Lost) { c.state = CrewState::Lost; ++m.lost; }
        m.aboard = 0; m.planeLost = true; m.prompt = "ORNITHOPTER LOST IN THE WORM'S MAW"; finishMission();
    } else if ((waiting == 0 && m.aboard == 0) || attack >= 60) {
        for (auto& c : m.crew) if (c.state != CrewState::Safe && c.state != CrewState::Lost) { c.state = CrewState::Lost; ++m.lost; }
        m.aboard = 0; finishMission();
    }
}
