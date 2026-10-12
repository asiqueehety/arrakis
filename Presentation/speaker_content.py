NOTES = {
    1: """Explain: Introduce ARRAKIS: Harvester Down - Desert Rescue, a C++17 and OpenGL 3.3 core real-time graphics project. The player flies an ornithopter to rescue 36 harvester workers before and during a sandworm attack. Eight seats require repeated pickup and delivery trips; only workers delivered to the cyan base count as secured.

Point out: Identify the presenter as Asique Ehetasamul Haque, roll 2107096, Year 4, Semester 1, Group B2, Department of CSE, KUET. The course is CSE 4102, Computer Graphics and Image Processing Laboratory. Connect the desert scene to the three central goals: recognizable procedural objects, mathematical motion, and meaningful player interaction.

Demonstrate: Use the opening gameplay image to identify the aircraft, dunes, and worm threat. Slides 3-4 supply enlarged harvester and safe-base views. Describe the task rather than waiting for a full mission during the introduction.

Implementation: The application combines reusable generated meshes, GLSL shaders, mission state, custom HUD, and spatial audio. Source responsibilities are expanded on Slide 2.

Accuracy: The project models and visual surface details are generated in code, not imported models or image textures. This statement does not include the externally loaded audio files. The deck contains exactly ten slides; the existing demonstration video is external, not embedded.""",
    2: """Explain: Follow the presentation from object construction to curved geometry, coordinated motion, controls and cameras, rendering, feedback, and evaluation. The rescue loop is fly to an amber group, hover low and slowly, winch workers aboard, return to the cyan base, unload, and repeat while the worm closes in.

Point out: Link the source modules to that loop: src/arrakis.cpp owns meshes, object assembly, shaders, callbacks, and the render loop; arrakis_game.h owns mission and rescue rules; arrakis_worm.h owns the detailed worm; arrakis_hud.h owns instrumentation; arrakis_audio.h owns sound. Tests and CMake provide verification and build integration.

Demonstrate: Trace one frame through input, bounded mission updates, camera and audio updates, shadow depth rendering, procedural sky, lit opaque geometry, sorted transparent dust, HUD, and buffer swap. Distinguish CPU state updates from GPU vertex and fragment processing.

Implementation: Mesh positions, normals, UVs, and triangle indices are uploaded through VAO/VBO/EBO resources. Model, view, and projection matrices connect assembled objects to the camera, while the light-space matrix supports shadows.

Accuracy: The main scene renders directly to the default framebuffer. Tone mapping is inside the surface shader, not a separate bloom or post-processing pipeline. Later slides give the detailed evidence; this outline is not an extra feature slide.""",
    3: """Explain: This first object contact sheet inventories the environment and industrial vehicles. The terrain is a sampled continuous dune height field, the aircraft is an articulated assembly, the harvester is a moving crawler, and the tanks and rocks establish recognizable desert infrastructure. Different materials make reused primitives read as different objects rather than identical blocks.

Point out: On the aircraft, identify the ellipsoidal fuselage and canopy, forward sensor probe, intake cowls, wing gimbals, eight tapered aerofoil blades, twin turbines and nozzles, emissive exhaust, tail boom, V-tail, and landing skids. On the harvester, identify the hopper and refinery decks, four track units, road wheels, circulating tread pads, cutter drum, side exit ramp, separator funnels, bridge viewports, lamps, stacks, and spice-colored vent rims.

Demonstrate: Read the labels against the actual close-ups. The three pressure tanks use a horizontal cylinder, overlapping rounded end heads, reinforcing bands, hatch, relief valve, inclined cradle supports, skid beams, and an emissive sight tube. Each of eight rock sites assembles four differently transformed eroded-rock pieces with sediment-like shader bands. The terrain supports all these objects, rather than serving as a flat backdrop.

Implementation: src/arrakis.cpp builds cube, cylinder, UV sphere, wing, terrain, and eroded-rock meshes, then drawPart applies parent-relative translation, rotation, and scale. Startup uses createDuneTerrain(260,260,900): 68,121 vertices and 135,200 triangles. Cylinder caps have separate normals; rock triangles use faceted normals.

Accuracy: Contact-sheet close-ups are staged isolated views using the exact project draw routines and actual models, not independently remodeled illustrations or ordinary gameplay moments. Tank positions and sight-tube emission are constant; there is no player deployment or pulsing level simulation. The canopy and bridge glass are shiny opaque materials, and emissive lamps are not spotlights.""",
    4: """Explain: The second object slide connects the rescue markers and worker models to the sandworm threat. The large mouth view should reveal a real recessed cavity, not a flat dark circle. Separate recognizable objects from the effects that communicate activity and danger around them.

Point out: Identify the worm's ridged deformable body, thick irregular lip, inward-facing funnel, deeply recessed throat, and six concentric rows of curved pointed teeth. The row counts are 192, 184, 176, 168, 152, and 128, totaling 1,000 teeth. The mouth narrows toward its recessed closure, leaving the visible opening unobstructed. Four sand-colored mound shapes form a wake behind the approaching worm.

Demonstrate: Identify the cyan base's circular pad, luminous ring, cross markings, and four beacon poles. Contrast these safe-base cues with an amber evacuation ring and its pole-top beacon. A worker uses ellipsoidal torso, head and visor, cylindrical arms and legs, and a small cyan overhead marker. During active pickup, the targeted worker rises and a thin cyan winch cylinder joins the aircraft to the worker. Amber group markers disappear when no running or waiting crew remain in that group.

Implementation: src/arrakis_worm.h creates five indexed meshes: body, lip, funnel, throat, and teeth. The body uses 192 longitudinal rows and 96 angular slices; revolved mouth profiles use 160 slices. Only the flexible body is streamed when the pose changes, and both shadow and scene passes share the pose. drawRescueWorld in src/arrakis.cpp constructs the base, markers, workers, and winch; marker rings align to the local terrain normal.

Accuracy: These are generated project objects shown through the shared draw routines. Worker close-ups can be staged for inspection. Dust is transparent billboard geometry, whereas the wake mounds are solid sphere-based objects. Workers aboard or already safe are state records, not separately rendered seated passengers or a populated interior cockpit.""",
    5: """Explain: Relate each equation to its visible object. With angles in radians, the raw dune height is h0=28*sin(.009*x+.004*z)+19*cos(.004*x-.012*z)+5*(1-abs(sin(.028*x+.016*z)))^2+1.8*sin(.065*x+.038*z)-4. The first terms create broad dunes, the squared term creates ridges, and the last adds small undulations.

Point out: The actual terrain adds base leveling: b=1-smoothstep(15,25,distance((x,z),(55,55))); h=(1-b)*h0+b*h0(55,55). An ellipsoid is (a*sin(theta)*cos(phi),b*cos(theta),c*sin(theta)*sin(phi)); the source sphere has radius .5 before scaling. Cylinder sides are (.5*cos(phi),y,.5*sin(phi)), y in [-.5,.5], with separate cap fans. The torus is ((1+.025*cos(beta))*cos(alpha),.025*sin(beta),(1+.025*cos(beta))*sin(alpha)), matching the base and evacuation rings.

Demonstrate: Follow the worm surface P(t,a)=C(t)+(R(t)+grain)*(cos(a),-Tz*sin(a),Ty*sin(a)), where T is the unit centerline tangent. Below t=.4, C is the cubic Bezier sum (1-s)^3*P0+3*(1-s)^2*s*P1+3*(1-s)*s^2*P2+s^3*P3. Above the join, C=end-(0,span*cos(mid),span*sin(mid)), span=64*(1-s)*sinc(tilt*(1-s)/2), mid=tilt*(1+s)/2. The radius blends 13.9 plus a .70-amplitude, 32-cycle cubed-cosine ridge toward 15*aperture at the mouth.

Implementation: Each tooth centerline is radial*(r-reach*u)+sideways*curl*u^2+(0,depth+.65*sin(pi*u)-drop*u^2,0). Sweep five-sided cross sections in its tangent frame with radius width*(1-u)^.85, then add a true apex. Source references are rawDuneHeight/createSphere/createCylinder/createRing in arrakis.cpp and deformBody/makeTeeth in arrakis_worm.h.

Accuracy: sinc(q)=sin(q)/q with its limiting value at zero. Curves are sampled triangle meshes, not exact analytic raster surfaces. Terrain normals use finite differences with epsilon .4; worm normals include bend and ridge derivatives. Increasing sampling density improves the approximation but does not change its mathematical definition. Distinguish a curved centerline from a straight cylinder merely rotated into place. The crater is a separate GPU displacement described on Slide 8.""",
    6: """Explain: Dynamic scenes combine continuous mathematical motion with discrete mission events. For flight, v'=v+(vDesired-v)*(1-exp(-k*dt)), then p'=p+v'*dt; k is 5.5 normally or 12 while winching. Normal and boost targets are 28 and 48 units/second. Heading smoothing is analytically integrated; bank, pitch, altitude, and camera elevation use exponential damping rather than abrupt changes.

Point out: Wing angles are 34*sin(phase) and 34*sin(phase+.75*pi) degrees, with mirrored signs, fixed sweep/dihedral, and 8*cos(phase)*side pitch twist. phase advances at 23 radians/second normally and 36 while boosting. The harvester follows x=-40+.62*t+15*sin(.045*t), z=-55+22*sin(.032*t)+9*sin(.071*t); yaw=degrees(atan2(-dx/dt,-dz/dt)). Accumulated horizontal travel drives tread pads, wheel details, and the cutter. Its body follows dune height and local slope.

Demonstrate: Read the four-stage worm filmstrip as approach, rise, lunge/swallow, and retreat. Approach gap interpolates 220 to 18 units. Relative to breach, rise uses smoothstep(0,6), lunge smoothstep(6,12), and retreat smoothstep(13.5,18); the harvester sinks 20 units during seconds 6-12 and then disappears. Crew batches contain 1-4 workers, released at 6+7*j seconds at the route's scheduled position. Workers run at 3.8 units/second; limb angles use 24*sin(11*time+i) while running or 4*sin(2*time+i) while waiting.

Implementation: arrakis_game.h handles release, pursuit, proximity losses, and rescue states. arrakis_worm.h uses the smoothed stages to drive height, reach, tilt, and aperture. Normal missions currently have a 150-second pursuit followed by at most 60 seconds of extraction, with earlier closure possible. Nearby low aircraft and exposed crew can be lost; remaining inside crew are lost at attack second 12.

Accuracy: resetMission currently uses max(150,10-(wave-1)*10), so ordinary waves do not progressively shorten. The report's older 210-to-150 progression is stale. This is authored kinematics and timed hazard logic, not a general physics or navigation engine.""",
    7: """Explain: The player can control translation, heading, height, boost, winching, mission state, and view selection. W/S provide forward/backward thrust; A/D strafe; Q/E raise/lower requested terrain-relative altitude. Mouse horizontal displacement and Left/Right arrows steer. Mouse vertical displacement and Up/Down arrows change chase-camera elevation, not aircraft pitch directly.

Point out: Space brakes to ten percent of the movement target, lowers requested hover altitude toward six units, disables boost, and enables pickup or unloading. Left Shift boosts while moving; the reserve drains at .22/second and refills at .14/second, with a low-reserve lock requiring Shift release. C cycles wide chase, close chase, and tactical overhead. Enter starts or advances after debrief; R retries; P pauses; M mutes; F toggles wireframe; F11 toggles fullscreen; Escape exits.

Demonstrate: Compare all three camera images. Wide chase uses distance 40, close chase distance 20 with an additional 2.2 vertical offset, and overhead uses 26 behind plus 100 above. For a rescue, approach within 12 horizontal units of a running or waiting worker, stay at or below 11 units of ground clearance and below speed 6, and hold Space for .42 seconds per person. Capacity is eight. Return within 14 horizontal units of base and meet the same low/slow conditions for a 1.2-second unload of the entire cabin. Only unloading increases the secured score.

Implementation: arrakis_game.h implements setMouseHover, updateMission, flightCameraPitch, and flightCameraOffset; src/arrakis.cpp supplies callbacks and lookAt/perspective matrices. Mouse steering has a normalized .12 deadzone. The camera shares the aircraft's heading exactly while its target and elevation are damped, and it stays above terrain. Aircraft altitude requests are clamped to 4-55 with extra obstacle clearance.

Accuracy: The close preset is external third-person, not a modeled cockpit. The cursor is window-position steering, not a captured free-look mouse. Invalid pickup conditions or a changed target reset progress; boarding alone does not save a worker from a subsequent aircraft loss.""",
    8: """Explain: Rendering adds readable form, depth, atmosphere, and activity without imported image textures. The pipeline uses a shadow depth pass followed by procedural sky, lit opaque objects, transparent dust, and HUD. A 2048-by-2048 shadow map follows the player; the scene samples a 5-by-5 PCF kernel with receiver-slope correction and bias to soften edges and reduce acne.

Point out: Materials combine warm ambient light, directional sunlight, Blinn-Phong specular, and emission. Sand uses wrapped diffuse max((N dot L+.35)/1.35,0), plus distance-faded sine ripples, slope-based coloring, perturbed normals, and sparse quartz sparkle. Rocks use procedural grain and height-dependent strata. Emission brightens beacons, spice indicators, and exhaust without making them separate lights. The sky shader draws a gradient and sun disc/halo from a full-screen triangle.

Demonstrate: Compare shadowed and sunlit areas, close sand detail and distant fog, then identify the worm crater and several dust sources. Fog weight is 1-exp(-(distance*density)^2). Surface tone mapping is pow(lit/(lit+.8),1/1.8), followed by fog blending. Crater displacement is -13*collapse*(1-smoothstep(5,48,d)), with collapse=smoothstep(7,14,attackTime); both scene and shadow vertex shaders apply it.

Implementation: src/arrakis.cpp maintains 2,400 CPU-updated particles with lifetime, velocity, ground contact, respawn, and gust modulation. Four emission categories are ambient wind, crawler plume, worm wake/breach, and winch downwash. Sorted instance records provide position/size and color/alpha for camera-facing quads; one glDrawElementsInstanced call renders them with alpha blending and depth writes disabled. Fade-in/out and a procedural soft mask replace particle image textures. Four-sample MSAA is requested, VSync is enabled, and F exposes mesh wireframe.

Accuracy: The crater is GPU-only; CPU terrain queries and collision clearance do not acquire the depression, and precomputed terrain normals are not rebuilt for it. Fog is analytic, shadows cover a finite region, and dust is billboards rather than volumetric simulation. There is no bloom, cloud system, thumper, true spotlight, imported model, or bitmap material texture; the sampled shadow map stores generated depth.""",
    9: """Explain: Feedback makes rescue state observable. The HUD shows secured, aboard, inside, sand, lost, groups, wave, best, pursuit, worm range, altitude, speed, FPS, boost, mute status, action prompts, pickup/unload progress, and breach/extraction timers. Title, pause, and debrief overlays explain the mission and distinguish secured workers from those merely aboard.

Point out: The north-up radar marks the aircraft with an oriented cyan triangle, base with a cyan outline, every running/waiting worker in amber, harvester in white, and worm in red. Text uses an in-code 5-by-7 glyph pattern emitted as colored rectangles, not a font image. Responsive layouts wrap controls and prompts, move or hide secondary panels, and preserve readable text sizes. The HUD streams a dynamic vertex buffer and saves/restores relevant OpenGL state.

Demonstrate: Follow a pickup and delivery counter change, then compare the HUD in different window sizes. Audio has eight spatial sound roles: ornithopter_flight, ornithopter_boost, worm_rumble, worm_breach, harvester_engine, rescue_winch, rescue_safe, and crew_help. The camera is the listener; inverse-distance attenuation, speed-dependent flight pitch, smoothed boost volume, worm proximity volume, and event-triggered cues reinforce the visuals. Missing files are skipped; engine failure permits silent operation. M or pause reduces master volume to zero, not playback suspension.

Implementation: arrakis_hud.h and arrakis_audio.h implement these systems. Focus loss clears held inputs and pauses flying; resize recenters steering; fullscreen is switchable; minimized windows wait for events. Frame dt is capped at .1 seconds and mission updates subdivide to at most 1/120 second. CMake builds C++17 with OpenGL, GLFW, GLAD, GLM, and miniaudio, copies assets/audio beside the executable, and registers RescueMissionLogic in CTest. The debugger uses the executable directory. The LaTeX report and source references document the pipeline.

Accuracy: Fresh verification of the current build passed 23 of 27 logic cases across 1,057 checks, with five failed assertions in four cases. All four smoke modes passed 120 frames each. Do not claim all tests pass: the changed timer violates older expectations. Render smokes establish limited execution coverage, not complete gameplay, subjective graphics quality, or audible output on every machine.""",
    10: """Explain: Close with Thank You and the three demonstrated requirements: recognizable procedural objects and curved geometry, coordinated mathematical motion and changing scenes, and controllable interaction supported by rendering, feedback, and audio.

Point out: Slides 3-5 provide object and equation evidence; Slides 6-7 provide motion and control evidence; Slides 8-9 provide additional features and honest evaluation. The project report is available alongside the presentation. The already available external demonstration video can be played after this closing slide; no video is embedded in the deck.

Demonstrate: Summarize the complete rescue loop once: find amber crew, board within eight-seat capacity, deliver to the cyan base, and avoid the worm. Use the external recording for sustained motion rather than claiming that a static screenshot proves player input.

Implementation: The outcome combines original generated models, shader-generated surface detail, authored animation, mission state, and an OpenGL frame pipeline. Technical references are in the existing LaTeX report and project source.

Accuracy: Acknowledge opaque glass, constant decorative tanks, GPU-only crater displacement without CPU terrain changes, and the current constant 150-second timer with failing legacy logic assertions. There is no interior cockpit, general physics/navigation engine, bloom, clouds, thumper, or spotlight. The assignment's undefined Karpov objects term is unsupported and is not claimed as an implemented feature.""",
}


