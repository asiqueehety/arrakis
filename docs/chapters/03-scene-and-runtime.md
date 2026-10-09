# Scene Assembly and Application Runtime

This chapter explains the scene-building and application-lifecycle portion of `src/arrakis.cpp`. Its source references use the **original, pre-relocation line numbers 968-1892** from the root-level `arrakis.cpp` inspected for this documentation. The relocated `src/arrakis.cpp` was also checked: the covered range still has the same line numbers and statements. The original ranges below identify the inspected statements, comments, and function boundaries. The complete source listings supplied elsewhere in the documentation remain the authority for the final physical file layout.

The covered code turns previously generated meshes into recognizable vehicles, machinery, landscape objects, and rescue markers, then connects them to GLFW input, simulation updates, spatial audio, OpenGL rendering, screenshots, automated render checks, and cleanup. It does not load vehicle models from external model files. The geometric pieces are reused meshes with different transformations and materials. The supporting application headers live alongside the application source in `src/`; mission tests are documented under `tests/arrakis_tests.h`. Third-party implementation files and audio assets belong under `third_party/` and `assets/audio/`, respectively. Those layout names do not imply additional runtime behavior in the original code explained here.

## Reading Transforms and Materials

### Shared Conventions Used Throughout This Chapter

**Context for original lines 968-1354:** the actual transform helper and default material definitions are immediately before this chapter's assigned range, at original lines 906-947. Understanding them is necessary to read every scene draw call accurately.

`drawPart(mesh, root, t, rDeg, s, material)` starts with the supplied parent matrix `root`, appends translation `t`, appends any nonzero Y rotation, then X rotation, then Z rotation, and finally appends scale `s`. In matrix notation the result is `root * T * Ry * Rx * Rz * S`. The rotation tuple is still written as `(X, Y, Z)` in the calls; it is not a description of the multiplication order. Degrees are converted to radians by the helper before calling GLM. With column-vector positions, the scale is applied to a mesh point first, then the local Z, X, and Y rotations, then the local translation, then all inherited parent transformations. A translated part therefore rotates about its own local origin before being placed at its attachment location. When a parent is tilted or scaled, the child's apparent world-space position and orientation inherit that parent change.

The notation in the tables below is literal: **T** is the translation tuple, **R** is the `(X, Y, Z)` degree tuple, and **S** is the scale tuple. All coordinates are in the parent's local space unless a section explicitly constructs a world-space root. Vehicle construction uses X for left/right, Y for up/down, and Z for front/back; the ornithopter's nose is toward negative Z. Numbers are scene units, not a declared real-world measurement system.

The generated cube is centered at the origin and has unit side length. The sphere has radius `0.5`, so its scale tuple gives its full axis diameters before other transforms. The generated cylinder has radius `0.5`, height `1`, and its long axis along local Y. A cylinder with S `(2.2, 3.2, 2.2)` therefore has diameter `2.2` and length `3.2`; rotating it 90 degrees around X places that length along Z. Nonuniform X/Z scaling changes a round cylinder into an elliptical one. A mesh called a cylinder remains the same capped-cylinder geometry even when a source comment calls the result a probe, funnel, nozzle, or drum. Likewise, an entire sphere is drawn for each nominal domed tank head; overlapping spheres and a cylinder produce the visible rounded ends rather than explicit hemisphere meshes.

Every `Material` initially has diffuse `(0.6, 0.6, 0.6)`, specular `(0.3, 0.3, 0.3)`, shininess `32`, emissive `(0, 0, 0)`, and `isSand = 0`. A construction function only overrides the fields shown in its body. In material tables, `default` means exactly these unchanged values, not a missing value or a random shader setting. A scalar `glm::vec3(v)` repeats `v` in all three color channels. Diffuse controls the surface color, specular and shininess control the highlight contribution, emissive adds self-colored brightness in the scene shader, and `isSand` selects special shader treatment. Emission does not create an additional OpenGL light source and does not, by itself, illuminate neighboring geometry.

`drawPart` ultimately calls `drawMeshPrimitive`. During the shadow pass, that helper sends the model matrix and material surface classification to the shadow program. During the normal scene pass, it also sends the color, specular, shininess, and emission values to the scene program. The same assembled object can therefore participate in both passes without duplicating its construction logic.

## Ornithopter Construction

### Function Inputs and Six Surface Materials

**Original source: lines 968-1001.** The section banner describes an Atreides-style ornithopter with eight tandem dragonfly blades, an insect-like hull, and glowing jets. These are design descriptions; the implementation that follows is a collection of scaled primitive meshes and a dedicated wing mesh.

`drawMovieOrnithopter(root, wingPhase, roll, pitch, boosting)` receives the assembled aircraft-to-world transform, the animation phase for the wings, roll and pitch values, and the boost state. In this function, `roll` and `pitch` are not read. Aircraft attitude has already been applied to `root` by `drawFullScene`; the redundant arguments do not add a second banking or pitching transform. `wingPhase` drives the flap and twist formulas. `boosting` chooses between two exhaust lengths and emission values.

The six local material objects are rebuilt on each invocation. They do not represent six texture files or persistent GPU objects. Each supplies the fields used by the primitive helper for that draw.

| Material | Diffuse RGB | Specular RGB | Shininess | Emissive RGB | Role |
|---|---|---|---|---|---|
| `matHull` | `(0.22, 0.23, 0.22)` | `(0.4, 0.4, 0.4)` | `64` | default zero | Main dark gray body and selected structural parts. |
| `matDark` | `(0.13, 0.14, 0.13)` | `(0.3, 0.3, 0.3)` | `32` | default zero | Darker probe, canopy frame, gimbals, tail pieces, and skids. |
| `matCanopy` | `(0.06, 0.08, 0.11)` | `(0.9, 0.9, 0.9)` | `128` | default zero | Very dark blue-gray cockpit surface with a sharp highlight. |
| `matFlame` | `(1, 0.55, 0.12)` | default `(0.3, 0.3, 0.3)` | default `32` | Boosting: `(1.2, 0.6, 0.15)`; otherwise `(0.6, 0.25, 0.05)` | Orange exhaust geometry with stronger self-brightness under boost. |
| `matJetNozzle` | `(0.08, 0.08, 0.08)` | `(0.5, 0.5, 0.5)` | `64` | default zero | Dark terminal engine nozzles. |
| `matBlade` | `(0.18, 0.19, 0.18)` | `(0.7, 0.7, 0.7)` | `90` | default zero | Thin dark metallic-looking wing blades. |

All six keep `isSand = 0`. The canopy is not rendered with an alpha-bearing glass material here. Its name and high specular response give it a glass-like appearance, but the call supplies no transparency, refraction, or view through the canopy. The flame's emission can exceed one in some channels because the assigned float values are not clamped in this construction function.

### Fuselage, Cockpit, and Dorsal Intakes

**Original source: lines 1002-1018.** These comments and seven draw calls build the central pod, front details, canopy, and paired intakes. Every call uses the incoming aircraft `root` directly, so none introduces a second articulated parent.

| Original line | Mesh and visual component | T | R | S | Material |
|---|---|---|---|---|---|
| 1004 | Sphere: central elongated body pod | `(0, 0, -0.4)` | `(0, 0, 0)` | `(1.3, 0.85, 3.2)` | `matHull` |
| 1006 | Cylinder: forward nose segment | `(0, -0.05, -2.1)` | `(85, 0, 0)` | `(0.7, 1.4, 0.45)` | `matHull` |
| 1008 | Cylinder: narrow forward sensor probe | `(0, -0.1, -3.1)` | `(90, 0, 0)` | `(0.06, 1.2, 0.06)` | `matDark` |
| 1011 | Sphere: tinted cockpit canopy | `(0, 0.32, -1.2)` | `(-10, 0, 0)` | `(0.85, 0.65, 1.6)` | `matCanopy` |
| 1013 | Cube: slim canopy spine | `(0, 0.58, -1.1)` | `(-10, 0, 0)` | `(0.12, 0.18, 1.5)` | `matDark` |
| 1016 | Cylinder: positive-X dorsal intake | `(0.42, 0.35, -0.2)` | `(82, 0, 0)` | `(0.35, 1.2, 0.28)` | `matHull` |
| 1017 | Cylinder: negative-X dorsal intake | `(-0.42, 0.35, -0.2)` | `(82, 0, 0)` | `(0.35, 1.2, 0.28)` | `matHull` |

The central sphere's nonuniform dimensions turn it into a long, low ellipsoid. The nose cylinder is nearly horizontal, with a slight departure from the probe's exact 90-degree X rotation. Its name says tapered, but this particular draw does not change the cylinder's radius along its length. The probe is long compared with its `0.06` diameter, producing the needle-like detail at the front. The canopy lies above the forward body, and the thin cube spine is higher still, making a dark structural line over the shiny surface.

The comment about multiple canopy facets does not correspond to multiple separate canopy calls in this range: one tessellated sphere is drawn. Its generated triangles and its ellipsoidal scale define its actual surface. The two intake cylinders differ only in the sign of their X positions, placing them symmetrically on the upper body. They are solid capped meshes; no separate open interior or airflow simulation is added by these calls.

### Wing Gimbals, Turbines, Nozzles, and Boost Exhaust

**Original source: lines 1019-1035.** Four shoulder cylinders suggest front and aft wing mounting hardware. Two larger cylinders create engine bodies, two shorter cylinders create the nozzles, and two final cylinders create visible exhaust. Their exact transforms are below.

| Original line | Mesh and component | T | R | S | Material |
|---|---|---|---|---|---|
| 1020 | Cylinder: front positive-X wing gimbal | `(0.72, 0.1, -0.3)` | `(0, 0, 90)` | `(0.45, 0.65, 0.45)` | `matDark` |
| 1021 | Cylinder: front negative-X wing gimbal | `(-0.72, 0.1, -0.3)` | `(0, 0, 90)` | `(0.45, 0.65, 0.45)` | `matDark` |
| 1022 | Cylinder: aft positive-X wing gimbal | `(0.68, 0.05, 0.5)` | `(0, 0, 90)` | `(0.42, 0.62, 0.42)` | `matDark` |
| 1023 | Cylinder: aft negative-X wing gimbal | `(-0.68, 0.05, 0.5)` | `(0, 0, 90)` | `(0.42, 0.62, 0.42)` | `matDark` |
| 1026 | Cylinder: positive-X turbine body | `(0.52, -0.15, 0.9)` | `(90, 0, 0)` | `(0.42, 1.6, 0.42)` | `matHull` |
| 1027 | Cylinder: negative-X turbine body | `(-0.52, -0.15, 0.9)` | `(90, 0, 0)` | `(0.42, 1.6, 0.42)` | `matHull` |
| 1029 | Cylinder: positive-X exhaust nozzle | `(0.52, -0.15, 1.72)` | `(90, 0, 0)` | `(0.36, 0.25, 0.36)` | `matJetNozzle` |
| 1030 | Cylinder: negative-X exhaust nozzle | `(-0.52, -0.15, 1.72)` | `(90, 0, 0)` | `(0.36, 0.25, 0.36)` | `matJetNozzle` |
| 1033 | Cylinder: positive-X luminous exhaust | `(0.52, -0.15, 1.8 + flameLen * 0.5)` | `(90, 0, 0)` | `(0.24, flameLen, 0.24)` | `matFlame` |
| 1034 | Cylinder: negative-X luminous exhaust | `(-0.52, -0.15, 1.8 + flameLen * 0.5)` | `(90, 0, 0)` | `(0.24, flameLen, 0.24)` | `matFlame` |

A 90-degree Z rotation puts each gimbal's long cylinder axis across the aircraft rather than upright. The front pair is slightly larger and higher than the aft pair. The visible mounting cylinders do not individually rotate with wing strokes; the wing matrices constructed later are separate transforms sharing the aircraft parent.

Both turbine bodies run along Z beneath the fuselage. Their nozzle centers are farther toward positive Z, the rear of the aircraft. Line 1032 sets `flameLen` to `1.4` under boost and `0.6` otherwise. Consequently the exhaust centers are at Z `2.5` or `2.1`, while the scaled cylinders extend between Z `1.8` and Z `3.2` or `2.4`. Moving the center by half the chosen length keeps the forward end at the same location. The glowing shapes exist even when the aircraft is not boosting. They become longer and use the stronger emission values when the boolean is true; no branch removes them, animates their noise, or makes a tapered flame mesh.

### Tail Boom, Stabilizers, and Landing Skids

**Original source: lines 1036-1048.** The remaining fixed aircraft pieces extend behind the engines and below the body.

| Original line | Mesh and component | T | R | S | Material |
|---|---|---|---|---|---|
| 1037 | Cylinder: main tail boom | `(0, 0, 2.4)` | `(90, 0, 0)` | `(0.32, 2.6, 0.28)` | `matHull` |
| 1038 | Cylinder: narrower aft boom | `(0, 0.05, 4.4)` | `(90, 0, 0)` | `(0.18, 2.2, 0.16)` | `matDark` |
| 1040 | Cylinder: terminal stinger | `(0, 0.05, 5.7)` | `(90, 0, 0)` | `(0.08, 1.1, 0.08)` | `matDark` |
| 1043 | Cube: positive-X canted tail fin | `(0.5, 0.55, 4.2)` | `(0, 0, 38)` | `(0.08, 0.9, 0.65)` | `matHull` |
| 1044 | Cube: negative-X canted tail fin | `(-0.5, 0.55, 4.2)` | `(0, 0, -38)` | `(0.08, 0.9, 0.65)` | `matHull` |
| 1046 | Cube: positive-X ventral skid | `(0.45, -0.6, -0.4)` | `(0, 0, 0)` | `(0.08, 0.45, 2.2)` | `matDark` |
| 1047 | Cube: negative-X ventral skid | `(-0.45, -0.6, -0.4)` | `(0, 0, 0)` | `(0.08, 0.45, 2.2)` | `matDark` |

