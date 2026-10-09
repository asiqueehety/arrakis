# 6. Spatial Audio and Vendor Implementations

## 6.1 Scope, Versions, and Reading Conventions

This chapter explains the complete Arrakis audio wrapper and the architecture of the supplied audio and OpenGL vendor implementations. The gameplay code chooses events, positions, pitch, and volume. miniaudio performs decoding, channel conversion, resampling, spatialization, mixing, and delivery to an operating-system audio device. GLAD performs a completely different task: discovering OpenGL capabilities and resolving graphics-driver entry points. The formerly bundled stb_vorbis source was unused and has been removed during cleanup; its historical architecture is discussed separately to explain the unintegrated Ogg fallback, not as remaining executable or appendix code.

The source was reorganized while this documentation was prepared. Audio references give the final organized line numbers as well as explicitly marked original numbers for historical comparison. The source appendix contains current remaining code, not the removed Vorbis file. Vendor implementation line numbers are unchanged except for GLAD's final blank line. Audio lines after the old search-directory list moved upward by two lines. Function names and the tables below are the primary navigation keys if later edits change line numbers.

| Source | Organized location | Original line total | Inspected organized line total | Build role |
| --- | --- | ---: | ---: | --- |
| `arrakis_audio.h` | `src/arrakis_audio.h` | 295 | 293 | Header-only gameplay audio wrapper; one C++17 inline global |
| `miniaudio_impl.cpp` | `third_party/miniaudio/miniaudio_impl.cpp` | 2 | 2 | Emits the miniaudio implementation in exactly one translation unit |
| `miniaudio.h` | `third_party/miniaudio/miniaudio.h` | 95,864 | 95,864 | miniaudio 0.11.25, dated 2026-03-04; includes embedded decoder implementations |
| `glad.c` | `third_party/glad/glad.c` | 1,833 | 1,832 | GLAD 0.1.36-generated OpenGL 4.6 core loader, generated 2026-09-11 |
| `stb_vorbis.c` | Removed unused root-level source, inspected before cleanup | 5,584 | Removed | Historical stb_vorbis 1.22; never integrated into this executable |

The chapter supplies complete, contiguous line-coverage maps and explanatory sections for the vendor functional families. It does **not** pretend that an architectural explanation is a separate semantic annotation of every one of miniaudio's 95,864 lines, or a security audit of all platform branches. The numbered source appendix is the exact-code companion for remaining project code. Use the family explanations here, the declaration/definition index strategy in Section 6.10, and the current source listings together when reading an individual vendor function. An unused backend, declaration, table, generated assignment, comment, or license is still accounted for in the coverage maps; being accounted for does not mean it executes in this Windows game. Historical Vorbis ranges are separate from current-source coverage.

## 6.2 `arrakis_audio.h`: Includes, File Lookup, and State

### 6.2.1 Includes and Linkage

Original lines 1-9, also organized lines 1-9, contain `#pragma once` and the dependency includes. The pragma prevents repeated definitions within one translation unit. `miniaudio.h` provides the engine, sound objects, result types, and C-compatible audio API. GLM supplies `glm::vec3`, length, normalization, clamp, and mix operations. `<string>` stores paths; `<vector>` stores ordered search directories and candidates; `<fstream>` probes readability; `<iostream>` prints status messages; `<algorithm>` is included but has no directly named algorithm invocation in this header.

The wrapper uses `std::exp` and `rand()` without directly including `<cmath>` and `<cstdlib>` here. Their availability currently depends on the surrounding include environment. This is an include-hygiene observation, not a claim that this documentation changed the header. No vendor or wrapper implementation is edited by this chapter.

The free function `findSoundFile` is `inline`, and methods defined inside `ArrakisAudio` are implicitly inline. Original line 295, now line 293, declares `inline ArrakisAudio g_audio;`. Under C++17 this is one program-wide global object rather than a separate audio engine per including translation unit. Its aggregate/default member initializers prepare storage and flags; they do not initialize an operating-system device.

Final lines 291-293 close the `ArrakisAudio` structure with a brace and semicolon, then define that global instance. The semicolon finishes the type declaration; the following object definition creates the shared owner used by input, mission, and runtime code. The intervening blank line only separates those declarations visually.

### 6.2.2 `findSoundFile(filename)`

Final lines 11-25; original lines 11-27.

The sole parameter is `const std::string& filename`: a read-only filename reference, normally without a directory component. The return value is either the first readable relative path or an empty string. The function neither decodes the file nor establishes that its contents are valid audio.

The **current organized search order** is exact:

1. `assets/audio/`
2. `../assets/audio/`
3. `../../assets/audio/`
4. `../../../assets/audio/`
5. `../../../../assets/audio/`

The **original search order**, recorded only for older laboratory launches and the cleanup change, was:

1. `sounds/`
2. `../sounds/`
3. `../../sounds/`
4. `../../../sounds/`
5. `sounds/kenney_interface-sounds/Audio/`
6. `../sounds/kenney_interface-sounds/Audio/`
7. `""`, meaning the current working directory itself.

The organized implementation no longer searches a Kenney subdirectory or the bare current working directory. A relative directory is resolved against the **process working directory**, not the header's location or necessarily the executable's directory. The revised CMake post-build command copies `assets/audio` under `$<TARGET_FILE_DIR:Arrakis>/assets/audio` and sets the Visual Studio debugger working directory to the executable directory; this makes the first candidate usable for normal configured IDE launches. Independently launching the executable from an unrelated directory can still make all five probes fail.

`static const std::vector<std::string> searchDirs` is initialized once and reused. The range-for binds `dir` as `const auto&`, avoiding string copies. `path = dir + filename` constructs one candidate. `std::ifstream f(path.c_str())` opens the path for input; `f.good()` checks the stream state. Success returns immediately, with the local stream closed by its destructor. On failure the loop moves to the next directory; after exhaustion it returns `""`.

There is no cache, filesystem canonicalization, absolute-path asset root, extension validation, binary content test, recursive search, or diagnostic for individual lookup failures. Each call can perform five filesystem probes in the organized layout. Importantly, it returns the first readable instance of a **particular filename**. If that file later fails audio decoding, `loadSound` advances to a different candidate filename; it does not ask `findSoundFile` to try a second directory containing the same filename. A corrupt nearer copy can therefore shadow a valid farther copy.

### 6.2.3 Every `ArrakisAudio` Field

Final lines 27-60; original lines 29-62.

| Field | Initial value | Meaning and lifetime |
| --- | --- | --- |
| `ma_engine engine` | `{}` | Stable, zero-initialized storage for the high-level engine, graph, listeners, and pointers to device/resource-manager state. Must be initialized before engine API use and uninitialized after dependent sounds. |
| `initialized` | `false` | Records successful engine initialization. Guards `update` and `shutdown`; does not mean every optional sound loaded. |
| `muted` | `false` | User's requested mute state. Persists across mission resets and is displayed by the HUD. |
| `masterVolume` | `1.0f` | Linear engine gain restored on active, unmuted updates. `1` is unity, not a percentage in decibels. No local clamp or UI adjustment is implemented. |
| `sndFlight` | `{}` | Flight-loop sound instance, located at the ornithopter; pitch changes with flight speed. |
| `sndBoost` | `{}` | Continuously started boost loop, made audible by smoothed volume. |
| `sndWormRumble` | `{}` | Worm-position rumble loop; controlled by mission phase, attack time, and proximity. |
| `sndWormBreach` | `{}` | Single reusable eruption/roar sound instance. A mission latch permits one trigger. |
| `sndRescueWinch` | `{}` | Single reusable pickup/winch one-shot located at the ornithopter. |
| `sndRescueSafe` | `{}` | Single reusable successful-delivery one-shot located at the base. |
| `sndHarvester` | `{}` | Harvester movement loop located at the crawler. |
| `sndCrewHelp` | `{}` | Single reusable periodic crew-help one-shot located at the nearest selected crew member. |
| `hasFlight` | `false` | `sndFlight` successfully initialized; permits its updates and uninitialization. |
| `hasBoost` | `false` | Equivalent validity flag for `sndBoost`. |
| `hasWormRumble` | `false` | Equivalent validity flag for `sndWormRumble`. |
| `hasWormBreach` | `false` | Equivalent validity flag for `sndWormBreach`. |
| `hasRescueWinch` | `false` | Equivalent validity flag for `sndRescueWinch`. |
| `hasRescueSafe` | `false` | Equivalent validity flag for `sndRescueSafe`. |
| `hasHarvester` | `false` | Equivalent validity flag for `sndHarvester`. |
| `hasCrewHelp` | `false` | Equivalent validity flag for `sndCrewHelp`. |
| `boostVolume` | `0.0f` | Application-side smoothed boost gain. Changes only when a boost sound exists and `update` passes its early guards. |
| `flightPitch` | `1.0f` | Application-side smoothed playback-rate/pitch multiplier. Changes only for a loaded flight sound. |
| `crewHelpTimer` | `3.0f` | Seconds until the next proximity check/call. Decremented only while flying, loaded, unpaused, and unmuted. |
| `breachTriggered` | `false` | Once-per-mission breach latch. Reset by `resetMission`, not by the sound ending. |
| `lastRescued` | `0` | Previous processed rescued count, used for positive delivery edges. |
| `lastAboard` | `0` | Previous processed aboard count, used for positive pickup edges. |
| `lastPickup` | `0.0f` | Previous processed pickup-progress value, used for a threshold crossing. |

Each sound has exactly one `ma_sound`, so retriggering the same cue restarts or seeks that voice; it does not allocate overlapping copies. The structs own internal pointers and participate in a live graph. Do not copy, move, or `memset` initialized engine/sound objects. The global's stable address satisfies miniaudio's object-address lifetime requirement.

## 6.3 Initialization, Lambda, Candidates, and Optional Assets

### 6.3.1 `init()` and the Silent-Mode Branch

Final lines 62-129; original lines 64-131. No parameters. The boolean return reports engine creation, not complete asset availability.

`ma_engine_config config = ma_engine_config_init()` uses vendor defaults instead of manually zeroing a configuration. `ma_engine_init(&config, &engine)` returns a `ma_result`. If it is not `MA_SUCCESS`, the wrapper writes exactly the warning prefix `[AUDIO] Warning: Failed to initialize audio engine (code: `, the numeric result, and `). Running in silent mode.`, sets `initialized = false`, and returns `false`. The game calls `g_audio.init()` without aborting on its return, making hardware failure nonfatal to rendering and mission logic. Later `update` calls immediately return in this mode.

Success sets `initialized = true`. `ma_engine_listener_set_world_up(&engine, 0, 0, 1, 0)` means listener **index 0**, then world-up components **(0, 1, 0)**. The two initial zeros have different meanings; this does not set an up vector of `(0,0,1)`. The game uses a Y-up world.

Default engine configuration creates one listener, a playback device unless explicitly disabled, and an internal resource manager unless supplied by the caller. The vendor engine normally auto-starts its device; individual sounds remain stopped until their own start calls. Channels and sample rate are negotiated with the device rather than hardcoded by Arrakis. The library's generic defaults include f32, two channels, 48,000 Hz, three periods, and 10 ms low-latency/100 ms conservative period sizes, but those are fallback/configuration defaults, not proof of the actual runtime endpoint format or latency.

`init()` has no already-initialized guard. Calling it twice without shutdown is not a supported wrapper lifecycle: it can overwrite live vendor objects. It also does not apply a preexisting `muted` state at initialization, so the normal frame update is responsible for enforcing it.

### 6.3.2 The `loadSound` Lambda

Final lines 75-91; original lines 77-93.

`[this]` captures the owning `ArrakisAudio*`, allowing the lambda to use `engine`. It is local to `init`, invoked immediately for each sound, and never escapes into an asynchronous callback. The trailing `-> bool` declares its explicit result type. The initializer-list arguments construct temporary candidate vectors valid for each call.

| Parameter | Exact type | Purpose |
| --- | --- | --- |
| `snd` | `ma_sound*` | Address of the destination sound storage. |
| `candidates` | `const std::vector<std::string>&` | Ordered filename alternatives; first successfully decoded/initialized candidate wins. |
| `loop` | `bool` | Converted to miniaudio's `MA_TRUE`/`MA_FALSE` loop setting. |
| `minDist` | `float` | Reference/minimum attenuation distance in the game's coordinate units. |
| `maxDist` | `float` | Maximum distance used by the clamped attenuation model, not an automatic cutoff radius. |
| `model` | `ma_attenuation_model` | Defaults to `ma_attenuation_model_inverse`; some calls repeat this explicitly. |

For each `const auto& name`, it calls `findSoundFile(name)`. An empty result skips initialization. A nonempty path is passed to `ma_sound_init_from_file(&engine, path.c_str(), 0, nullptr, nullptr, snd)`. Its six arguments mean engine owner, narrow-character path, **flags 0**, no initial sound group, no completion fence, and output sound storage.

Flags 0 do not request streaming, asynchronous loading, predecoded PCM, disabled pitch, disabled spatialization, or detached graph placement. The resource manager's normal non-streaming path loads encoded data into memory and makes a decoder-backed data source; decoding/conversion can occur during reads. Vendor sound initialization internally adds a wait-for-initialization requirement so the sound's data format is known. This is not the same as setting `MA_SOUND_FLAG_DECODE` to predecode an entire MP3 file.

If `r == MA_SUCCESS`, the lambda sets looping, minimum distance, maximum distance, and attenuation model in that order, prints `[AUDIO] Loaded: ` followed by the selected path, and returns `true`. If initialization fails, it silently continues to the next candidate. There is no per-candidate decode error report and no assertion that filename existence implies codec support. Exhaustion returns `false`, which becomes the relevant `has*` flag. Vendor initialization handles its internal failure cleanup; the wrapper never uninitializes a sound that did not report successful creation.

### 6.3.3 Complete Sound-Candidate and Constant Table

All filename orderings are literal and significant. Every sound uses inverse-distance attenuation, whether supplied explicitly or by the lambda default.