FEATURE_COVERAGE = {
    "Project title and desert rescue objective": 1,
    "Author Asique Ehetasamul Haque and roll 2107096": 1,
    "Year 4 Semester 1 Group B2 Department of CSE KUET": 1,
    "CSE 4102 Computer Graphics and Image Processing Laboratory": 1,
    "C++17 and OpenGL 3.3 core": 1,
    "36-worker rescue mission and eight-seat aircraft": 1,
    "Original generated models and procedural visual appearance": 1,
    "Presentation outline": 2,
    "Fly pickup return unload repeat rescue loop": 2,
    "Source module responsibilities": 2,
    "CPU input state camera audio and GPU frame pipeline": 2,
    "VAO VBO EBO indexed mesh upload": 2,
    "Model view projection and light-space transforms": 2,
    "Desert terrain object and 260-by-260 grid over 900 units": 3,
    "Ornithopter fuselage nose sensor and opaque canopy": 3,
    "Ornithopter intake cowls and wing gimbals": 3,
    "Eight tapered aerofoil wing blades": 3,
    "Twin turbines nozzles and emissive exhaust": 3,
    "Aircraft tail boom V-tail and landing skids": 3,
    "Harvester hopper and upper refinery decks": 3,
    "Four harvester track units road wheels and tread pads": 3,
    "Harvester cutter drum and spice intake scoop": 3,
    "Harvester crew exit door and ramp": 3,
    "Harvester separator funnels bridge and opaque viewports": 3,
    "Harvester emissive lamps stacks and vent rims": 3,
    "Three fixed spice pressure tanks": 3,
    "Tank rounded heads cylinder bands hatch and relief valve": 3,
    "Tank transport cradle skid beams and inclined supports": 3,
    "Constant emissive spice sight tubes": 3,
    "Eight rock sites with four eroded pieces each": 3,
    "Parent-relative reusable primitive assembly": 3,
    "Generated cube cylinder sphere wing and eroded-rock meshes": 3,
    "Faceted rock normals and separate cylinder-cap normals": 3,
    "Actual draw-routine isolated model contact sheets": 3,
    "Sandworm five-mesh object assembly": 4,
    "Ridged deformable worm body": 4,
    "Irregular worm lip inward funnel and recessed throat": 4,
    "Six tooth rows totaling 1000 curved pointed teeth": 4,
    "Four solid sphere-based worm wake mounds": 4,
    "Cyan base pad ring cross and four beacon poles": 4,
    "Amber evacuation rings and pole beacons": 4,
    "Terrain-normal-aligned evacuation markers": 4,
    "Worker torso head visor limbs and overhead marker": 4,
    "Active pickup worker elevation and winch cylinder": 4,
    "Group markers disappear when outside crew are gone": 4,
    "Dynamic worm body streaming and shared render-pass pose": 4,
    "Exact raw dune height-field equation": 5,
    "Smooth base terrace blend": 5,
    "Ellipsoid and UV sphere parameterization": 5,
    "Cylinder side and cap construction": 5,
    "Torus major radius 1 and minor radius 0.025": 5,
    "Worm radial sweep and tangent frame": 5,
    "Buried cubic Bezier worm centerline": 5,
    "Constant-curvature 64-unit worm neck": 5,
    "Worm 32-cycle ridge radius and aperture blend": 5,
    "Curved tooth centerline tangent frame taper and true apex": 5,
    "Finite-difference terrain and derivative worm normals": 5,
    "Damped flight velocity and position integration": 6,
    "28 normal and 48 boosted movement targets": 6,
    "Analytically integrated damped aircraft heading": 6,
    "Bank pitch height and camera damping": 6,
    "34-degree wing oscillation and 0.75-pi phase offset": 6,
    "23-to-36-radians-per-second wing phase rate": 6,
    "Eight-degree wing pitch twist sweep and dihedral": 6,
    "Harvester trigonometric route and derivative yaw": 6,
    "Harvester terrain-height and local-slope following": 6,
    "Travel-driven tread wheel and cutter animation": 6,
    "Crew release batches of one to four at 6+7*j seconds": 6,
    "Scheduled route-position crew release": 6,
    "Crew running at 3.8 units per second": 6,
    "Running and waiting limb animation cycles": 6,
    "Worm approach gap from 220 to 18 units": 6,
    "Worm approach rise lunge swallow and retreat stages": 6,
    "Harvester 20-unit sinking and removal": 6,
    "Current constant 150-second pursuit and 60-second extraction": 6,
    "Timed proximity hazards crew losses and aircraft loss": 6,
    "W S thrust and A D strafe": 7,
    "Q E altitude requests and obstacle clearance": 7,
    "Mouse horizontal and arrow-key aircraft steering": 7,
    "Mouse vertical and Up Down camera elevation": 7,
    "Normalized mouse deadzone and cursor-position control": 7,
    "Space braking low hover pickup and unloading": 7,
    "Left Shift boost reserve drain recharge and lock": 7,
    "C camera cycling": 7,
    "Wide close and tactical overhead camera presets": 7,
    "Heading-locked damped camera with terrain clearance": 7,
    "Enter start or next mission and R retry": 7,
    "P pause M mute F wireframe F11 fullscreen Escape exit": 7,
    "12-unit pickup range 11-unit clearance and speed below 6": 7,
    "0.42-second per-worker pickup and eight-seat capacity": 7,
    "14-unit base radius and 1.2-second cabin unloading": 7,
    "Delivery-only secured scoring and pickup progress reset": 7,
    "2048-by-2048 moving directional shadow depth map": 8,
    "5-by-5 PCF receiver-slope correction and shadow bias": 8,
    "Warm ambient directional sun and Blinn-Phong lighting": 8,
    "Wrapped diffuse sand lighting": 8,
    "Material emission without extra lights": 8,
    "Procedural sand ripples slope color and normal detail": 8,
    "Procedural quartz sparkle grain and rock strata": 8,
    "Procedural sky gradient sun disc and halo": 8,
    "Exponential-squared distance fog": 8,
    "In-shader tone mapping and gamma approximation": 8,
    "GPU crater 13-unit depth and 48-unit radius": 8,
    "Matching scene and shadow crater displacement": 8,
    "2400 CPU-updated sand particles": 8,
    "Ambient crawler worm and downwash emission categories": 8,
    "Particle lifetime gust motion terrain contact and respawn": 8,
    "Sorted camera-facing instanced dust billboards": 8,
    "Procedural soft particle mask alpha blend and lifetime fade": 8,
    "Requested four-sample MSAA VSync and wireframe inspection": 8,
    "HUD rescue counters inside outside groups wave and best": 9,
    "HUD breach extraction pursuit and worm-distance feedback": 9,
    "HUD altitude speed FPS boost mute and action prompts": 9,
    "HUD eight-seat indicators and pickup unloading progress": 9,
    "Title pause and debrief mission overlays": 9,
    "North-up radar with aircraft base all outside crew harvester worm": 9,
    "In-code 5-by-7 glyphs rendered as rectangles": 9,
    "Responsive HUD wrapping panel layout and readable sizing": 9,
    "HUD OpenGL state preservation and dynamic buffer": 9,
    "Eight spatial sound roles": 9,
    "Ornithopter flight and boost sounds": 9,
    "Worm rumble and breach sounds": 9,
    "Harvester engine and crew help sounds": 9,
    "Rescue winch and safe delivery sounds": 9,
    "Camera audio listener and inverse-distance attenuation": 9,
    "Speed-dependent flight pitch and smoothed boost volume": 9,
    "Proximity-modulated worm volume and event-triggered cues": 9,
    "Missing-audio fallback silent engine fallback and mute": 9,
    "Focus-loss input clearing pause and resize recentering": 9,
    "Fullscreen switching and minimized-window event wait": 9,
    "Delta-time cap and bounded 120-Hz mission substeps": 9,
    "CMake OpenGL GLFW GLAD GLM and miniaudio build": 9,
    "Runtime audio asset copying and debugger working directory": 9,
    "CTest RescueMissionLogic and current partial-pass results": 9,
    "Four current 120-frame rendering smoke modes": 9,
    "Timer regression versus older test and report expectations": 9,
    "Existing LaTeX report and implementation references": 9,
    "Three core requirements closing synthesis": 10,
    "Available external demonstration video without embedding": 10,
    "Available project report": 10,
    "Known limits opaque glass fixed tanks and GPU-only crater": 10,
    "Unsupported undefined Karpov objects category": 10,
}
