#pragma once

#include "miniaudio.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

inline std::string findSoundFile(const std::string& filename) {
    static const std::vector<std::string> searchDirs = {
        "assets/audio/",
        "../assets/audio/",
        "../../assets/audio/",
        "../../../assets/audio/",
        "../../../../assets/audio/"
    };
    for (const auto& dir : searchDirs) {
        std::string path = dir + filename;
        std::ifstream f(path.c_str());
        if (f.good()) return path;
    }
    return "";
}

struct ArrakisAudio {
    ma_engine engine{};
    bool initialized = false;
    bool muted = false;
    float masterVolume = 1.0f;

    // Sound objects
    ma_sound sndFlight{};
    ma_sound sndBoost{};
    ma_sound sndWormRumble{};
    ma_sound sndWormBreach{};
    ma_sound sndRescueWinch{};
    ma_sound sndRescueSafe{};
    ma_sound sndHarvester{};
    ma_sound sndCrewHelp{};

    bool hasFlight = false;
    bool hasBoost = false;
    bool hasWormRumble = false;
    bool hasWormBreach = false;
    bool hasRescueWinch = false;
    bool hasRescueSafe = false;
    bool hasHarvester = false;
    bool hasCrewHelp = false;

    // Dynamic smoothing
    float boostVolume = 0.0f;
    float flightPitch = 1.0f;
    float crewHelpTimer = 3.0f;
    bool breachTriggered = false;

    int lastRescued = 0;
    int lastAboard = 0;
    float lastPickup = 0.0f;

    bool init() {
        ma_engine_config config = ma_engine_config_init();
        ma_result res = ma_engine_init(&config, &engine);
        if (res != MA_SUCCESS) {
            std::cerr << "[AUDIO] Warning: Failed to initialize audio engine (code: " << res << "). Running in silent mode.\n";
            initialized = false;
            return false;
        }
        initialized = true;

        // Configure listener
        ma_engine_listener_set_world_up(&engine, 0, 0, 1, 0);

        auto loadSound = [this](ma_sound* snd, const std::vector<std::string>& candidates, bool loop, float minDist, float maxDist, ma_attenuation_model model = ma_attenuation_model_inverse) -> bool {
            for (const auto& name : candidates) {
                std::string path = findSoundFile(name);
                if (!path.empty()) {
                    ma_result r = ma_sound_init_from_file(&engine, path.c_str(), 0, nullptr, nullptr, snd);
                    if (r == MA_SUCCESS) {
                        ma_sound_set_looping(snd, loop ? MA_TRUE : MA_FALSE);
                        ma_sound_set_min_distance(snd, minDist);
                        ma_sound_set_max_distance(snd, maxDist);
                        ma_sound_set_attenuation_model(snd, model);
                        std::cout << "[AUDIO] Loaded: " << path << "\n";
                        return true;
                    }
                }
            }
            return false;
        };

        // 1. Ornithopter flight (looping engine drone & wing flutter)
        hasFlight = loadSound(&sndFlight, {"ornithopter_flight.mp3", "ornithopter_flight.wav"}, true, 12.0f, 150.0f);
        if (hasFlight) ma_sound_set_volume(&sndFlight, 0.75f);

        // 2. Ornithopter boost (looping afterburner)
        hasBoost = loadSound(&sndBoost, {"ornithopter_boost.mp3", "ornithopter_boost.wav"}, true, 12.0f, 150.0f);
        if (hasBoost) {
            ma_sound_set_volume(&sndBoost, 0.0f);
            ma_sound_start(&sndBoost);
        }

        // 3. Sandworm rumble (sub-bass approach tremor)
        hasWormRumble = loadSound(&sndWormRumble, {"worm_rumble.mp3", "worm_rumble.wav"}, true, 30.0f, 320.0f, ma_attenuation_model_inverse);
        if (hasWormRumble) ma_sound_set_volume(&sndWormRumble, 0.9f);

        // 4. Sandworm breach (one-shot eruption & roar)
        hasWormBreach = loadSound(&sndWormBreach, {"worm_breach.mp3", "worm_breach.wav"}, false, 45.0f, 380.0f, ma_attenuation_model_inverse);
        if (hasWormBreach) ma_sound_set_volume(&sndWormBreach, 1.0f);

        // 5. Harvester movement (crawler treads & engine)
        hasHarvester = loadSound(&sndHarvester, {"harvester_engine.wav", "harvester_engine.mp3", "harvester.wav"}, true, 18.0f, 200.0f, ma_attenuation_model_inverse);
        if (hasHarvester) ma_sound_set_volume(&sndHarvester, 0.75f);

        // 6. Rescuing winch / pickup
        hasRescueWinch = loadSound(&sndRescueWinch, {"rescue_winch.wav", "rescue_winch.mp3"}, false, 15.0f, 120.0f);
        if (hasRescueWinch) ma_sound_set_volume(&sndRescueWinch, 0.85f);

        // 7. Safe rescue delivery at base
        hasRescueSafe = loadSound(&sndRescueSafe, {"rescue_safe.wav", "rescue_safe.ogg", "rescue_safe.mp3", "confirmation_004.ogg"}, false, 20.0f, 140.0f);
        if (hasRescueSafe) ma_sound_set_volume(&sndRescueSafe, 0.95f);

        // 8. Stranded crew calling for help
        hasCrewHelp = loadSound(&sndCrewHelp, {"crew_help.wav", "crew_help.mp3", "question_001.ogg"}, false, 16.0f, 85.0f);
        if (hasCrewHelp) ma_sound_set_volume(&sndCrewHelp, 0.85f);

        return true;
    }