The tail is assembled from progressively narrower, overlapping cylinders instead of one continuously tapered extrusion. The final stinger is a cylinder too. The two fins are thin cubes tilted in opposite Z directions to make a V-like arrangement. They have no separate steering animation in this function. The skids are also fixed cubes, long in Z and thin in X, positioned below the pod. No extension or retraction branch is present. In total, the fixed portion of this aircraft function issues 24 primitive draws before the wings.

### Flap Waveforms and Eight Wing Definitions

**Original source: lines 1049-1081.** The wing section begins with its own descriptive banner and defines a `34`-degree flap amplitude. `flap1 = sin(wingPhase) * 34` and `flap2 = sin(wingPhase + pi * 0.75) * 34`. A phase difference of `0.75 * pi` is 135 degrees, not an exact 180-degree opposition. The comment's term anti-phase is therefore an informal description of staggered strokes. The aft definitions introduce an additional sign reversal of selected flap values.

The local `WingDef` structure stores five pieces of information: `side` is `+1` for the right/positive-X blade and `-1` for the left/negative-X blade; `pivot` is the attachment point; `flapAng` is the current waveform sample; `sweepBack` is a fixed backward-sweep angle in degrees; and `dihedral` is the resting vertical angle, also in degrees. The eight-element array is recreated from the current phase at each draw.

| Pair and side | `side` | Pivot | `flapAng` | `sweepBack` | `dihedral` |
|---|---|---|---|---|---|
| Forward upper, right | `1` | `(0.95, 0.22, -0.35)` | `flap1` | `14` | `12` |
| Forward upper, left | `-1` | `(-0.95, 0.22, -0.35)` | `flap1` | `14` | `12` |
| Forward lower, right | `1` | `(0.95, -0.05, -0.25)` | `flap2` | `18` | `-10` |
| Forward lower, left | `-1` | `(-0.95, -0.05, -0.25)` | `flap2` | `18` | `-10` |
| Aft upper, right | `1` | `(0.90, 0.18, 0.50)` | `-flap2` | `28` | `8` |
| Aft upper, left | `-1` | `(-0.90, 0.18, 0.50)` | `-flap2` | `28` | `8` |
| Aft lower, right | `1` | `(0.90, -0.08, 0.60)` | `-flap1` | `32` | `-12` |
| Aft lower, left | `-1` | `(-0.90, -0.08, 0.60)` | `-flap1` | `32` | `-12` |

The upper and lower attachment points are separated vertically and slightly longitudinally. The aft pivots are closer to the centerline and farther back. Greater sweep values for the aft blades keep their resting directions distinct from the forward blades. A mirrored pair shares its scalar phase and dihedral settings; the side multiplier in the matrix logic is what turns the two sides into corresponding opposite-space movements.

### Wing Matrix Order, Mirroring, and Culling

**Original source: lines 1082-1101.** The range-based loop visits all eight array entries. It copies the aircraft matrix to `wm`, translates to the current wing's pivot, and appends a Z rotation of `side * (dihedral + flapAng)` degrees. That is the flap motion about the aircraft's longitudinal axis. It then appends a Y rotation of `side * sweepBack` degrees. Finally it computes `pitchTwist = cos(wingPhase) * 8 * side` and appends that X rotation.

All blades use the same cosine phase for twist; the twist is not computed from the wing's own `flapAng`, and the aft blades do not receive an additional twist-phase shift. The twist amplitude is eight degrees. The sign flips left versus right. The resulting wing matrix is `root * T(pivot) * Rz(flap and dihedral) * Ry(sweep) * Rx(twist) * S(side * 5.2, 1, 1)`.

The final scale stretches the generated wing's original X span from one to `5.2` units. On the left, negative X scale both extends the blade to the opposite side and reverses the winding of its triangles. Instead of selecting a different front-face convention for those blades, the code disables face culling before drawing each wing, draws `g_meshWing` using `matBlade`, and then enables face culling again. Both sides of the thin blades are therefore drawable. This is an explicit state setting, not a saved-state restoration: the loop always leaves culling enabled. It does not change the configured `glCullFace(GL_BACK)` choice.

The wing draw calls use `drawMeshPrimitive` directly because their composite transform is already built. Together with the 24 fixed draws, the aircraft has 32 mesh draws per invocation. Since the full scene is invoked for both depth and color rendering, a visible aircraft is assembled and submitted in each of those passes with the same frame phase. The closing braces at lines 1100-1101 end the wing loop and aircraft function; they introduce no additional update of animation state.

## Harvester Construction

### Materials, Main Hopper, and Refinery Deck

**Original source: lines 1103-1119.** The harvester banner identifies four crawler units, a rotary scoop, and a multi-level industrial body. `drawSpiceHarvester(root, crawlerAnim)` accepts the already positioned and slope-adjusted vehicle matrix plus an animation amount. The caller supplies accumulated harvester travel, not wall-clock time. The visible rotation formulas multiply this distance-like value by their own constants.

| Material | Diffuse RGB | Specular RGB | Shininess | Emissive RGB |
|---|---|---|---|---|
| `matChassis` | `(0.42, 0.34, 0.25)` | `(0.2, 0.2, 0.2)` | `20` | default zero |
| `matTread` | `(0.11, 0.11, 0.10)` | `(0.15, 0.15, 0.15)` | `10` | default zero |
| `matRust` | `(0.48, 0.24, 0.12)` | `(0.2, 0.2, 0.2)` | `16` | default zero |
| `matSpice` | `(0.96, 0.42, 0.04)` | default `(0.3, 0.3, 0.3)` | default `32` | `(0.8, 0.35, 0.05)` |
| `matGlass` | `(0.1, 0.2, 0.25)` | `(0.8, 0.8, 0.8)` | `90` | default zero |
| `matLight` | `(1, 0.95, 0.7)` | default `(0.3, 0.3, 0.3)` | default `32` | `(1.2, 1.1, 0.8)` |

All retain `isSand = 0`. The chassis is warm brown, the tread almost black, and the rust reddish brown. Bright orange spice details and pale floodlights use emission rather than additional point-light uniforms. The bridge glass, like the aircraft canopy, is shiny colored opaque geometry in this construction path.

Line 1116 draws the main body using a cube with T `(0, 2.6, 0)`, R `(0, 0, 0)`, S `(9.5, 3.8, 6.2)`, and `matChassis`. The large width and length establish a broad rectangular hopper. Line 1118 draws another chassis cube at T `(0, 4.8, -0.4)`, R zero, S `(8.2, 1.2, 5.0)` for the upper deck. It is narrower, shorter, elevated, and slightly forward of the main body's center. These meshes are closed cubes, not hollow storage compartments. Their names do not add an interior refinery simulation.

### Four Track Units, Wheels, and Traveling Pads

**Original source: lines 1120-1139.** The track loops enumerate X offsets `-4.6` and `4.6` and Z offsets `-2.4` and `2.4`. Every combination creates one track parent `tm = translate(root, (ox, 0.95, oz))`, yielding four units. Their left/right placement is inherited from `ox`, while their front/rear placement is inherited from `oz`.

Each unit first draws a tread-colored housing cube with local T zero, R zero, and S `(1.8, 1.7, 3.8)`. It is centered at the track parent, making the same housing usable at all four corners. A wheel loop starts `wz` at `-1.4`, continues while `wz <= 1.4`, and adds `0.93` each iteration. It produces four wheel locations approximately `-1.4`, `-0.47`, `0.46`, and `1.39`; the next value exceeds the bound. The endpoint is not exactly `1.4` because the spacing and floating-point accumulation determine the actual sample locations.

At each wheel location line 1130 draws a chassis cylinder with T `(ox > 0 ? 0.95 : -0.95, -0.2, wz)`, R `(0, 0, 90)`, and S `(1.2, 0.35, 1.2)`. The sign test depends on the enclosing track's X location. It places each wheel on that track's outward-facing side. The Z rotation lays the `0.35` cylinder thickness along X, while the other scale values establish a `1.2` diameter. The full world X position includes the track parent offset as well as this local side offset.

Line 1131 adds a narrow rusty cube on the outer face: T `(ox > 0 ? 1.15 : -1.15, -0.2, wz)`, R `(crawlerAnim * 80, 0, 0)`, S `(0.04, 0.95, 0.12)`. This is the changing decorative wheel-face detail. The `80` coefficient supplies degrees per unit of `crawlerAnim` through `drawPart`; no conversion to radians is done at this line because the helper does it. Its explicit rotation axis is local X before the track parent transforms, aligned with the cylinder axle after that cylinder's 90-degree Z rotation. The wheel cylinder itself does not receive an animation angle; the bar's changing orientation supplies the visible spinning detail while the smooth wheel body remains unchanged.

Ten pad cubes are drawn per track. For integer `pad` values zero through nine, line 1134 computes `a = pad * two_pi / 10 + crawlerAnim * 0.7`. Here `a` is in radians and feeds sine and cosine directly. The pad center is `(0, 0.85 * sin(a), 1.8 * cos(a))`, an ellipse in the unit's Y/Z plane. The angle is `-degrees(atan2(0.85 * cos(a), -1.8 * sin(a)))`. The two `atan2` arguments are the Y and Z components of the derivative of that elliptical path. Negating the resulting angle sets the cube's local X rotation so its long Z direction follows the path tangent.

Line 1136 therefore draws each pad with that computed T, R `(angle, 0, 0)`, S `(1.9, 0.12, 0.38)`, and `matRust`. Pads span the track's width but are thin in Y and short in Z. Their centers revolve around the housing as `crawlerAnim` grows. The code does not translate a continuous tread texture or construct a linked belt mesh; it moves ten separate cubes around each ellipse. One unit submits one housing, four cylinders, four bars, and ten pads, or 19 draws. The nested loops submit 76 track-related draws in total. The closing braces finish the pad loop, front/rear loop, and left/right loop without changing the harvester root.

### Cutter, Exit Door, Ramp, and Side Separators

**Original source: lines 1141-1153.** This section constructs the low forward harvesting equipment and several side details. Although the cutter comment mentions teeth, no separate tooth draw calls occur in this range: the primary cutter is the reused cylinder.

| Original line | Mesh and component | T | R | S | Material |
|---|---|---|---|---|---|
| 1143 | Cylinder: wide front cutter drum | `(0, 1.1, -4.2)` | `(crawlerAnim * 70, 0, 90)` | `(2.2, 8.8, 2.2)` | `matRust` |
| 1145 | Cube: dark side door | `(4.78, 1.8, 1.2)` | `(0, 0, 0)` | `(0.12, 2.4, 1.5)` | `matDoor` |
| 1146 | Cube: outward side ramp | `(5.5, 0.45, 1.2)` | `(0, 0, -16)` | `(2, 0.15, 1.7)` | `matChassis` |
| 1148 | Cube: luminous intake scoop | `(0, 0.9, -4.8)` | `(15, 0, 0)` | `(8.2, 0.6, 1.4)` | `matSpice` |
| 1151 | Cylinder: positive-X separator | `(4.8, 2.2, -1.8)` | `(0, 0, -22)` | `(1.2, 2.4, 1.2)` | `matRust` |
| 1152 | Cylinder: negative-X separator | `(-4.8, 2.2, -1.8)` | `(0, 0, 22)` | `(1.2, 2.4, 1.2)` | `matRust` |

Line 1144 constructs `matDoor` with diffuse `(0.06, 0.07, 0.06)` and leaves every other field at its default. The door is an almost black thin rectangle on the positive-X side; the ramp extends farther out and slopes through its negative Z rotation. Neither has an opening animation in this function. The scoop is a thin orange slab angled about X at the front and lower than the drum's center.

The drum call appends X rotation before Z rotation in the helper's matrix multiplication. At zero `crawlerAnim`, the 90-degree Z rotation turns its original Y axis across the vehicle along X; subsequent changes to the X angle rotate around that resulting axis. The `70` coefficient is in degrees per supplied animation unit. The two separators are straight cylinders tilted oppositely around Z. Their comment calls them funnels, but there is no tapered or hollow funnel primitive in these draw calls.

### Bridge, Viewport, Floodlights, and Exhaust Stacks

**Original source: lines 1154-1168.** The elevated details complete the harvester and close its draw function.

| Original line | Mesh and component | T | R | S | Material |
|---|---|---|---|---|---|
| 1155 | Cube: observation bridge body | `(2.8, 5.8, -2.2)` | zero | `(2.6, 1.4, 2)` | `matChassis` |
| 1157 | Cube: forward bridge viewport | `(2.8, 5.9, -3.25)` | zero | `(2.4, 0.65, 0.2)` | `matGlass` |
| 1159 | Sphere: first forward floodlight | `(1.8, 5.2, -3.3)` | zero | `(0.35, 0.35, 0.35)` | `matLight` |
| 1160 | Sphere: second forward floodlight | `(3.8, 5.2, -3.3)` | zero | `(0.35, 0.35, 0.35)` | `matLight` |
| 1163 | Cylinder: positive-X exhaust stack | `(1.6, 6.2, 1.8)` | zero | `(0.75, 2.6, 0.75)` | `matChassis` |
| 1164 | Cylinder: negative-X exhaust stack | `(-1.6, 6.2, 1.8)` | zero | `(0.75, 2.6, 0.75)` | `matChassis` |
| 1166 | Cylinder: positive-X glowing vent rim | `(1.6, 7.55, 1.8)` | zero | `(0.85, 0.2, 0.85)` | `matSpice` |
| 1167 | Cylinder: negative-X glowing vent rim | `(-1.6, 7.55, 1.8)` | zero | `(0.85, 0.2, 0.85)` | `matSpice` |

