# 🪱 ARRAKIS — Cinematic Dynamic Dune Simulation
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