| Cue; final lines | Candidates in exact order | Loop | Minimum / maximum distance | Initial sound gain | Initial playback |
| --- | --- | --- | --- | --- | --- |
| Flight; 93-95 | `ornithopter_flight.mp3`, `ornithopter_flight.wav` | Yes | `12.0f / 150.0f` | `0.75f` | Stopped; first active update starts it |
| Boost; 97-102 | `ornithopter_boost.mp3`, `ornithopter_boost.wav` | Yes | `12.0f / 150.0f` | `0.0f` | Started immediately, initially inaudible |
| Worm rumble; 104-106 | `worm_rumble.mp3`, `worm_rumble.wav` | Yes | `30.0f / 320.0f` | `0.9f` | Stopped |
| Worm breach; 108-110 | `worm_breach.mp3`, `worm_breach.wav` | No | `45.0f / 380.0f` | `1.0f` | Stopped |
| Harvester; 112-114 | `harvester_engine.wav`, `harvester_engine.mp3`, `harvester.wav` | Yes | `18.0f / 200.0f` | `0.75f` | Stopped |
| Winch; 116-118 | `rescue_winch.wav`, `rescue_winch.mp3` | No | `15.0f / 120.0f` | `0.85f` | Stopped |
| Safe delivery; 120-122 | `rescue_safe.wav`, `rescue_safe.ogg`, `rescue_safe.mp3`, `confirmation_004.ogg` | No | `20.0f / 140.0f` | `0.95f` | Stopped |
| Crew help; 124-126 | `crew_help.wav`, `crew_help.mp3`, `question_001.ogg` | No | `16.0f / 85.0f` | `0.85f` | Stopped |

Original lines for these calls are two greater than the final numbers. `if (has...)` guards every initial volume assignment and the boost start. The final `return true` occurs even if **all eight files fail** after the engine succeeded. That separation intentionally permits a partly or wholly silent asset configuration.

### 6.3.4 What Is Present and What Is Optional or Absent

The organized active asset folder `assets/audio/` contains all eight preferred files:

- `ornithopter_flight.mp3`
- `ornithopter_boost.mp3`
- `worm_rumble.mp3`
- `worm_breach.mp3`
- `harvester_engine.wav`
- `rescue_winch.wav`
- `rescue_safe.wav`
- `crew_help.wav`

The following alternative candidate names were **not present in the inspected active folder**: `ornithopter_flight.wav`, `ornithopter_boost.wav`, `worm_rumble.wav`, `worm_breach.wav`, `harvester_engine.mp3`, `harvester.wav`, `rescue_winch.mp3`, `rescue_safe.ogg`, `rescue_safe.mp3`, `confirmation_004.ogg`, `crew_help.mp3`, and `question_001.ogg`. They are optional fallback names, not mandatory missing dependencies. Adding one is useful only if its format is supported and it is placed in a searched directory.

Before reorganization the wider `sounds/` tree did include `rescue_safe.ogg` and Kenney `Audio/confirmation_004.ogg` and `Audio/question_001.ogg`, along with many unused Kenney interface clips. Those duplicate/unused Ogg assets were removed during cleanup while the eight active files and license were retained. A separately supplied old/external pack would not be discovered unless its files were placed in the organized searched asset locations. More importantly, the current miniaudio implementation does not enable Vorbis decoding, so a discoverable `.ogg` fallback still would not become playable just because its file exists. The primary WAV/MP3 cues avoid this issue.

`assets/audio/Kenney-License.txt` is attribution/license material, not a decoded cue. Asset presence was established by filename inspection, not by listening to every file, proving its binary encoding, or validating the semantic content implied by names such as `crew_help.wav`. Runtime load success is a separate check.

## 6.4 `update`: Parameters, Math, Every Branch, and Event Semantics

### 6.4.1 Complete Parameter Contract

Final lines 137-154; original lines 139-156. The function returns `void`; it consumes a snapshot rather than querying mission globals itself.

| Parameter | Type | Meaning and actual caller input |
| --- | --- | --- |
| `dt` | `float` | Frame time in seconds. Used by exponential smoothing and crew timer; not a count of audio samples. |
| `camPos` | `const glm::vec3&` | Smoothed camera eye position; becomes listener position. |
| `camTarget` | `const glm::vec3&` | Smoothed camera target; determines listener forward vector. |
| `ornPos` | `const glm::vec3&` | Ornithopter position, used by flight, boost, and pickup sounds. |
| `ornSpeed` | `float` | Ornithopter speed scalar; drives artificial flight pitch rather than velocity-based Doppler. |
| `isBoosting` | `bool` | Boost-input/gameplay state; reduces flight-loop gain and can raise boost-loop gain. |
| `harvPos` | `const glm::vec3&` | `harvesterPosition()`; harvester sound origin. |
| `wormPos` | `const glm::vec3&` | `wormPosition()`; rumble origin and breach origin at trigger time. |
| `wormDist` | `float` | `wormGap()`; a gameplay separation measure driving extra rumble modulation. It is not the listener-source distance calculated by miniaudio. |
| `attackTime` | `float` | `wormAttackTime()`, defined as mission elapsed time minus `breachAt`; negative before breach and nonnegative afterward. |
| `rescuedCount` | `int` | Cumulative delivered crew; an increase causes a safe-delivery cue. |
| `aboardCount` | `int` | Current onboard crew; an increase causes a pickup cue. |
| `pickupProgress` | `float` | Current winch/pickup progress; the upward crossing of `0.08f` causes a pickup cue. |
| `nearestCrewPos` | `const glm::vec3&` | `nearestCrewPosition()`; selected help-call origin. |
| `nearestCrewDist` | `float` | Caller computes `horizontalDistance(g_ornPos, nearestCrewAudio)`; this is horizontal ornithopter-to-crew distance, not camera distance. |
| `basePos` | `const glm::vec3&` | Mission base position; safe-delivery sound origin. |
| `isFlying` | `bool` | Exact test `g_mission.phase == MissionPhase::Flying`; not inferred from speed or altitude. |
| `isPaused` | `bool` | Mission pause flag; suppresses master output and further wrapper processing. |

The original call is in `arrakis.cpp` lines 1683-1703. Position references are borrowed for this call only. miniaudio setters copy scalar/vector values into its objects; the audio callback does not retain references to these temporary snapshot arguments.

### 6.4.2 Engine Guard, Pause, and Mute

Final lines 155-164; original lines 157-166. The opening brace begins the update body; its matching close is final line 268.

If `!initialized`, return without touching vendor state. Otherwise, if `isPaused || muted`, set engine gain to `0.0f` and return. The `else` path restores `masterVolume` every active frame.

This is **output muting, not transport pausing**. The engine/device is not stopped, sound objects are not stopped, decoder cursors can continue, and a playing one-shot can finish inaudibly. The early return also freezes the wrapper's listener updates, pitch/boost interpolation, crew-help timer, breach latch processing, and `last*` snapshots. When unmuting, count differences accumulated during silence may cause delayed winch or delivery triggers. A breach occurring entirely outside the `0 <= attackTime < 2.5` interval while muted can be missed permanently. Pause normally also pauses mission progression elsewhere, whereas mute alone does not.

### 6.4.3 Listener Direction

Final lines 166-172; original lines 168-174.

`look = camTarget - camPos` constructs the camera forward displacement. If `glm::length(look) > 0.001f`, it is normalized to unit length. Otherwise `look = glm::vec3(0, 0, -1)` provides a valid negative-Z forward direction rather than dividing an almost-zero vector by its length. Equality at exactly `0.001f` takes the fallback branch.

`ma_engine_listener_set_position(&engine, 0, camPos.x, camPos.y, camPos.z)` and `ma_engine_listener_set_direction(&engine, 0, look.x, look.y, look.z)` both target listener zero. Camera movement changes perceived panning/attenuation even when gameplay objects do not move. No listener velocity or source velocity is supplied, so the library's Doppler capability is not driven by the game's actual velocities.

### 6.4.4 Exponential Smoothing, with Exact Interpretation

The flight and boost branches use `glm::mix(current, target, 1 - exp(-k*dt))`. For scalars, mix is `(1-a)*current + a*target`. Therefore the update is:

```text
a = 1 - exp(-k * dt)
next = target + (current - target) * exp(-k * dt)
```

For constant targets and nonnegative finite `dt`, the remaining error decays exponentially, and successive updates compose by elapsed time. This is more frame-rate-independent than using `k*dt` directly as an unclamped lerp weight. `dt = 0` changes nothing; large positive `dt` approaches the target without overshoot. Negative `dt` yields a negative interpolation weight and moves away from the target; the wrapper does not validate it. NaNs/infinities likewise are not rejected here.

The flight rate is `k = 6.0f`, time constant `1/6` second and half-life `ln(2)/6`, approximately `0.1155` seconds. The boost rate is `k = 8.0f`, time constant `0.125` seconds and half-life about `0.0866` seconds. At 60 Hz, the weights are approximately `0.09516` and `0.12483`. These application-side states are distinct from miniaudio's own short per-channel gain smoothing.

### 6.4.5 Flight Loop

Final lines 174-187; original lines 176-189.

If `hasFlight` is false, the entire branch is skipped. Otherwise source position follows `ornPos` every processed frame. The target pitch is exactly `0.92f + glm::clamp(ornSpeed / 42.0f, 0.0f, 0.38f)`. It is bounded from `0.92` to `1.30`; the upper clamp is reached at speed `42*0.38 = 15.96`, **not at speed 42**. Nonpositive speed yields `0.92`; speed `8.4` yields `1.12`. The divisor is a gain scale, not a normalized full-speed endpoint.

The exponential mix updates `flightPitch`, then `ma_sound_set_pitch` applies it as a playback-rate/pitch factor. Changing pitch also changes the rate at which the sound's audio content is consumed; it is not time-preserving spectral pitch shifting.

`targetVol = isBoosting ? 0.40f : 0.78f` switches flight gain immediately, with no wrapper volume smoothing. The initial `0.75f` is replaced by one of these values on the first update. Boosting lowers the ordinary flight voice so the boost voice can dominate.

If `!ma_sound_is_playing(&sndFlight)`, start it. There is **no `isFlying` condition** and no flight-stop branch. A loaded flight loop therefore runs in menu/end phases as long as updates pass pause/mute guards, and its location continues to follow the ornithopter. Describing this as strictly flight-phase-only would be incorrect.

### 6.4.6 Boost Loop

Final lines 189-195; original lines 191-197.

The `hasBoost` guard protects the sound. Position follows `ornPos`. The target is `(isFlying && isBoosting) ? 0.90f : 0.0f`; both flags must be true. `boostVolume` is smoothed with rate `8.0f`, then applied to the sound. Initialization already started this looping sound, so update does not repeatedly start/stop it and does not restart its waveform when boost begins. After release or leaving the Flying phase, the volume decays toward zero rather than instantly reaching it. Muted/paused frames freeze the application-side decay even though the loop's playback cursor continues.

### 6.4.7 Harvester Loop

Final lines 197-205; original lines 199-207.

If loaded, always update its position to `harvPos`. If `isFlying && attackTime < 11.0f`, start only if not already playing. Otherwise stop only if currently playing. Because `attackTime` is breach-relative, the `< 11` condition includes all negative pre-breach times and permits playback for the first eleven seconds after breach. At exactly `11.0f`, it takes the stop branch. A stop does not seek to frame zero; restarting generally resumes the source rather than rewinding unless it was at the end. The initial gain remains `0.75f` because update never changes it.

### 6.4.8 Worm Rumble and Two Different Distances

Final lines 207-218; original lines 209-220.

If loaded, update position to `wormPos`. `isFlying && attackTime < 16.0f` enables the loop, again including all pre-breach times. In this branch:

```text
t = clamp(wormDist / 200.0, 0.0, 1.0)
proximityBoost = mix(1.2, 0.8, t) = 1.2 - 0.4*t
```

The gain is `1.2` for distance zero or less, `1.0` at distance 100, and `0.8` at distance 200 or more. Despite its name, this value is not restricted below unity. It replaces the initial `0.9f` sound gain. This gameplay modulation uses `wormGap()`; the spatializer separately applies source-to-**camera-listener** attenuation from the source's 3D position. Both factors contribute to the final signal.

Start only when needed; if the phase/time predicate fails, stop only when playing. At `attackTime == 16.0f`, stop. No local fade is requested in the stop branch and no new position is assigned for an absent cue.

### 6.4.9 Breach One-Shot

Final lines 220-228; original lines 222-230.

The loaded-sound branch contains `attackTime >= 0.0f && attackTime < 2.5f && !breachTriggered`. Zero is inclusive; `2.5` is exclusive. Once eligible, set `breachTriggered = true`, set position to the worm's **current trigger-time** position, seek to PCM frame `0`, and start. Position is not updated again after the event, so a moving worm does not drag an already-triggered roar through space.

The latch is set before the ignored seek/start result codes. A failed start still consumes the once-per-mission event in this wrapper. The predicate does not include `isFlying`; time and latch determine eligibility if the engine update is active. If frames skip the entire 2.5-second window, no catch-up branch exists. `resetMission` re-arms it.

### 6.4.10 Winch/Pickup One-Shot

Final lines 230-238; original lines 232-240.

Trigger when `aboardCount > lastAboard` **or** `(pickupProgress > 0.08f && lastPickup <= 0.08f)`. The first condition detects any positive onboard-count change, not one sound per person. The second detects an upward threshold crossing from below-or-equal to strictly above 0.08. Remaining above the threshold produces no new threshold event. Falling below it and later crossing again can trigger another. A simultaneous count increase and threshold crossing still runs the body only once.

The body sets the source to `ornPos`, requests seek frame zero, and starts. This cue was initialized with `loop = false`. The source comment saying "Trigger or loop while picking up" is not an implementation of continuous looping. Long pickups do not continuously retrigger unless a new edge appears. Retriggering while the single sound is playing schedules a seek on the existing voice rather than mixing a second voice.

### 6.4.11 Safe Delivery One-Shot

Final lines 240-247; original lines 242-249.

If `hasRescueSafe` and `rescuedCount > lastRescued`, locate the cue at `basePos`, seek to zero, and start. A batch delivery with multiple rescued people emits one trigger for that processed frame. Equal or smaller totals do not trigger. The cue is spatialized at the base, not played as an unconditional nonspatial UI sound, so camera distance can make confirmation quiet. The safe-delivery branch has no separate phase restriction.

### 6.4.12 Periodic Crew Help

Final lines 249-262; original lines 251-264.

Proceed only if `hasCrewHelp && isFlying`. Subtract `dt` from `crewHelpTimer`. When `crewHelpTimer <= 0.0f`, check the **strict** interval `nearestCrewDist > 0.0f && nearestCrewDist < 75.0f`. Exactly zero and exactly 75 are excluded. An eligible crew member triggers position assignment, seek to zero, and start, then:

```text
crewHelpTimer = 5.5f + (rand() % 30) * 0.1f
```

`rand()%30` gives an integer from 0 through 29. The timer is therefore one of 30 nominal values `5.5, 5.6, ..., 8.4` seconds, not a continuous uniform float and not a range ending at 8.5. Modulo reduction can have small distribution bias; this code does not create a dedicated random generator or seed it locally. It consumes the program's shared C random sequence.

