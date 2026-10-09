# ARRAKIS / Harvester Down

## Play the Rescue Game

A sandworm is pursuing a moving spice harvester across a winding dune escape
route. Every seven seconds the harvester releases another group of one to four
workers, starting six seconds into the mission. They run clear, then wait at that
fixed drop-off location, marked by an amber beacon. Rescue as many of the 36 crew
as possible, eight at a time, and deliver them to the cyan landing pad. Only
delivered survivors count toward your score.

The first mission gives you 210 seconds (3:30) before the worm catches the
harvester. Later missions shorten this by ten seconds, down to a 150-second
minimum. The surfaced worm and its sand wake move closer throughout the pursuit;
the HUD shows its closing distance and the radar tracks every waiting group.

- `Enter`: start or advance to the next mission.
- Mouse hover: steer without clicking. Hover right or left of center to turn the
  ornithopter and camera together. The center brackets mark a neutral dead zone;
  move the cursor back inside to stop turning. Farther from center turns faster.
- `W / S`: fly forward / backward along the current heading.
- `A / D`: strafe left / right without changing which way the aircraft faces.
- `Q / E`: raise / lower terrain-following flight altitude.
- `Space`: brake, descend to rescue altitude, and winch nearby workers. Hold it
  over the cyan pad to unload. Pickup requires low, steady flight.
- `Left Shift`: rechargeable boost; release it to recharge.
- `C`: cycle wide, close, and tactical cameras.
- Hover above / below center to change camera elevation. Left / right arrows
  provide keyboard steering; up / down arrows adjust camera elevation.
- `P`: pause / resume. Losing window focus also pauses the mission.
- `R`: retry the current mission. `F11`: fullscreen. `Esc`: exit.

All camera modes rotate with the aircraft, keeping its nose forward on screen.
The cursor stays free; moving it out of the window stops mouse steering. Starting,
retrying, resizing, switching fullscreen, or resuming centers the cursor to avoid
an accidental turn. Losing focus pauses flight and clears held inputs.

The worm rises, bends its circular tooth-lined mouth toward the harvester,
swallows it and nearby stranded crew, then disappears into a collapsing sand
basin. Workers farther away remain available for rescue during a 60-second final
extraction window. Approaching the moving worm too closely is dangerous for both
the aircraft and crew left behind. Its procedural shape follows `sandworm.jpg`;
the image is a visual reference, not a flat sprite or a texture pasted onto a
cylinder. The cloud-free sky blends golden horizon haze into blue, with a visible
warm sun lighting the dunes.
The best delivered score is retained for the current application session.

## Build and Run

```powershell
cmake -S . -B build/rescue -G "Visual Studio 17 2022" -A x64
cmake --build build/rescue --config Release
.\build\rescue\Release\Arrakis.exe
```

Requires C++17, GLFW, GLAD, GLM, and OpenGL 3.3. The existing `E:/glfw_nec` and
`E:/glm` installs are discovery hints. Override `GLAD_INCLUDE_DIR`,
`GLFW_INCLUDE_DIR`, `GLM_INCLUDE_DIR`, and `GLFW_LIBRARY` through CMake for other
installations. The Visual Studio project also includes the new game headers.

```powershell
ctest --test-dir build/rescue -C Release --output-on-failure
.\build\rescue\Release\Arrakis.exe --smoke-test
.\build\rescue\Release\Arrakis.exe --smoke-breach
.\build\rescue\Release\Arrakis.exe --smoke-mouse
.\build\rescue\Release\Arrakis.exe --smoke-pursuit
```

The first command runs deterministic headless rescue-logic tests. The smoke
tests render 120 frames, check OpenGL errors, and exit; an optional second
argument saves a BMP capture. They require an OpenGL-capable desktop.
The mouse smoke test turns both directions while flying, cycles all three camera
modes, and verifies the camera keeps the aircraft facing forward every frame.
The pursuit smoke test renders the moving evacuation at mid-mission with both
the fleeing harvester and approaching worm in view.

## Original Renderer Reference

The following material describes the earlier free-flight renderer. The game
controls, build instructions, and behavior above supersede the older sections.

# Original Cinematic Dynamic Dune Simulation
### CSE 4102: Computer Graphics and Image Processing Laboratory Project

---

## 🌟 Project Overview

**ARRAKIS** is a high-fidelity, real-time 3D interactive graphics simulation inspired by the *Dune* universe (2021 / 2024 films). Built completely from scratch in modern **OpenGL 3.3 Core Profile** (C++17), the project delivers a game-grade cinematic desert experience.