The bridge is offset to the positive-X front of the refinery deck. Its viewport is thin in Z and just forward of the bridge. The two small emissive spheres sit on either side below that viewport. The comment says they illuminate the desert, but the local code submits no spotlight parameters; their actual local effect is visibly bright geometry. Overall scene lighting remains the directional sun and ambient contribution described later.

The vertical stacks sit at the rear of the top deck. Because no rotation is supplied, their `2.6` Y scales make them upright. The slightly wider, much shorter orange cylinders sit at their tops as luminous rims. This function does not emit particles from those exact stack positions. Particle placement is handled separately by the particle simulation. With two body cubes, 76 track draws, six cutter/door/ramp/separator draws, and eight elevated-detail draws, the harvester function submits 92 meshes per invocation.

## Spice Tank Construction

### Four Materials and the Transport Cradle

**Original source: lines 1170-1192.** The tank banner describes a heavy horizontal vessel with rounded heads, supporting members, and an illuminated spice indicator. `drawMovieSpiceTank(root)` has no animation parameter. Any placement, yaw, or instance scale must already be present in `root`.

`matCradle` has diffuse `(0.24, 0.23, 0.22)`, specular `(0.3, 0.3, 0.3)`, and shininess `32`. `matVessel` uses tank steel diffuse `(0.52, 0.46, 0.38)`, specular `(0.6, 0.6, 0.6)`, and shininess `64`. `matSeam` is darker, with diffuse `(0.18, 0.17, 0.16)`, specular `(0.2, 0.2, 0.2)`, and shininess `20`. `matSpice` uses diffuse `(0.96, 0.42, 0.04)` and emission `(1.2, 0.55, 0.08)`, keeping default specular and shininess. None changes `isSand`, and only the spice material changes default zero emission.

| Original line | Mesh and cradle component | T | R | S | Material |
|---|---|---|---|---|---|
| 1182 | Cube: positive-X ground skid | `(1.35, 0.18, 0)` | zero | `(0.35, 0.35, 4.4)` | `matCradle` |
| 1183 | Cube: negative-X ground skid | `(-1.35, 0.18, 0)` | zero | `(0.35, 0.35, 4.4)` | `matCradle` |
| 1185 | Cube: rear cross support | `(0, 0.25, 1.6)` | zero | `(3, 0.25, 0.35)` | `matCradle` |
| 1186 | Cube: forward cross support | `(0, 0.25, -1.6)` | zero | `(3, 0.25, 0.35)` | `matCradle` |
| 1189 | Cube: positive-X inclined stanchion, at each Z | `(1.3, 1.1, z)` | `(0, 0, -22)` | `(0.28, 1.8, 0.35)` | `matCradle` |
| 1190 | Cube: negative-X inclined stanchion, at each Z | `(-1.3, 1.1, z)` | `(0, 0, 22)` | `(0.28, 1.8, 0.35)` | `matCradle` |

The stanchion loop uses exactly two Z values, `-1.3` and `1.3`, so its two draw statements produce four inclined support members. The long skids run along Z; the cross supports run along X; and the upright members lean through opposite Z rotations. These are independent solid boxes that visually overlap where necessary. There is no separate welding, constraint, or load-bearing solver associated with the comments. The cradle contains eight primitive draws.

### Vessel Body, Domed Ends, Weld Bands, and Indicator

**Original source: lines 1193-1210.** The rest of the tank uses eight draws to form its pressure vessel and visible fittings.

| Original line | Mesh and vessel component | T | R | S | Material |
|---|---|---|---|---|---|
| 1195 | Cylinder: horizontal central vessel | `(0, 1.55, 0)` | `(90, 0, 0)` | `(2.2, 3.2, 2.2)` | `matVessel` |
| 1197 | Sphere: positive-Z rounded head | `(0, 1.55, 1.6)` | zero | `(2.2, 2.2, 1.3)` | `matVessel` |
| 1198 | Sphere: negative-Z rounded head | `(0, 1.55, -1.6)` | zero | `(2.2, 2.2, 1.3)` | `matVessel` |
| 1201 | Cylinder: positive-Z reinforcing band | `(0, 1.55, 0.8)` | `(90, 0, 0)` | `(2.28, 0.18, 2.28)` | `matSeam` |
| 1202 | Cylinder: negative-Z reinforcing band | `(0, 1.55, -0.8)` | `(90, 0, 0)` | `(2.28, 0.18, 2.28)` | `matSeam` |
| 1205 | Cylinder: top loading hatch | `(0, 2.72, 0)` | zero | `(0.65, 0.25, 0.65)` | `matSeam` |
| 1206 | Sphere: bright relief-valve detail | `(0, 2.92, 0)` | zero | `(0.4, 0.25, 0.4)` | `matSpice` |
| 1209 | Cube: vertical side sight indicator | `(1.15, 1.55, 0)` | zero | `(0.08, 1.5, 0.18)` | `matSpice` |

There are eight rows in this table and eight draws; together with the eight cradle draws, the complete tank submits 16 meshes. The central vessel extends from local Z `-1.6` to `1.6`. The full ellipsoidal head meshes are centered at those endpoints, overlapping the cylinder and projecting out by `0.65` along Z. They keep the same X/Y diameter as the body. The narrow band cylinders have a slightly larger `2.28` diameter and only `0.18` axial length, causing their outer circumferences to show as dark reinforcing bands around the shell.

The top hatch is a short upright cylinder. The flattened emissive sphere above it supplies the bright valve-like shape. The side sight indicator is a thin, tall cube at positive X. Its material remains constant; no fill-level calculation, changing length, pulsing brightness, or animated spice level appears in this function. The comments identify the intended industrial interpretation of the visible detail, while the draw calls determine its actual implementation. Line 1210 closes the function.

## Desert Rock Construction

### Strata Materials and Four Eroded Mesh Pieces

**Original source: lines 1212-1225.** `drawDesertRockFormation(root, variant)` reuses the dedicated eroded-rock mesh four times to suggest one larger wind-carved formation. `matStrata1` has diffuse `(0.42, 0.28, 0.17)`, specular `(0.15, 0.15, 0.15)`, and shininess `16`. `matStrata2` has darker diffuse `(0.32, 0.20, 0.11)`, specular `(0.12, 0.12, 0.12)`, and shininess `12`. Both retain zero emission.

Line 1220 performs a chained assignment, setting both materials' `isSand` fields to `2`. The integer is a surface category consumed by the rendering shaders, not a C++ boolean assertion that the rocks are sand. It distinguishes their treatment from ordinary metal or cloth (`0`) and the main dune terrain (`1`). The shader chapter explains the category's detailed lighting and procedural color effect.

| Original line | Visual component | T | R | S | Material |
|---|---|---|---|---|---|
| 1221 | Dominant base crag | `(0, 2.2, 0)` | `(8, float(variant * 19), -5)` | `(8, 7, 6)` | `matStrata1` |
| 1222 | Upper darker crag | `(1, 5, -1)` | `(0, 55, 10)` | `(5, 5, 4)` | `matStrata2` |
| 1223 | Lower negative-X outcrop | `(-3, 0.8, 2)` | `(5, -40, 12)` | `(4, 3, 3)` | `matStrata2` |
| 1224 | Small positive-X rubble piece | `(3.2, 0.3, 2.2)` | `(15, 70, 20)` | `(2.2, 1.6, 2.1)` | `matStrata1` |

Every row draws `g_meshRock`, not a cube or sphere. The variant only changes the first piece's Y rotation by `19` degrees per integer variant. It does not select another mesh or alter the other three pieces. A separate per-site rotation in the master scene rotates the whole formation as well. Large nonuniform scales make the repeated mesh less obviously identical, and the different relative positions create a layered cluster. The function does not animate erosion or generate a fresh random rock on each frame; it transforms the mesh already generated during startup.

## Rescue Objects and People

### Header Integration, Materials, and Landing Base

**Original source: lines 1227-1245.** The two include directives pull in `arrakis_worm.h` and `arrakis_hud.h` before the rescue-world function and later runtime references. In the organized project these are application headers under `src/`. Including them makes their definitions available to this translation unit; it does not itself draw a worm or show the HUD.

`drawRescueWorld(time)` constructs five materials. `cyan` uses diffuse `(0.05, 0.5, 0.55)` and emission `(0.03, 0.6, 0.65)`. `amber` uses diffuse `(0.9, 0.5, 0.1)` and emission `(0.6, 0.25, 0.03)`. `dark` changes diffuse to `(0.16, 0.19, 0.19)`. `cloth` changes diffuse to `(0.30, 0.24, 0.17)` and specular to `(0.03, 0.03, 0.03)`, leaving shininess `32`. `visor` has diffuse `(0.08, 0.20, 0.27)`, specular `(0.8, 0.8, 0.8)`, and shininess `90`. Other fields stay at the shared defaults, including `isSand = 0` for all five.

The base parent is an identity matrix translated to `g_mission.base`. Mission reset has already put that position at the intended terrain elevation. This function does not sample the base height again. It draws the following local parts.

| Original line | Mesh and base component | T | R | S | Material |
|---|---|---|---|---|---|
| 1237 | Cylinder: broad landing-pad disk | `(0, 0.25, 0)` | zero | `(24, 0.5, 24)` | `dark` |
| 1238 | Ring: illuminated landing perimeter | `(0, 0.55, 0)` | zero | `(11, 1, 11)` | `cyan` |
| 1239 | Cube: long Z-oriented cross stripe | `(0, 0.56, 0)` | zero | `(1.2, 0.05, 10)` | `cyan` |
| 1240 | Cube: long X-oriented cross stripe | `(0, 0.56, 0)` | zero | `(10, 0.05, 1.2)` | `cyan` |
| 1243 | Cylinder: perimeter beacon pole | `(cos(a) * 13, 2, sin(a) * 13)` | zero | `(0.2, 4, 0.2)` | `dark` |
| 1244 | Sphere: beacon light | `(cos(a) * 13, 4.1, sin(a) * 13)` | zero | `(0.65, 0.65, 0.65)` | `cyan` |

The ring scale is literal; it multiplies the ring generator's existing radii rather than specifying a cylinder diameter. The pad disk extends from Y zero to `0.5` relative to the base. The ring and cross are slightly above its top, avoiding exactly coincident surfaces. The pole loop uses integer `i` from zero through three and sets `a = i * half_pi`, so four beacon pairs occupy the cardinal directions at radius `13`. Their cylinder centers are two units high and their heights are four, placing their bottoms at base level; the spheres are just above their tops. Four fixed pad draws plus eight beacon draws give 12 base meshes per invocation. None blinks as a function of `time` in this code.

### Released Group Markers and Terrain Alignment

**Original source: lines 1246-1261.** The group loop uses integer indices from zero up to `g_mission.groups.size() - 1`, with a cast so the container size can be compared to an `int`. It takes a const reference to each group. An unreleased group immediately executes `continue`, so there is no ground marker for workers still associated with an unreleased evacuation group.

For a released group, `waiting` starts at zero. An inner crew loop increments it only for a crew member whose `group` equals the current index and whose state is `Running` or `Waiting`. Other groups and states do not count. If the final count is zero, the group is skipped as well. Thus a released marker remains visible while at least one member is actively running or waiting outside, and disappears after that condition no longer holds. The code bases this visibility on crew states, not on proximity to the camera or aircraft.

The marker position `p` copies `group.center` but replaces its Y value with `getDuneHeight(p.x, p.z) + 0.2`. A translation to that position forms `ring`. The surface normal is sampled at the same X/Z. The code computes `axis = cross((0, 1, 0), normal)` and rotates the ring only if `length(axis) > 0.001`. The angle is `acos(clamp(normal.y, -1, 1))`, and the rotation axis is normalized. The clamp prevents small numeric excursions beyond the legal inverse-cosine range. This rotates the ring's local upward direction toward the terrain normal when there is a usable axis.

Line 1258 draws `g_meshRing` using the tilted root, local T and R zero, S `(4.5, 1, 4.5)`, and `amber`. Lines 1259-1260 deliberately use a fresh translation-only root at `p`, rather than the tilted ring matrix. They draw a dark cylinder at T `(4, 2, 0)`, R zero, S `(0.12, 4, 0.12)`, then an amber sphere at T `(4, 4.1, 0)`, R zero, S `(0.5, 0.5, 0.5)`. The pole and light therefore stay vertical even when the ring follows a slope.

The pole's height is not independently adjusted for the terrain four units to the side: both use the sampled center height through `p`. On sloped ground its bottom can consequently sit differently relative to the nearby surface. If the cross-product axis is too small, the ring remains unrotated; there is no explicit opposite-normal fallback in this group-marker branch. The function uses the actual upward-facing dune normals produced by the terrain helper rather than adding a general orientation solver.

### Visible Crew Bodies, Strides, and Pickup Lift

**Original source: lines 1262-1278.** The crew loop examines every crew vector entry using its integer index `i`. It only renders `Waiting` and `Running` members. `Inside`, aboard, rescued, or lost crew states do not receive body geometry through this branch. The const reference `c` remains unchanged; rendering works with a copied `crewPosition`.

If `i` equals `g_mission.target` and `g_mission.pickup > 0`, line 1267 raises the copied Y coordinate by `(g_ornPos.y - crewPosition.y - 1.8) * smoothstep(0, 1, pickup)`. At zero eased progress the original height is retained; at full eased progress the crew root reaches `g_ornPos.y - 1.8`. Because the change is made to a copy, it does not overwrite the gameplay crew position. X and Z stay at the member's stored location; this rendering formula only interpolates height.