If the distance is ineligible, set the timer to `2.0f` seconds and retry later. A large `dt` still produces at most one call/check because there is no catch-up while-loop. Negative overshoot is discarded when resetting the timer. No decrement occurs outside Flying, during pause/mute, or when the sound is absent. The first normal mission call is eligible after approximately three processed seconds, provided a crew member is within the strict distance interval. The selected source position is captured only at trigger time.

### 6.4.13 End-of-Frame State

Final lines 264-268; original lines 266-270. After all sound branches, unconditionally assign `lastRescued = rescuedCount`, `lastAboard = aboardCount`, and `lastPickup = pickupProgress`. "Unconditionally" here means only after getting past the engine/pause/mute early returns. The snapshots advance even if the corresponding cues are absent, preventing missing assets from continually generating the same logical edge. They are not updated when a paused/muted call returns early.

### 6.4.14 Spatial Attenuation Math from the Actual Vendor Code

`miniaudio.h` lines 51664-51688 implement inverse, linear, and exponential distance gains. The active inverse formula is:

```text
if minDistance >= maxDistance: gain = 1
otherwise:
  d = clamp(distance, minDistance, maxDistance)
  gain = minDistance / (minDistance + rolloff*(d - minDistance))
```

Default rolloff is `1`; then gain simplifies to `minDistance/d`. The distance is the listener/source relative vector's 3D length, with gain limits and directional/channel processing applied by the spatializer afterward. At or below the minimum distance, inverse attenuation is unity. Above the maximum distance, the distance is clamped and attenuation stops decreasing. There is no implicit hard culling at `maxDist`.

| Cue | Inverse gain at its maximum distance, with default rolloff 1 |
| --- | ---: |
| Flight and boost | `12/150 = 0.08` |
| Worm rumble | `30/320 = 0.09375` |
| Worm breach | `45/380`, approximately `0.11842` |
| Harvester | `18/200 = 0.09` |
| Winch | `15/120 = 0.125` |
| Safe delivery | `20/140`, approximately `0.14286` |
| Crew help | `16/85`, approximately `0.18824` |

The unused linear alternative is `1 - rolloff*(d-min)/(max-min)`. The unused exponential alternative is `(d/min)^(-rolloff)`. All three return unity when `min >= max` to avoid invalid division. In the default spatializer configuration, source positioning is absolute, handedness is right-handed, forward is negative Z, min/max gain are `0/1`, cones are full-circle `6.283185f`, Doppler and directional factors are `1`, and the minimum spatialization channel gain is `0.2f`. Standalone spatializer smoothing defaults to `360` frames; the engine derives its own default `8` ms smoothing duration. The wrapper overrides attenuation distances/model, not the whole spatializer configuration.

A useful schematic, not a complete implementation equation for multichannel spatialization, is `audible sample = decoded sample * cue gain * spatial/channel gain * master gain`. The engine sums voices and clips its output; rumble gain above one and multiple overlapping voices can therefore increase clipping risk. No occlusion, environmental reverb, custom filter node, underwater/subterranean propagation model, or velocity-driven Doppler is configured by Arrakis.

## 6.5 Mute, Mission Reset, Shutdown, and API-Lifetime Details

### 6.5.1 `toggleMute()`

Final lines 131-135; original lines 133-137. No parameters or return. First flip `muted = !muted`; then, if the engine is uninitialized, return while preserving the new user state. Otherwise immediately set engine gain to `0.0f` when muted or `masterVolume` when unmuted. The keyboard M handler invokes this method, and the HUD reads `g_audio.muted`.

The method does not accept or examine `isPaused`. Consequently unmuting during pause can momentarily restore engine gain until the next `update` mutes it again. This follows from the source; it is not an intentional pause-state integration supplied by miniaudio.

### 6.5.2 `resetMission()`

Final lines 270-276; original lines 272-278. It sets `breachTriggered = false`, `crewHelpTimer = 3.0f`, `lastRescued = 0`, `lastAboard = 0`, and `lastPickup = 0.0f`. Game-level mission reset calls this routine before rebuilding mission state.

It does not need a device and is safe before initialization because it changes only wrapper scalars. It does **not** reset `boostVolume`, `flightPitch`, `muted`, `masterVolume`, any `has*` flag, sound position, playback cursor, or playing state. Existing one-shots can continue into a restarted mission; boost/pitch smoothing history can carry across resets. The method re-arms events, not the whole audio engine.

### 6.5.3 `shutdown()`

Final lines 278-290; original lines 280-292. If uninitialized, return. Otherwise uninitialize every successfully created sound in this exact order: flight, boost, rumble, breach, winch, safe delivery, harvester, crew help. Then call `ma_engine_uninit(&engine)` and set `initialized = false`.

Destroying sounds first keeps their engine/resource manager alive during sound teardown. `ma_sound_uninit` detaches the sound's engine node before freeing caches and owned resource-manager data sources. `ma_engine_uninit` stops/uninitializes the owned device before tearing down the graph so the device's callback cannot read freed nodes; it then cleans up owned resources and listener state. The outer initialized guard makes a second wrapper shutdown a no-op even though the `has*` flags are not cleared.

There is no custom destructor or RAII owner in `ArrakisAudio`; normal application exit explicitly calls `g_audio.shutdown()`. Failure paths that terminate before this call must be considered separately. A future owner should not rely on aggregate destruction of `ma_engine` to free vendor allocations.

### 6.5.4 Exact Vendor Calls Used by the Wrapper

| API | Inputs in Arrakis | What actually happens |
| --- | --- | --- |
| `ma_engine_config_init` | None | Produces logical defaults; does not create a device. Definition at vendor line 77455. |
| `ma_engine_init` | Config pointer, stable engine address | Negotiates/creates device, graph, listeners, and internal resource manager; can fail with a `ma_result`. Definition 77521. |
| `ma_engine_listener_set_world_up` | Engine, index `0`, `(0,1,0)` | Establishes listener orientation's up axis. |
| `ma_sound_init_from_file` | Engine, path, flags `0`, group null, fence null, destination | Wraps sound config and creates a resource-managed data source and engine node. Definition 78567. |
| `ma_sound_set_looping` | Sound, `MA_TRUE`/`MA_FALSE` | Changes data-source loop behavior; not a call to start playback. |
| `ma_sound_set_min_distance`, `ma_sound_set_max_distance`, `ma_sound_set_attenuation_model` | Sound and configured scalar/enum | Update spatializer attenuation configuration. |
| `ma_sound_set_volume` | Sound and linear gain | Delegates to engine-node volume state. Definition 78832. |
| `ma_engine_set_volume` | Engine, zero or `masterVolume` | Controls endpoint/master output gain; does not pause source transport. |
| `ma_engine_listener_set_position`, `ma_engine_listener_set_direction` | Engine, listener `0`, three components | Set listener spatial values; positions/directions use vendor synchronized vector storage. |
| `ma_sound_set_position` | Sound and three world components | Sets source spatial position; no pointer to the caller's GLM vector is retained. |
| `ma_sound_set_pitch` | Sound and `flightPitch` | Changes engine-node resampling/pitch ratio. |
| `ma_sound_is_playing` | Sound pointer | Queries node/playback state to avoid unnecessary starts/stops. |
| `ma_sound_start` | Sound pointer | Returns success if already playing; if at end, seeks/re-arms; sets node state started. Definition 78746. |
| `ma_sound_stop` | Sound pointer | Sets stopped node state; does not destroy or automatically rewind. Definition 78774. |
| `ma_sound_seek_to_pcm_frame` | Sound, frame `0` | Schedules an atomic seek target for the mixing thread; actual seek is not performed synchronously by this wrapper call. Definition 79425. |
| `ma_sound_uninit` | Each valid sound pointer | Detaches node, releases processing cache and owned source; definition 78699. |
| `ma_engine_uninit` | Engine pointer | Stops callback access before releasing graph/device/manager/listener resources; definition 77795. |

The wrapper checks engine/sound **initialization** results but ignores later start, stop, seek, and master-volume results. A gameplay event can be consumed even if the device or underlying source later fails. Documentation should distinguish intended cue dispatch from confirmed audibility.

## 6.6 `miniaudio_impl.cpp` and Compile-Time Selection

### 6.6.1 Both Lines of the Implementation Unit

Line 1 is `#define MINIAUDIO_IMPLEMENTATION`; line 2 is `#include "miniaudio.h"`. That macro selects the definitions below the public header region. It must be used in **one** compiled translation unit to avoid multiple external definitions. Other game headers include `miniaudio.h` without it and see declarations/types only.

The public header guard is `miniaudio_h`; the implementation has a separate `miniaudio_c` guard and is selected by either `MINIAUDIO_IMPLEMENTATION` or `MA_IMPLEMENTATION` at line 11552. Separate guards permit the common pattern of including declarations and later including an implementation in the same translation unit. Editor-analysis macros around lines 11538-11543 can enable implementation visibility for IntelliSense/Qt Creator/CDT; that is not the normal executable build's source-selection mechanism.

CMake and the inspected Visual Studio project compile the main application, GLAD C unit, and this miniaudio C++ unit. Merely retaining another `.c` file on disk does not compile it. Include directories make `miniaudio.h` resolvable from its organized third-party location; physical relocation by itself does not enable a codec.

### 6.6.2 Is stb_vorbis Compiled or Necessary?

The answer for the inspected project is **no, it was never compiled or included and has now been removed; no, it is not necessary for the eight active preferred WAV/MP3 files**. The two-line implementation unit does not include it. Neither executable source list contains it. The miniaudio Vorbis adapter begins at line 65330 under `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H`; that macro is defined by stb_vorbis's declaration section, but no project source makes those declarations visible before the miniaudio implementation.

To use this particular optional adapter, stb_vorbis declarations must be visible when miniaudio's implementation is compiled and stb_vorbis definitions must be linked exactly once. A declaration-only include using `STB_VORBIS_HEADER_ONLY`, followed by miniaudio implementation and one separately compiled stb_vorbis implementation, is one possible integration design. A single-translation-unit inclusion arrangement is another. These are explanations of requirements, **not changes made to the build**. Adding `stb_vorbis.c` alone to CMake without exposing its declarations to miniaudio does not activate the adapter. Defining its header guard alone without declarations is also not valid integration.

An alternate custom Vorbis decoder could satisfy Ogg support instead. Vorbis would become necessary only if the game relies on Ogg assets and chooses this codec implementation. The filename suffix `.ogg` denotes a container; arbitrary Ogg-contained codecs such as Opus are not decoded by a Vorbis-only decoder.

### 6.6.3 Compile-Switch Families and Dependencies

Build-switch documentation is at lines 547-718, platform/type detection starts at 3860, backend feature selection is at 6654-6699, and implementation selection is at 11552.

| Switch family | Effects and cautions |
| --- | --- |
| `MA_NO_WASAPI`, `MA_NO_DSOUND`, `MA_NO_WINMM`, `MA_NO_ALSA`, `MA_NO_PULSEAUDIO`, `MA_NO_JACK`, `MA_NO_COREAUDIO`, `MA_NO_SNDIO`, `MA_NO_AUDIO4`, `MA_NO_OSS`, `MA_NO_AAUDIO`, `MA_NO_OPENSL`, `MA_NO_WEBAUDIO`, `MA_NO_CUSTOM`, `MA_NO_NULL` | Remove individual device backends. They do not automatically remove the engine's platform-neutral DSP. |
| `MA_ENABLE_ONLY_SPECIFIC_BACKENDS` and matching `MA_ENABLE_*` macros | Change backend selection to an explicit allowlist. Platform support is still required; enabling ALSA on Windows does not manufacture Linux APIs. |
| `MA_NO_DEVICE_IO` | Removes context/device playback/capture APIs; allows decode/conversion-only uses, or a no-device engine with explicit channels/sample rate. Not selected in Arrakis. |
| `MA_NO_THREADING` | Removes thread/mutex/semaphore/event support. Device I/O must also be disabled; worker behavior and resource-manager configuration change. |
| `MA_NO_DECODING`, `MA_NO_ENCODING` | Remove decoder or encoder API sections. The resource manager requires decoding; disabling decoding forces the related dependency change. |
| `MA_NO_WAV`, `MA_NO_FLAC`, `MA_NO_MP3`; optional `MA_NO_VORBIS` | Remove individual built-in/adapted codecs. WAV also supplies the built-in encoder. None of these are defined in the two-line implementation unit. |
| `MA_NO_RESOURCE_MANAGER` | Removes resource-managed file loading and file-based engine helpers, including the exact `ma_sound_init_from_file` API the game needs. External data sources remain possible. |
| `MA_NO_NODE_GRAPH`, `MA_NO_ENGINE` | Remove graph or high-level engine APIs. The engine depends on the graph; Arrakis needs both. |
| `MA_NO_GENERATION` | Removes waveform/noise generator APIs; the game does not use them. |
| `MA_NO_SSE2`, `MA_NO_AVX2`, `MA_NO_NEON` | Remove CPU-specific optimization paths while retaining scalar implementations where available. |
| `MA_NO_RUNTIME_LINKING` | Replaces dynamic system-library loading requirements with directly linked backend symbols; may require additional linker configuration. |
| `MA_USE_STDINT`, `MA_API`, allocation/assert/log customization macros | Control sized types, symbol decoration, allocation hooks, diagnostics, and embedding policy rather than cue semantics. |
| `MA_COINIT_VALUE`, `MA_FORCE_UWP` | Windows COM initialization and Windows-app backend behavior. Default COM model is multithreaded. |
| `MA_ON_THREAD_ENTRY`, `MA_ON_THREAD_EXIT`, `MA_THREAD_DEFAULT_STACK_SIZE` | Hooks/stack size for internally created threads; must not be confused with gameplay frame callbacks. |
| `MA_ENABLE_AUDIO_WORKLETS` | Emscripten worklet path; requires matching toolchain options such as `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY`. Not the Windows path. |

Declarations, implementation, and consumers must agree on configuration macros and struct layouts. miniaudio explicitly does not guarantee ABI compatibility even across patch versions. The safest use here is the supplied header and implementation compiled together, not arbitrary interchange with another binary miniaudio version.

## 6.7 miniaudio: Complete Coverage Map and Functional Architecture

### 6.7.1 Complete Contiguous Source Map

All numbers in this subsection refer to the inspected 95,864-line `miniaudio.h`, unchanged by relocation. Boundaries include separator comments/blank lines so there are no uncovered source regions. A broad bin can contain shared helpers belonging to the adjacent family; the declaration and definition names provide the exact semantic entry points.