**CRITICAL SPECIFICATION**: Every single 3D object in the world is generated **procedurally from mathematical primitive shapes** (cubes, cylinders, UV spheres, aerodynamic aerofoil blades, and continuous heightfield grids). **NO external `.obj` or 3D model files are loaded.**

### Core Highlights:
- 🚁 **Movie-Accurate Atreides Ornithopter**: 8 tandem dragonfly aerofoil wings with high-frequency anti-phase flutter, faceted stealth cockpit canopy with tinted armored glass, twin turbine air intakes, afterburner reheat flame nozzles, and realistic aerodynamic flight banking.
- 🏭 **Massive Industrial Spice Harvester**: Quad crawler tread pontoon units with road wheels, forward rotating harvesting cutter drum with crusher teeth and glowing cinnamon suction scoop, multi-tiered refinery deck, bridge floodlights, and exhaust funnels venting burning spice fumes.
- 🛢️ **Movie-Realistic Spice Pressure Tanks**: Heavy-duty cylindrical pressure vessels with dished hemispherical heads, diagonal structural transport cradles with ISO lifting eyelets, high-pressure relief valves, and an illuminated vertical Melange spice level gauge that pulses with rich orange radiance.
- 🏜️ **Seamless Rolling Dune Landscape**: Continuous 140×140 vertex heightfield terrain spanning 700 units, featuring large barchan dune swells, sharp windward/leeward crests, procedural wind ripple micro-shading, and analytical normal calculations.
- 💨 **Atmospheric Wind & Blowing Sand Particle System**: Over 2,400 dynamic sand particles streaming across the dunes with wind gusts, active crawler dust plumes behind the harvester, and wing downwash dust dispersion underneath the ornithopter.
- ☀️ **Game-Grade Lighting & Shading Pipeline**:
  - Blinn-Phong specular highlights + Half-Lambert wrap diffuse for powdery sand diffusion.
  - Directional blinding Arrakis sun with sun disk glow and atmospheric corona.
  - High-resolution (2048×2048) soft PCF shadow mapping with slope-scale bias.
  - Exponential squared desert dust distance fog (`#D89447`) smoothly blending distant dunes.
- 🎬 **Dynamic Cinematic Chase Camera**: Spring-damped third-person camera following the ornithopter with inertia, banking reaction, and multiple view modes (Cinematic Chase, Cockpit, Overhead Tactical).

---

## 🎮 Flight & Scene Controls

| Key | Function |
|---|---|
| `W` | Accelerate / Thrust Forward |
| `S` | Decelerate / Reverse Thrust |
| `A` | Bank & Turn Left (aerodynamic roll) |
| `D` | Bank & Turn Right (aerodynamic roll) |
| `Q` | Ascend / Gain Altitude (nose tilts up) |
| `E` | Descend / Lose Altitude (nose tilts down) |
| `Left Shift` | **Afterburner Boost** (high-speed cruise + extended jet flame) |
| `Space` | **Airbrake / Hover Mode** (rapid deceleration) |
| `Arrow Keys` | Orbit Camera freely (Yaw & Pitch) around Ornithopter |
| `C` | **Cycle Camera View** (Cinematic Chase → Cockpit Close → Overhead Tactical) |
| `F` | Toggle Wireframe Rendering |
| `F11` | Toggle True Fullscreen / Windowed Mode |
| `ESC` | Exit Application |

---

## 🚀 How to Run the Project

The project is already pre-compiled and ready for execution.

### Method 1: Command Line (PowerShell)
From the project directory, run:
```powershell
.\build\Release\Arrakis.exe
```

### Method 2: File Explorer
Navigate to:
```
Arrakis_001\Arrakis_001\build\Release\Arrakis.exe
```
Double-click `Arrakis.exe`.

### Method 3: From Visual Studio 2022
1. Open `Arrakis_001.vcxproj` in Visual Studio 2022.
2. Select configuration **Release** and platform **x64**.
3. Press **F5** (or `Ctrl+F5` to run without debugger).

---

## 🛠️ How to Rebuild (If modifying source)

### Prerequisites
- Visual Studio 2022 (with Desktop C++ workload)
- CMake ≥ 3.20
- OpenGL 3.3 compatible GPU

### Rebuilding via CMake
Open PowerShell in the project root:
```powershell
# Stop any running instances
Stop-Process -Name "Arrakis" -Force -ErrorAction SilentlyContinue

# Configure & Build
cmake -S . -B build
cmake --build build --config Release

# Run
.\build\Release\Arrakis.exe
```

---

## 📐 Procedural Geometric Modeling (Zero External Meshes)