The crew root is identity translated to that resulting position. `stride` is a degree-valued animation amount: running uses `sin(time * 11 + i) * 24`, while waiting uses `sin(time * 2 + i) * 4`. Index `i` adds a phase offset so all people do not swing together. Running uses a faster, wider movement than the small waiting sway. The input `time` comes from the paused-aware simulation clock in the main loop, not directly from the latest GLFW wall clock after that clock is reassigned.

| Original line | Mesh and person component | T | R | S | Material |
|---|---|---|---|---|---|
| 1270 | Sphere: torso | `(0, 1.05, 0)` | zero | `(0.55, 0.85, 0.4)` | `cloth` |
| 1271 | Sphere: head/helmet | `(0, 1.65, 0)` | zero | `(0.43, 0.46, 0.43)` | `cloth` |
| 1272 | Sphere: front visor | `(0, 1.67, 0.17)` | zero | `(0.32, 0.15, 0.10)` | `visor` |
| 1274 | Cylinder: leg for each `side` | `(side * 0.14, 0.4, 0)` | `(side * stride, 0, 0)` | `(0.17, 0.8, 0.17)` | `cloth` |
| 1275 | Cylinder: arm for each `side` | `(side * 0.34, 1.1, 0)` | `(-side * stride, 0, side * 14)` | `(0.15, 0.7, 0.15)` | `cloth` |
| 1277 | Sphere: overhead locator light | `(0, 2.25, 0)` | zero | `(0.18, 0.18, 0.18)` | `cyan` |

The side loop runs exactly twice, for `-1` and `1`. Leg X rotations have opposite signs on the two sides. Arm X rotations oppose the corresponding leg rotations, while fixed `side * 14` Z rotations splay the arms. These cylinders rotate about their own centers because there is no additional hip or shoulder pivot matrix. There are no separate elbows, knees, hands, feet, or skeletal joints in this function. One visible crew member generates eight draws: torso, head, visor, two legs, two arms, and locator sphere.

No heading rotation is applied to the person root. Regardless of the member's walking direction, the locally positive-Z visor remains oriented according to the same world-axis convention after translation. This is an implementation detail of the primitive figure, not an undocumented facing system. The overhead cyan sphere is attached to the same root, so it rises with the pickup animation.

### Winch Rope Orientation and Sandworm Call

**Original source: lines 1279-1293.** A rope is drawn only when `g_mission.pickup > 0` and `g_mission.target >= 0`. The branch then indexes `g_mission.crew[target]` directly; it does not independently check that the target is less than the vector size. Valid target management is the mission logic's responsibility.

The initial lower endpoint `end` is the target's stored position plus `(0, 1, 0)`. Its Y is eased by `(g_ornPos.y - end.y - 0.8) * smoothstep(0, 1, pickup)`. This is consistent with the lifted person's root: the person root tends toward aircraft Y minus `1.8`, and adding one puts the rope endpoint at aircraft Y minus `0.8`. The upper endpoint `start` is `g_ornPos - (0, 0.7, 0)`. Aircraft roll and pitch are not used to rotate this attachment offset; it is a world-vertical subtraction from the aircraft position.

`dir = end - start` gives the endpoint displacement, `length = length(dir)` gives the desired rope length, and a translation to `(start + end) * 0.5` puts the cylinder center halfway between them. To align the cylinder's original positive-Y axis, the code crosses world up with `dir / length`. A usable cross product causes a rotation by `acos(clamp(dir.y / length, -1, 1))` about the normalized cross-product axis. If that axis is too small and `dir.y < 0`, it instead rotates by `pi` around X, handling a downward parallel rope. If the displacement points upward with an effectively zero cross product, no rotation is necessary.

Line 1289 draws a cyan cylinder with the assembled rope parent, local T and R zero, S `(0.055, length, 0.055)`. Its thickness is fixed while its axial length tracks the endpoints. The code divides by `length` before testing the cross-product length and supplies no zero-length guard. It therefore assumes distinct endpoints; this chapter describes the implementation rather than adding a hypothetical correction. The model is a straight cylinder, not a simulated flexible cable or a segmented rope.

After the optional rope, lines 1291-1292 always call `drawSandworm(wormPosition(), wormAttackTime(), time, g_mission.wormYaw, clamp(elapsed / breachAt, 0, 1))`. These arguments supply current terrain-following worm position, time relative to breach, animation time, worm heading, and normalized pursuit progress. The worm renderer itself decides what to show for those phases; the caller does not suppress the call before breach or after mission results. The separate worm chapter explains that renderer's internal meshes. Line 1293 closes `drawRescueWorld`.

## Master Scene Traversal

### Terrain and Aircraft Visibility

**Original source: lines 1295-1316.** The banner states that `drawFullScene(curTime)` is used twice per frame, for shadow depth and then the main view. It does not advance gameplay or the wing phase; it constructs the scene from already updated state.

The first object is the terrain. A fresh `matSand` gets diffuse `(0.76, 0.49, 0.23)`, specular `(0.25, 0.25, 0.25)`, shininess `16`, and `isSand = 1`, with default zero emission. `drawMeshPrimitive(g_meshTerrain, identity, matSand)` submits the terrain with no extra model translation, rotation, or scaling. The terrain generator has already placed its vertices in scene space.

The aircraft branch runs only if `!g_mission.planeLost`. Its matrix starts as identity translated to `g_ornPos`, then appends Y rotation by `g_ornYaw`, X rotation by `g_ornPitch`, and Z rotation by `g_ornRoll`, each converted from degrees to radians. The composed matrix is passed to the aircraft function along with current phase, roll, pitch, and boost state. The latter two numeric inputs are redundant inside the aircraft function, as noted earlier; the actual bank and pitch are already encoded in this parent matrix. If `planeLost` is true, none of the aircraft's hull, wings, or exhaust draws is submitted here.

### Harvester Terrain Slope and Swallowing Transform

**Original source: lines 1317-1333.** The harvester is rendered only while `wormAttackTime() < 12`. Because attack time is mission elapsed time minus breach time, this includes the entire negative pre-breach interval as well as the first twelve seconds of attack. At exactly `12`, this branch stops drawing it.

Its base height `hy` is the dune height at `(g_harvX, g_harvZ)`. `sink = smoothstep(6, 12, attackTime)` stays zero until attack second six, eases upward between six and twelve, and reaches one at twelve. The root translation uses height `hy - sink * 20`, sinking the machinery by as much as twenty units while it is still being drawn. It then receives the harvester's Y heading in degrees.

Slope alignment is expressed in the vehicle's heading-relative axes. `hnorm` is the sampled dune normal, `forward` is the horizontal forward vector from `harvesterForward(harvesterYaw)`, and `right = cross(forward, (0, 1, 0))`. The X rotation is `atan2(-dot(hnorm, forward), hnorm.y)` and the Z rotation is `atan2(-dot(hnorm, right), hnorm.y)`. Unlike the heading, these inverse-trigonometric angles are already radians and are passed directly to GLM. Using the projections onto forward and right makes the apparent uphill/downhill tilt depend on the crawler's heading instead of blindly applying world-X/world-Z slopes.

An additional X rotation of `sink * 0.8` radians supplies the swallowing tilt. It is not `0.8` degrees: GLM receives it directly. The final harvester matrix includes translation, yaw, terrain pitch, terrain roll, and this extra pitch in that append order. `drawSpiceHarvester` receives it together with `g_mission.harvesterTravel`, which animates tracks and drum according to accumulated travel. The master draw does not itself advance that travel or move the crawler's route.

### Tank Instances, Eight Rock Sites, and Rescue World

**Original source: lines 1334-1354.** Every tank in `g_tanks` is considered separately. Its branch uses `if (wormAttackTime() > 12) continue`, which differs from the harvester's strict `< 12` draw condition. A tank still draws at exactly attack second twelve; the harvester does not. At later times every tank iteration skips the remaining body. This skips rendering, not deletion of tank entries from the vector.

For a drawn tank, terrain height is recomputed from the tank's X/Z coordinates. The stored `tank.pos.y` is not used. The root becomes translation to `(tank.pos.x, terrainHeight, tank.pos.z)`, followed by Y rotation from `tank.yaw` degrees and uniform scale `(tank.scale, tank.scale, tank.scale)`. The vessel function then supplies all its local geometry. No dune-normal tilt is applied to the tanks here; their roots remain upright even on a slope. The comment's description of tanks deployed behind the harvester does not make this loop continuously relocate them with the crawler. Their positions come from the instance data maintained elsewhere.

The rocks use a fixed eight-iteration loop. Each `g_rockSites[i]` is read as a two-dimensional X/Z site: its `.x` becomes `rx` and its `.y` becomes `rz`. The world Y value is the dune height minus `0.5`, partially embedding each formation's root. A site root is translated there and rotated around Y by `i * 45` degrees. The same integer `i` is passed as the formation variant, giving its first rock an additional local `i * 19` Y rotation. Eight sites times four rock meshes yields 32 rock draws per scene traversal.

Finally the function calls `drawRescueWorld(curTime)`, which adds the base, active group markers, visible people, optional rope, and worm. Rescue markers are not rendered as a separate screen overlay here; they are world-space meshes and therefore participate in both full-scene passes. The HUD is drawn later, outside `drawFullScene`, only in the main framebuffer sequence. The closing brace completes the shared scene traversal.

## GLFW Input Callbacks

### Framebuffer Resize and Mouse Recentering

**Original source: lines 1356-1374.** The callbacks section begins with a framebuffer-size function taking an unnamed window pointer and new framebuffer width/height. Only when both `w > 0` and `h > 0` does it update `g_scrW`, `g_scrH`, and the viewport to `(0, 0, w, h)`. Zero-size dimensions, such as can occur while minimizing, are ignored rather than stored. This keeps the last positive dimensions available to projection and screenshot code. Framebuffer dimensions are physical rendering pixels and can differ from the logical window dimensions used for cursor movement.

`centerMouse(win)` obtains logical window width and height using `glfwGetWindowSize`. It immediately zeros `g_mouseHover` and `g_mouseTurnRate`, then places the cursor at `(width * 0.5, height * 0.5)` using floating-point halves. This resets both the instantaneous hover displacement and the smoothed turning state; it is more than a visual mouse reposition. It does not resize the framebuffer, clear held keyboard keys, or alter the aircraft heading.

### Cursor Motion Eligibility and Coordinate Space

**Original source: lines 1375-1387.** Cursor motion is accepted only when the mission is `Flying`, is not paused, and the window reports focus. If any of those conditions fails, both mouse-hover and turn-rate globals are set to zero and the callback returns immediately. Thus moving the mouse at the title screen, during results, while paused, or without focus cannot keep injecting steering through this callback.

For accepted motion, the code queries the current logical window dimensions and calls `setMouseHover(x, y, width, height)`. It deliberately does not use `g_scrW`/`g_scrH`, since cursor coordinates are logical window pixels. The helper validates dimensions and coordinates, converts them to centered hover values, and applies its dead zone; that helper is documented with the mission code. This callback itself neither recenters the mouse after every movement nor hides or locks the pointer. Its model is cursor position relative to the window center, not accumulated relative mouse displacement.

### Key State and Immediate Actions

**Original source: lines 1388-1427.** `key_callback` names the window, key, and action arguments but leaves scan code and modifier flags unnamed because it does not inspect them. For valid integer key indices `0 <= key < 1024`, press sets `g_keys[key]` true and release sets it false. Repeat is not treated as another press in those statements, so it neither toggles features nor changes the held state. A held key remains true from its original press until release or the focus-loss clearing callback.

The rest of the function uses independent `if` statements for the following immediate key actions. Every action below requires `GLFW_PRESS`; holding a key does not repeatedly toggle it through GLFW's repeat action.

| Key | Exact callback behavior | Phase restriction |
|---|---|---|
| Escape | Sets the window should-close flag to true. | None. |
| F | Flips `g_wireframe`; selects `GL_LINE` or `GL_FILL` for front and back polygon faces. | None. |
| C | Advances `g_camMode` with `(g_camMode + 1) % 3`. | None. |
| Enter | From Title calls `resetMission(true)`; from Debrief calls `resetMission(true, true)`; then calls `centerMouse` regardless. | Reset depends on phase; recentering does not. |
| R | Calls `resetMission(true)` and `centerMouse`. | None. |
| M | Calls `g_audio.toggleMute()`. | None. |
| P | Flips `g_mission.paused` and recenters the mouse. | Only Flying. |
| F11 | Toggles the fullscreen flag, switches window monitor mode, then recenters. | None. |

Escape requests termination of the loop but does not immediately delete resources inside the callback. F changes rasterization mode for subsequent geometry, while the HUD later explicitly forces filled polygons for its own draw. C's three values are chase, close view, and overhead in the helper; the word cockpit in the source label does not insert an interior cockpit mesh. Enter while already Flying only centers the mouse, because neither reset condition matches. Enter from Debrief passes the second `true` argument to advance the mission wave, while R retries using the reset function's default second argument. The callback does not apply modifier requirements such as Ctrl or Alt.

F11 always fetches the primary monitor and its current video mode. When entering fullscreen, it attaches the existing window to that monitor at position `(0, 0)`, using the mode's width, height, and refresh rate. When leaving fullscreen, it detaches from any monitor and requests a window at `(100, 100)` with size `1280 x 720` and refresh-rate argument zero. It does not save and restore the prior arbitrary window position or size. This branch assumes that primary-monitor and video-mode queries return usable values; it includes no null check. Resizing and cursor-centering can also trigger the separately registered GLFW callbacks. The function's final brace ends input handling without any mission update call.