| Lines | Functional contents |
| --- | --- |
| 1-3737 | Version/provenance; complete in-header manual, examples, build switches, low/high-level APIs, resource manager, graph, decoding/encoding, conversion, filters, generation, buffering, backends, and optimization notes |
| 3738-4515 | Public guard/linkage, fixed-width/platform types, result/error enums, formats/channels, allocation callbacks, atomic/thread type declarations, version APIs |
| 4516-4590 | Logging callback/configuration APIs |
| 4591-5003 | Biquad, low-pass, high-pass, band-pass, notch, peaking EQ, low/high shelf declarations |
| 5004-5317 | Delay, gainer, stereo panner, fader, vector/spatializer/listener declarations |
| 5318-5830 | Linear/general resampling, channel conversion, data conversion, format conversion, channel-map helpers |
| 5831-6111 | Data-source base/vtable APIs; borrowed/owned/paged audio buffers; ring-buffer declarations |
| 6112-6593 | Results/allocations/miscellaneous helpers, spinlocks and OS synchronization, fences, notifications, slot allocation, job queue |
| 6594-9751 | Backend selection, context/device IDs/configs/callbacks/native structs; enumeration, lifecycle, volume, buffer-size and device helper APIs |
| 9752-9865 | PCM copying, silencing, clipping, volume/mix/interleave utilities |
| 9866-9944 | Virtual filesystem and file-I/O abstraction declarations |
| 9945-10110 | Generic decoder and custom decoding-backend APIs |
| 10111-10169 | Encoder configuration/callback/API declarations |
| 10170-10296 | Waveform, pulsewave, and white/pink/brownian noise declarations |
| 10297-10614 | Resource-manager configuration, flags, buffer/stream/source objects, registration, notifications, job processing |
| 10615-11107 | Node graph, input/output buses, node/vtable/state flags, endpoint/source/splitter and filter/delay node APIs |
| 11108-11535 | Engine/engine-node/sound/group configuration, listener/control/scheduling APIs and public guard closure |
| 11536-13569 | Implementation gate/includes; architecture/SIMD/compiler detection; platform linkage, memory/string and portability groundwork |
| 13570-18578 | Logging definitions; internal math/random/vector/string/platform helpers, atomics, thread creation and synchronization, fences/notifications |
| 18579-19470 | Slot allocator and bounded job queue implementation |
| 19471-20933 | Common device-I/O definitions, backend platform types/constants, conversions, descriptors, and shared setup |
| 20934-21707 | Null backend: virtual timing/buffering device without physical output |
| 21708-25216 | WASAPI backend, COM/interface shims, endpoint enumeration, format negotiation, event/client processing, notifications/routing |
| 25217-27001 | DirectSound backend and playback/capture/notification interface/buffer management |
| 27002-28101 | WinMM waveform input/output backend |
| 28102-30262 | ALSA PCM backend, including enabled body through 30254 and boundary/shared setup |
| 30263-32799 | PulseAudio backend and server/mainloop/stream interactions |
| 32800-33476 | JACK client, port, sample-rate and callback backend |
| 33477-36716 | Core Audio/AudioUnit backend and Apple device/routing handling |
| 36717-37563 | sndio backend |
| 37564-38460 | BSD audio(4) backend |
| 38461-39094 | OSS backend |
| 39095-40230 | AAudio backend, error/reroute jobs, and absent-backend no-op job glue |
| 40231-41494 | OpenSL ES backend and Android object/queue/channel mapping |
| 41495-42450 | Web Audio JavaScript/Emscripten bridges and optional worklet processing |
| 42451-44574 | Shared context/backend dispatch, enumeration, device lifecycle, device jobs, state transitions, period/buffer selection |
| 44575-45089 | Buffer-duration calculations; PCM copy/silence/clip/volume/mix helpers |
| 45090-47163 | Sample-format conversion, scalar/SIMD conversion paths, normalization/dithering/interleaving support |
| 47164-50374 | Biquad and all filter-family implementations |
| 50375-53026 | Delay, gainer, panner, fader, listener/spatializer, attenuation, cone, Doppler and directional gains |
| 53027-54276 | Linear and generic/custom-vtable resampling implementations |
| 54277-55901 | Channel conversion, mono expansion, rearrangement, weighted mixing |
| 55902-57072 | Composite data converter and frame-count/latency processing |
| 57073-58096 | Channel maps and one-shot conversion helper implementations |
| 58097-58922 | Byte/PCM ring buffer operations and duplex buffering support |
| 58923-60903 | Result descriptions, allocation wrappers, sample-size helpers, data-source operations, borrowed/owned/paged audio-buffer implementations |
| 60904-61933 | VFS callback dispatch and default filesystem implementation; generated-decoder-header preamble |
| 61934-62368 | Embedded dr_wav 0.14.5 declarations, structs, metadata, read/write/seek/format conversion APIs |
| 62369-62662 | Embedded dr_flac 0.13.3 declarations, bitstream/metadata/seek/read APIs |
| 62663-62839 | Embedded dr_mp3 0.7.3 declarations, frame/decoder state, read/seek/metadata APIs |
| 62840-65329 | Generic decoding groundwork and WAV/FLAC/MP3 data-source/backend adapters |
| 65330-66159 | Optional stb_vorbis adapter, excluded unless stb_vorbis declarations are visible |
| 66160-67830 | Generic decoder initialization/probing, callback/file/memory paths, output conversion, reading/seeking/length/cursor helpers |
| 67831-68147 | Built-in WAV encoder adapter, encoder initialization/writing/teardown |
| 68148-69308 | Waveform/pulsewave and noise generator implementations |
| 69309-73426 | Resource manager: path/hash registry, cached buffers, streamed pages, job dispatch, synchronization, source operations |
| 73427-76536 | Node graph: graph traversal/mixing, attachment lists, state/scheduling, source/splitter/filter/delay nodes |
| 76537-79893 | Engine, listeners, engine-node processing, sound/group loading/control/seeking/fades and inlined sounds |
| 79894-79907 | Embedded codec-implementation preamble and warning/integration setup |
| 79908-84837 | Embedded dr_wav implementation |
| 84838-92696 | Embedded dr_flac implementation |
| 92697-95804 | Embedded dr_mp3/minimp3-derived implementation |
| 95805-95864 | Warning-state restoration, implementation guards, public-domain/MIT No Attribution license alternatives |

### 6.7.2 Object Model, Results, Memory, and PCM Units

The main recurring pattern is `*_config_init`, `*_init` or `*_init_preallocated`, `*_process_pcm_frames`/read/control calls, and `*_uninit`. Configs are temporary descriptions; initialized objects are long-lived, noncopyable-in-practice structures containing caches, locks, pointers, and ownership bits. Some components expose `*_get_heap_size` plus preallocated initialization to let the application provide one suitably aligned backing block; ordinary initialization allocates using `ma_allocation_callbacks`.

`ma_result` is a signed result enum: `MA_SUCCESS = 0`, with negative error/status values. General argument/memory/I/O errors, no-data/at-end states, unsupported-format errors, device/backend failures, and cancellation are distinct. Device errors include `MA_NO_BACKEND = -203`, `MA_BACKEND_NOT_ENABLED = -208`, and initialization/open/start/stop backend errors `-400` through `-403`. `ma_result_description` at line 58926 translates known result values to readable text; Arrakis prints numeric initialization results instead. Success from sound start means the request/state transition was accepted, not that the operating-system speakers audibly emitted the cue.

`ma_malloc`, `ma_realloc`, `ma_free`, aligned allocation, and copied allocation callbacks keep allocation/free policy paired. Borrowed data must outlive its reader; owned data is released by the corresponding uninit. Resource-manager buffers can be shared/refcounted, while independent decoder cursors belong to their consumers. Caller-provided memory and an external manager/device must not be freed as if owned; ownership flags decide teardown.

A PCM **frame** contains one sample for each channel. For interleaved audio, bytes required are `frameCount * channels * bytesPerSample`; stereo f32 uses 8 bytes per frame. A stereo frame count of 480 is 10 ms at 48 kHz, not 960 ms or 960 frames. f32 is conventionally `[-1,1]`; s16 is `[-32768,32767]`; packed s24 uses three bytes; s32 uses four; unsigned u8 silence is **128**, while signed/float silence is zero. The source's `ma_silence_pcm_frames` explicitly handles that distinction at lines 44606-44616.

Conversion and decode functions often return both a result and an actual frame count, or update consumed/produced count pointers. The caller must respect partial output and buffer capacities instead of assuming a requested count was produced. Frame-index APIs can use 64-bit types even when an underlying optional decoder has a narrower seek limitation.

### 6.7.3 Logs, Atomics, Threads, Notifications, and Jobs

Logging (`ma_log_*`) stores registered callbacks, severity, user data, and synchronization around callback registration/posting. Version/math/string/vector and platform helpers underpin all later subsystems; they are not game logic. SIMD dispatch selects suitable implementations after compiler/architecture detection rather than assuming every CPU supports all enabled instruction sets.

Atomic helpers implement integer/pointer/float access and synchronized vector snapshots using supported compiler intrinsics or fallback locking. They let the game publish spatial/control state while the mixer consumes it. This does **not** imply that arbitrary direct edits to transparent struct members are safe, or that every public function is callback-safe.

`ma_thread_*`, `ma_mutex_*`, `ma_event_*`, `ma_semaphore_*`, and spinlocks abstract Windows and pthread/platform primitives. Events are wakeup mechanisms; a fence tracks outstanding work and permits waiting until completion; asynchronous notifications can use callbacks, event/fence adapters, or their combinations. `ma_sound_seek_to_pcm_frame` illustrates deferred ownership: the game thread atomically publishes a target; the thread reading audio performs the actual decoder seek, avoiding concurrent decoder mutation.

The slot allocator reserves a fixed collection of job storage slots with generation/counter bookkeeping, reducing allocation churn and avoiding reuse confusion. The bounded job queue posts and retrieves typed jobs, optionally waits or reports `MA_NO_DATA_AVAILABLE`, and uses quit/cancellation semantics. Jobs include resource load/free/page/seek work and AAudio rerouting; a queue-capacity failure is different from decoding failure. The resource manager defaults to **one** managed job thread, as seen at line 69977, not one thread per sound.

The device's realtime callback must produce exactly the requested frame region without blocking filesystem work, expensive unbounded allocation, or waiting on device lifecycle operations. The manual explicitly forbids device init/uninit/start/stop from within its own callback because that can deadlock. Arrakis performs these lifecycle actions from application setup/shutdown, not from a custom device callback.

### 6.7.4 Device/Context Abstraction and Backend Families

`ma_context` selects a usable backend, holds global backend state/function tables, manages logs/allocation callbacks, and enumerates playback/capture devices. `ma_device_id` is a backend-specific identifier; display names are not a universal persistent device handle. `ma_device_config` expresses playback/capture format, channels/maps, rate, share mode, periods, callbacks, user data, and backend options. `ma_device` owns negotiated native state, conversion paths, intermediate buffers, synchronization, and start/stop state.

Device types are playback, capture, duplex, and supported loopback modes. Playback callbacks write output; capture callbacks read input; duplex can do both. Native device format can differ from callback format, so conversion occurs at the boundary. Period sizes, native sample rates, backend buffering, and callbacks determine actual latency; the game's graphics `dt` does not drive sample delivery.

Default backend priority from the supplied manual is WASAPI, DirectSound, WinMM, Core Audio, sndio, audio(4), OSS, PulseAudio, ALSA, JACK, AAudio, OpenSL ES, Web Audio, Custom, Null. Unavailable/uncompiled backends are skipped; initialization can fall through until a usable backend is found. A Windows compilation excludes Linux/BSD/Apple/Android implementations through feature guards even though their source remains in the header.

| Backend | Native mechanism, threading/buffering, and limits |
| --- | --- |
| WASAPI; 21708-25216 | Windows COM MMDevice/AudioClient interfaces, endpoint discovery, shared/exclusive format negotiation, event/client buffering, render/capture services, loopback, endpoint notifications and rerouting. COM startup and some commands need carefully chosen threads. Low-latency shared mode has sample-rate/autoconversion caveats; UWP has default-device and capability restrictions. |
| DirectSound; 25217-27001 | Windows playback/capture buffer interfaces and notifications. Ring/circular-buffer positions and notifications drive chunk transfer. Interface release and DLL ownership are separate from sound data ownership. This is a fallback backend, not OpenGL's Direct State Access. |
| WinMM; 27002-28101 | Older Windows `waveOut`/`waveIn` style devices and queued waveform headers/buffers. Completion/requeue logic feeds output or gathers input; format support and latency differ from WASAPI. |
| ALSA; 28102-30262 | Linux PCM devices, hardware/software parameter negotiation, read/write or supported mapped transfer, polling/wakeup and recovery for underruns/overruns. Dynamic libasound resolution avoids requiring its development headers in the normal embedded build. |
| PulseAudio; 30263-32799 | Server-based playback/capture streams, context/mainloop synchronization, server/device enumeration and stream buffering/latency requests. Server policy can route devices automatically; the game does not issue PulseAudio calls directly. |
| JACK; 32800-33476 | JACK client/ports and server realtime process callback. Server rate/channel/period constraints and port connection define operation; callback processing must fit realtime deadlines. |
| Core Audio; 33477-36716 | Apple device/AudioUnit machinery, render/capture callbacks, stream properties and route changes. macOS/iOS platform branches have different device/default-route behavior and Apple linkage paths. |
| sndio; 36717-37563 | OpenBSD libsndio handle/options and PCM transfer with polling/drain/volume support as provided by the backend. Normally enabled on OpenBSD only. |
| audio(4); 37564-38460 | NetBSD/OpenBSD device files and audio ioctl/configuration structures with read/write transfer. On OpenBSD, an active sndiod can affect availability. |
| OSS; 38461-39094 | FreeBSD/OSS-style device file and ioctl format/rate/channel negotiation, read/write/polling; platform-specific capability/device constraints. |
| AAudio; 39095-40230 | Android stream builder/stream APIs, realtime data/error callbacks, usage/content/input presets, disconnect handling and reroute jobs. Android 8+; native enumeration is limited to default devices unless the app supplies Java-discovered IDs. |
| OpenSL ES; 40231-41494 | Android engine/mix/player/recorder objects, PCM channel-mask translation and buffer-queue callbacks. Single application-level engine/context constraints, queued buffer lifetime and API 16+ requirements matter. |
| Web Audio; 41495-42450 | Emscripten JavaScript audio context/device bridges, script processing and optional AudioWorklet path. Browser user-gesture/autoplay restrictions and no-pthread builds change startup/job processing. |
| Custom | Application-supplied backend callbacks/vtable enter the common context/device dispatch; there is no separate magic hardware driver implementation to describe. Correct lifecycle, enumeration and data transfer are the application's responsibility. |
| Null; 20934-21707 | Software clock/buffer/callback device without physical playback. Useful for testing or fallback; a successfully initialized null endpoint does not prove audible output. |

