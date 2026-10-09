#pragma once

// Include after arrakis_game.h. These tests never initialize GLFW or call GL.
#include <iostream>
#include <limits>

namespace arrakis_tests_detail {
struct Results {
    const char* current = "";
    int cases = 0;
    int failedCases = 0;
    int checks = 0;
    int failures = 0;

    void check(bool condition, const char* message) {
        ++checks;
        if (!condition) {
            ++failures;
            std::cerr << "[FAIL] " << current << ": " << message << '\n';
        }
    }

    template <typename Test>
    void run(const char* name, Test test) {
        current = name;
        ++cases;
        const int before = failures;
        test();
        if (failures != before) ++failedCases;
        std::cout << (failures == before ? "[PASS] " : "[FAIL] ") << name << '\n';
    }
};

inline bool near(float a, float b) { return std::abs(a - b) < 0.001f; }

inline int count(CrewState state) {
    int total = 0;
    for (const auto& c : g_mission.crew) if (c.state == state) ++total;
    return total;
}

inline void fresh(bool start = true) {
    for (bool& key : g_keys) key = false;
    g_mouseHover = glm::vec2(0);
    g_mouseTurnRate = 0;
    g_mission = RescueMission{};
    g_isBoosting = false;
    g_wingPhase = 0;
    resetMission(start);
}

inline void advance(float seconds) {
    const int steps = std::max(1, static_cast<int>(std::ceil(seconds * 120.0f)));
    for (int i = 0; i < steps; ++i) updateMission(seconds / steps);
}

inline void hover(glm::vec3 position, float altitude = 6.0f) {
    // Teleports exercise mission state transitions, not flight-route assistance.
    position.y = getDuneHeight(position.x, position.z) + altitude;
    g_ornPos = position;
    g_ornVel = glm::vec3(0);
    g_ornSpeed = 0;
    g_mouseHover = glm::vec2(0);
    g_mouseTurnRate = 0;
    g_mission.altitude = altitude;
}

inline glm::vec3 staging() {
    return g_mission.crew.empty() ? harvesterPosition() : g_mission.crew.front().destination;
}

inline void readyCrew() {
    // Compact explicit-state fixture isolates winch/capacity rules from routing.
    fresh();
    const glm::vec3 center(0, getDuneHeight(0, -10), -10);
    for (auto& group : g_mission.groups) {
        group.released = true;
        group.center = center;
    }
    for (std::size_t i = 0; i < g_mission.crew.size(); ++i) {
        auto& c = g_mission.crew[i];
        c.state = CrewState::Waiting;
        c.pos = center + glm::vec3(float(i % 6) * 0.6f, 0, float(i / 6) * 0.6f);
        c.pos.y = getDuneHeight(c.pos.x, c.pos.z);
        c.destination = c.pos;
    }
}

inline void collect(int occupants) {
    hover(staging());
    g_keys[GLFW_KEY_SPACE] = true;
    advance(occupants * 0.45f);
}

inline void deliver() {
    hover(g_mission.base);
    g_keys[GLFW_KEY_SPACE] = true;
    advance(1.3f);
}

inline void reconciled(Results& r) {
    const int unrescued = count(CrewState::Inside) + count(CrewState::Running) + count(CrewState::Waiting);
    r.check(g_mission.crew.size() == 36, "mission must contain 36 crew");
    r.check(count(CrewState::Safe) == g_mission.rescued, "Safe count must equal banked rescues");
    r.check(count(CrewState::Aboard) == g_mission.aboard, "Aboard count must equal cabin occupants");
    r.check(count(CrewState::Lost) == g_mission.lost, "Lost count must equal losses");
    r.check(g_mission.rescued + g_mission.aboard + g_mission.lost + unrescued == 36,
            "all 36 crew must be accounted for exactly once");
    r.check(g_mission.aboard >= 0 && g_mission.aboard <= RESCUE_CAPACITY, "cabin count must stay within capacity");
    if (g_mission.phase == MissionPhase::Debrief)
        r.check(unrescued == 0 && g_mission.aboard == 0, "debrief must leave no unresolved crew");
}
} // namespace arrakis_tests_detail