## Screenshot File Encoding

### Readback Buffer and Little-Endian Writer

**Original source: lines 1429-1439.** The main-entry section banner precedes `captureScreenshot(path)`. The function allocates `g_scrW * g_scrH * 4` unsigned bytes. The width is converted to `size_t` before multiplication, avoiding a purely signed intermediate for the full allocation expression under the normal positive-dimension invariant.

`glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data())` reads the entire current framebuffer region starting at its lower-left corner. Each pixel contributes blue, green, red, and alpha bytes. The function does not choose a new framebuffer or call `glReadBuffer`; it relies on the rendering state at the call site, where the main framebuffer has been rendered and buffer swapping has not yet occurred. It also does not explicitly set pixel pack state. At four bytes per pixel, every row length is a multiple of four, compatible with the usual default pack alignment.

It opens the requested path as a binary `std::ofstream`. If opening fails, it returns false immediately. Readback has already taken place before this check. The function does not create missing parent directories or infer an image format from the filename extension. Its local `word(value, bytes)` lambda emits `bytes` least-significant bytes of a `uint32_t` in order, using right shifts by `i * 8`, a mask of `255`, and `out.put`. That implements little-endian integer serialization independently of directly writing a C++ header struct with platform padding.

### BMP Header, Row Direction, and Result

**Original source: lines 1440-1446.** The output is a 32-bit uncompressed BMP. The first two bytes are literal `B` and `M`. The following fields are written in this exact order.

| Header field | Bytes | Assigned value and meaning |
|---|---|---|
| File signature | `2` | `BM`, identifying a Windows bitmap file. |
| Total file size | `4` | `54 + pixels.size()`, cast to `uint32_t`. |
| Reserved fields | `4` combined | Zero for both reserved 16-bit fields. |
| Pixel-data offset | `4` | `54`, immediately after the two headers. |
| DIB header size | `4` | `40`, the BITMAPINFOHEADER layout. |
| Width | `4` | `g_scrW`. |
| Height | `4` | Positive `g_scrH`, specifying bottom-up storage. |
| Planes | `2` | `1`. |
| Bits per pixel | `2` | `32`. |
| Compression | `4` | `0`, uncompressed BI_RGB. |
| Pixel-data byte count | `4` | `pixels.size()`, cast to `uint32_t`. |
| Horizontal resolution | `4` | Zero. |
| Vertical resolution | `4` | Zero. |
| Palette colors used | `4` | Zero. |
| Important palette colors | `4` | Zero. |

OpenGL supplies the bottom framebuffer row first, and positive BMP height also expects bottom-up rows, so no explicit vertical flip is needed. Four bytes per pixel make rows naturally aligned for BMP storage without added row padding. The BGRA byte order matches the pixel layout written here. BI_RGB at 32 bits stores the fourth byte, but this routine does not add alpha bit masks or promise that every bitmap viewer treats that byte as usable transparency.

Line 1444 writes the entire byte vector as raw data after the header. It casts the pointer to `const char*` and the length to `std::streamsize` to match the stream API. `return out.good()` reports the stream's status after these writes; the local stream closes on scope exit. The function does not test for an OpenGL readback error itself or perform image-content validation. Sizes are serialized into 32-bit fields without a separate oversized-image guard. It is a screenshot of what was rendered, including the HUD when called from the frame loop, not a scene-only render or a PDF export.

## Application Startup

### Command-Line Dispatch and GLFW Initialization

**Original source: lines 1448-1459.** `main(argc, argv)` first checks whether `argv[1]` is exactly `--test-game`. If so, it directly returns `runMissionTests()`, bypassing GLFW initialization, window creation, GLAD loading, audio initialization, and the render loop. The tests are implemented in `tests/arrakis_tests.h` in the organized project. This chapter does not duplicate their individual assertions.

Otherwise it sets four booleans by comparing only the first argument. `smokeTest` corresponds to `--smoke-test`, `breachTest` to `--smoke-breach`, `mouseTest` to `--smoke-mouse`, and `pursuitTest` to `--smoke-pursuit`. `renderTest` is their logical OR. Since each equality tests the same single string, normally only one mode can be selected. There is no general command-line parser, combined-flag scan, help option, or validation of unrecognized arguments. With an unrecognized first argument, the normal interactive path is followed. An optional second argument is considered later only as the screenshot path for render-test modes; additional arguments are not used in this code.

| First argument | Startup/runtime specialization |
|---|---|
| `--test-game` | Runs mission tests and returns their result without graphics startup. |
| `--smoke-test` | Starts a flying mission and renders with fixed simulation steps for the bounded test run. |
| `--smoke-breach` | Additionally advances mission state to seven seconds after its breach time and uses the breach camera. |
| `--smoke-mouse` | Supplies synthetic horizontal mouse positions, holds W, cycles camera modes, and checks camera heading. |
| `--smoke-pursuit` | Advances mission state to `110` seconds and uses a wider harvester/worm pursuit camera. |
| No recognized first argument | Starts in the ordinary title-screen path. |

If `glfwInit()` fails, the program writes `Failed to initialize GLFW` to standard error and returns `-1`. No GLFW window or OpenGL resources have been created by the following startup sequence yet. This failure is separate from the later render-test failure status, which uses exit code one.

### Context Hints, Monitor Information, and Window Fallback

**Original source: lines 1460-1485.** GLFW hints request OpenGL version `3.3` with a core profile. The program gets the primary monitor and its current video mode, then copies that mode's red, green, and blue bit depths and refresh rate into window hints. It also requests four multisample samples. These are requested capabilities; GLFW and the underlying platform determine the actual context/window characteristics. No explicit alpha-bit, depth-bit, stencil-bit, or hidden-window hint is set in this range.

Although the monitor's properties are queried, the first `glfwCreateWindow` uses `nullptr` for its monitor argument, so startup is windowed. It uses the current global dimensions, initially `1280 x 720`, with the title `ARRAKIS | Harvester Down - Desert Rescue`, and `nullptr` for the shared-context argument. The comment explicitly favors desktop accessibility and leaves fullscreen switching to F11. Render smoke tests also use this windowed creation path rather than a special headless or hidden window.

If creation returns null, the program prints `Window creation failed, falling back to windowed mode...`, clears `g_isFullscreen`, sets the dimensions to `1280 x 720`, and attempts another window titled `ARRAKIS`, again with no monitor and no shared context. The fallback message does not mean the first attempt was fullscreen: both calls are windowed in the inspected implementation. The second attempt changes the title and resets the dimensions but does not clear the previously requested context, color, refresh, or sample hints. If it also fails, GLFW is terminated and `main` returns `-1`.

Primary-monitor and mode pointers are dereferenced without a null guard in this startup range, just as they are in F11 handling. This is an assumption of the actual code, not a fallback for a machine with no available monitor.

### Context Activation, Callback Registration, and Focus Loss

**Original source: lines 1486-1505.** The successful window's OpenGL context is made current. Its framebuffer size is read into `g_scrW` and `g_scrH`, so rendering dimensions reflect actual framebuffer pixels rather than only the requested logical window size. The named framebuffer-size, key, and cursor-position callbacks described above are registered next.

Three additional callbacks are inline lambdas. The cursor-enter callback ignores both its window pointer and the entered/left flag and always zeros `g_mouseHover` and `g_mouseTurnRate`. It therefore clears steering on either entry or exit. The window-size callback ignores the supplied new size arguments and calls `centerMouse(win)`, which queries the current logical size itself. The focus callback only acts when `focused` is false: it clears every element of the 1024-entry key array using `std::fill`, zeros the two mouse steering values, and sets mission pause true if the mission is Flying.

On focus regain, that callback has no corresponding resume branch. A flying mission remains paused until another action, such as P, changes the flag; a render test later forces it false on each normal frame. Clearing held keys avoids a key remaining logically down after a release event occurs outside focus. The focus callback does not close the window, reset the mission, mute the audio flag, or save a special focus-loss result state.

`glfwSwapInterval(1)` requests vertical synchronization against the current context. It governs swap timing, not the physics step size or the number of mission substeps. GLAD entry points are loaded only afterward. GLFW callback registration does not itself invoke the OpenGL viewport callback here; event processing is deferred to the later loop.

### GLAD, Audio Initialization, Diagnostic Banner, and GL State

**Original source: lines 1506-1526.** The GLAD loader is called with GLFW's procedure-address function cast to `GLADloadproc`. If it fails, the program prints `Failed to initialize GLAD`, calls `glfwTerminate`, and returns `-1`. This path does not reach the final normal resource cleanup block. GLFW termination handles GLFW-owned window lifetime, but no application geometry or shader resources have yet been created by the later lines.

`g_audio.init()` runs after GLAD succeeds. Its boolean return value is not checked by `main`, so an audio initialization failure is not itself fatal to graphics startup. The audio subsystem records whether it initialized and supports its silent-mode path; its implementation and asset lookup are covered separately. Organizing the audio files under `assets/audio/` concerns that subsystem's file locations, not extra arguments passed here.

Six output statements print separator lines, the game title, renderer string from `glGetString(GL_RENDERER)`, OpenGL version from `glGetString(GL_VERSION)`, and current framebuffer resolution. These diagnostics describe the active driver and framebuffer, not a benchmark. The program does not inspect these strings to select a different renderer or shader variant.

Initial OpenGL state enables depth testing and multisampling, chooses `GL_LEQUAL` for depth comparison, enables face culling, and selects back-face culling. Less-or-equal allows a fragment at exactly the stored depth to pass. Individual render phases temporarily change selected state later; they do not reinitialize all possible OpenGL settings every frame. Blend remains unenabled by this initialization block until the particle stage explicitly enables it.

### Procedural Meshes and Shader Programs

**Original source: lines 1527-1543.** Startup generates and uploads the reusable scene meshes in a fixed order. `createCube()` creates the box used for structural details; `createCylinder(24)` requests 24 radial slices; `createSphere(16, 24)` uses 16 latitude and 24 longitude subdivisions; and `createWingBlade()` creates the tapered aerofoil reused by all aircraft wings. `createDuneTerrain(260, 260, 900.0)` builds the broad terrain using those grid and extent arguments. The actual startup values here take precedence over looser descriptions elsewhere in source comments.

`createQuad()` supplies the billboard mesh for particle rendering. `createRing()` supplies landing and evacuation markers, and `createErodedRock()` supplies rock formations. `initSandwormMeshes()` initializes the worm renderer's own resources. Each result is stored in a corresponding global mesh so multiple objects and frames can reuse it. These calls happen once during startup rather than regenerating primitives inside every `drawPart`.

Four `buildProgram` calls compile/link the scene, shadow, particle, and sky programs from their respective vertex and fragment shader strings: `SCENE_VERT/SCENE_FRAG`, `SHADOW_VERT/SHADOW_FRAG`, `PARTICLE_VERT/PARTICLE_FRAG`, and `SKY_VERT/SKY_FRAG`. The resulting IDs are stored in `g_progScene`, `g_progShadow`, `g_progParticle`, and `g_progSky`. The preceding rendering chapter explains the individual shader expressions and mesh-generation loops; this range connects those resources to the application's lifecycle.

### Shadow/Sky/HUD Resources and Particle Instance Attributes

**Original source: lines 1544-1563.** `setupShadowFramebuffer()` creates the depth-rendering target, `setupSkyVAO()` prepares the sky's screen-filling triangle resources, and `initRescueHud()` prepares HUD resources. Immediately afterward, startup checks both `g_shaderOkay` and the HUD resource program ID. If any earlier shader build reported failure or the HUD program is zero, it destroys the window, terminates GLFW, and returns `1`.

This early failure path does not run the comprehensive cleanup at the bottom of `main`: it has no explicit calls here to delete the already created meshes/programs or shut down audio. Context destruction and process termination end their usable lifetime, but the code's explicit successful-path deletion sequence is not executed. The condition is also not a full audit of every OpenGL object or framebuffer; it checks the displayed shader status and HUD program specifically.

`initParticles()` initializes the CPU particle collection. The program generates `g_particleInstanceVBO`, binds the quad VAO, and binds that new buffer as `GL_ARRAY_BUFFER`. It initially allocates `MAX_PARTICLES * sizeof(ParticleInstance)` bytes with null data and `GL_STREAM_DRAW`. The earlier constant is `MAX_PARTICLES = 2400`. Each `ParticleInstance` contains two `glm::vec4` fields, `posSize` and `colorAlpha`.

The two-iteration loop enables vertex attribute locations `3` and `4`. Both attributes contain four `GL_FLOAT` values, are not normalized, use `sizeof(ParticleInstance)` as stride, and have offsets `0` and `sizeof(glm::vec4)` respectively. The offset is passed using a `reinterpret_cast<void*>`, which OpenGL interprets as a byte offset into the currently bound VBO rather than a CPU pointer to read. Setting each divisor to one makes its value advance once per drawn instance rather than once per quad vertex. The quad's existing per-vertex attributes remain attached to the same VAO.

After unbinding the VAO, `g_particleInstances.reserve(MAX_PARTICLES)` reserves CPU vector capacity without filling it. `resetMission(renderTest)` initializes the world: normal interactive startup receives false and opens at Title; any recognized render-test mode receives true and starts Flying. `centerMouse(g_window)` then clears steering and places the pointer at the logical center. This order ensures reset mission state exists before the camera and timing state are initialized.

### Render-Test Fast-Forward and Environment Constants

**Original source: lines 1564-1578.** Breach and pursuit modes advance the mission before ordinary frames begin. `targetTime` is `g_mission.breachAt + 7` for breach mode and `110.0` for pursuit mode. `steps = int(ceil(targetTime * 120))`, and the loop calls `updateMission(targetTime / steps)` exactly that many times. This supplies roughly 120 simulation updates per target second instead of passing the entire interval as one unstable update.