    void toggleMute() {
        muted = !muted;
        if (!initialized) return;
        ma_engine_set_volume(&engine, muted ? 0.0f : masterVolume);
    }

    void update(float dt,
                const glm::vec3& camPos,
                const glm::vec3& camTarget,
                const glm::vec3& ornPos,
                float ornSpeed,
                bool isBoosting,
                const glm::vec3& harvPos,
                const glm::vec3& wormPos,
                float wormDist,
                float attackTime,
                int rescuedCount,
                int aboardCount,
                float pickupProgress,
                const glm::vec3& nearestCrewPos,
                float nearestCrewDist,
                const glm::vec3& basePos,
                bool isFlying,
                bool isPaused)
    {
        if (!initialized) return;

        // Master pause / mute handling
        if (isPaused || muted) {
            ma_engine_set_volume(&engine, 0.0f);
            return;
        } else {
            ma_engine_set_volume(&engine, masterVolume);
        }

        // 1. Update 3D Audio Listener (Camera eye & look direction)
        glm::vec3 look = camTarget - camPos;
        if (glm::length(look) > 0.001f) look = glm::normalize(look);
        else look = glm::vec3(0, 0, -1);

        ma_engine_listener_set_position(&engine, 0, camPos.x, camPos.y, camPos.z);
        ma_engine_listener_set_direction(&engine, 0, look.x, look.y, look.z);

        // 2. Ornithopter flight sound
        if (hasFlight) {
            ma_sound_set_position(&sndFlight, ornPos.x, ornPos.y, ornPos.z);
            float targetPitch = 0.92f + glm::clamp(ornSpeed / 42.0f, 0.0f, 0.38f);
            flightPitch = glm::mix(flightPitch, targetPitch, 1.0f - std::exp(-dt * 6.0f));
            ma_sound_set_pitch(&sndFlight, flightPitch);

            float targetVol = isBoosting ? 0.40f : 0.78f;
            ma_sound_set_volume(&sndFlight, targetVol);

            if (!ma_sound_is_playing(&sndFlight)) {
                ma_sound_start(&sndFlight);
            }
        }

        // 3. Ornithopter boost sound
        if (hasBoost) {
            ma_sound_set_position(&sndBoost, ornPos.x, ornPos.y, ornPos.z);
            float targetBoostVol = (isFlying && isBoosting) ? 0.90f : 0.0f;
            boostVolume = glm::mix(boostVolume, targetBoostVol, 1.0f - std::exp(-dt * 8.0f));
            ma_sound_set_volume(&sndBoost, boostVolume);
        }

        // 4. Harvester moving sound (3D spatialized at crawler)
        if (hasHarvester) {
            ma_sound_set_position(&sndHarvester, harvPos.x, harvPos.y, harvPos.z);
            if (isFlying && attackTime < 11.0f) {
                if (!ma_sound_is_playing(&sndHarvester)) ma_sound_start(&sndHarvester);
            } else {
                if (ma_sound_is_playing(&sndHarvester)) ma_sound_stop(&sndHarvester);
            }
        }

        // 5. Sandworm rumble (distance relative subterranean tremor)
        if (hasWormRumble) {
            ma_sound_set_position(&sndWormRumble, wormPos.x, wormPos.y, wormPos.z);
            if (isFlying && attackTime < 16.0f) {
                // Modulate volume slightly as worm gets closer
                float proximityBoost = glm::mix(1.2f, 0.8f, glm::clamp(wormDist / 200.0f, 0.0f, 1.0f));
                ma_sound_set_volume(&sndWormRumble, proximityBoost);
                if (!ma_sound_is_playing(&sndWormRumble)) ma_sound_start(&sndWormRumble);
            } else {
                if (ma_sound_is_playing(&sndWormRumble)) ma_sound_stop(&sndWormRumble);
            }
        }

        // 6. Sandworm breach eruption (triggered once on breach)
        if (hasWormBreach) {
            if (attackTime >= 0.0f && attackTime < 2.5f && !breachTriggered) {
                breachTriggered = true;
                ma_sound_set_position(&sndWormBreach, wormPos.x, wormPos.y, wormPos.z);
                ma_sound_seek_to_pcm_frame(&sndWormBreach, 0);
                ma_sound_start(&sndWormBreach);
            }
        }

        // 7. Winch / Rescuing people
        if (hasRescueWinch) {
            // Trigger or loop while picking up or when a person is secured aboard
            if (aboardCount > lastAboard || (pickupProgress > 0.08f && lastPickup <= 0.08f)) {
                ma_sound_set_position(&sndRescueWinch, ornPos.x, ornPos.y, ornPos.z);
                ma_sound_seek_to_pcm_frame(&sndRescueWinch, 0);
                ma_sound_start(&sndRescueWinch);
            }
        }

        // 8. Safe delivery at cyan base
        if (hasRescueSafe) {
            if (rescuedCount > lastRescued) {
                ma_sound_set_position(&sndRescueSafe, basePos.x, basePos.y, basePos.z);
                ma_sound_seek_to_pcm_frame(&sndRescueSafe, 0);
                ma_sound_start(&sndRescueSafe);
            }
        }

        // 9. Stranded crew asking for help (periodic 3D calls when near)
        if (hasCrewHelp && isFlying) {
            crewHelpTimer -= dt;
            if (crewHelpTimer <= 0.0f) {
                if (nearestCrewDist > 0.0f && nearestCrewDist < 75.0f) {
                    ma_sound_set_position(&sndCrewHelp, nearestCrewPos.x, nearestCrewPos.y, nearestCrewPos.z);
                    ma_sound_seek_to_pcm_frame(&sndCrewHelp, 0);
                    ma_sound_start(&sndCrewHelp);
                    crewHelpTimer = 5.5f + (rand() % 30) * 0.1f;
                } else {
                    crewHelpTimer = 2.0f;
                }
            }
        }

        // State update for next frame
        lastRescued = rescuedCount;
        lastAboard = aboardCount;
        lastPickup = pickupProgress;
    }

    void resetMission() {
        breachTriggered = false;
        crewHelpTimer = 3.0f;
        lastRescued = 0;
        lastAboard = 0;
        lastPickup = 0.0f;
    }

    void shutdown() {
        if (!initialized) return;
        if (hasFlight) ma_sound_uninit(&sndFlight);
        if (hasBoost) ma_sound_uninit(&sndBoost);
        if (hasWormRumble) ma_sound_uninit(&sndWormRumble);
        if (hasWormBreach) ma_sound_uninit(&sndWormBreach);
        if (hasRescueWinch) ma_sound_uninit(&sndRescueWinch);
        if (hasRescueSafe) ma_sound_uninit(&sndRescueSafe);
        if (hasHarvester) ma_sound_uninit(&sndHarvester);
        if (hasCrewHelp) ma_sound_uninit(&sndCrewHelp);
        ma_engine_uninit(&engine);
        initialized = false;
    }
};

inline ArrakisAudio g_audio;