All objects are generated mathematically in memory and uploaded to GPU Vertex Array Objects (VAOs):

| Primitive | Mathematical Construction | Key Uses |
|---|---|---|
| **Cylinder** | Parametric circle extrusion with radial segments, normals, UVs, and capped end discs. | Jet intakes, reheat nozzles, tail boom, pressure tanks, harvester cutter drum, road wheels. |
| **UV Sphere** | Polar latitude/longitude tessellation with smooth spherical normals. | Cockpit canopy bubble, hemispherical tank heads, bridge searchlights, sensor pods. |
| **Cube** | 6 independent faces with outward normals and UV coordinates. | Fuselage bulkheads, crawler tracks, refinery chassis, transport cradles, rock strata. |
| **Aerofoil Blade** | Tapered aerofoil profile spanning from root chord (1.0) to tip chord (0.22) with thickness drop. | 8 movie dragonfly wings, tail stabilizer fins, landing skids. |
| **Dune Heightfield** | 140×140 grid tessellating a 700×700 unit terrain using continuous multi-harmonic sinusoidal and ridge functions: `h(x, z) = sin(x*0.009 + z*0.004)*14 + cos(x*0.004 - z*0.012)*9.5 + (1 - |sin(x*0.028 + z*0.016)|)^2 * 5`. | Vast seamless desert sand dunes with analytical finite-difference surface normals. |
| **Quad** | 2-triangle camera-facing planar billboard. | Sand dust particle streaks and atmospheric skybox quad. |

---

## 🔬 Shading, Lighting & Atmospheric Pipeline

### 1. Blinn-Phong + Powder Wrap Lighting
```glsl
// Half-Lambert wrap lighting prevents unnatural black shadows on dunes:
float diff = isSand == 1 ? max((dot(N, L) + 0.35) / 1.35, 0.0) : max(dot(N, L), 0.0);
vec3 diffuse = diff * sunColor * baseColor;

// Blinn-Phong specular highlight:
vec3 H = normalize(L + V);
float spec = pow(max(dot(N, H), 0.0), shininess);
vec3 specular = spec * specularColor * sunColor;
```

### 2. Procedural Sand Texture & Micro-Sparkle
- **Wind Dune Ripples**: Two sinusoidal wave harmonics aligned with the wind direction modulate the surface albedo subtly in the fragment shader.
- **Slope-Based Color Blending**: Sun-bleached golden sand on dune crests (`#E5A65D`) blends into deep ochre tones (`#945222`) in shadowed troughs.
- **Quartz Specular Sparkles**: Pseudo-random high-frequency glints simulate sunlight reflecting off individual sand grains.

### 3. Soft PCF Shadow Mapping
- **Pass 1 (Depth Only)**: Renders the scene from the directional sun's point of view into a `2048×2048` 32-bit floating-point depth texture.
- **Slope-Scaled Bias**: `glPolygonOffset(2.5, 4.0)` completely eliminates shadow acne on sloping dunes.
- **Pass 2 (Percentage Closer Filtering)**: A 3×3 sampling kernel samples neighboring depth texels, producing soft realistic shadow penumbras across the sand.

### 4. Exponential Squared Desert Fog
```glsl
float dist = length(viewPos - FragPos);
float fogFactor = 1.0 - exp(-pow(dist * fogDensity, 2.0));
vec3 finalColor = mix(litColor, fogColor, fogFactor);
```
Fades distant dunes and structures smoothly into warm atmospheric desert haze (`#D89447`).

### 5. Dynamic Blowing Sand Particle System
- 2,400 persistent sand particles moving along the global wind vector `(-0.92, -0.04, 0.38)`.
- Features wind gust dynamics (`sin(time * 1.8)`), terrain hugging, crawler tread dust plumes, and ornithopter downwash.

---

## 📁 Repository Structure

```
Arrakis_001/
├── arrakis.cpp          ← Master OpenGL C++ implementation (all systems in one file)
├── glad.c               ← GLAD OpenGL 3.3 Core function loader
├── CMakeLists.txt       ← CMake build configuration
├── Arrakis_001.vcxproj  ← Visual Studio 2022 project file
├── README.md            ← Comprehensive project documentation & run guide
├── CODE_EXPLANATION.txt ← Deep line-by-line technical code explanation
└── build/
    └── Release/
        └── Arrakis.exe  ← Standalone compiled executable
```

---

## 👥 Authors & Laboratory Information

- **Course**: CSE 4102 — Computer Graphics and Image Processing Laboratory
- **Project**: Arrakis — Dynamic Dune Scene Simulation
- *All rights reserved. "The spice must flow."*