It then places the aircraft at `harvesterPosition() + (35, 16, 40)` and sets `g_mission.altitude = 16`. The direct position is a world offset from the harvester; the altitude field records a separate intended terrain-following height. This range does not reset velocity, yaw, or all remaining mission fields after fast-forward. It also does not explicitly assign `elapsed = targetTime`: elapsed state is whatever the repeated float-valued mission updates produced, subject to their phase checks and floating-point accumulation. Other render-test modes do not execute this pre-advance block.

Lighting constants are local to `main`: `sunDir` is the normalized vector `(-0.35, 0.12, -1.0)`, `sunColor` is `(1.15, 1.04, 0.85)`, `ambientColor` is `(0.34, 0.38, 0.44)`, `fogColor` is `(0.90, 0.74, 0.47)`, and `fogDensity` is `0.0022`. Normalizing the direction keeps subsequent dot products and light-camera placement independent of the vector's original magnitude. Sun color values above one allow a bright warm contribution; these constants are not clamped by `main`. Fog supplies both the framebuffer clear color and the scene shader's atmospheric color. No day/night clock or runtime light-color change appears here.

### Initial Clocks, Camera Damping State, and Test Counters

**Original source: lines 1579-1593.** `prevTime` records the current GLFW time as a float. `camPosSmoothed` starts at aircraft position plus `(0, 26, 42)`, and `camTargetSmoothed` starts at the aircraft itself. This initial position is a world-space offset, not yet the helper's heading-relative chase placement. The first normal camera update below replaces X/Z placement with the actual current offset while smoothing height and target.

`simulationTime` starts at `g_mission.elapsed`, including any pre-advanced test state. It is a separate local animation clock, not an alias of the mission timer. `fps` starts at `60`, `frameCount` at zero, and `cameraRevision` copies `g_missionRevision`. `renderFailed` starts false. `smokeStarted` records a fresh double-precision GLFW timestamp after resource setup and any fast-forward work; therefore the printed render-test duration later excludes those earlier initialization costs.

The banner at lines 1591-1593 introduces the main loop. There is no second thread or scheduler in this section: event handling, simulation stepping, audio parameter updates, both geometry passes, particles, and the HUD are driven sequentially by the loop.

## Frame Timing and Simulation

### Window Lifetime, Minimize Wait, and Delta Clamp

**Original source: lines 1594-1606.** The loop condition continues while GLFW's should-close flag is false. On an iconified/minimized window, it calls `glfwWaitEvents()`, updates `prevTime` to the current time after that wait, and immediately continues. That iteration performs no ordinary polling, mission update, audio update, rendering, buffer swap, screenshot, or render-test frame-count increment. Resetting `prevTime` avoids counting the entire minimized wait as the next physics interval.

On a normal iteration, `curTime` initially means GLFW wall time, `dt = curTime - prevTime`, and `prevTime` is updated before events are polled. If `dt > 0.1`, it is reduced to `0.1`; there is no corresponding lower clamp in these lines. `realDt` copies this already capped value. Thus the FPS estimate later uses the capped interval, not the uncapped duration of an unusually slow frame. The clamp bounds simulation work and jump size at low rendering rates but deliberately discards excess elapsed wall time rather than accumulating a catch-up backlog.

`glfwPollEvents()` delivers callbacks near the beginning of a normal frame. A callback can reset the mission, pause it, change rendering mode, or request close before subsequent operations. Setting should-close during polling does not instantly jump out of this body: the existing frame still continues unless other control flow changes it, and the next loop-condition check ends the run.

### Fixed Render-Test Input and FPS Smoothing

**Original source: lines 1607-1618.** Every render-test frame replaces `dt` with `1 / 60` and forces `g_mission.paused = false`. It clears `g_mouseHover` but does not clear all keys or explicitly zero `g_mouseTurnRate` at that point. The synthetic test time is independent of actual wall time and VSync timing; `realDt` still retains the earlier measured, capped interval for FPS display.

Mouse mode queries logical window dimensions and calls `setMouseHover` at 85 percent of the width for `frameCount < 60`, or 15 percent otherwise, always halfway down the window. Frames zero through 59 therefore steer from the right side; frames 60 through 119 use the left side. It sets the W key state true and selects `g_camMode = (frameCount / 40) % 3`, testing mode zero for the first 40 frames, one for the next 40, and two for the last 40. No synthetic release of W is needed after completion because the test closes the application; the code does not restore prior input state mid-run.

The FPS value is updated with `mix(fps, 1 / max(realDt, 0.0001), 0.03)`. Only three percent of the new instantaneous estimate is incorporated each frame, giving a damped display. The minimum denominator protects against division by zero and caps the instantaneous estimate at `10000`. Because `realDt` has already been capped above at `0.1`, an extremely long frame supplies an estimate of ten rather than its truly lower uncapped FPS. This statistic is display telemetry, not a pass/fail performance threshold.

### Bounded Mission Substeps, Debrief Time, and Particle Updates

**Original source: lines 1619-1630.** Simulation work runs only when the mission is not paused. It computes `steps = max(1, int(ceil(dt / (1 / 120))))`, then invokes `updateMission(dt / steps)` repeatedly. At the normal render-test `dt = 1 / 60`, this is approximately two substeps per rendered frame. At the maximum `0.1` interval it is approximately twelve. The `max(1, ...)` prevents a zero-iteration loop for a zero interval. Every substep receives an equal fraction of the frame interval.

The mission helper itself rejects updates outside Flying or while paused, so entering this outer block does not force title or debrief gameplay to advance. After the substep loop, however, if the phase is Debrief, the code adds the full `dt` to `g_mission.elapsed`. This allows breach/swallow/retreat visuals to continue behind the results UI. If a substep changes the phase to Debrief during the frame, the same post-loop addition still occurs; it is not prorated to only the leftover fraction of that frame.

The separate `simulationTime` always advances by `dt` inside the unpaused block, even on Title or Debrief. On Title, `g_wingPhase` additionally advances by `18 * dt`, keeping the preview aircraft's wings moving despite the mission helper not running flight gameplay. During normal flight the mission helper manages the wing phase. `updateParticles(dt, g_ornPos, harvesterPosition())` runs once per frame, not once per mission substep, with current aircraft and crawler positions.

Finally `curTime = simulationTime` repurposes the local variable. All later rendering in that frame uses the simulation animation clock, not the initial GLFW timestamp. Pausing freezes this clock, wing/title advancement, and particles, while the later render and audio call paths still execute. Resetting the mission does not reassign this local clock after its startup initialization, so visual animation phase can continue across retries even as mission elapsed resets. Minimization skips the block entirely rather than merely setting its `dt` to zero.

## Camera System

### Mission Revision and Keyboard Elevation

**Original source: lines 1631-1643.** The camera section compares its saved revision with `g_missionRevision`. Mission resets increment that global. On a mismatch, it updates the saved value, puts `camPosSmoothed` at aircraft position plus `(0, 14, 38)`, and sets `camTargetSmoothed` directly to the aircraft. This prevents the new mission camera target from easing all the way from an obsolete world location. It is a different initial height/offset from the first startup `(0, 26, 42)` values.

If not paused, Up raises `g_camOrbitPitch` by `40 * dt` degrees but caps it at `45`; Down lowers it by the same rate but floors it at `-10`. These are separate ordered statements, not an `else if`. If both keys are held, both adjustments run, and the bounds can affect the outcome near the limits. This block changes the camera's elevation-control variable, not the aircraft's physical pitch. While paused, held arrow keys do not advance that variable here.

### Normal and Test Camera Targets

**Original source: lines 1644-1654.** The default target is aircraft position. `flightCameraOffset(g_camMode)` computes the current heading-relative offset, and the default desired position is aircraft plus that offset. The helper's actual conventions are relevant because source labels call mode one cockpit even though its implementation is a close third-person offset.

The helper forms `behind = -flightForward()`. Mode two is `behind * 26 + (0, 100, 0)`. Other modes use a pitch angle derived from `clamp(12 - mouseHover.y * 22 + g_camOrbitPitch, 6, 65)` degrees. Mode one uses distance `20` and an additional `2.2` vertical offset; mode zero uses distance `40` with no such addition. The horizontal distance is `distance * cos(pitch)`, and the height is `distance * sin(pitch)` plus the mode-one addition. All three remain behind the aircraft in X/Z rather than becoming an actual first-person eye inside its hull.

Breach mode overrides the target to `harvesterPosition() + (0, 14, 0)` and desired position to `harvesterPosition() + (65, 48, 80)`. Pursuit mode, in an `else if`, aims at the midpoint of harvester and worm plus `(0, 6, 0)` and places its desired eye at that target plus `(65, 30, 155)`. If no specialized test camera is active, the helper's ordinary aircraft target and offset remain. The test overrides change desired position and target but do not overwrite the separately computed `camOffset`; the next stage handles their X/Z values explicitly.

### Terrain Clearance and Heading-Preserving Smoothing

**Original source: lines 1655-1669.** First, desired camera Y is raised if necessary to at least `getDuneHeight(desiredX, desiredZ) + 1.5`. The target is not terrain-clamped by this statement. Camera height then eases toward desired height with coefficient `1 - exp(-9 * dt)`. Target position eases toward `camTarget` with the faster coefficient `1 - exp(-12 * dt)`.

Camera X and Z are not independently eased from their previous world positions. They are assigned `camTargetSmoothed.x + camOffset.x` and `camTargetSmoothed.z + camOffset.z`. Because the horizontal offset is exactly behind the aircraft's current heading, this preserves the view direction's horizontal alignment even when the target itself lags slightly in translation. Independently smoothing old world X/Z positions could leave the aircraft visibly side-on during turns; the source comments explain why this alternative is not used.

For breach or pursuit modes, X and Z are then assigned directly from `camDesiredPos` instead. Their height still uses the easing calculation, and their target still eases. A second terrain safety check clamps the final smoothed eye Y to at least terrain height at its actual X/Z plus `2`. This second threshold is higher than the earlier `1.5` desired-position threshold and applies after horizontal reassignment, so it protects the actual rendered eye rather than only the requested one.

The smoothing calculations occur even while paused because the outer pause check only surrounds the keyboard elevation change, not this matrix preparation. With a positive measured `dt`, a paused camera can finish settling toward a stationary target. No terrain-based camera-target obstruction test, wall collision, or automatic zoom is implemented in this range.

### Mouse-Test Heading Check and View/Projection Matrices

**Original source: lines 1670-1682.** In mouse smoke mode, `viewForward = camTargetSmoothed - camPosSmoothed` has its Y component set to zero. The code normalizes that horizontal direction and compares its dot product with `flightForward()` against `0.9999`. A smaller value prints `Camera heading diverged from aircraft`, sets `renderFailed`, and requests window close. It does not immediately break the frame body, so rendering and final accounting can still continue for that iteration. There is no zero-vector normalization guard in this test, although the ordinary camera helper supplies a nonzero behind offset for its three modes.

`glm::lookAt(camPosSmoothed, camTargetSmoothed, (0, 1, 0))` creates the viewing matrix with world Y as up. The aspect ratio is framebuffer width divided by framebuffer height as floats. The projection is perspective with a 58-degree vertical field of view, that aspect ratio, near distance `0.4`, and far distance `1200`. These values are fixed in the frame loop; none is changed by C or fullscreen except that a framebuffer resize changes aspect. The positive-size framebuffer callback ordinarily preserves a usable denominator during minimization, which is also skipped earlier in the loop.

## Spatial Audio Handoff

### All Eighteen Update Arguments

**Original source: lines 1683-1704.** The runtime first asks `nearestCrewPosition()` for a sound-interest position, then measures its horizontal distance from `g_ornPos`. The mission helper considers Running and Waiting crew and falls back to harvester position when no suitable closer candidate is found. That fallback means the returned point need not be a real crew member, and the caller sends it anyway. Distance is horizontal X/Z distance rather than a full camera-to-source or three-dimensional aircraft-to-person distance.

The single `g_audio.update` call occurs after camera calculation and before shadow rendering. Its arguments appear in the following exact order.

| Position | Supplied expression | Meaning at the call boundary |
|---|---|---|
| 1 | `dt` | Current frame update interval; fixed to `1/60` for render tests. |
| 2 | `camPosSmoothed` | Listener eye position from the final terrain-safe camera. |
| 3 | `camTargetSmoothed` | Listener look target. |
| 4 | `g_ornPos` | Aircraft sound-source position. |
| 5 | `g_ornSpeed` | Current flight speed input. |
| 6 | `g_isBoosting` | Current boost boolean. |
| 7 | `harvesterPosition()` | Current harvester sound-source position. |
| 8 | `wormPosition()` | Current worm sound-source position. |
| 9 | `wormGap()` | Harvester-to-worm pursuit gap supplied by mission logic. |
| 10 | `wormAttackTime()` | Elapsed time relative to breach, negative before breach. |
| 11 | `g_mission.rescued` | Delivered-rescue count. |
| 12 | `g_mission.aboard` | Current onboard crew count. |
| 13 | `g_mission.pickup` | Pickup/winch progress. |
| 14 | `nearestCrewAudio` | Nearest eligible crew point or helper fallback. |
| 15 | `nearestCrewDistAudio` | Horizontal aircraft distance to that point. |
| 16 | `g_mission.base` | Base position for delivery-related audio. |
| 17 | `phase == Flying` | Whether gameplay is currently Flying. |
| 18 | `g_mission.paused` | Pause state, passed rather than suppressing the call. |