Backend structs and shims include ABI-compatible native declarations/function pointers, library opening/closing, native enumeration/configuration, format/rate conversion, device init/uninit/start/stop, processing or callback entry, and backend-specific error translation. The common dispatch at lines 42451-44574 wires these into one public API. Unsupported branches return result codes rather than selecting an unrelated graphics API. Runtime linking is platform-specific; `MA_NO_RUNTIME_LINKING` changes linker obligations. On Linux the supplied manual calls for `-ldl -lpthread -lm`, possibly `-latomic` for 32-bit ARM; the Windows-specific project link configuration should not be described as already implementing every portability requirement.

### 6.7.5 PCM Utilities and DSP Filter Families

Utilities copy/silence PCM, compute offsets, clip values, apply gains, mix streams and interleave/deinterleave channel storage. They distinguish frames from samples and signed from unsigned silence. Format converters cover u8/s16/s24/s32/f32 with scalar/SIMD paths, scaling, clipping and requested dithering behavior. A conversion's direction determines destination capacity and precision loss; changing byte representation is not resampling.

The biquad family stores coefficients plus per-channel delay/register state. Its mathematical prototype is `y[n] = (b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2])/a0`; implementations normalize coefficients and choose supported processing formats. `reinit` can replace configuration while retaining history, unlike tearing down/reinitializing the object. Separate channels need separate state.

| DSP family | Declaration lines; definition entry | Explanation |
| --- | --- | --- |
| `ma_biquad_*` | 4593-4642; config at 47166 | General second-order filter with caller coefficients, heap/preallocated state and in-place/buffer processing. |
| `ma_lpf1_*`, `ma_lpf2_*`, `ma_lpf_*` | 4645-4731; 47487 onward | First/second/higher-order low-pass filters, cutoff and sample rate; cascaded sections for higher-order Butterworth behavior. Removes high frequencies and supplies resampler antialias smoothing. |
| `ma_hpf1_*`, `ma_hpf2_*`, `ma_hpf_*` | 4734-4817; 48376 onward | High-pass counterparts, retaining high frequencies and rejecting low/DC components. |
| `ma_bpf2_*`, `ma_bpf_*` | 4820-4879; 49200 onward | Band-pass sections controlled by center/cutoff frequency and Q; higher-order family cascades sections. |
| `ma_notch2_*` | 4882-4909; 49686 | Rejects a narrow band while retaining frequencies outside it. |
| `ma_peak2_*` | 4912-4940; 49859 | Peaking equalization with frequency/Q/gain in dB; boosts or cuts around a center. |
| `ma_loshelf2_*` | 4943-4971; 50034 | Low-frequency shelf, gain in dB and slope. |
| `ma_hishelf2_*` | 4974-5003; 50207 | High-frequency shelf counterpart. |
| `ma_delay_*` | 5005-5039; 50379 | Circular delayed sample history, delay frame count and decay/dry-wet controls; stateful echo, not decoder seeking. |
| `ma_gainer_*` | 5040-5074; 50533 | Per-channel gains and gradual transition over configured frames; smooths control changes. |
| `ma_panner_*` | 5075-5109; 51078 | Stereo balance/pan modes modify left/right gains; not full 3D geometry. |
| `ma_fader_*` | 5110-5137; 51286 | Time/frame-position-dependent fades and gain endpoints; seconds/milliseconds must be converted using sample rate. |
| `ma_spatializer_listener_*`, `ma_spatializer_*` | 5138-5317; spatial config at 52051 | Source/listener coordinate transform, distance/cone/directional attenuation, speaker-channel weighting, gain interpolation, and Doppler ratio. |

Arrakis invokes gain, pitch and spatialization through the engine; it does not construct custom low/high-pass, shelf, notch, delay or noise objects. Their vendor code is still included by the unspecialized implementation configuration, though unused functions may be removed by link-time/dead-code optimization depending on the build.

### 6.7.6 Resampling, Channel Conversion, and Composite Conversion

`ma_linear_resampler_*` config begins at 53032. It maintains fractional source position, previous samples/filter state and an input/output rate relationship. Linear interpolation estimates between adjacent samples; low-pass filtering limits aliasing when rates change. Required-input/expected-output/latency helpers describe how much input is needed and how delay affects frames. Dynamic rate/ratio setters support pitch or device-rate changes without rebuilding every object.

`ma_resampler_*` dispatches either the built-in linear resampler or a custom resampler vtable, handling init/heap/process/reset/rate/count/latency operations. An external implementation must obey frame-count and lifetime contracts. Pitch is usually expressed by modifying the consumption ratio, which changes duration as well as frequency. The default engine pitch-resampling config uses f32 linear resampling with low-pass order `0` at lines 77464-77465; the vendor comment explicitly cites instability concerns for some pitch-filter cases. Do not infer a separate studio-quality time-stretch algorithm.

`ma_channel_converter_*` creates mono expansion, positional channel mapping, reordering and weighted mixing. It can use passthrough/copy paths when layouts match; adding or removing channels is a different operation from changing sample rate. Channel maps encode speaker positions, not merely ordinal indexes; surround-to-stereo and mono-to-multichannel policies affect spatial perception.

`ma_data_converter_*` composes format conversion, channel conversion and resampling, chooses efficient processing order/caches, calculates heap requirements, and updates consumed/produced frame counts. Unknown/default fields are resolved before processing. PCM format changes can lose precision; resampling adds state and latency; channel conversion may mix information. Single-shot `ma_convert_*` helpers build or drive these operations for one buffer, while long-running converters retain state across blocks.

### 6.7.7 Data Sources, Buffers, Rings, and Filesystem

`ma_data_source` is a common read/seek/format/cursor/length/loop interface implemented through a vtable and base state. It also supports source ranges, loop points and chaining. The base operations enforce ranges and looping around underlying readers. Not all sources implement seeking or reliable length; callers must inspect results rather than assuming a finite file-backed stream.

`ma_audio_buffer_ref` borrows a PCM array and maintains a cursor without owning the array. `ma_audio_buffer` can allocate/copy/own storage according to its config, and its uninit releases owned storage through matching callbacks. Paged buffers link PCM pages and let producers expose growing decoded data; page ownership and lifetime must not race readers. Data-source initialization entry is 59138, borrowed-buffer init 60044, owned-buffer init 60361, paged-buffer init 60733.

`ma_rb` is a byte ring; `ma_pcm_rb` adds format/channel/frame knowledge. Acquire/commit read and write APIs expose contiguous regions, handle wraparound, and advance cursors only on commit. Ring buffers are documented for **one consumer and one producer**, not arbitrary multi-reader/multi-writer access. Available read/write counts prevent overrunning unread data or reading unwritten data. Duplex helpers buffer between capture/playback clock domains; differing clocks can drift even when nominal sample rates match.

The VFS is a callback table for open/close/read/write/seek/tell/info. Default VFS uses OS/CRT filesystem operations with narrow/wide path variants and 64-bit-position support where available. Decoders and resource manager can use a custom archive/memory/package filesystem without hardcoding game paths. Arrakis does not supply one: it performs a standard `ifstream` pre-probe and then lets miniaudio's default VFS open the selected path. That double-open pattern allows a file to change/disappear between lookup and decode.

### 6.7.8 Decoders, Probing, Adapters, and Encoder

`ma_decoder_config` describes requested output format/channels/rate, maps/mixing/resampling, allocation callbacks and optional custom backend vtables. `ma_decoder` owns one selected backend plus conversion/cursor/cache state. Its callback, VFS-file, wide-file and memory initializers attempt supported backends, establish native audio properties and configure output conversion. Built-in and custom backend attempts can fail and be cleaned up before the next attempt; an encoding hint/extension can guide selection but existence or suffix is not authoritative content validation.

The WAV adapter, FLAC adapter, MP3 adapter and optional Vorbis adapter expose codec-specific states as `ma_data_source`s. Their vtables translate read/seek/get-format/get-cursor/get-length into codec functions and normalize result reporting. Definitions and declarations inside the implementation region are internal integration scaffolding, not evidence the wrapper calls those functions directly.

`ma_decoder_read_pcm_frames` reads native decoded PCM and converts it to configured output while honoring partial counts/EOF. Seek operations reset or reposition dependent state; cursor and length are in PCM frames, not encoded bytes. Memory decoders borrow encoded memory for their decoder lifetime unless the surrounding manager owns a loaded copy. File decoders must close file state during teardown. Whole-file decode helpers allocate output PCM that the caller must free with the corresponding allocator.

The optional `ma_stbvorbis` state stores a base data source, read/seek/tell callbacks and user pointer, allocation callbacks, f32 format, channels/rate, 64-bit reported cursor, the stb decoder pointer, push/pull mode and push-buffer/cache state. `MA_VORBIS_DATA_CHUNK_SIZE` is **4096 bytes**. Callback mode uses pushdata because stb_vorbis's pull API lacks generic read callbacks; file/memory modes can use stb's native pull APIs. Push mode accumulates encoded bytes, tracks consumed bytes and remaining planar samples, then copies/interleaves output. Buffer growth and stb's `int` length APIs impose checked size limits. Push seeking can rebuild/redecode from the beginning because flush-and-arbitrary-seek is not robust enough for this adapter. Push-mode length is reported as zero where reliable length cannot be obtained; native pull mode can query stream length. Adapter allocation callbacks do not automatically control stb_vorbis's own malloc/alloca use.

The encoder family supports the built-in WAV encoder, not MP3/Vorbis/FLAC encoding merely because those decoders exist. It initializes a write/seek callback or file target with an explicit encoding format and PCM format/rate/channels, writes PCM frames, and finalizes/patches container bookkeeping during uninit where supported. Sequential writers need total-size knowledge when seeking is unavailable. Arrakis does not record or encode audio.

### 6.7.9 Embedded WAV Implementation: All Subfamilies

The embedded copy is dr_wav **0.14.5**, renamed with `ma_dr_wav_*` symbols to coexist with independent dr_wav users. Declarations occupy 61934-62368; implementation occupies 79908-84837. This is actual codec/container code included in miniaudio, not a reference to a separately installed DLL.

| Implementation lines | Functional explanation |
| --- | --- |
| 79908-81736 | Version/platform/memory/endian helpers; RIFF/WAVE, Wave64 and RF64 container identification and chunk iteration; `fmt`/data discovery; padding/size accounting; format/subformat validation; metadata parsing/allocation and setup/read callbacks. Unknown chunks are skipped subject to lengths; truncated/invalid headers fail instead of becoming arbitrary PCM. |
| 81737-82850 | Public callback/memory/file initialization, sequential and seekable writer setup, metadata-aware writer variants, file wrapper callbacks, uninitialization/finalization, raw reads/writes, cursor/length/seek and common PCM transfer. Ownership flags distinguish library-opened files from borrowed callback data. |
| 82851-83967 | Native PCM reads and endian variants, format-specific s16 and f32 conversion helpers, A-law/mu-law expansion, Microsoft ADPCM and IMA/DVI ADPCM block decoding with predictor/history and cached decoded frames. Encoded bytes and PCM frames have different ratios for compressed WAV. |
| 83968-84837 | f32/s32 normalized read variants, remaining scalar conversion families, callback/file/memory whole-file decoding helpers, allocation/free/error handling and embedded implementation closure. Whole-file helpers allocate a buffer, repeatedly decode and return actual frame count; they do not leak ownership to the engine without an explicit handoff. |

RIFF chunk sizes/padding, extensible format tags and channel/sample metadata are validated before data transfer. Supported WAV encodings include integer PCM, IEEE float and the listed telephony/ADPCM formats; `.wav` is a container, not a guarantee of uncompressed 16-bit PCM. Writes use supported uncompressed/container formats and header bookkeeping; having an ADPCM decoder does not imply the library writes every compressed WAV format. The game uses ordinary WAV reads for harvester, winch, safe and crew cues, without directly invoking metadata-aware authoring APIs.

### 6.7.10 Embedded FLAC Implementation: All Subfamilies

The embedded copy is dr_flac **0.13.3**, namespaced as `ma_dr_flac_*`. It is a lossless decoder; no FLAC assets are selected by the wrapper's candidate lists.

| Implementation lines | Functional explanation |
| --- | --- |
| 84838-86342 | Portability/SIMD/byte-order helpers, allocation callbacks, CRC tables/checks, buffered bitstream reads, signed/unsigned extraction and FLAC frame/header number decoding. Efficient cache/bit reads are bounded by available data and the decoded field width. |
| 86343-87624 | Rice-coded residual reading; reference/scalar/SSE4.1/NEON predictor paths; escape/unencoded residual partitions and residual seeking. Unary quotient plus Rice remainder reconstruct signed residuals, which are added to a prediction rather than treated as finished audio. |
| 87625-88555 | Constant/verbatim/fixed-order/LPC subframe decoding, warm-up samples, coefficients and shift, wasted-bit restoration, frame/subframe headers, channel decorrelation, frame validation, skipping and seek-forward helpers. Fixed predictors and LPC reconstruct original integer samples losslessly. |
| 88556-90162 | Metadata block parsing, STREAMINFO, seektable and optional metadata callbacks, decoder allocation/init preparation, native FLAC/Ogg mapping infrastructure and callback/memory/file setup groundwork. Container metadata is distinct from PCM. |
| 90163-92265 | Public open/close variants, file/memory adapters, seek strategies and output-read paths, conversion/interleaving to s32/s16/f32, optimized channel decorrelation and buffering. Seek tables accelerate location but absent tables do not necessarily make decoding impossible. |
| 92266-92696 | Public f32 frame-reading entry, remaining whole-file/memory convenience and cleanup/implementation closure. Allocated output and decoder-object ownership are documented separately. |

For predictive subframes, conceptual reconstruction is `sample[n] = prediction(previous samples, coefficients) + residual[n]`, with shifts and bit widths dictated by the stream. Rice residual decoding maps a quotient/remainder code back to a signed integer. Stereo channel assignments include independent, left-side, right-side and mid-side; decoding reverses the selected decorrelation. CRC and validated bounds are part of handling malformed data but do not establish that every vendor branch was audited here. FLAC's Ogg-container facilities do not make it a Vorbis decoder.

### 6.7.11 Embedded MP3 Implementation: All Subfamilies

The embedded copy is dr_mp3 **0.7.3**, namespaced as `ma_dr_mp3_*`; low-level decode machinery derives from minimp3. This decoder is directly relevant to flight, boost and worm cues.