inline int runMissionTests() {
    using namespace arrakis_tests_detail;
    Results r;

    r.run("Reset and title do not advance", [&] {
        fresh(false);
        r.check(g_mission.phase == MissionPhase::Title, "reset(false) must enter title");
        r.check(g_mission.wave == 1 && near(g_mission.breachAt, 210), "first wave must have a 210-second timer");
        r.check(g_mission.rescued == 0 && g_mission.aboard == 0 && g_mission.lost == 0 && g_mission.best == 0,
                "fresh reset must clear all score counters");
        r.check(count(CrewState::Inside) == 36, "all crew must initially be inside");
        r.check(near(g_mission.altitude, 8) && near(g_mission.boost, 1), "reset must restore altitude and boost");
        const glm::vec3 position = g_ornPos;
        const float yaw = g_ornYaw;
        const glm::vec3 harvester = harvesterPosition(), worm = wormPosition();
        setMouseHover(1000, 250, 1000, 500);
        g_keys[GLFW_KEY_D] = g_keys[GLFW_KEY_SPACE] = true;
        advance(100.0f);
        r.check(near(g_mission.elapsed, 0), "title must not consume mission time");
        r.check(glm::distance(g_ornPos, position) < 0.001f && near(g_wingPhase, 0), "title update must not advance flight");
        r.check(near(g_ornYaw, yaw) && near(g_mouseTurnRate, 0), "title must not steer even with off-center mouse hover");
        r.check(count(CrewState::Inside) == 36 && near(g_mission.pickup, 0), "title must not release or pick up crew");
        r.check(glm::distance(harvesterPosition(), harvester) < 0.001f && glm::distance(wormPosition(), worm) < 0.001f,
                "title must freeze harvester and pursuing worm");
        reconciled(r);
    });

    r.run("Pause freezes mission and resumes", [&] {
        fresh();
        setMouseHover(1000, 250, 1000, 500);
        advance(7.0f);
        const RescueMission before = g_mission;
        const glm::vec3 position = g_ornPos;
        const float wings = g_wingPhase;
        const float yaw = g_ornYaw, turnRate = g_mouseTurnRate;
        const glm::vec3 harvester = harvesterPosition(), worm = wormPosition();
        const float gap = wormGap();
        g_mission.paused = true;
        g_keys[GLFW_KEY_W] = g_keys[GLFW_KEY_LEFT_SHIFT] = g_keys[GLFW_KEY_SPACE] = true;
        advance(100.0f);
        r.check(near(g_mission.elapsed, before.elapsed), "pause must freeze the breach clock");
        r.check(glm::distance(g_ornPos, position) < 0.001f && near(g_wingPhase, wings), "pause must freeze flight");
        r.check(near(g_ornYaw, yaw) && near(g_mouseTurnRate, turnRate), "pause must freeze yaw and smoothed turn rate");
        r.check(near(g_mission.boost, before.boost) && near(g_mission.pickup, before.pickup), "pause must freeze resources and winch progress");
        bool crewUnchanged = g_mission.crew.size() == before.crew.size();
        for (std::size_t i = 0; crewUnchanged && i < before.crew.size(); ++i)
            crewUnchanged = g_mission.crew[i].state == before.crew[i].state &&
                            glm::distance(g_mission.crew[i].pos, before.crew[i].pos) < 0.001f;
        r.check(crewUnchanged, "pause must freeze crew release and walking");
        r.check(glm::distance(harvesterPosition(), harvester) < 0.001f && glm::distance(wormPosition(), worm) < 0.001f &&
                near(g_mission.harvesterYaw, before.harvesterYaw) && near(g_mission.harvesterTravel, before.harvesterTravel) && near(wormGap(), gap),
                "pause must freeze moving harvester, travel, yaw, and pursuing worm");
        bool groupsUnchanged = g_mission.groups.size() == before.groups.size();
        for (std::size_t i = 0; groupsUnchanged && i < before.groups.size(); ++i)
            groupsUnchanged = g_mission.groups[i].released == before.groups[i].released &&
                              glm::distance(g_mission.groups[i].center, before.groups[i].center) < 0.001f;
        r.check(groupsUnchanged, "pause must freeze every evacuation group");
        for (bool& key : g_keys) key = false;
        setMouseHover(500, 250, 1000, 500);
        g_mission.paused = false;
        advance(0.1f);
        r.check(g_mission.elapsed > before.elapsed, "unpausing must resume mission time");
        reconciled(r);
    });

    r.run("Crew groups release on schedule and walk to fixed clusters", [&] {
        fresh();
        r.check(g_mission.groups.size() == 15, "exactly fifteen groups must contain thirty-six crew");
        int total = 0;
        for (std::size_t i = 0; i < g_mission.groups.size(); ++i) {
            const auto& group = g_mission.groups[i];
            r.check(group.count == int(i % 4) + 1 && near(group.release, 6.0f + 7.0f * float(i)) && !group.released,
                    "group sizes must repeat one through four, with first release six and cadence seven");
            int members = 0;
            unsigned slots = 0;
            for (const auto& c : g_mission.crew) if (c.group == int(i)) {
                ++members;
                r.check(c.slot >= 0 && c.slot < group.count && near(c.release, group.release),
                        "each crew member must have its group's scheduled release and a valid slot");
                if (c.slot >= 0 && c.slot < 4) slots |= 1u << c.slot;
            }
            r.check(members == group.count && slots == (1u << group.count) - 1,
                    "group membership and slots must cover each member exactly once");
            total += group.count;
        }
        r.check(total == 36 && g_mission.groups.back().count == 3, "group counts must total thirty-six with final group three");
        const glm::vec3 initialInside = g_mission.crew.back().pos;
        const glm::vec3 initialHarvester = harvesterPosition();
        advance(5.99f);
        r.check(count(CrewState::Inside) == 36, "no crew may release before six seconds");
        r.check(horizontalDistance(g_mission.crew.back().pos, initialInside) > 2 &&
                horizontalDistance(g_mission.crew.back().pos, harvesterPosition()) < 15 &&
                horizontalDistance(initialHarvester, harvesterPosition()) > 2,
                "Inside crew must follow the moving harvester, not remain at its initial location");
        advance(0.02f);
        r.check(g_mission.crew.front().state == CrewState::Running && count(CrewState::Inside) == 35,
                 "the first one-person group must release while the others remain inside");
        const glm::vec3 start = g_mission.crew.front().pos;
        const glm::vec3 destination = g_mission.crew.front().destination;
        const glm::vec3 center = g_mission.groups.front().center;
        const float scheduledYaw = glm::radians(harvesterRouteYaw(6));
        const glm::vec3 scheduledForward(-std::sin(scheduledYaw), 0, -std::cos(scheduledYaw));
        const glm::vec3 scheduledRight = glm::cross(scheduledForward, glm::vec3(0, 1, 0));
        const glm::vec3 scheduledExit = harvesterRoute(6) + scheduledRight * 6.1f - scheduledForward * 1.2f;
        r.check(horizontalDistance(center, scheduledExit + scheduledRight * 14.0f) < 0.001f &&
                horizontalDistance(destination, center) < 0.001f,
                "release exit and first group center must use the route and tangent at exactly scheduled time");
        advance(0.25f);
        r.check(std::abs(horizontalDistance(g_mission.crew.front().pos, start) - 3.8f * 0.25f) < 0.01f,
                "released crew must run outward at 3.8 meters per second");
        advance(5.74f);
        r.check(count(CrewState::Inside) == 35, "second group must remain inside before its thirteen-second release");
        advance(1.02f);
        r.check(g_mission.groups[1].released && count(CrewState::Inside) == 33, "second release must evacuate two crew together");
        advance(106.0f);
        r.check(count(CrewState::Waiting) == 36, "all real groups must eventually wait at separate fixed clusters");
        r.check(glm::distance(g_mission.crew.front().destination, destination) < 0.001f &&
                glm::distance(g_mission.groups.front().center, center) < 0.001f,
                "released destinations and group centers must never follow the harvester");
        bool atDestinations = true;
        for (const auto& c : g_mission.crew)
            atDestinations = atDestinations && horizontalDistance(c.pos, c.destination) < 0.15f &&
                             near(c.pos.y, getDuneHeight(c.pos.x, c.pos.z));
        r.check(atDestinations, "crew must arrive at their destinations and follow terrain height");
        r.check(horizontalDistance(g_mission.groups.front().center, g_mission.groups.back().center) > 20,
                "scheduled releases must produce different fixed evacuation clusters");
        const glm::vec3 waitingPosition = g_mission.crew.front().pos;
        advance(5);
        r.check(glm::distance(waitingPosition, g_mission.crew.front().pos) < 0.001f,
                "waiting crew must stay fixed while harvester and worm keep moving");
        reconciled(r);
    });

    r.run("Hover pickup requires Space and caps at eight", [&] {
        readyCrew();
        hover(staging());
        advance(1.0f);
        r.check(g_mission.aboard == 0, "hovering without Space must not pick up crew");
        g_keys[GLFW_KEY_SPACE] = true;
        advance(0.3f);
        r.check(g_mission.aboard == 0 && g_mission.pickup > 0, "pickup must require a dwell interval");
        g_keys[GLFW_KEY_SPACE] = false;
        advance(0.05f);
        r.check(near(g_mission.pickup, 0), "releasing Space must reset pickup progress");
        g_keys[GLFW_KEY_SPACE] = true;
        advance(0.5f);
        r.check(g_mission.aboard == 1 && count(CrewState::Aboard) == 1, "a steady low hover must winch a crew member");
        r.check(g_mission.rescued == 0 && g_mission.best == 0, "pickup must not bank score");
        advance(5.0f);
        r.check(RESCUE_CAPACITY == 8 && g_mission.aboard == 8, "cabin capacity must be exactly eight");
        r.check(count(CrewState::Waiting) == 28, "a full cabin must leave remaining crew on the ground");
        reconciled(r);
    });

    r.run("Moving too fast disallows pickup", [&] {
        readyCrew();
        hover(staging());
        g_keys[GLFW_KEY_SPACE] = true;
        g_ornVel = glm::vec3(20, 0, 0);
        updateMission(0.05f);
        r.check(g_ornSpeed > 6, "test setup must remain above pickup speed limit");
        r.check(g_mission.aboard == 0 && near(g_mission.pickup, 0), "moving aircraft must not accumulate winch progress");
        reconciled(r);
    });

    r.run("High altitude blocks pickup until Space lowers", [&] {
        readyCrew();
        hover(staging(), 35.0f);
        advance(0.75f);
        r.check(g_mission.aboard == 0 && near(g_mission.altitude, 35), "high hover must remain too high without Space");
        g_keys[GLFW_KEY_SPACE] = true;
        advance(0.5f);
        r.check(g_ornPos.y - getDuneHeight(g_ornPos.x, g_ornPos.z) > 11,
                "initial lowering interval must still exceed pickup clearance");
        r.check(g_mission.aboard == 0 && near(g_mission.pickup, 0), "Space must not pick up crew while clearance is too high");
        advance(2.0f);
        r.check(g_ornPos.y - getDuneHeight(g_ornPos.x, g_ornPos.z) <= 11, "Space must lower the aircraft into pickup range");
        r.check(g_mission.aboard > 0 && g_mission.rescued == 0, "lowered hover must allow pickup without banking score");
        reconciled(r);
    });

    r.run("Delivery banks score only at base", [&] {
        readyCrew();
        collect(3);
        r.check(g_mission.aboard == 3 && g_mission.rescued == 0, "three collected crew must remain unbanked");
        hover(g_mission.base + glm::vec3(15, 0, 0));
        advance(1.4f);
        r.check(g_mission.aboard == 3 && g_mission.rescued == 0 && near(g_mission.unload, 0),
                "holding Space just outside base radius must not deliver");
        hover(g_mission.base);
        g_keys[GLFW_KEY_SPACE] = false;
        advance(1.3f);
        r.check(g_mission.rescued == 0, "delivery at base still requires Space");
        g_keys[GLFW_KEY_SPACE] = true;
        advance(0.6f);
        r.check(g_mission.rescued == 0 && g_mission.unload > 0, "delivery must require unloading dwell time");
        g_keys[GLFW_KEY_SPACE] = false;
        advance(0.02f);
        r.check(near(g_mission.unload, 0), "interrupting unloading must reset its progress");
        deliver();
        r.check(g_mission.rescued == 3 && g_mission.best == 3 && g_mission.aboard == 0, "base must bank delivered crew and update best");
        r.check(count(CrewState::Safe) == 3 && g_mission.phase == MissionPhase::Flying, "delivered crew become Safe while rescue continues");
        reconciled(r);
    });

    r.run("Swallowing loses Inside and local crew but spares remote waiting crew", [&] {
        fresh();
        for (auto& group : g_mission.groups) group.release = 1000;
        for (auto& c : g_mission.crew) c.release = 1000;
        g_mission.elapsed = g_mission.breachAt + 11.5f;
        updateHarvester(g_mission.elapsed);
        g_mission.crew[1].state = CrewState::Running;
        g_mission.crew[1].pos = g_mission.crew[1].destination = wormPosition() + glm::vec3(40, 0, 0);
        g_mission.crew[2].state = CrewState::Waiting;
        g_mission.crew[2].pos = g_mission.crew[2].destination = g_mission.base;
        g_mission.crew[3].state = CrewState::Waiting;
        g_mission.crew[3].pos = g_mission.crew[3].destination = g_mission.base + glm::vec3(2, 0, 0);
        updateMission(0.25f);
        r.check(g_mission.lost == 0 && g_mission.phase == MissionPhase::Flying, "Inside and forty-meter crew must survive before swallow radius expands at twelve");
        updateMission(0.25f);
        r.check(near(wormAttackTime(), 12) && g_mission.lost == 34 && count(CrewState::Inside) == 0,
                "swallowing must lose Inside crew and local outside crew within forty-four meters");
        r.check(count(CrewState::Waiting) == 2 && g_mission.phase == MissionPhase::Flying && g_mission.rescued == 0,
                "remote waiting crew must survive harvester swallowing and keep extraction active");
        advance(8);
        hover(g_mission.base);
        g_keys[GLFW_KEY_SPACE] = true;
        advance(1.0f);
        r.check(g_mission.aboard == 2 && g_mission.lost == 34, "remote survivors must remain rescuable after worm retreat");
        deliver();
        r.check(g_mission.rescued == 2 && g_mission.phase == MissionPhase::Debrief, "remote survivors must be bankable after swallowing");
        reconciled(r);
    });

    r.run("Carried crew survive breach and can deliver", [&] {
        readyCrew();
        collect(8);
        r.check(g_mission.aboard == 8, "test setup must carry eight crew");
        for (auto& c : g_mission.crew) if (c.state != CrewState::Aboard) {
            c.state = CrewState::Inside;
            c.release = 1000;
        }
        g_keys[GLFW_KEY_SPACE] = false;
        g_mission.elapsed = g_mission.breachAt + 11.75f;
        updateMission(0.25f);
        r.check(g_mission.lost == 28 && g_mission.aboard == 8, "breach must spare carried crew but consume those left behind");
        r.check(g_mission.phase == MissionPhase::Flying && g_mission.rescued == 0, "survivors must still need extraction");
        reconciled(r);
        deliver();
        r.check(g_mission.phase == MissionPhase::Debrief && g_mission.rescued == 8 && g_mission.best == 8,
                "post-breach delivery must bank survivors and finish the mission");
        reconciled(r);
    });

    r.run("Worm collision loses occupants but preserves banked score", [&] {
        readyCrew();
        collect(8);
        deliver();
        collect(8);
        r.check(g_mission.rescued == 8 && g_mission.aboard == 8, "collision setup must include banked and carried crew");
        g_keys[GLFW_KEY_SPACE] = false;
        g_mission.elapsed = g_mission.breachAt + 4;
        updateHarvester(g_mission.elapsed);
        hover(wormPosition());
        updateMission(0.01f);
        r.check(g_mission.phase == MissionPhase::Debrief && g_mission.aboard == 0, "worm collision must end flight and empty cabin");
        r.check(g_mission.lost == 28 && g_mission.rescued == 8 && g_mission.best == 8,
                "collision must lose all unbanked crew and preserve Safe crew and best score");
        reconciled(r);
        const float elapsed = g_mission.elapsed;
        advance(10.0f);
        r.check(near(g_mission.elapsed, elapsed) && g_mission.lost == 28, "debrief must freeze time and avoid counting losses twice");
    });

    r.run("Extraction timeout reconciles all totals", [&] {
        readyCrew();
        collect(8);
        deliver();
        collect(8);
        hover(g_mission.base + glm::vec3(20, 0, 0));
        g_keys[GLFW_KEY_SPACE] = false;
        for (auto& c : g_mission.crew) if (c.state == CrewState::Waiting) {
            c.pos = c.destination = g_mission.base + glm::vec3(40, 0, 40);
        }
        g_mission.elapsed = g_mission.breachAt + 59.75f;
        updateMission(0.125f);
        r.check(g_mission.phase == MissionPhase::Flying && g_mission.aboard == 8 && g_mission.lost == 0 && count(CrewState::Waiting) == 20,
                 "carried and remote ground survivors must remain extractable before timeout");
        reconciled(r);
        updateMission(0.125f);
        r.check(near(wormAttackTime(), 60) && g_mission.phase == MissionPhase::Debrief, "extraction must end at attack time sixty");
        r.check(g_mission.rescued == 8 && g_mission.best == 8 && g_mission.lost == 28 && g_mission.aboard == 0,
                 "timeout must lose both undelivered occupants and unresolved remote crew without removing banked score");
        reconciled(r);
    });

    r.run("Retry resets counters and preserves best", [&] {
        readyCrew();
        collect(8);
        deliver();
        g_keys[GLFW_KEY_SPACE] = false;
        g_mission.elapsed = g_mission.breachAt + 60;
        updateMission(0);
        r.check(g_mission.phase == MissionPhase::Debrief && g_mission.best == 8, "retry setup must finish with a banked best score");
        g_mission.paused = true;
        g_mission.boost = 0.2f;
        g_mission.pickup = 0.7f;
        g_mission.unload = 0.4f;
        g_mission.target = 5;
        g_ornVel = glm::vec3(10, 0, 0);
        setMouseHover(1000, 0, 1000, 500);
        g_mouseTurnRate = -50;
        resetMission(true);
        r.check(g_mission.phase == MissionPhase::Flying && !g_mission.paused && g_mission.wave == 1, "retry must restart the same wave unpaused");
        r.check(g_mission.best == 8 && g_mission.rescued == 0 && g_mission.aboard == 0 && g_mission.lost == 0,
                "retry must clear current counters but retain best");
        r.check(near(g_mission.elapsed, 0) && near(g_mission.pickup, 0) && near(g_mission.unload, 0) && g_mission.target == -1,
                "retry must reset clock and winch state");
        r.check(near(g_mission.boost, 1) && near(g_mission.altitude, 8) && glm::length(g_ornVel) < 0.001f,
                "retry must reset flight resources and velocity");
        r.check(glm::length(g_mouseHover) < 0.001f && near(g_mouseTurnRate, 0) && near(flightCameraPitch(), 12),
                "retry must center mouse steering, clear turn inertia, and restore camera pitch");
        r.check(glm::distance(g_ornPos, g_mission.base + glm::vec3(0, 8, 0)) < 0.001f && count(CrewState::Inside) == 36,
                "retry must return aircraft to base and replenish crew");
        reconciled(r);
    });

    r.run("Next wave shortens timer with a floor", [&] {
        fresh();
        g_mission.best = 17;
        finishMission();
        resetMission(true, true);
        r.check(g_mission.wave == 2 && near(g_mission.breachAt, 200), "next wave must increment wave and shorten timer by ten seconds");
        r.check(g_mission.best == 17 && g_mission.phase == MissionPhase::Flying && count(CrewState::Inside) == 36,
                "next wave must retain best and initialize fresh crew");
        for (int i = 0; i < 8; ++i) {
            const float previous = g_mission.breachAt;
            resetMission(true, true);
            r.check(g_mission.breachAt <= previous && g_mission.breachAt >= 150, "later waves must never lengthen timer or fall below one hundred fifty");
        }
        r.check(g_mission.wave == 10 && near(g_mission.breachAt, 150), "high waves must clamp timer to one hundred fifty seconds");
        reconciled(r);
    });

    r.run("Complete all thirty-six rescues through real state flow", [&] {
        fresh();
        advance(120);
        r.check(count(CrewState::Waiting) == 36, "complete-rescue setup must use released and walked crew");
        int banked = 0;
        for (int flight = 0; flight < 5; ++flight) {
            const int batch = std::min(8, 36 - banked);
            for (int person = 0; person < batch; ++person) {
                int nearest = -1;
                float distance = std::numeric_limits<float>::infinity();
                for (int i = 0; i < int(g_mission.crew.size()); ++i) if (g_mission.crew[i].state == CrewState::Waiting) {
                    const float d = horizontalDistance(g_ornPos, g_mission.crew[i].pos);
                    if (d < distance) { distance = d; nearest = i; }
                }
                if (nearest < 0) break;
                hover(g_mission.crew[nearest].pos);
                g_keys[GLFW_KEY_SPACE] = true;
                advance(0.45f);
            }
            r.check(g_mission.aboard == batch && g_mission.rescued == banked, "each staging visit must load the expected batch without banking it");
            reconciled(r);
            deliver();
            banked += batch;
            r.check(g_mission.rescued == banked && g_mission.aboard == 0, "each base visit must bank exactly its carried batch");
            r.check(g_mission.phase == (banked == 36 ? MissionPhase::Debrief : MissionPhase::Flying),
                    "mission must remain active until the final batch is delivered");
            reconciled(r);
        }
        r.check(g_mission.rescued == 36 && g_mission.best == 36 && g_mission.lost == 0 && count(CrewState::Safe) == 36,
                "all thirty-six crew must finish Safe with full banked score and no losses");
        r.check(g_mission.elapsed < g_mission.breachAt, "state-flow simulation must finish before breach without changing its timer");
    });

    r.run("Input-only round trips rescue all thirty-six before breach", [&] {
        fresh();
        r.check(g_mission.wave == 1 && near(g_mission.breachAt, 210), "playability must use the unmodified first mission timer");
        // Neutral mouse keeps yaw zero. After reset, only keys and updateMission
        // change game state; no hover(), collect(), or deliver() assistance.
        const glm::vec3 base = g_mission.base;
        constexpr float dt = 1.0f / 120.0f;
        int steps = 0, travelSteps = 0, boostSteps = 0, directionsUsed = 0, directionReversals = 0;
        bool finiteState = true, clearOfGround = true, neutralHeading = true;
        float minimumClearance = g_ornPos.y - getDuneHeight(g_ornPos.x, g_ornPos.z);
        auto finiteVector = [](glm::vec3 v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        auto step = [&] {
            if (g_keys[GLFW_KEY_W]) directionsUsed |= 1;
            if (g_keys[GLFW_KEY_A]) directionsUsed |= 2;
            if (g_keys[GLFW_KEY_S]) directionsUsed |= 4;
            if (g_keys[GLFW_KEY_D]) directionsUsed |= 8;
            updateMission(dt);
            ++steps;
            if (g_isBoosting) ++boostSteps;
            finiteState = finiteState && finiteVector(g_ornPos) && finiteVector(g_ornVel) &&
                          finiteVector(g_mission.base) && std::isfinite(g_ornYaw) &&
                          std::isfinite(g_ornPitch) && std::isfinite(g_ornRoll) &&
                          std::isfinite(g_ornSpeed) && std::isfinite(g_wingPhase) &&
                          std::isfinite(g_mission.elapsed) && std::isfinite(g_mission.breachAt) &&
                          std::isfinite(g_mission.altitude) && std::isfinite(g_mission.boost) &&
                           std::isfinite(g_mission.pickup) && std::isfinite(g_mission.unload);
            finiteState = finiteState && std::isfinite(g_mouseHover.x) && std::isfinite(g_mouseHover.y) &&
                          std::isfinite(g_mouseTurnRate);
            neutralHeading = neutralHeading && near(g_ornYaw, 0) && near(g_mouseTurnRate, 0) &&
                             glm::length(g_mouseHover) < 0.001f;
            for (const auto& c : g_mission.crew)
                finiteState = finiteState && finiteVector(c.pos) && finiteVector(c.destination) && std::isfinite(c.release);
            const float clearance = g_ornPos.y - getDuneHeight(g_ornPos.x, g_ornPos.z);
            clearOfGround = clearOfGround && std::isfinite(clearance) && clearance >= 3.0f - 0.001f;
            minimumClearance = std::min(minimumClearance, clearance);
        };
        auto active = [&] {
            return finiteState && g_mission.phase == MissionPhase::Flying && g_mission.elapsed < g_mission.breachAt;
        };
        auto fly = [&](glm::vec3 target) {
            const int started = steps;
            int previousX = 0, previousZ = 0;
            float pulseX = 0, pulseZ = 0;
            bool braking = false;
            while (active() && steps - started < 12 * 120) {
                const glm::vec3 delta = target - g_ornPos;
                const float distance = horizontalDistance(target, g_ornPos);
                // SPACE has damping 12/s. Begin braking before the remaining
                // distance falls below the coast distance plus a 4-unit margin.
                braking = braking || distance < 4.0f + g_ornSpeed / 12.0f;
                for (bool& key : g_keys) key = false;
                g_keys[GLFW_KEY_SPACE] = braking;
                g_keys[GLFW_KEY_LEFT_SHIFT] = !braking && distance > 20 && g_mission.boost > 0.08f;
                if (distance <= 2.0f && g_ornSpeed < 0.5f) {
                    travelSteps += steps - started;
                    return true;
                }
                if (distance > 2.0f) {
                    const float speed = braking ? 2.8f : (g_keys[GLFW_KEY_LEFT_SHIFT] ? 48.0f : 28.0f);
                    const float damping = braking ? 12.0f : 5.5f;
                    // Project into the aircraft's right/back axes, then express
                    // proportional commands through bool keys, not velocity edits.
                    const glm::vec3 backward = -flightForward();
                    const glm::vec3 right = glm::cross(flightForward(), glm::vec3(0, 1, 0));
                    auto axis = [&](float error, float velocity, float& pulse, int negative, int positive) {
                        const float predicted = error - velocity / damping;
                        if (std::abs(error) < 1.0f || predicted * error <= 0) { pulse = 0; return; }
                        pulse += std::min(1.0f, std::abs(predicted) * (braking ? 2.0f : 4.0f) / speed);
                        if (pulse >= 1) {
                            g_keys[error < 0 ? negative : positive] = true;
                            pulse -= 1;
                        }
                    };
                    axis(glm::dot(delta, right), glm::dot(g_ornVel, right), pulseX, GLFW_KEY_A, GLFW_KEY_D);
                    axis(glm::dot(delta, backward), glm::dot(g_ornVel, backward), pulseZ, GLFW_KEY_W, GLFW_KEY_S);
                }
                const int commandX = int(g_keys[GLFW_KEY_D]) - int(g_keys[GLFW_KEY_A]);
                const int commandZ = int(g_keys[GLFW_KEY_S]) - int(g_keys[GLFW_KEY_W]);
                if (commandX) {
                    if (previousX && previousX != commandX) ++directionReversals;
                    previousX = commandX;
                }
                if (commandZ) {
                    if (previousZ && previousZ != commandZ) ++directionReversals;
                    previousZ = commandZ;
                }
                step();
            }
            travelSteps += steps - started;
            return false;
        };
        auto holdSpace = [&] {
            for (bool& key : g_keys) key = false;
            g_keys[GLFW_KEY_SPACE] = true;
            step();
        };
        auto seconds = [dt](int ticks) { return std::round(ticks * dt * 1000.0f) / 1000.0f; };
        int flights = 0, clustersVisited = 0, departure = steps, outboundSteps = 0, boardingSteps = 0;
        unsigned visitedGroups = 0;
        while (active()) {
            const int outside = count(CrewState::Running) + count(CrewState::Waiting);
            const bool allReleased = count(CrewState::Inside) == 0;
            if (g_mission.aboard == RESCUE_CAPACITY || (allReleased && outside == 0 && g_mission.aboard > 0)) {
                const int batch = g_mission.aboard, banked = g_mission.rescued, boarded = steps;
                r.check(batch == 8 || (allReleased && outside == 0), "return only with a full cabin or the final released survivors");
                const bool reachedBase = fly(base);
                const int arrivedBase = steps;
                r.check(reachedBase, "input-driven return leg must reach base without timing out");
                if (!reachedBase) break;
                while (active() && g_mission.aboard > 0) holdSpace();
                r.check(g_mission.rescued == banked + batch && g_mission.aboard == 0,
                        "Space at base must bank exactly the actual carried load");
                reconciled(r);
                std::cout << "Input flight " << ++flights
                          << ": outbound=" << seconds(outboundSteps)
                          << "s boarding/waiting=" << seconds(boardingSteps)
                          << "s return=" << seconds(arrivedBase - boarded)
                          << "s unloading=" << seconds(steps - arrivedBase)
                          << "s batch=" << batch << " rescued=" << g_mission.rescued
                          << " elapsed=" << g_mission.elapsed << "s trip=" << seconds(steps - departure) << "s\n";
                departure = steps;
                outboundSteps = boardingSteps = 0;
                continue;
            }
            int nearest = -1;
            float nearestDistance = std::numeric_limits<float>::infinity();
            for (int i = 0; i < int(g_mission.crew.size()); ++i) {
                const auto& c = g_mission.crew[i];
                if (c.state != CrewState::Running && c.state != CrewState::Waiting) continue;
                const float distance = horizontalDistance(g_ornPos, c.pos);
                if (distance < nearestDistance) { nearestDistance = distance; nearest = i; }
            }
            if (nearest < 0) {
                holdSpace();
                ++boardingSteps;
                continue;
            }
            const auto& crew = g_mission.crew[nearest];
            const int group = crew.group;
            const glm::vec3 pickupPoint = crew.state == CrewState::Running ? crew.destination : crew.pos;
            const int started = steps;
            const bool reachedCrew = fly(pickupPoint);
            outboundSteps += steps - started;
            r.check(reachedCrew, "input-driven outbound leg must reach the nearest released crew's fixed cluster");
            if (!reachedCrew) break;
            ++clustersVisited;
            if (group >= 0 && group < 15) visitedGroups |= 1u << group;
            const int arrivedCrew = steps;
            while (active() && g_mission.aboard < RESCUE_CAPACITY && steps - arrivedCrew < 3 * 120 &&
                   (g_mission.crew[nearest].state == CrewState::Running || g_mission.crew[nearest].state == CrewState::Waiting))
                holdSpace();
            boardingSteps += steps - arrivedCrew;
        }
        for (bool& key : g_keys) key = false;
        r.check(finiteState, "all flight and crew float state must remain finite at every substep");
        r.check(neutralHeading, "neutral-hover WASD rescue route must never auto-align the nose to its velocity");
        r.check(clearOfGround, "aircraft must maintain at least three units of terrain clearance at every substep");
        r.check(directionsUsed == 15 && boostSteps > 0, "round trips must exercise all WASD directions and actual SHIFT boosting");
        r.check(directionReversals == 0, "each flight leg must settle without reversing cardinal input or oscillating about its target");
        r.check(g_mission.rescued == 36 && g_mission.lost == 0 && count(CrewState::Safe) == 36,
                "real input simulation must rescue all thirty-six crew without losses");
        r.check(g_mission.phase == MissionPhase::Debrief && g_mission.best == 36 && g_mission.aboard == 0,
                "final partial delivery must end the mission with a fully banked best score");
        r.check(g_mission.elapsed < 210 && near(g_mission.breachAt, 210), "input-only rescue must finish before the unchanged 210-second breach");
        r.check(flights == 5 && clustersVisited >= 15 && visitedGroups == (1u << 15) - 1,
                "real input route must visit the fifteen released groups and bank four full loads plus a final partial load");
        reconciled(r);
        std::cout << "Input-only rescue: travel=" << seconds(travelSteps)
                  << "s elapsed=" << g_mission.elapsed << "s rescued=" << g_mission.rescued
                  << "/36 aboard=" << g_mission.aboard << " lost=" << g_mission.lost
                    << " flights=" << flights << " clusters=" << clustersVisited
                    << " min-clearance=" << minimumClearance << " approach-reversals=" << directionReversals << '\n';
    });

    r.run("Harvester follows analytic terrain route and freezes at breach", [&] {
        fresh();
        for (float t : {0.0f, 6.0f, 25.0f, 60.0f, 104.0f, 150.0f, 210.0f}) {
            const float x = -40.0f + 0.62f * t + 15.0f * std::sin(0.045f * t);
            const float z = -55.0f + 22.0f * std::sin(0.032f * t) + 9.0f * std::sin(0.071f * t);
            const glm::vec3 expected(x, getDuneHeight(x, z), z);
            r.check(glm::distance(harvesterRoute(t), expected) < 0.001f, "route must match both sinusoidal coordinates and terrain height");
            const float yaw = glm::radians(harvesterRouteYaw(t));
            const glm::vec3 heading(-std::sin(yaw), 0, -std::cos(yaw));
            const glm::vec3 tangent(0.62f + 0.675f * std::cos(0.045f * t), 0,
                                    0.704f * std::cos(0.032f * t) + 0.639f * std::cos(0.071f * t));
            r.check(std::isfinite(yaw) && glm::dot(heading, glm::normalize(tangent)) > 0.99999f,
                    "harvester yaw must align model negative-Z forward with the analytic tangent");
            updateHarvester(t);
            r.check(glm::distance(harvesterPosition(), expected) < 0.001f &&
                    std::abs(std::remainder(g_mission.harvesterYaw - harvesterRouteYaw(t), 360.0f)) < 0.001f,
                    "elapsed-time harvester helper must update world position and tangent yaw");
        }
        const glm::vec3 atBreach = harvesterPosition();
        const float yawAtBreach = g_mission.harvesterYaw, travelAtBreach = g_mission.harvesterTravel;
        updateHarvester(g_mission.breachAt + 50);
        r.check(glm::distance(harvesterPosition(), atBreach) < 0.001f && near(g_mission.harvesterYaw, yawAtBreach) &&
                near(g_mission.harvesterTravel, travelAtBreach), "harvester pose and travel must freeze at breach");
        fresh();
        float previousGap = 221;
        for (float t : {0.0f, 1.0f, 30.0f, 75.0f, 104.0f, 150.0f, 209.0f, 210.0f, 230.0f}) {
            g_mission.elapsed = t;
            updateHarvester(t);
            const float expected = 220.0f - 202.0f * glm::clamp(t / g_mission.breachAt, 0.0f, 1.0f);
            r.check(near(wormGap(), expected) && wormGap() <= previousGap,
                    "worm pursuit gap must shrink monotonically and linearly from220 to18, then clamp");
            r.check(horizontalDistance(wormPosition(), harvesterPosition() - glm::vec3(expected, 0, 0)) < 0.001f,
                    "pursuing worm must remain the current gap west of the moving harvester");
            r.check(near(g_mission.wormYaw, 90) && near(wormPosition().y, getDuneHeight(wormPosition().x, wormPosition().z)),
                    "pursuing worm must face east at ninety degrees and follow terrain");
            previousGap = wormGap();
        }
    });

    r.run("Scheduled releases have no frame-spawn or destination drift", [&] {
        std::vector<Evacuee> reference;
        std::vector<EvacuationGroup> groups;
        for (int frequency : {30, 60, 120}) {
            fresh();
            // Absolute ticks avoid elapsed accumulation differences in the fixture.
            // The real controller above never edits time or any world state.
            for (int tick = 1; tick <= 30 * frequency; ++tick) {
                g_mission.elapsed = float(tick - 1) / float(frequency);
                updateMission(1.0f / float(frequency));
            }
            if (reference.empty()) {
                reference = g_mission.crew;
                groups = g_mission.groups;
            } else {
                for (std::size_t i = 0; i < reference.size(); ++i)
                    r.check(g_mission.crew[i].state == reference[i].state &&
                            horizontalDistance(g_mission.crew[i].pos, reference[i].pos) < 0.01f &&
                            glm::distance(g_mission.crew[i].destination, reference[i].destination) < 0.001f,
                            "equal absolute time must give identical release, walk position, and scheduled destination across frame rates");
                for (std::size_t i = 0; i < groups.size(); ++i)
                    r.check(g_mission.groups[i].released == groups[i].released &&
                            glm::distance(g_mission.groups[i].center, groups[i].center) < 0.001f,
                            "group center must be computed at scheduled route pose, not at the frame that notices release");
            }
        }
        fresh();
        g_mission.elapsed = 5.99f;
        updateHarvester(g_mission.elapsed);
        updateMission(0.02f);
        const glm::vec3 shortlyAfterRelease = g_mission.crew.front().pos;
        const glm::vec3 fixedDestination = g_mission.crew.front().destination;
        const glm::vec3 fixedCenter = g_mission.groups.front().center;
        fresh();
        g_mission.elapsed = 5.75f;
        updateHarvester(g_mission.elapsed);
        updateMission(0.26f);
        r.check(horizontalDistance(g_mission.crew.front().pos, shortlyAfterRelease) < 0.001f &&
                glm::distance(g_mission.crew.front().destination, fixedDestination) < 0.001f &&
                glm::distance(g_mission.groups.front().center, fixedCenter) < 0.001f,
                "crossing six-second release in a coarse frame must only walk the time since scheduled release");
    });

    r.run("Pursuing worm applies local crew and aircraft collision envelopes", [&] {
        fresh();
        for (auto& group : g_mission.groups) group.release = 1000;
        for (auto& c : g_mission.crew) c.release = 1000;
        g_mission.elapsed = 100;
        updateHarvester(g_mission.elapsed);
        for (int i = 0; i < 2; ++i) {
            auto& c = g_mission.crew[i];
            c.state = i == 0 ? CrewState::Running : CrewState::Waiting;
            c.pos = c.destination = wormPosition() + glm::vec3(i == 0 ? 15.0f : 17.0f, 0, 0);
        }
        updateMission(0.01f);
        r.check(g_mission.crew[0].state == CrewState::Lost && g_mission.crew[1].state == CrewState::Waiting && g_mission.lost == 1,
                "approaching worm must kill outside crew within16 meters but spare those beyond16");
        r.check(count(CrewState::Inside) == 34 && g_mission.phase == MissionPhase::Flying,
                "pursuit must not swallow Inside crew or end a remote aircraft's mission");
        hover(wormPosition(), 30);
        updateMission(0.01f);
        r.check(g_mission.phase == MissionPhase::Flying && !g_mission.planeLost,
                "approaching worm must spare an aircraft above22 meters terrain clearance");
        hover(wormPosition(), 21);
        updateMission(0.01f);
        r.check(g_mission.phase == MissionPhase::Debrief && g_mission.planeLost && g_mission.lost == 36,
                "approaching worm must collide with an aircraft within18 meters and below22 terrain clearance");
        reconciled(r);

        fresh();
        for (auto& group : g_mission.groups) group.release = 1000;
        for (auto& c : g_mission.crew) c.release = 1000;
        g_mission.elapsed = g_mission.breachAt + 4;
        updateHarvester(g_mission.elapsed);
        for (int i = 0; i < 2; ++i) {
            auto& c = g_mission.crew[i];
            c.state = CrewState::Waiting;
            c.pos = c.destination = wormPosition() + glm::vec3(i == 0 ? 31.0f : 33.0f, 0, 0);
        }
        hover(wormPosition(), 45);
        updateMission(0.01f);
        r.check(g_mission.crew[0].state == CrewState::Lost && g_mission.crew[1].state == CrewState::Waiting,
                "breach must kill local outside crew within32 meters but spare crew beyond32 before swallowing");
        r.check(g_mission.phase == MissionPhase::Flying, "breach must spare aircraft above forty meters");
        hover(wormPosition(), 39);
        updateMission(0.01f);
        r.check(g_mission.planeLost && g_mission.phase == MissionPhase::Debrief,
                "active breach must collide with aircraft within24 meters below ground plus40");
        reconciled(r);

        fresh();
        for (auto& group : g_mission.groups) group.release = 1000;
        for (auto& c : g_mission.crew) c.release = 1000;
        g_mission.elapsed = 8;
        updateHarvester(g_mission.elapsed);
        auto& remote = g_mission.crew.front();
        remote.state = CrewState::Waiting;
        remote.pos = remote.destination = wormPosition();
        updateMission(0);
        r.check(remote.state == CrewState::Waiting, "pursuit crew hazards must remain inactive at exactly eight seconds");
        updateMission(0.01f);
        r.check(remote.state == CrewState::Lost && g_mission.lost == 1, "pursuit crew hazard must activate only after eight seconds");

        fresh();
        for (auto& group : g_mission.groups) group.release = 1000;
        for (auto& c : g_mission.crew) c.release = 1000;
        g_mission.elapsed = g_mission.breachAt + 18;
        updateHarvester(g_mission.elapsed);
        g_mission.crew.front().state = CrewState::Waiting;
        g_mission.crew.front().pos = g_mission.crew.front().destination = wormPosition();
        hover(g_mission.base);
        updateMission(0);
        r.check(g_mission.lost == 35 && count(CrewState::Waiting) == 1 && g_mission.phase == MissionPhase::Flying,
                "outside crew must survive within the former swallow radius once attack reaches eighteen");
        reconciled(r);
    });

    r.run("Cursor normalization, deadzone, and resize are arithmetic only", [&] {
        fresh();
        const glm::vec3 position = g_ornPos;
        const float yaw = g_ornYaw, elapsed = g_mission.elapsed;
        setMouseHover(500, 250, 1000, 500);
        r.check(glm::length(g_mouseHover) < 0.001f, "window center must produce neutral hover");
        for (double x : {440.0, 460.0, 500.0, 540.0, 560.0}) {
            setMouseHover(x, 250, 1000, 500);
            r.check(near(g_mouseHover.x, 0), "horizontal +/-0.12 deadzone must not steer");
        }
        for (double y : {220.0, 230.0, 250.0, 270.0, 280.0}) {
            setMouseHover(500, y, 1000, 500);
            r.check(near(g_mouseHover.y, 0), "vertical +/-0.12 deadzone must remain centered");
        }
        setMouseHover(570, 250, 1000, 500);
        r.check(g_mouseHover.x > 0 && g_mouseHover.x < 1, "just beyond right deadzone must gently steer right");
        setMouseHover(430, 250, 1000, 500);
        r.check(g_mouseHover.x < 0 && g_mouseHover.x > -1, "just beyond left deadzone must gently steer left");
        setMouseHover(0, 250, 1000, 500);
        r.check(near(g_mouseHover.x, -1) && near(g_mouseHover.y, 0), "left window edge must produce full left hover");
        setMouseHover(1000, 250, 1000, 500);
        r.check(near(g_mouseHover.x, 1) && near(g_mouseHover.y, 0), "right window edge must produce full right hover");
        setMouseHover(800, 100, 1000, 500);
        const glm::vec2 original = g_mouseHover;
        r.check(near(original.x, 0.48f / 0.88f) && near(original.y, -0.48f / 0.88f),
                "cursor values beyond the deadzone must rescale the remaining range to full normalized steering");
        setMouseHover(1600, 200, 2000, 1000);
        r.check(glm::distance(g_mouseHover, original) < 0.001f, "resizing must preserve normalized hover at equivalent coordinates");
        setMouseHover(1000, 500, 2000, 1000);
        r.check(glm::length(g_mouseHover) < 0.001f, "new window center must remain neutral after resize");
        r.check(glm::distance(g_ornPos, position) < 0.001f && near(g_ornYaw, yaw) && near(g_mission.elapsed, elapsed) && near(g_mouseTurnRate, 0),
                "cursor arithmetic alone must not update flight, mission time, or turn inertia");
    });

    r.run("Outside and invalid cursor coordinates clear both hover axes", [&] {
        fresh();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double infinity = std::numeric_limits<double>::infinity();
        struct Cursor { double x, y; int width, height; };
        const Cursor invalid[] = {
            {-0.01, 250, 1000, 500}, {1000.01, 250, 1000, 500},
            {500, -0.01, 1000, 500}, {500, 500.01, 1000, 500},
            {nan, 250, 1000, 500}, {500, nan, 1000, 500},
            {infinity, 250, 1000, 500}, {-infinity, 250, 1000, 500},
            {500, infinity, 1000, 500}, {500, -infinity, 1000, 500},
            {0, 0, 0, 500}, {0, 0, 1000, 0}, {0, 0, -1, 500}, {0, 0, 1000, -1}
        };
        for (const auto& cursor : invalid) {
            setMouseHover(1000, 0, 1000, 500);
            r.check(glm::length(g_mouseHover) > 0.5f, "invalid-input setup must first have active hover");
            setMouseHover(cursor.x, cursor.y, cursor.width, cursor.height);
            r.check(std::isfinite(g_mouseHover.x) && std::isfinite(g_mouseHover.y) && glm::length(g_mouseHover) < 0.001f,
                    "outside, nonfinite, or invalid-size cursor input must clear both axes without NaNs");
        }
    });

    r.run("Mouse hover turns stationary aircraft without clicks", [&] {
        for (int side : {-1, 1}) {
            fresh();
            const glm::vec3 position = g_ornPos;
            setMouseHover(side < 0 ? 0 : 1000, 250, 1000, 500);
            advance(0.5f);
            r.check(g_ornYaw * side < -1 && g_mouseTurnRate * side < -1,
                    "right hover must decrease yaw and left hover must increase it without any key or click");
            r.check(near(g_mouseTurnRate, -side * 80.0f * (1.0f - std::exp(-8.0f * 0.5f))),
                    "hover yaw rate must exponentially approach eighty degrees per second at rate eight");
            r.check(horizontalDistance(g_ornPos, position) < 0.001f && near(g_ornSpeed, 0),
                    "stationary hover steering must rotate without creating translation");
            r.check(flightForward().x * side > 0 && flightForward().z < 0,
                    "right hover must face positive X and left hover negative X from initial negative-Z heading");
            setMouseHover(500, 250, 1000, 500);
            advance(2.0f);
            const float steeredYaw = g_ornYaw;
            const glm::vec3 steeredForward = flightForward();
            const glm::vec3 departure = g_ornPos;
            g_keys[GLFW_KEY_W] = true;
            advance(0.5f);
            const glm::vec3 displacement(g_ornPos.x - departure.x, 0, g_ornPos.z - departure.z);
            r.check(glm::length(displacement) > 1 && glm::dot(glm::normalize(displacement), steeredForward) > 0.999f &&
                    displacement.x * side > 0,
                    "forward input after mouse steering must fly toward the hovered side in the new heading");
            r.check(std::abs(g_ornYaw - steeredYaw) < 0.001f,
                    "centered forward flight must retain the mouse-selected nose heading");
        }
    });

    r.run("Arrow keys remain a stationary yaw backup", [&] {
        for (int side : {-1, 1}) {
            fresh();
            const glm::vec3 position = g_ornPos;
            g_keys[side < 0 ? GLFW_KEY_LEFT : GLFW_KEY_RIGHT] = true;
            advance(0.5f);
            r.check(g_ornYaw * side < -1 && g_mouseTurnRate * side < -1,
                    "left/right arrow fallback must use the same yaw sign as mouse hover");
            r.check(horizontalDistance(g_ornPos, position) < 0.001f && near(g_ornSpeed, 0),
                    "arrow yaw fallback must work while stationary");
        }
    });

    r.run("Forward, reverse, and strafe stay relative to yaw without auto-align", [&] {
        const int keys[] = {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D};
        const float headings[] = {0, 90, -90, 179, -181, 359, 361};
        for (float heading : headings) {
            const float radians = glm::radians(heading);
            const glm::vec3 forward(-std::sin(radians), 0, -std::cos(radians));
            const glm::vec3 right(std::cos(radians), 0, -std::sin(radians));
            for (int key : keys) {
                fresh();
                g_ornYaw = heading;
                r.check(glm::distance(flightForward(), forward) < 0.001f, "flightForward must match the yaw convention including wraparound");
                const glm::vec3 expected = key == GLFW_KEY_W ? forward : key == GLFW_KEY_S ? -forward : key == GLFW_KEY_D ? right : -right;
                const glm::vec3 position = g_ornPos;
                g_keys[key] = true;
                advance(0.5f);
                const glm::vec3 displacement(g_ornPos.x - position.x, 0, g_ornPos.z - position.z);
                r.check(glm::length(g_ornVel) > 1 && glm::dot(glm::normalize(g_ornVel), expected) > 0.999f &&
                        glm::length(displacement) > 1 && glm::dot(glm::normalize(displacement), expected) > 0.999f,
                        "W/S and A/D must translate along aircraft forward/back and left/right axes");
                r.check(std::abs(std::remainder(g_ornYaw - heading, 360.0f)) < 0.001f && near(g_mouseTurnRate, 0),
                        "translation, especially reverse and strafe, must never turn the nose toward velocity");
            }
        }
    });

    r.run("Centered hover damps turn rate instead of snapping heading", [&] {
        fresh();
        setMouseHover(1000, 250, 1000, 500);
        advance(0.5f);
        const float yaw = g_ornYaw, rate = g_mouseTurnRate;
        setMouseHover(500, 250, 1000, 500);
        advance(0.25f);
        r.check(near(g_mouseTurnRate, rate * std::exp(-8.0f * 0.25f)), "returning mouse to deadzone must exponentially damp yaw rate");
        r.check(g_ornYaw < yaw && g_mouseTurnRate < 0, "turn inertia must decay smoothly without snapping nose back to center");
        advance(2.0f);
        r.check(std::abs(g_mouseTurnRate) < 0.001f, "centered hover must settle to stationary yaw");
        const float settled = g_ornYaw;
        advance(0.25f);
        r.check(std::abs(g_ornYaw - settled) < 0.001f, "settled neutral hover must retain the last heading");
    });

    r.run("Hover turn smoothing is stable across frame rates", [&] {
        float yaw[3], rate[3];
        const int frequencies[] = {30, 60, 120};
        for (int i = 0; i < 3; ++i) {
            fresh();
            setMouseHover(1000, 250, 1000, 500);
            for (int step = 0; step < frequencies[i]; ++step) updateMission(1.0f / frequencies[i]);
            yaw[i] = g_ornYaw;
            rate[i] = g_mouseTurnRate;
            r.check(std::abs(rate[i] + 80.0f * (1.0f - std::exp(-8.0f))) < 0.002f,
                    "exponential target yaw rate must be independent of thirty, sixty, or one-hundred-twenty Hz updates");
            const float integral = -80.0f * (1.0f - (1.0f - std::exp(-8.0f)) / 8.0f);
            r.check(std::abs(yaw[i] - integral) < 0.002f, "integrated one-second yaw must match the continuous smoothed-turn solution");
        }
        r.check(std::abs(yaw[0] - yaw[2]) < 0.002f && std::abs(yaw[1] - yaw[2]) < 0.002f,
                "yaw trajectories must agree across frame rates without a frame-count-dependent integration error");
    });

    r.run("Every camera stays behind heading with matching screen-right", [&] {
        fresh();
        const float headings[] = {0, 90, -90, 179, -181, 359, 361, 720};
        for (float heading : headings) {
            g_ornYaw = heading;
            const glm::vec3 forward = flightForward();
            const glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
            for (double mouseY : {0.0, 250.0, 500.0}) {
                setMouseHover(500, mouseY, 1000, 500);
                for (int mode = 0; mode < 3; ++mode) {
                    const glm::vec3 offset = flightCameraOffset(mode);
                    const glm::vec3 horizontal(offset.x, 0, offset.z);
                    r.check(std::isfinite(offset.x) && std::isfinite(offset.y) && std::isfinite(offset.z) && offset.y > 0,
                            "all camera modes and vertical hover extremes must return a finite elevated offset");
                    r.check(glm::length(horizontal) > 0.001f && glm::dot(glm::normalize(horizontal), forward) < -0.999f,
                            "camera XZ offset must stay directly behind forward at every heading including wraparound");
                    const glm::vec3 viewRight = glm::normalize(glm::cross(-offset, glm::vec3(0, 1, 0)));
                    r.check(glm::dot(viewRight, right) > 0.999f, "camera view-right must match aircraft-right so right input moves right on screen");
                    if (mode == 0) r.check(near(glm::length(offset), 40), "follow camera distance must be forty units");
                    if (mode == 1) r.check(near(glm::length(offset - glm::vec3(0, 2.2f, 0)), 20), "close camera must use twenty units plus 2.2 units of height");
                    if (mode == 2) r.check(near(offset.y, 100) && near(glm::length(horizontal), 26), "overview camera must be one hundred units high and twenty-six behind");
                }
            }
        }
    });

    r.run("Vertical hover controls clamped pitch and reset centers it", [&] {
        fresh();
        setMouseHover(500, 250, 1000, 500);
        r.check(near(flightCameraPitch(), 12), "centered mouse must give default twelve-degree camera pitch");
        setMouseHover(500, 0, 1000, 500);
        const float topPitch = flightCameraPitch();
        r.check(near(std::abs(g_mouseHover.y), 1) && near(topPitch, glm::clamp(12.0f - g_mouseHover.y * 22.0f, 6.0f, 65.0f)),
                "top window edge must apply the specified vertical pitch mapping");
        setMouseHover(500, 500, 1000, 500);
        const float bottomPitch = flightCameraPitch();
        r.check(near(std::abs(g_mouseHover.y), 1) && near(bottomPitch, glm::clamp(12.0f - g_mouseHover.y * 22.0f, 6.0f, 65.0f)),
                "bottom window edge must apply the specified vertical pitch mapping");
        r.check(near(std::min(topPitch, bottomPitch), 6) && near(std::max(topPitch, bottomPitch), 34),
                 "vertical mouse extremes must span the clamped six-to-thirty-four-degree pitch range");
        for (float orbit : {-100.0f, 0.0f, 15.0f, 100.0f}) {
            g_camOrbitPitch = orbit;
            r.check(near(flightCameraPitch(), glm::clamp(12.0f - g_mouseHover.y * 22.0f + orbit, 6.0f, 65.0f)),
                     "camera orbit pitch must combine with mouse pitch and clamp between six and sixty-five degrees");
        }
        for (bool start : {false, true}) {
            setMouseHover(1000, 0, 1000, 500);
            g_mouseTurnRate = -60;
            g_camOrbitPitch = 35;
            resetMission(start);
            r.check(glm::length(g_mouseHover) < 0.001f && near(g_mouseTurnRate, 0) && near(g_ornYaw, 0),
                    "title and playing resets must center both hover axes and clear yaw inertia");
            r.check(near(g_camOrbitPitch, 0) && near(flightCameraPitch(), 12), "reset must restore centered camera pitch without stale vertical hover");
        }
    });

    std::cout << "Mission tests: " << (r.cases - r.failedCases) << '/' << r.cases << " cases passed, "
              << r.checks << " checks, " << r.failures << " failures\n";
    return r.failures == 0 ? 0 : 1;
}