Camera position and target make listener orientation match what the user sees. Mission counters and progress let the subsystem recognize transitions rather than merely play a uniform ambient loop. `wormGap()` is not the listener's distance to the worm; the separate worm position enables spatial positioning. The caller does not pass mute because the audio object already owns its `muted` field, which M toggles.

This call still happens on Title, Debrief, and paused rendered frames; `isFlying` and `isPaused` convey the correct phase. The audio implementation can return early when uninitialized or silence its master engine while paused/muted. Minimized frames do not reach it because the iconified branch continues earlier. The main source does not manually start every sound here or check an audio-update return status. The audio chapter describes volumes, pitch changes, one-shot detection, and asset loading in `src/arrakis_audio.h` rather than attributing those internal actions to this call site.

## Shadow Rendering Pass

### Depth Target and Player-Centered Light Camera

**Original source: lines 1705-1721.** The first render pass sets `g_isShadowPass = true`, changes the viewport to `SHADOW_RES x SHADOW_RES` (`2048 x 2048` from the earlier constant), binds `g_shadowFBO`, and clears its depth buffer. It does not clear a color buffer because this target is used for shadow depth. The pass flag makes the shared mesh helper choose shadow-specific model/surface uniforms.

The shadow camera follows the aircraft's X/Z area, even in breach or pursuit smoke modes where the view camera may look elsewhere. `shadowCenter` starts at aircraft position but replaces Y with the dune height at the aircraft X/Z, so the light aims at ground under the player rather than the aircraft's altitude. `shadowBoxSize` is `90`. The orthographic light projection spans `[-90, 90]` in both X and Y, with near/far distances `2` and `320`, giving a 180-unit-wide square coverage area in light-view coordinates.

The light eye is `shadowCenter + sunDir * 120`. A look-at matrix points it at `shadowCenter` using world Y up. `lightSpaceMatrix = lightProj * lightView` transforms a world-space position into the light's projected space. There is no perspective narrowing or per-object light camera, and no second cascade for distant terrain in this range. Following the player prioritizes shadow detail near flight and rescue action rather than uniformly covering the entire 900-unit terrain extent.

### Shadow Uniforms, Collapse Synchronization, and Bias

**Original source: lines 1722-1736.** The shadow shader becomes current. Its `lightSpaceMatrix` uniform receives one matrix, without transpose, using `glm::value_ptr`. The program then stores `wormCenter = wormPosition()` and `collapse = smoothstep(7, 14, wormAttackTime())`, sending those to shadow uniforms `wormCenter` and `collapse`.

The collapse scalar is zero through attack second seven, eases to one between seven and fourteen, and remains one afterward. It differs from the harvester sink interval of six to twelve. The preceding shader implementation uses these values for the affected terrain treatment. Computing them once here and reusing them in the scene pass keeps the shadow geometry's deformation parameters synchronized with visible terrain. `wormCenter` is a saved frame value used later for HUD coordinates as well.

The shader's terrain lowering applies only to `isSand == 1`. Rock materials marked `2` do not receive that terrain displacement. The scene shader lowers affected terrain vertices by `collapse * (1 - smoothstep(5, 48, distanceFromWormXZ)) * 13`, but the CPU `getDuneHeight` queries used for object placement, camera clearance, and HUD altitude still describe the original dune surface. The shader also retains the uploaded normal rather than recomputing a crater normal. Consequently the synchronized color/depth crater is a visual deformation, not a replacement CPU terrain or a general physical ground-collapse model. The separately sinking harvester gets its own explicit root transform.

The code enables `GL_POLYGON_OFFSET_FILL` and sets `glPolygonOffset(2.5, 4.0)` before drawing the scene. Polygon offset combines a slope-dependent contribution with a fixed depth-unit contribution, biasing filled depth polygons to reduce self-shadow acne. It does not change each model matrix or shift geometry in world coordinates. The mode is specifically fill-offset; the user-selected line rasterization mode is not forcibly replaced for this pass by these statements.

`drawFullScene(curTime)` submits terrain, any visible aircraft/harvester/tanks, rocks, rescue world, and worm to this shadow target using the current simulation time. Particles and the screen HUD are not included because they are outside that traversal. The code disables polygon-offset fill afterward and binds framebuffer zero to return to the default window framebuffer. It does not delete or copy the shadow texture; the next pass samples the existing texture written by this pass.

## Main Rendering Pass

### Framebuffer Clear and Atmospheric Sky

**Original source: lines 1737-1758.** The main-pass banner begins by setting `g_isShadowPass = false` and restoring the viewport to the actual framebuffer dimensions. `glClearColor(fogColor.r, fogColor.g, fogColor.b, 1)` chooses the warm fog color with alpha one, and `glClear` clears both color and depth. This resets the main render surface independently of the shadow map's depth buffer.

For the sky, face culling is disabled and depth writes are disabled with `glDepthMask(GL_FALSE)`. Depth testing itself remains enabled from startup. The sky program receives `lightDir = sunDir`, `sunColor`, and `invViewProj = inverse(proj * view)`. The inverse transforms projected sample directions into the view/world representation used by the sky shader; no sky cube, external panorama texture, or vehicle model is loaded by this stage.

The runtime binds `g_skyVAO` and issues `glDrawArrays(GL_TRIANGLES, 0, 3)`, a three-vertex screen-filling triangle. The shader generates the desert atmosphere described in the shader chapter. Afterward, depth writes are reenabled and face culling is enabled again. Those are explicit known-state settings, not restoration of queried prior values. The sky is only drawn here, not into the shadow map. If global wireframe mode is active, this range does not force filled sky polygons; filled-mode forcing is reserved for the HUD later.

### Scene Matrices, Lighting/Fog Uniforms, and Shadow Texture

**Original source: lines 1759-1780.** The scene program becomes current. All frame-wide uniforms are assigned before the shared object traversal supplies each model/material.

| Uniform | Upload value | Type and count |
|---|---|---|
| `view` | Camera `view` matrix | One mat4, no transpose. |
| `projection` | Perspective `proj` matrix | One mat4, no transpose. |
| `lightSpaceMatrix` | Current shadow projection/view product | One mat4, no transpose. |
| `lightDir` | Normalized `(-0.35, 0.12, -1)` | One vec3. |
| `sunColor` | `(1.15, 1.04, 0.85)` | One vec3. |
| `ambientColor` | `(0.34, 0.38, 0.44)` | One vec3. |
| `viewPos` | Final `camPosSmoothed` | One vec3. |
| `fogColor` | `(0.90, 0.74, 0.47)` | One vec3. |
| `fogDensity` | `0.0022` | One float. |
| `wormCenter` | Position saved during the shadow pass | One vec3. |
| `collapse` | The same attack-time smoothstep scalar | One float. |
| `shadowMap` | Texture unit index `0` | One integer sampler selection. |

Each location is looked up from `g_progScene` at the call site using `glGetUniformLocation`; these frame-wide locations are not cached by this block. Matrix and vector pointers come from `glm::value_ptr`. The usual OpenGL behavior for location `-1` is an ignored uniform write; this code does not separately assert the presence of each named uniform. The material helper adds model transform and surface-specific uniforms for every mesh.

Before `drawFullScene`, the runtime activates `GL_TEXTURE0`, binds `g_shadowTex` as `GL_TEXTURE_2D`, and assigns sampler `shadowMap` to integer zero. The sampler receives a texture-unit number, not the texture object's numeric ID. The shared scene draw then reuses the same visible-object conditions and transforms as the depth pass, now shaded with light, ambient color, shadows, material properties, and fog. The block does not update mission state between passes, so drawing twice does not move the harvester or advance rescue progress twice.

### Particle Blend State and Billboard Camera Basis

**Original source: lines 1781-1796.** The particle stage enables blending, chooses `GL_SRC_ALPHA` and `GL_ONE_MINUS_SRC_ALPHA`, disables depth writes, and disables culling. Depth testing remains on, so dust can be hidden behind already drawn solid geometry while avoiding opaque depth marks that would incorrectly block later transparent particles.

The particle program receives `view` and `projection`. Camera-right is extracted as `(view[0][0], view[1][0], view[2][0])`, and camera-up as `(view[0][1], view[1][1], view[2][1])`. GLM indexes a matrix by column and then row, so these expressions gather the desired view-basis rows instead of merely taking one entire stored column. They are uploaded to uniforms `camRight` and `camUp` to orient quad billboards toward the camera.

This stage uses one mesh and per-instance data rather than invoking `drawPart` for each grain of dust. It does not sample the shadow texture through a scene-material draw, create new CPU particle motion, or emit additional particles here. Particle updates already occurred once in the earlier unpaused simulation block.

### Particle Fades, Sorting, Buffer Upload, and Instanced Draw

**Original source: lines 1797-1814.** The CPU instance vector is cleared every frame without discarding its reserved capacity. For every `p` in `g_particles`, it computes `fade = clamp(p.life / 0.5, 0, 1) * clamp((p.maxLife - p.life) / 0.3, 0, 1)`. Since `life` is remaining life, the first factor fades out during the last half-second. The second uses elapsed age, fading in during the first `0.3` seconds. The product limits both ends of the lifespan smoothly through linear clamped factors.

Each push stores `posSize = vec4(p.pos, p.size)` and `colorAlpha = vec4(COL_DUNE_SAND, p.alpha * fade)`. All particles share the dune-sand RGB `(0.76, 0.49, 0.23)` at this call site; their category-dependent differences are position, size, base alpha, life, and velocity from the particle simulation. The loop pushes every entry, including one whose resulting alpha is zero, rather than culling fully transparent entries here.

The vector is sorted with a comparator that subtracts camera position from each instance position and compares squared distances via `dot(delta, delta)`. A larger distance sorts before a smaller one, giving far-to-near order for alpha compositing. Squared distance avoids a square root without changing distance ordering. It is radial world-space distance, not exact projected eye-depth ordering, and `std::sort` does not promise stable order for equal-distance entries.

The program binds the particle instance VBO and replaces its data store with exactly `instances.size() * sizeof(ParticleInstance)` bytes using `GL_STREAM_DRAW`. This is a new `glBufferData` upload rather than a per-particle `glBufferSubData` series; the driver receives the whole sorted frame's instance array. The quad VAO is then bound and `glDrawElementsInstanced` uses triangle topology, the quad's index count, unsigned-int indices at offset null, and the vector size converted to `GLsizei` as instance count. Attribute divisors configured at startup provide one position/size and color/alpha record per quad instance.

After drawing, the runtime restores depth writes, disables blending, and enables culling. Those changes prepare the known scene state for HUD handling and the next frame. No glow accumulation buffer or post-processing smoke compositor is present. Here smoke-like plumes are ordinary sand-colored blended billboards; the term smoke test later refers to automated render execution and is not this dust particle system.

## HUD State Assembly

### Crew Counts, Markers, and Basic Metadata

**Original source: lines 1815-1827.** A new `RescueHudState hud` is constructed every frame, using its header-defined default member values. Width and height become current framebuffer dimensions. Rescued, aboard, lost, best, and wave are copied from the mission, and muted is copied from `g_audio.muted`. The latter displays the mute toggle state, not whether an audio device initialized successfully.

`hud.inside` is explicitly reset to zero because its default in the HUD structure is 36. The crew loop increments it for each `Inside` member. For `Running` or `Waiting`, it increments `hud.waiting` and appends `(c.pos.x, c.pos.z)` to `crewMarkers`. The new HUD object's waiting counter already starts at zero and marker vector starts empty, so no explicit clearing statement is needed. Other crew states are not placed on this marker list.

These markers use each crew member's stored gameplay position, not the temporary lifted Y position in `drawRescueWorld`; they are two-dimensional map points, so their Y would not be used anyway. The HUD struct's total-crew value remains its default 36 because the main loop does not assign it in this range. The loop counts actual states each frame instead of deriving all numbers by subtracting rescued/lost/aboard from that total.

### Timers, Flight Values, Phase Flags, and Map Coordinates

**Original source: lines 1828-1846.** Every following assignment connects an explicit gameplay or runtime value to the display state.

| HUD field | Supplied value | Exact interpretation |
|---|---|---|
| `missionDuration` | `g_mission.breachAt` | Current wave's planned pre-breach duration. |
| `pursuit` | `clamp(elapsed / breachAt, 0, 1)` | Normalized chase progress, saturated after breach. |
| `wormDistance` | `wormGap()` | The pursuit gap, not camera distance. |
| `extractionRemaining` | `max(0, 60 - wormAttackTime())` | Time relative to attack second sixty; before breach it can exceed sixty. |
| `releasedGroups` | Count of all groups with `released` true | Historical release count, regardless of whether their marker remains visible. |
| `remaining` | `max(0, breachAt - elapsed)` | Nonnegative pre-breach countdown. |
| `altitude` | `aircraftY - getDuneHeight(aircraftX, aircraftZ)` | Actual aircraft terrain clearance, not merely the mission's target-altitude field. |
| `speed` | `g_ornSpeed` | Current flight speed. |
| `boost` | `g_mission.boost` | Current boost reserve. |
| `pickupProgress` | `g_mission.pickup` | Winch progress. |
| `unloadProgress` | `g_mission.unload` | Base unloading progress. |
| `fps` | Smoothed local `fps` | Runtime's damped frame-rate estimate. |
| `prompt` | `g_mission.prompt` | Mission-generated contextual text. |
| `heading` | `g_ornYaw` | Aircraft yaw in the game's degree convention. |
| `title` | `phase == Title` | Enables the HUD's title presentation. |
| `ended` | `phase == Debrief` | Enables results presentation. |
| `paused` | `g_mission.paused` | Current pause display flag. |
| `player` | `(g_ornPos.x, g_ornPos.z)` | Aircraft map coordinates. |
| `base` | `(g_mission.base.x, g_mission.base.z)` | Base map coordinates. |
| `harvester` | `(g_harvX, g_harvZ)` | Current crawler map coordinates, even if hidden after swallowing. |
| `worm` | `(wormCenter.x, wormCenter.z)` | Position cached earlier in this frame. |
| `pickup` | X/Z of a fresh `nearestCrewPosition()` result | Nearest eligible crew or the helper's harvester fallback. |
| `attackTime` | `wormAttackTime()` | Signed elapsed time relative to breach. |