| Implementation lines | Functional explanation |
| --- | --- |
| 92697-94591 | Portability/SIMD setup, callback/file I/O, seek/metadata helper groundwork and minimp3-derived core: MPEG frame synchronization/header interpretation, bit readers, Layer I/II scale-factor/dequantization processing, Layer III side information/scalefactors/Huffman decode, bit reservoir, stereo processing, requantization, short-block reorder, antialiasing, inverse transforms and polyphase synthesis. Tables encode codec constants; SIMD accelerates equivalent PCM reconstruction. |
| 94592-94979 | Higher-level next-frame decoding for callback and memory inputs, encoded-buffer refill/compaction, metadata/frame-start and stream setup, native format properties and decoded-frame cache management. A frame can need reservoir bytes retained from earlier compressed frames. |
| 94980-95264 | Public callback/file/memory init variants, metadata callbacks, teardown and common read preparation; ownership/allocation errors and first-frame initialization are handled here. |
| 95265-95804 | f32/s16 frame reads, PCM cursor/length, seeking and seek-point tables, frame/PCM count helpers, whole-file convenience decoding, free functions, and implementation closure. Seeking generally locates an encoded frame then decodes/discards as necessary to reach a requested PCM position. |

Constants in declarations include maximum **1152 PCM frames per MP3 frame**, maximum **2304 samples** for two channels, maximum **511 reservoir bytes**, and **2304** free-format frame-size constant. These are codec-buffer limits, not the game's audio-callback frame size. Layer III uses a compressed bit reservoir, so a sound cannot always begin decoding correctly from an arbitrary encoded byte without reconstructing prior state.

MP3 reconstruction is lossy: compressed spectral values are entropy decoded, requantized/scaled, possibly jointly stereo-reconstructed, transformed and synthesized into PCM. dr_mp3 provides format decoding, not device output. The generic miniaudio converter/engine subsequently adapts rate/channels and spatial gain. A `.mp3` filename is checked by decoding/probing; an invalid or truncated file causes failure or a short read, not an automatic successful sound.

### 6.7.12 Generators

`ma_waveform` synthesizes sine/square/triangle/sawtooth-style periodic waves using configured amplitude/frequency/rate and persistent phase/time. `ma_pulsewave` adds pulse width/duty behavior. `ma_noise` supplies white, pink and brownian noise with seed/random state and per-channel history/filtering where required. Their data-source interfaces permit reads without a finite file. Setters change parameters; seek/cursor semantics are different from reopening a recorded asset. White noise is uncorrelated random output; pink and brownian variants apply frequency/history shaping. These families are compiled under generation support but Arrakis creates none of them: rumble is an asset, not synthesized brown noise here.

### 6.7.13 Resource Manager: Registry, Caches, Streaming, and Lifetime

The resource manager lives at lines 69309-73426, with declarations at 10297-10614. Config initialization at 69969 defaults to unknown output format, zero channels/rate (defer resolution), one worker thread, and a bounded job queue. The engine's internal manager overrides decoded format to f32 and decoded rate to engine rate while leaving decoded channels at zero to preserve useful native-channel/spatialization behavior.

The registry hashes paths (default hash seed **42**) and stores reusable buffer nodes in a search-tree structure. Loading the same nonstreamed path can share encoded or decoded backing data with reference counting; sound instances still have independent playback state/cursors. Registering preloaded encoded or decoded data avoids repeated file I/O, but caller-supplied registered data has its own lifetime contract.

Flag families are `STREAM = 0x1`, `DECODE = 0x2`, `ASYNC = 0x4`, `WAIT_INIT = 0x8`, `UNKNOWN_LENGTH = 0x10`, and `LOOPING = 0x20`. Streaming uses paged/double-buffered decoded data so worker threads can read/decode/refill without a full file-sized allocation. Nonstreaming without DECODE keeps compressed/encoded bytes in memory; DECODE trades larger memory for less mixer decoding work. ASYNC queues loading/decoding instead of completing all work on the caller. WAIT_INIT establishes initial usable format/data even when later completion is asynchronous.

Typed jobs load/free a shared buffer node, load/free an instance buffer, page decoded data, load/free/page a stream, or seek a stream. Order/execution counters, notification/fence state and result fields coordinate initialization, future pages and destruction. A stream cannot free its decoder/page memory while a queued job still accesses it. Free/uninit must wait or otherwise honor those outstanding jobs. Job-thread shutdown posts quit/cancellation and joins threads before freeing queue/manager storage.

The manager API exposes register/unregister, buffer/stream/source init/uninit/copy, read/seek/format/cursor/length/loop controls and explicit process-next-job for application-managed execution. Custom VFS and custom decoder backends plug in at this level. On single-threaded Emscripten the engine has special job-processing behavior in its callback; that is not the usual Windows resource-manager thread model.

### 6.7.14 Node Graph and Every Built-In Node Family

Declarations 10615-11107; implementation 73427-76536. A graph mixes audio by pulling frames from its endpoint. Nodes have input and output buses, compatible channel counts, process vtables, state, times, gains and caches. Each input bus can receive several output buses; those sources are mixed before the destination process. A splitter's multiple outputs can feed distinct downstream branches. Channels must match at attachments; graph connections are not arbitrary byte-stream joins.

The implementation traverses upstream attachments, caches/mixes blocks, handles nodes with different consumed/produced rates, accounts for node start/stop scheduling and per-bus volume, and produces silence where required. Output-bus attachment lists are doubly linked with synchronized iteration/refcounts so detachment can wait for readers before destruction. This is why `ma_sound_uninit` first detaches its node. Feedback cycles should not be treated as a supported general-purpose cyclic graph merely because nodes can be connected.

The default node-cache capacity is **480 frames per bus** and default premix stack size is **524288 bytes per channel**, as named near 73492-73496. These are implementation/configuration defaults; engine device processing size can influence graph batching. Node caches are not the application's graphics frame buffers.

| Node family | Role |
| --- | --- |
| Graph endpoint/base node | Common state, scheduling, bus attachments, pulling/mixing and endpoint output. |
| Data-source node | Reads a `ma_data_source`, emits PCM, optionally loops; source must remain valid until detached/uninitialized. |
| Splitter node | Copies/routes an input to multiple output buses for parallel processing. |
| Biquad node | Wraps a general coefficient filter in graph processing. |
| Low-pass node | Wraps the low-pass DSP family. |
| High-pass node | Wraps high-pass DSP. |
| Band-pass node | Wraps band-pass DSP. |
| Notch node | Wraps narrow rejection filtering. |
| Peaking node | Wraps parametric/peaking equalization. |
| Low-shelf node | Wraps low-frequency shelf equalization. |
| High-shelf node | Wraps high-frequency shelf equalization. |
| Delay node | Wraps stateful delayed feedback/output mixing. |

Node flags express passthrough (`0x1`), continuous processing (`0x2`), allow-null input (`0x4`), differing processing rates (`0x8`) and silent output (`0x10`). They guide graph traversal/caching and callback contracts; they do not enable a physical device backend. Stateful effect nodes need their own DSP heap/register cleanup when removed.

### 6.7.15 Engine, Engine Nodes, Sounds, and Groups

Engine definitions 76537-79893 compose the graph and resource manager with device output. Config initialization at 77455 chooses one listener, default mono expansion, and linear resource/pitch resampling. Initialization at 77521 zeroes output storage, copies config/callbacks, optionally allocates and initializes an f32 playback device, resolves channel count/rate, initializes graph, validates listener count against `MA_ENGINE_MAX_LISTENERS = 4`, creates listeners, selects gain-smoothing time, creates/configures an internal resource manager when needed, and starts the engine last unless `noAutoStart` is set. Error labels unwind the portions reached; exact ownership/error behavior should be checked in source when changing this vendor version.

The internal device callback at 77472 reads `ma_engine*` from device user data and calls `ma_engine_read_pcm_frames`. That pulls the graph and writes the device's requested frame block independently of the render loop. The engine manages clipping itself and disables redundant device presilencing/clipping when it creates its device.

An engine node supplies gain, pitch resampling, spatialization, fader state, listener choice, mono expansion and caches around node processing. A `ma_sound` adds a data source, seek/end/loop state, processing cache and optional end callback. File initialization allocates a resource-manager source, initializes it, sets ownership, and creates/attaches the sound node. A group is essentially a sound-like engine node without a data-source voice, accepting other voices and applying shared controls. Group controls mirror sound controls for volume, pitch, pan, spatial configuration and timed fades.

APIs cover explicit device/manager/log retrieval, engine read/start/stop/time/volume, listener position/direction/up/velocity/cone/enabled state, sound/group init/uninit/control, data-source access, scheduled starts/stops in frames/milliseconds, fades, looping, seeking, cursor/length, and play-once inlined sounds. `ma_engine_play_sound` internally manages a temporary/inlined sound list and reclamation; Arrakis instead owns eight explicit sounds. Seeking is deferred via an atomic target, and end-state handling can rewind on restart. Setting master gain to zero does not remove nodes or stop consumption.

Owned device teardown precedes graph teardown to stop callback access. Sound teardown precedes backing-source release to stop graph readers. External device/resource-manager ownership differs from internal ownership: a caller must retain external objects while engine/sounds use them and release them in an order consistent with the docs. The wrapper's explicit stable global storage and sound-before-engine shutdown follow the core required order, without implementing a general lifetime-safe reusable audio-owner class.

## 6.8 GLAD: Generated Loader, Every Version, and Platform Branch

### 6.8.1 What This File Implements and What It Does Not

`third_party/glad/glad.c` is generated with API `gl=4.6`, core profile, loader enabled, and **an empty selected-extension list**. Its provenance records `Local files: False`, `Omit khrplatform: False`, and `Reproducible: False`. It includes `<glad/glad.h>` supplied by the project's configured external include directory. This `.c` file is not the OpenGL renderer implementation and not the driver: its function pointers lead to driver-provided implementations in the current context.

Arrakis explicitly uses `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)` after GLFW creates and makes an OpenGL context current. That uses GLFW's resolver, not GLAD's optional direct-system-library `gladLoadGL()` entry. Both paths are nevertheless present and documented because the request includes the entire supplied file.

### 6.8.2 Complete GLAD Source Coverage

| Original lines | Contents |
| --- | --- |
| 1-26 | Generator provenance/options and C/header includes |
| 27-157 | Procedure resolver declaration; Windows/UWP or dlopen platform loader; resolver fallback; `gladLoadGL` |
| 158-171 | Version struct/state, max loaded version, extension storage |
| 172-205 | Extension enumeration/storage |
| 206-216 | Extension-storage cleanup |
| 217-257 | Exact extension-name membership helper |
| 258-276 | Nineteen `GLAD_GL_VERSION_*` flags, all initially zero |
| 277-975 | Alphabetically arranged typed `glad_gl*` function-pointer globals, initially null |
| 976-1735 | Nineteen version-specific loading functions, individually mapped below |
| 1736-1742 | Selected-extension discovery wrapper; this generated selection is empty |
| 1743-1803 | Current-context version parsing and all version-flag tests |
| 1804-1832 | Public caller-resolver entry, core loads, extension handling and boolean success |
| 1833 | Original trailing blank line; absent after relocation |

### 6.8.3 System Library Opening, Resolution, and Closing

Windows/Cygwin uses an `HMODULE libGL` and a typed pointer to `wglGetProcAddress`. The preprocessor manages `APIENTRY` and includes `<windows.h>`. MSVC SDK detection conditionally includes `winapifamily.h`; a non-desktop app partition defines `IS_UWP`. On desktop, `open_gl` calls `LoadLibraryW(L"opengl32.dll")`, obtains `wglGetProcAddress` using `GetProcAddress`, and succeeds only if both the library and resolver are available. UWP's disabled desktop opening path returns zero. `close_gl` calls `FreeLibrary` if non-null and clears `libGL`.

Non-Windows uses `<dlfcn.h>`, a `void* libGL`, and on non-Apple/non-Haiku platforms a `glXGetProcAddressARB` pointer. Apple attempts these paths in exact order:

1. `../Frameworks/OpenGL.framework/OpenGL`
2. `/Library/Frameworks/OpenGL.framework/OpenGL`
3. `/System/Library/Frameworks/OpenGL.framework/OpenGL`
4. `/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL`

Other non-Windows platforms attempt `libGL.so.1`, then `libGL.so`. Each is opened with `RTLD_NOW | RTLD_GLOBAL`. Apple/Haiku return success as soon as a library opens; other platforms require `dlsym(libGL, "glXGetProcAddressARB")` to succeed. `close_gl` uses `dlclose` and clears the pointer. The loop returns immediately after the first opened library's resolver check; an opened library without the required resolver is not automatically followed by testing the next name. The failure path's exact cleanup is worth reading before embedding this generated direct-library helper in another host.

`get_proc(const char* namez)` initializes result null, rejects null `libGL`, tries the platform context resolver where compiled, then falls back to Windows `GetProcAddress` or POSIX `dlsym` only when the first result is null. It returns the raw procedure address. The supplied Windows helper does not explicitly reject the unusual non-null sentinel values some `wglGetProcAddress` implementations can return. Arrakis relies on GLFW's resolver instead.

`gladLoadGL()` calls `open_gl`; on success calls `gladLoadGLLoader(&get_proc)`, then `close_gl`; returns the loader status. It does not create an OpenGL context. Resolution remains context dependent, and closing the helper's temporary module handle should not be interpreted as destroying GLFW's context/driver lifetime. The application must keep the context alive for graphics calls and must not assume pointer validity after context/provider changes.

### 6.8.4 Function Pointer Storage and Assignment Semantics

Every global declaration has the form `PFNGL...PROC glad_glName = NULL;`. The typedef from `glad.h` encodes the driver function's return type, parameters and platform calling convention. The header normally aliases familiar `glName` spellings to these globals. The declaration does not implement that OpenGL operation; it reserves a call target.

Every generated loading assignment has the form `glad_glName = (PFNGL...PROC)load("glName")`. Its one parameter is `GLADloadproc load`, the caller's resolver taking a symbol name. Its only branch is the version guard `if (!GLAD_GL_VERSION_M_N) return;`, followed by a fixed sequence of lookups. There is no per-assignment validation, wrapper around arguments, shader compilation, mathematical rendering work, GPU object allocation, or error checking here. Two appearances of a symbol in successive version blocks mean it can be assigned again, not that two GPU implementations are compiled.

The globals are process-global and this loader is not a multi-context dispatch table with automatic context switching. Reloading for a new context updates shared targets. Version success alone is not a proof that every pointer needed by the game is non-null. Unloaded/unsupported pointers must not be called.

### 6.8.5 Every Generated Core-Version Loading Family

These ranges are exact boundaries of the generated functions, not a generic list of OpenGL release headlines. Every assignment in a range resolves a driver function; the described operation is performed **later by the driver when called**, not by GLAD.

