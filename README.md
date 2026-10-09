# Arrakis: Harvester Down

A C++17 / OpenGL 3.3 desert rescue game for CSE 4102, Computer Graphics and Image
Processing Laboratory. All scene geometry is generated procedurally. No external
3D models or image textures are loaded; the only runtime file assets are audio.

## Project Layout

```text
src/                    Application, procedural graphics, game, HUD, and audio
tests/                  Deterministic headless mission tests
third_party/glad/       Generated OpenGL function loader
third_party/miniaudio/  Vendored audio library and implementation translation unit
assets/audio/           Eight runtime sound effects and retained asset license
docs/chapters/          Detailed, editable explanation chapters
docs/reference/         Original proposal and visual-reference images
docs/Arrakis_Code_Explanation.pdf  Complete project explanation and source reference
tools/                  Reproducible documentation generator
.vscode/                Shared IntelliSense and debugger configuration
CMakeLists.txt          Primary build configuration
Arrakis_001.vcxproj     Alternative Visual Studio project
vcpkg.json              Optional GLM dependency manifest
build/                  Generated builds only (ignored by Git)
```

Unused downloaded OBJ/material/texture packs, old build trees, the unused Vorbis
decoder, redundant Ogg sound copies, and outdated code notes have been removed.
Visual references and library/asset license notices are preserved.

## Gameplay

Rescue the 36 workers evacuating a moving spice harvester before a pursuing worm
reaches it. Groups of one to four start leaving at six seconds, then every seven
seconds. They run away from the harvester and wait at fixed amber beacons. Winch
up to eight aboard, then deliver them to the cyan pad. Only delivered survivors
count toward the score. The initial pursuit lasts 210 seconds; later missions
shorten it by ten seconds to a minimum of 150. After the attack, a 60-second final
extraction window allows recovery of distant survivors. The best score persists
only while the application is open.

| Input | Action |
| --- | --- |
| Enter | Start or advance to the next mission |
| Mouse hover | Turn aircraft and camera together; center dead zone stops turning |
| W / S | Forward / backward |
| A / D | Strafe without changing heading |
| Q / E | Raise / lower terrain-following altitude |
| Space | Brake, descend, winch nearby workers, or unload over the cyan pad |
| Left Shift | Rechargeable boost |
| C | Cycle wide, close, and tactical camera |
| Arrow keys | Turn and adjust camera elevation |
| P | Pause / resume |
| M | Mute / unmute |
| R | Retry the current mission |
| F | Toggle wireframe |
| F11 | Toggle fullscreen |
| Esc | Exit |

Pickup requires low, steady flight. Window-focus loss automatically pauses and
clears held inputs. Mission reset, resume, resize, and fullscreen changes center
the mouse to prevent unintended steering. Audio gracefully falls back to silence
if the device or a file cannot be initialized.

## Build and Run

Requires CMake 3.20+, a C++17 compiler, GLFW, GLAD headers, GLM, and an OpenGL
3.3-capable graphics driver. This laboratory installation uses `E:/glfw_nec` and
`E:/glm`; these paths are discovery hints, not bundled dependencies.

```powershell
cmake -S . -B build/rescue -G "Visual Studio 17 2022" -A x64
cmake --build build/rescue --config Release
.\build\rescue\Release\Arrakis.exe
```

On another machine, pass `-DGLAD_INCLUDE_DIR=...`, `-DGLFW_INCLUDE_DIR=...`,
`-DGLM_INCLUDE_DIR=...`, and `-DGLFW_LIBRARY=...` during configuration. The include
paths must contain `glad/glad.h`, `GLFW/glfw3.h`, and `glm/glm.hpp` respectively.
The vendored loader was generated for GL 4.6 core; use matching GLAD headers.
The application itself requests only OpenGL 3.3. `vcpkg.json` installs GLM only,
not GLFW or GLAD. The optional Visual Studio project retains local laboratory
paths for its x64 configurations; CMake is the portable primary build route.

The build copies `assets/audio/` beside the executable. Run from the project root
or the executable directory, or distribute the executable with that asset folder.

## Verification

```powershell
ctest --test-dir build/rescue -C Release --output-on-failure
.\build\rescue\Release\Arrakis.exe --smoke-test
.\build\rescue\Release\Arrakis.exe --smoke-breach
.\build\rescue\Release\Arrakis.exe --smoke-mouse
.\build\rescue\Release\Arrakis.exe --smoke-pursuit
```

CTest runs `Arrakis --test-game` without a window or audio device. Each graphical
smoke command renders 120 frames, checks OpenGL errors, and exits; an optional
second argument saves a BMP capture. These tests require a graphics desktop.

## Detailed PDF

Read [the project explanation](docs/Arrakis_Code_Explanation.pdf) for architecture,
C++ and graphics foundations, code-range walkthroughs, game state transitions,
each deterministic test, procedural models, shader mathematics, HUD/font/radar,
sandworm animation, audio, build/configuration, limitations, and full numbered
source appendices. Third-party library internals are explained separately from
the project's own code.

Regenerate after modifying code or chapter text:

```powershell
python -m pip install -r tools/requirements.txt
python tools/generate_docs.py
```

The generator also writes `docs/documentation_manifest.json` with source SHA-256
hashes and line counts so the documented snapshot can be checked.