`releasedGroups` is explicitly initialized to zero before counting. The loop does not test whether a released group still has waiting survivors; that filtering is specific to world marker visibility. Before breach, attack time is negative, so `60 - attackTime` exceeds sixty. This caller applies only a lower clamp to extraction remaining, not an upper clamp to sixty. The HUD renderer may format or selectively display values, but the passed field is exactly this expression.

There is no division-by-zero check around `elapsed / breachAt` in the caller. Normal mission reset supplies a positive duration. Altitude also receives no lower clamp here, so if aircraft Y were below the mathematical terrain height, the raw clearance would be negative. The display renderer's formatting rules are separate from these assignments.

### Filled HUD and Restored Wireframe Selection

**Original source: lines 1847-1850.** Immediately before `drawRescueHud(hud)`, the main loop sets both front and back polygon modes to `GL_FILL`. This ensures text, panels, and HUD geometry stay solid even when F selected wireframe for world geometry. It then restores front/back mode to `GL_LINE` or `GL_FILL` according to `g_wireframe`.

The HUD renderer internally manages its required OpenGL state as described in its own chapter. These three caller statements add a specific rasterization guarantee around that draw. They do not switch the scene back to normal mode permanently or alter `g_wireframe` itself. The HUD is last in the normal rendering order, after sky, solids, and dust, so the later screenshot call includes its final visible result.

## Render Smoke Tests and Frame Presentation

### Error Sampling, Last-Frame Screenshot, and Swap

**Original source: lines 1851-1866.** The section banner mentions buffer swap and event polling, but the actual polling operation already occurred near the beginning of the frame. This tail block performs test checks and swaps; it does not contain another `glfwPollEvents()`.

Only render-test modes call `glGetError()` here. If the returned enum is not `GL_NO_ERROR`, standard error receives `OpenGL error in frame <frameCount>: <error>`, `renderFailed` becomes true, and the window is marked for closing. A single `glGetError` call retrieves one error value; this block does not drain a queue of multiple errors or turn each OpenGL operation into a checked call. It also does not compare screenshot pixels, validate object visibility, or impose a frame-time limit. An error can cause an early exit rather than completing all 120 planned frames.

If `frameCount == 119` and `argc > 2`, it calls `captureScreenshot(argv[2])`. Thanks to left-to-right short-circuit evaluation, screenshot capture is not attempted before the final planned frame, without a second argument, or in normal interactive mode. Since the counter begins at zero, frame index 119 is the 120th rendered test frame. A failed capture prints `Screenshot write failed` and sets `renderFailed`; it does not add a separate immediate close call, because the frame limit closes the test afterward anyway. The screenshot is taken before swapping, after the HUD draw, while the default framebuffer is still the current render target.

`glfwSwapBuffers(g_window)` presents the completed frame in both normal and test modes. In render-test mode, `++frameCount` is then evaluated and a count at least 120 sets should-close true. In non-test mode, short-circuit evaluation of `renderTest && ...` means `frameCount` is not incremented by this statement at all. A heading or GL error that already requested closure still allows this swap and test increment for the current body. If the user closes a smoke-test window early, the loop exits with fewer frames; no separate assertion here changes an otherwise false `renderFailed` solely because the count is less than 120.

With an uninterrupted test, the fixed `1/60` update interval gives approximately two simulated seconds over 120 rendered frames, in addition to breach/pursuit pre-advance where applicable. The actual wall-clock duration depends on rendering, swap synchronization, driver behavior, and any event waits. No timer-based retry or unattended restart is present.

### What the Smoke Test Result Does and Does Not Mean

**Original source context: lines 1449-1454, 1564-1570, 1607-1617, 1670-1677, and 1854-1865.** The smoke modes exercise real graphics initialization and rendering rather than merely unit-testing state functions. All run the ordinary geometry, both render passes, particles, HUD assembly, and audio calls. Mouse mode additionally checks horizontal view alignment against the aircraft. Breach/pursuit modes select their requested mission times and viewpoints so those scenes are rendered during the bounded run.

A reported pass means `renderFailed` remained false under the checks this program actually performs. It is not proof that every pixel is correct, all requested assets played, no earlier unchecked startup assumption failed, or a user-closed test reached exactly 120 frames. Screenshot success means the file stream accepted the bitmap writes, not that the bitmap contents were compared with a reference image. These distinctions preserve the existing implementation's actual test semantics without claiming hypothetical visual-regression coverage.

## Resource Teardown and Exit

### Programs, Targets, Renderer Resources, and Mesh Buffers

**Original source: lines 1868-1885.** After the loop ends normally, cleanup occurs while the window's context still exists. The four main shader programs are deleted in scene, shadow, particle, and sky order. The shadow framebuffer and depth texture are deleted. The particle instance VBO, sky VBO, and sky VAO are deleted next.

`cleanupSandwormMeshes()` releases the worm renderer's resources, and `cleanupRescueHud()` releases HUD resources. Those helpers own resources distinct from the eight basic globals in the following loop. No draw is attempted after these cleanup calls.

The range-based loop then iterates pointers to `g_meshCube`, `g_meshCylinder`, `g_meshSphere`, `g_meshWing`, `g_meshTerrain`, `g_meshQuad`, `g_meshRing`, and `g_meshRock`. For each, it deletes the VAO, VBO, and EBO using that object's stored IDs. A VAO describes attribute/index bindings, while the VBO and EBO hold vertex and index data; deleting only the VAO would not release those buffer objects, so all three deletions are explicit. The order is VAO first, vertex buffer second, element buffer third.

The cleanup does not manually clear the CPU particle vectors, tank vector, or mission crew containers. Their ordinary C++ lifetime ends with program termination. It also does not reset every deleted global ID to zero in this loop because no new render cycle follows. Startup failures that returned before reaching the loop do not run this bottom-of-main sequence, as documented with those branches.

### Audio Shutdown, Window Destruction, Timing Output, and Return

**Original source: lines 1886-1892.** `g_audio.shutdown()` runs after the graphics-resource deletions. The window is then explicitly destroyed. In render-test mode, standard output prints `Render smoke test: ` followed by `FAILED` if `renderFailed` is true or `PASS` otherwise, then the frame count and `(glfwGetTime() - smokeStarted)` seconds.

The time is queried after window destruction but before `glfwTerminate`, while GLFW timing remains available. Because `smokeStarted` was taken after setup and fast-forward, this duration measures the render-loop interval plus its normal resource cleanup, audio shutdown, and window destruction, not full process startup. The frame count is the number incremented at the end of rendered test frames; iconified waits do not increment it.

Finally `glfwTerminate()` shuts down GLFW, and `main` returns `1` for a recorded render failure or `0` otherwise. Normal interactive execution ordinarily leaves `renderFailed` false because the failure-setting render checks are guarded by test modes, so its normal exit returns zero. The earlier `--test-game` return, GLFW/window/GLAD `-1` failures, and shader/HUD startup failure `1` remain separate exit paths. The final brace at original line 1892 ends `main` and the inspected source file.

## Coverage Ledger

### Original Lines Accounted For

This ledger is an index to the detailed explanations above. Blank separator lines have no executable behavior. Every nonblank line in the requested original interval is covered, including descriptive banners, includes, declarations, assignments, branches, loops, calls, lambda bodies, return statements, and closing braces. Line references are deliberately original-source references rather than promises about numbering after relocation.

| Original range | Covered source responsibility |
|---|---|
| 968-1001 | Ornithopter banner, signature, inputs, six materials, boost emission selection. |
| 1002-1018 | Central pod, nose, probe, canopy/frame, paired intakes. |
| 1019-1035 | Four gimbals, turbine bodies/nozzles, conditional exhaust length and both exhaust draws. |
| 1036-1048 | Three tail sections, paired canted fins, paired landing skids. |
| 1049-1081 | Wing banner, waveforms, five-field definition, all eight blade entries. |
| 1082-1101 | Wing iteration, ordered matrices, twist, mirrored span, culling, draw, function end. |
| 1103-1119 | Harvester banner/signature, six materials, hopper and upper deck. |
| 1120-1139 | Track offsets, nested unit loops, housing, wheel/bar loop, ten-pad ellipse and rotations. |
| 1141-1153 | Cutter, door material, door, ramp, luminous scoop, two separators. |
| 1154-1168 | Bridge, viewport, floodlights, exhaust stacks/rims, harvester function end. |
| 1170-1192 | Tank banner/signature, four materials, skids/crosspieces, two-station support loop. |
| 1193-1210 | Vessel cylinder, two full spherical heads, bands, hatch, valve, sight indicator, function end. |
| 1212-1225 | Rock banner/signature, strata materials/category, four transformed rock meshes, function end. |
| 1227-1245 | Worm/HUD includes, rescue signature/materials, base root, pad/ring/cross, four beacons. |
| 1246-1261 | Group eligibility, member counting, terrain-normal alignment, amber ring/pole/light. |
| 1262-1278 | Crew eligibility, visual pickup lift, stride selection, complete body and locator draws. |
| 1279-1293 | Rope eligibility/endpoints/length/orientation, cyan cable draw, worm arguments, function end. |
| 1295-1316 | Master banner/signature, terrain material/draw, aircraft loss branch and attitude matrix. |
| 1317-1333 | Harvester visibility boundary, sinking, heading-relative slope rotations, swallowing tilt/draw. |
| 1334-1354 | Tank boundary and roots, eight terrain-embedded rock sites, rescue traversal, function end. |
| 1356-1374 | Callback banner, positive framebuffer resize, logical cursor recentering/reset. |
| 1375-1387 | Cursor eligibility, early reset/return, logical dimensions and hover helper. |
| 1388-1427 | Key bounds/press/release, Escape/F/C/Enter/R/M/P/F11, fullscreen branch, function end. |
| 1429-1439 | Entry banner, screenshot allocation/readback/open check, little-endian lambda. |
| 1440-1446 | Complete BMP header, raw pixels, stream status and screenshot function end. |
| 1448-1459 | Main signature, mission-test early return, four render flags, GLFW failure. |
| 1460-1485 | GL version/profile and monitor hints, multisampling, first window and fallback/error branch. |
| 1486-1505 | Current context/framebuffer size, named callbacks, three lambda callbacks, VSync. |
| 1506-1526 | GLAD failure path, audio startup, diagnostic output, initial depth/MSAA/cull state. |
| 1527-1543 | Eight reusable meshes, worm mesh initialization, four shader-program builds. |
| 1544-1563 | Shadow/sky/HUD setup, startup shader failure, particles/VBO/attributes/divisors/reserve/reset. |
| 1564-1578 | Breach/pursuit fixed-substep pre-advance, aircraft placement, sun/ambient/fog constants. |
| 1579-1593 | Initial timing, smoothed camera/animation/FPS state, revision/failure/count/timer, loop banner. |
| 1594-1606 | Loop condition, iconified wait/continue, wall-time delta/cap/copy, event polling. |
| 1607-1618 | Render-test fixed dt/unpause, synthetic mouse/W/camera modes, FPS filtering. |
| 1619-1630 | Bounded mission updates, debrief clock, animation time, title wings, particles, clock handoff. |
| 1631-1643 | Camera banner, revision reset, pause-gated Up/Down elevation bounds. |
| 1644-1654 | Aircraft target/helper offset, breach and pursuit override branches. |
| 1655-1669 | Desired terrain safety, height/target smoothing, heading-locked X/Z, test X/Z, final clearance. |
| 1670-1682 | Mouse heading invariant/error/close, view, framebuffer aspect, perspective projection. |
| 1683-1704 | Nearest crew/distance calculation, all eighteen spatial-audio arguments. |
| 1705-1721 | Shadow banner/flag/viewport/FBO/depth clear, player-ground center, ortho and light matrices. |
| 1722-1736 | Shadow program/uniforms, worm/collapse, polygon bias, complete scene, target unbind. |
| 1737-1758 | Main banner/flag/viewport/clear, sky states/uniforms/inverse matrix/triangle/state restoration. |
| 1759-1780 | Scene program and all frame uniforms, unit-zero shadow texture, complete scene traversal. |
| 1781-1796 | Particle blend/depth/cull state, matrices, camera-right/up uniforms. |
| 1797-1814 | Instance clearing/fade/packing/sorting, stream upload/instanced quad draw, state reset. |
| 1815-1827 | Fresh HUD, size/counters/mute, inside and outside crew counting/map markers. |
| 1828-1846 | HUD timers/progress/gap/groups/flight data/text/flags/map coordinates/attack time. |
| 1847-1850 | Filled HUD draw and user wireframe mode restoration. |
| 1851-1866 | Presentation banner, render GL error, optional last-frame BMP, swap/count/close, loop end. |
| 1868-1885 | Four programs, shadow objects, particle/sky resources, worm/HUD cleanup, eight mesh deletions. |
| 1886-1892 | Audio shutdown, window destruction, test result/time output, GLFW termination, return/file end. |

The requested interval ends at the end of the original file; there is no further scene or runtime code after original line 1892. This chapter explains that interval without modifying source behavior, adding features, or duplicating the final full-source appendix.