| Loader and lines | Loaded functional families and meaning |
| --- | --- |
| `load_GL_VERSION_1_0`, 976-1026 | Rasterization state (culling/front face/hints/line/point/polygon/scissor), texture parameters and 1D/2D images, draw/read buffers, clear values/masks, enable/disable, finish/flush, blending/logic/stencil/depth, pixel packing/readback, scalar/vector state queries, errors/string queries, texture inspection, depth range and viewport. This generated core subset does not supply classic immediate-mode `glBegin` rendering. |
| `load_GL_VERSION_1_1`, 1027-1042 | Array/indexed draws, polygon offset, texture copy/subimage updates, bind/generate/delete/is-texture. These provide object-oriented texture lifetime and bulk geometry submission. |
| `load_GL_VERSION_1_2`, 1043-1049 | Draw-range indexed draw and 3D texture image/subimage/copy operations. |
| `load_GL_VERSION_1_3`, 1050-1061 | Active texture unit, sample coverage, compressed texture image/subimage/get for 1D/2D/3D. Texture-unit selection is separate from binding/creating texture objects. |
| `load_GL_VERSION_1_4`, 1062-1073 | Separate blend factors, multidraw arrays/elements, float/integer point parameters, blend color/equation. Multidraw combines submission descriptions; GLAD merely resolves the entry. |
| `load_GL_VERSION_1_5`, 1074-1095 | Query object create/delete/begin/end/results and buffer object bind/create/delete/data/subdata/readback/map/unmap/parameter/pointer queries. Buffer data calls later allocate/update GPU-visible storage. |
| `load_GL_VERSION_2_0`, 1096-1191 | Separate blend equations, multiple render targets, per-face stencil; shader/program create/source/compile/attach/link/validate/use/delete/reflection/logs; float/integer scalar/vector and square-matrix uniforms; vertex attribute values/pointers/enabling/query. Shader failure handling belongs to application calls and logs, not loader initialization. |
| `load_GL_VERSION_2_1`, 1192-1200 | Six rectangular float uniform matrix forms: 2x3, 3x2, 2x4, 4x2, 3x4, 4x3. |
| `load_GL_VERSION_3_0`, 1201-1287 | Indexed state/masks, transform feedback and buffer-base/range binding, color clamp, conditional rendering, integer vertex attributes, unsigned uniforms, fragment-output binding, integer texture parameters, typed clears, indexed strings, renderbuffers/framebuffers and attachments/status/blit/multisample/layer, range mapping/flush and vertex array objects. This contains Arrakis's framebuffer/VAO-era infrastructure. |
| `load_GL_VERSION_3_1`, 1288-1305 | Instanced draws, buffer textures, primitive restart, buffer copies, uniform index/block reflection/binding and repeated base/range/indexed integer lookups. Uniform blocks share packed state across draw programs. |
| `load_GL_VERSION_3_2`, 1306-1327 | Base-vertex indexed draw variants, provoking vertex, sync object fence/waits/delete/query, 64-bit state/buffer queries, layered framebuffer texture, multisample textures and sample masks. Sync functions govern CPU/GPU completion, not audio thread synchronization. |
| `load_GL_VERSION_3_3`, 1328-1388 | Indexed fragment outputs, sampler-object lifetime/parameters, timestamp/query-counter and 64-bit results, vertex attribute divisors for instancing, packed vertex/texture/normal/color attribute variants. The supplied file contains some packed legacy-named targets despite its core provenance; presence is not a guarantee they are valid in every core context. |
| `load_GL_VERSION_4_0`, 1389-1437 | Sample shading, per-draw-buffer blending, indirect draws, double scalar/vector/matrix uniforms and queries, shader subroutine reflection/selection, tessellation patch parameters, transform feedback objects/stream draws, indexed queries. |
| `load_GL_VERSION_4_1`, 1438-1529 | Shader/compiler binary/precision controls, float depth range/clear, program binaries/parameters, separate program pipelines/stage selection/validation/logs, all direct program-uniform forms (float/int/uint/double and matrices), double vertex attributes/pointers, viewport/scissor/depth-range arrays/indexed forms and indexed float/double queries. |
| `load_GL_VERSION_4_2`, 1530-1544 | Base-instance instanced draws, internal-format queries, atomic counter buffer reflection, image texture binding, memory barriers, immutable texture storage, instanced transform feedback/stream drawing. Barriers control visibility of GPU memory operations. |
| `load_GL_VERSION_4_3`, 1545-1591 | Clear buffer data/subdata, compute dispatch/direct-indirect, image copies, framebuffer parameter configuration, 64-bit format queries, invalidate texture/buffer/framebuffer storage, multidraw indirect, program-interface/resource reflection and shader storage binding, ranged buffer textures, multisample storage, texture views, separate vertex-binding/format APIs and debug message/group/object-label APIs. |
| `load_GL_VERSION_4_4`, 1592-1603 | Immutable/persistent-capable buffer storage, texture clears, multibind buffer base/range, textures, samplers, images and vertex buffers. Capability flags supplied by later callers decide persistence/coherency, not loader assignment. |
| `load_GL_VERSION_4_5`, 1604-1728 | Clip control; direct-state-access transform feedback/buffer/framebuffer/renderbuffer/texture/vertex-array creation and named operations; texture unit/image/storage/subimage/mipmap/readback APIs; named object/map/query APIs; sampler/pipeline/query creation; query-buffer results; regional barriers; robustness reset/bounded readback/uniform calls, several legacy-named bounded calls, texture barrier. Direct-state access operates on explicitly named objects rather than implicit bind state. |
| `load_GL_VERSION_4_6`, 1729-1735 | Exactly four resolutions: `glSpecializeShader`, `glMultiDrawArraysIndirectCount`, `glMultiDrawElementsIndirectCount`, `glPolygonOffsetClamp`. Shader specialization supplies SPIR-V entry/specialization data to the driver; indirect-count draws read draw counts; polygon offset clamp adds a bound. |

GLAD includes versions through 4.6 so the application can resolve newer operations where available. That does not mean Arrakis requires or requests a 4.6 context, or that it uses compute, tessellation, SPIR-V, persistent mapping and every other loaded family. Shader language versions, requested GLFW context and actual driver version are separate facts.

### 6.8.6 Extension Storage, Matching, and Empty Selection

`GLVersion` starts `{0,0}`. `max_loaded_major/minor`, `exts`, `num_exts_i`, and `exts_i` hold discovery state. `_GLAD_IS_SOME_NEW_VERSION` is compiled when suitable GL/ES 3.0 declarations exist.

`get_exts` takes the legacy path for loaded major below 3: `glGetString(GL_EXTENSIONS)` supplies one driver-owned space-separated string. Otherwise it obtains `GL_NUM_EXTENSIONS`, allocates a `char**` array, queries each `glGetStringi(GL_EXTENSIONS,index)`, allocates `len+1` bytes and copies a null-terminated name. Those copied strings are GLAD-owned; the legacy string is not. Array allocation failure returns zero. Individual string allocation failures leave a null element. The source also returns failure when a modern context reports zero extensions and `exts_i` remains null; this is a generated-loader edge case, not proof the GL context is absent.

`free_exts` frees every copied element (null is safe), frees the array, and clears `exts_i`. `has_ext(ext)` uses `strstr` plus boundaries in legacy mode: a match must begin at the string start or after a space and end at space or terminator. This avoids accepting `GL_FOO` inside `GL_FOOBAR`. Modern mode iterates copied names with exact `strcmp`, skipping null entries. Null requested/legacy strings reject membership.

`find_extensionsGL` calls `get_exts`, returns zero on failure, references `has_ext` only to silence unused-function concerns, frees extensions, then returns one. **No selected extension flags and no extension loader blocks are generated** because the selected extension list was empty. The code enumerates extensions but does not expose a complete extension registry to Arrakis. There is therefore no missing list of hundreds of extension-loading functions to explain in this particular file; they are genuinely not present.

### 6.8.7 Version Parsing and Public Loader Success

`find_coreGL` obtains `glGetString(GL_VERSION)`; null returns immediately. It strips the first matching prefix among `"OpenGL ES-CM "`, `"OpenGL ES-CL "`, and `"OpenGL ES "`, then scans `%d.%d` using `sscanf_s` on MSVC or `sscanf` otherwise. Major/minor are stored in `GLVersion` and maximum-loaded state. The source does not check the scan's return count before using locals; valid driver version strings are assumed.

For every supported version M.N, the exact flag test is `(major == M && minor >= N) || major > M`. Thus a 3.3 context sets flags 1.0 through 3.3 true and 4.x false. A reported version above 4.6 is capped in **max-loaded discovery state** to 4.6, while `GLVersion` records the reported version. That cap reflects generator coverage, not a request to downgrade a context.

`gladLoadGLLoader(load)` resets `GLVersion`, resolves `glGetString` first, fails if that pointer is null or its GL_VERSION query is null, calls version discovery, invokes all nineteen loaders in ascending version order (each self-guards), invokes extension discovery, and returns `GLVersion.major != 0 || GLVersion.minor != 0`. It does not validate the resolver pointer before its first call, confirm every required graphics symbol, compile shaders, or check framebuffer completeness. On a failed/repeated load, not every old global target is proactively cleared; callers should not keep using stale state just because some pointers remain non-null.

The game checks the returned boolean and terminates graphics setup on failure. A current context must exist before loading. A successful 4.6-capable loader build running against a 3.3 context is normal: unsupported later blocks simply do nothing. The renderer must limit calls to the supported/requested API subset.

## 6.9 Historical Vorbis Study: Removed, Never Integrated

### 6.9.1 Complete Source Coverage Map

These are historical original `stb_vorbis.c` line numbers recorded from inspection before cleanup. The unused 5,584-line file has been removed; it is not remaining project code, not compiled, and not part of the current-source appendix. This optional decoder study explains the previously supplied file and the Ogg integration requirements. Restoring it would be a future codec feature, not a missing requirement of the current WAV/MP3 game.

| Lines | Contents and purpose |
| --- | --- |
| 1-68 | Decoder version 1.22, authors/history, explicit codec/seek limitations |
| 69-171 | Header guard/linkage, allocator/info/comment structs, shared handle/query/close APIs and thread-safety rules |
| 172-246 | Pushdata declarations and detailed consumed-byte/output/flush contracts |
| 247-371 | Pull/file/memory/seek/length/frame/sample/whole-file declarations, buffer sizing and channel coercion |
| 372-419 | Error enum, header closure, implementation boundary |
| 420-662 | Compile configuration, defaults, includes, endian/inlining/alloca/CRT portability and implementation setup |
| 663-937 | Internal codebook/floor/residue/mapping/mode/page/decoder-state types, packet/sample/buffer state, error/allocation macro groundwork |
| 938-1232 | Block arrays and permanent/temporary allocator helpers, CRC32, bit reversal/integer log/float unpacking, canonical/sorted/accelerated Huffman construction |
| 1233-1336 | Header validation, vector-quantization lookup size, twiddle/window/bit-reverse tables, blocksize setup, floor-neighbor/sorting helpers |
| 1337-1633 | File/memory byte access/offsets, Ogg capture/page parsing, packet/lacing/segment handling and bit accumulator |
| 1634-1934 | Huffman scalar fast/slow decoding and vector/codebook expansion into output/residue arrays |
| 1935-2082 | Floor prediction and inverse-dB lookup table, line rasterization into spectral gains |
| 2083-2407 | Residue types and partition/codebook decoding, spectral vectors, reference/conditional transform groundwork |
| 2408-3058 | Optimized inverse-MDCT stages, butterflies, rotations, permutations and transform reconstruction |
| 3059-3518 | Window lookup, floor application, packet-mode/short-long block decoding, channel coupling inversion, residue/floor combination, inverse transforms, overlap/add and initial-frame priming |
| 3519-4207 | Push whole-packet availability and complete decoder-start/setup parsing: identification/comments/codebooks/time/floor/residue/mapping/modes, validation/allocation |
| 4208-4340 | Deep state cleanup, close/init/allocate and info/comment/error/sample-offset queries |
| 4341-4557 | Push flush, CRC page resynchronization, push-frame decoding/open and file-offset query |
| 4558-5027 | Pull seeking: Ogg page search/probes, coarse/binary-guided location, frame/sample-exact refinement, seek start and length queries |
| 5028-5120 | Pull float-frame decode and file-section/file/filename/memory opening |
| 5121-5425 | Channel assignment/conversion tables, float-to-short clipping/mixing, frame/sample short APIs and whole-file filename/memory decode |
| 5426-5479 | Caller-buffer float sample/interleaved APIs, buffered remainder handling, pull implementation closure |
| 5480-5543 | Detailed historical changes and implementation guard closure |
| 5544-5584 | MIT/public-domain alternative license text |

### 6.9.2 Public Structures, Modes, and Ownership

`stb_vorbis*` is an opaque decoder handle. Unlike miniaudio's transparent outer engine structs, application code does not allocate its internal decoder struct by ordinary public field initialization. `stb_vorbis_alloc` contains `char* alloc_buffer` and `int alloc_buffer_length_in_bytes`; a caller-provided block must remain alive until close and be large enough for setup and temporary decode memory. Without it, startup uses heap allocation and frame temporaries commonly use stack `alloca`.

`stb_vorbis_info` reports sample rate, channel count, permanent setup memory, setup temporary memory, frame temporary memory and maximum frame size. `stb_vorbis_comment` reports a vendor string, comment-list length and borrowed comment pointers. `get_info` copies scalar information; comments and frame outputs point into decoder-owned storage. `get_error` obtains and clears the last stored error. `close` releases owned internal allocations and, when configured, the opened file. An individual handle is not safe for simultaneous decode calls on multiple threads; separate handles can operate independently.

Push mode accepts successive encoded-byte windows. `open_pushdata` parses complete initial headers or returns null with an error such as `VORBIS_need_more_data`; on success it reports bytes consumed. `decode_frame_pushdata` reports consumed **encoded bytes** separately from output **samples per channel**, and returns planar `float**` output through a `float***` output parameter. Zero consumed/zero output means more bytes are needed; consumed bytes with zero samples can mean resynchronization or initial-frame discard; consumed bytes with samples means a decoded frame. The first audio frame after open is discarded as part of Vorbis overlap priming. Internal output is transient and overwritten by later decoding; do not free or modify it.

`flush_pushdata` says the next encoded block is noncontiguous and resets the scanner/overlap continuity so the decoder searches for a valid page. The caller owns buffering and seeks its own source; flush alone does not select an exact PCM frame. Pull mode instead owns traversal of a complete memory buffer or a `FILE*` section. Open-file parameters specify close-on-free behavior and optional section length; external repositioning of an active owned stream confuses the decoder.

Frame APIs return a codec-frame-sized planar/interleaved output; sample APIs fill caller buffers across frame boundaries and cache unused samples. Interleaved capacity arguments are counts of scalar floats/shorts, while return values are samples **per channel**. Whole-file short decoders allocate a new interleaved buffer and return sample count or failure; the caller frees that buffer. Float sample APIs and integer coercion APIs do not all apply identical channel-mixing rules.

### 6.9.3 Container, Entropy Decode, Floor, Residue, and Mapping

Ogg is the container. Page parsing recognizes the capture pattern, version/flags, granule position, stream serial/sequence, checksum and lacing/segment table. A lacing byte of 255 continues a packet; packet boundaries can span pages. Byte/bit readers distinguish encoded EOF from an incomplete push buffer. CRC32 tables/updates validate candidate pages during push resynchronization, where several apparent headers may overlap in corrupted/unrelated data.

Vorbis setup packets define codebooks, floor, residue, mapping and modes. Codebooks contain symbol code lengths, Huffman decode tables and optional vector-quantization lookup values. Initialization computes canonical codes with Vorbis bit ordering, a short-code accelerated lookup, and sorted slow-path tables. Scalar decode consumes a codeword to produce a symbol; vector decode converts a symbol/index into values added or interleaved into spectral residue. Fast lookup is an optimization of entropy decoding, not an alternate audio codec.

The supported floor-1 algorithm decodes a sparse spectral envelope. Neighbor prediction reconstructs control-point heights; integer line drawing and an inverse-dB table build frequency-bin gains. Residue decodes the remaining spectral detail with partition classes/codebooks and type-specific arrangement. Mapping selects submaps and channel coupling; reverse coupling reconstructs channel spectral coefficients. Applying the floor to residue yields the spectrum supplied to the inverse transform.

Header setup validates dimensions/counts/types before allocating permanent tables and working buffers. It rejects unsupported floor 0 and invalid mappings/types rather than treating them as a supported generalized format. Comments/vendor strings are metadata, not synthesis data. The code has many bounds/overflow/truncation checks, but reading them for this chapter is not an independent claim of vulnerability freedom.

### 6.9.4 Inverse MDCT, Windowing, and Overlap

Compressed Vorbis packets reconstruct frequency-domain coefficients. `inverse_mdct` converts a block's spectrum into time-domain audio using precomputed trigonometric twiddles, staged butterfly/rotation loops, bit-reversed ordering and final rearrangement. The reference/conditional transform paths and optimized helper loops compute the same codec transform; they are not unrelated game-specific math.

Short and long blocks use different window regions, selected by packet mode and previous/next-window flags. The Vorbis window shape is built using the codec's nested-sine taper, conceptually `sin((pi/2) * sin(pi*(i+0.5)/n)^2)` over the rising half. Windowed adjacent blocks overlap/add to avoid discontinuities while permitting long-block compression and short-block transient response. `vorbis_finish_frame` combines retained previous-window samples with the current block, establishes valid output bounds, updates sample location and retains the next overlap. The first packet primes history; the final granule position can trim valid final output.

The decoder therefore requires retained state across frames. Reopening or seeking may need preroll/neighbor-frame decoding; simply inverse-transforming a random packet does not produce sample-exact continuous output. The declared absolute maximum frame output is 4096 samples per channel under the Vorbis I block-size constraints, but callers should use reported info/capacities and the actual returned count.

### 6.9.5 Push Resynchronization, Pull Seeking, and Conversion

Push resynchronization scans for candidate Ogg pages, incrementally checks CRC and tracks a limited number of overlapping candidates. Once it finds a valid stream/page/frame boundary it restores continuity and sample-position knowledge where possible. Consumed bytes can be less than the supplied block so a capture pattern split across calls is not lost. More input may be required even when some garbage was consumed.

Pull seeking searches/probes pages using byte position and granule/sample information to narrow an interval, then decodes/refines around the target. `seek_frame` permits a returned frame that contains the target; `seek` positions buffered sample output so sample APIs begin exactly at the requested sample; `seek_start` resets to the beginning. Length queries inspect stream/page information and return sample or seconds forms. The source documents 32-bit sample-position limits despite Ogg supporting 64-bit granule positions.

Integer conversion scales/clamps float values to signed short output and implements simple channel coercion. Requested mono can sum all source channels; requested stereo combines left/center and right/center assignments; requesting more than present fills extra channels with zero; other cases select available channels. The vendor explicitly warns that this is not high-quality general surround mixing. Buffer-based float readers manage channel copying and cached frame remainders separately, and should not be assumed to follow all short-output coercion rules.

### 6.9.6 Compile Switches, Exact Defaults, and Explicit Limits

| Switch or default | Effect |
| --- | --- |
| `STB_VORBIS_HEADER_ONLY` | Emits declarations/header section only; used when implementations are compiled separately. |
| `STB_VORBIS_NO_PUSHDATA_API` | Removes push-data APIs and supporting scanner behavior. |
| `STB_VORBIS_NO_PULLDATA_API` | Removes pull APIs and implies no stdio/integer conversion. |
| `STB_VORBIS_NO_STDIO` | Removes file APIs; memory/push paths can remain. |
| `STB_VORBIS_NO_INTEGER_CONVERSION` | Removes short conversion/whole-file short APIs while float decode can remain. |
| `STB_VORBIS_NO_CRT` | Changes allocator/CRT assumptions and implies no stdio; caller-provided memory becomes essential where malloc is unavailable. |
| `STB_VORBIS_MAX_CHANNELS = 16` | Default compile-time decoder channel cap; container field's theoretical 255 limit does not override this build default. |
| `STB_VORBIS_PUSHDATA_CRC_COUNT = 4` | Four overlapping page candidates during push resynchronization. |
| `STB_VORBIS_FAST_HUFFMAN_LENGTH = 10` | Fast lookup uses a 10-bit prefix, nominally 1024 table positions; larger permitted values trade memory/cache behavior for fewer slow decodes. |
| `STB_VORBIS_FAST_HUFFMAN_INT` / default short table | Changes accelerated table entry width; default chooses `STB_VORBIS_FAST_HUFFMAN_SHORT`. |
| `STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH` | Uses smaller/slower fallback lookup choices instead of the usual sorted-code search where applicable. |
| `STB_VORBIS_DIVIDES_IN_RESIDUE`, `STB_VORBIS_DIVIDES_IN_CODEBOOK` | Trade precomputed storage for runtime divide/index reconstruction. |
| `STB_VORBIS_DIVIDE_TABLE` | Optional floor integer-division lookup tradeoff; documented as disabled by default for small benefit. |
| `STB_VORBIS_NO_INLINE_DECODE` | Disables scalar fast-decoder inlining; useful for size/debugging tradeoffs. |
| `STB_VORBIS_NO_DEFER_FLOOR` | Synthesizes floor earlier rather than deferring, changing working-memory/processing behavior. |
| `STB_VORBIS_NO_FAST_SCALED_FLOAT`, `STB_VORBIS_BIG_ENDIAN` | Select safe/fast float-to-int conversion and endian handling. |
| `STB_VORBIS_CODEBOOK_SHORTS` | Explicitly rejected with `#error` because prior behavior could produce incorrect results. |

The top-of-file limitations are literal: no floor 0 (older pre-2004 streams), ignored lossless sample truncation at the beginning, no concatenated multiple Vorbis streams, and 32-bit sample positions limiting high-rate long seeks (approximately six hours at 192 kHz). It is a Vorbis decoder, not an encoder, Opus decoder, playback backend, resampler, game mixer or replacement for miniaudio's engine. None of these compile switches are currently selected by the game because the file is not part of its compiled path.

## 6.10 API Reference and Annotated-Appendix Strategy

### 6.10.1 Index All Code Without Replacing Explanation with Symbol Lists

The prose above explains behavior by family; the exact source appendix supplies the full declarations, constants, generated assignments, tables and branches. A maintainable API reference should index a symbol by **source path, original line, family, declaration, definition, conditional guards, ownership, callback/thread restrictions, parameters and result semantics**. A bare alphabetical dump is helpful for lookup but cannot explain interactions such as a sound holding a resource-manager source or a graph detach waiting for callback readers.

For miniaudio, search `MA_API` declarations in the public region and matching definitions in the implementation. Many symbols occur twice; retaining both locations avoids mistaking a prototype for an implementation. Also index internal `static` helpers by family, structs/enums/macros and embedded `ma_dr_*` functions. Preprocessor guards should be recorded because source inclusion, compile inclusion and runtime selection are three different things. For GLAD, pair each `glad_gl*` global declaration with every `load("glName")` assignment and its enclosing core-version guard; the actual OpenGL signature is in the external `glad/glad.h`. For stb_vorbis, public declarations and matching implementation definitions plus static decode helpers need separate entries.

### 6.10.2 Family-Level API Lookup Index

| Symbol prefix / group | Public/source entry region | Why consult it |
| --- | --- | --- |
| `ma_version*`, `ma_result_description`, allocation helpers | 4508; 6112-6182; definitions 12298, 58926 onward | Version/diagnostics, allocator/free matching and PCM byte sizing |
| `ma_log_*` | 4516-4590; definitions around 13572 | Callback logging and registration lifetime |
| `ma_biquad_*`, `ma_lpf*`, `ma_hpf*`, `ma_bpf*`, `ma_notch*`, `ma_peak*`, `ma_loshelf*`, `ma_hishelf*` | 4591-5003; 47164-50374 | Filter coefficients/configs, per-channel state, reinit/processing/preallocation |
| `ma_delay_*`, `ma_gainer_*`, `ma_panner_*`, `ma_fader_*`, `ma_spatializer*` | 5004-5317; 50375-53026 | Time/gain/pan/spatial behavior and channel-state memory |
| `ma_linear_resampler_*`, `ma_resampler_*` | 5331-5523; 53027-54276 | Ratio/rate, custom vtables, latency and consumed/produced frame counts |
| `ma_channel_converter_*`, `ma_data_converter_*`, `ma_convert_*`, channel-map helpers | 5524-5830; 54277-58096 | Format/rate/channel composition and buffer capacities |
| `ma_data_source_*`, `ma_audio_buffer*`, `ma_paged_audio_buffer*` | 5831-6025; 59138 onward | Borrowed/owned data, cursor/seek/loop/range and paging lifetime |
| `ma_rb_*`, `ma_pcm_rb_*`, duplex buffer | 6026-6111; 58097-58922 | Producer/consumer buffering and acquire/commit rules |
| `ma_spinlock_*`, `ma_mutex_*`, `ma_event_*`, `ma_semaphore_*`, `ma_fence_*`, notification APIs | 6183-6335; 17544 onward | Cross-thread synchronization and callback-safety requirements |
| `ma_slot_allocator_*`, `ma_job_*`, `ma_job_queue_*` | 6336-6593; 18579-19470 | Bounded work storage, worker dispatch and shutdown |
| `ma_context_*`, `ma_device_*`, backend IDs/configs | 6594-9751; 19471-44574 | Device enumeration/init/start/stop/native negotiation and platform guards |
| PCM copy/silence/clip/volume/mix/interleave helpers | 9752-9865; 44575-47163 | Samples/frames, signed silence, clipping and conversions |
| `ma_vfs_*`, `ma_default_vfs_*` | 9866-9944; 60904-61933 | Custom/default file streams and file ownership |
| `ma_decoder_*`, `ma_decoding_backend_*` | 9945-10110; 62840-67830 | Codec probing/output conversion, custom decoding, read/seek/length |
| `ma_encoder_*` | 10111-10169; 67831-68147 | WAV write/finalization contract |
| `ma_waveform_*`, `ma_pulsewave_*`, `ma_noise_*` | 10170-10296; 68148-69308 | Infinite/generated data sources, phase/random state |
| `ma_resource_manager_*` | 10297-10614; 69309-73426 | Encoded/decoded cache, stream pages, worker jobs/refcounts |
| `ma_node_*`, `ma_node_graph_*`, source/splitter/filter/delay nodes | 10615-11107; 73427-76536 | Graph pull/attachment/state/bus contracts |
| `ma_engine_*`, `ma_engine_node_*`, `ma_sound_*`, `ma_sound_group_*` | 11108-11535; 76537-79893 | High-level playback, listener, pitch/gain/event/lifetime APIs used by Arrakis |
| `ma_dr_wav_*` | 61934-62368; 79908-84837 | WAV container metadata, PCM/ADPCM reads, conversion/write helpers |
| `ma_dr_flac_*` | 62369-62662; 84838-92696 | Lossless subframes/residuals/metadata/channel decorrelation/seek |
| `ma_dr_mp3*` | 62663-62839; 92697-95804 | MPEG decode/reservoir/synthesis and MP3 wrapper APIs |
| `ma_stbvorbis_*` | 65330-66159, conditional | Optional callback/push/file/memory Vorbis bridge |
| `gladLoadGL`, `gladLoadGLLoader`, version/ext helpers | GLAD 27-257, 1736-1832 | Loading success/context/platform/extension behavior |
| `glad_gl*`, `load_GL_VERSION_*` | GLAD 277-1735 | Every resolved graphics symbol and version guard |
| Historical `stb_vorbis_*` and decode helpers | Removed source, former lines 69-5479 | Historical codec study only; absent from current code/appendix/build |

### 6.10.3 Auditable Limits and Practical Recommendations

The source listings and line maps establish coverage of the supplied files, including unused vendor branches. They do not establish audible playback on every machine, validity of every sound's binary contents, support for absent optional assets, correctness of all native API drivers, or a separate explanation of every generated graphics signature supplied by the external GLAD header. External GLAD/GLFW/GLM headers and actual driver/backend libraries are not reproduced by pretending they are contained in these four vendor files.

For verification, keep three results separate: engine initialization, per-file `[AUDIO] Loaded` messages, and actual audible/spatial behavior. Useful checks include running with no device; removing/renaming an optional primary asset in a controlled test; launching from a supported and unsupported working directory; comparing camera movement with fixed source position; muting across rescue/breach edges; restarting a mission during active one-shots; and observing boost gain/pitch thresholds. Those are recommended tests, not tests claimed to have been executed for a documentation-only chapter.

If Ogg fallbacks are retained as an advertised feature, integrate and test an actual Vorbis backend, or replace/remove those fallback expectations. The organized package currently retains the eight WAV/MP3 primaries and removed the unused stb source; it is not needed in the executable. Archive/library-license handling is separate from compilation necessity. Keep the current line-numbered appendix with its vendor version identifiers so future library updates do not silently invalidate this chapter's references. Historical references here are not a promise that removed files remain available.
