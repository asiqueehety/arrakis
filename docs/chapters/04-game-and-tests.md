# Chapter 4: Mission Rules, Flight Controls, and Tests

This chapter explains the complete gameplay header and the complete mission-test header in beginner-friendly terms. The original files were `arrakis_game.h` and `arrakis_tests.h` at the project root when this chapter was researched. In the reorganized project their locations are `src/arrakis_game.h` and `tests/arrakis_tests.h`. Every source range in a heading below refers to the original header, not to this Markdown file. The numbered source appendices provide the exact text; the explanations here describe what that text does rather than replacing explanations with a copied listing.

The gameplay header has 307 original lines. The test header has 1,041 original lines. The ranges below collectively account for every nonblank line in both. Opening and closing braces belong to the constructs explained in their ranges; empty separator lines have no runtime behavior. Both headers depend on types, variables, libraries, and functions already introduced by the translation unit that includes them. They are not independently compilable programs.

## Reading the Mission

The player controls an ornithopter, a fictional aircraft with flapping wings. A moving harvester carries 36 people. Those people leave in timed groups, run to fixed gathering points, and wait for pickup. Holding Space near people, while low and slow enough, winches them into an eight-person cabin. Holding Space near the base unloads the whole cabin after a separate dwell interval. Only delivered people contribute to the rescue score.

A pursuing worm approaches the moving harvester. Its danger rules depend on elapsed mission time, the time of breach, horizontal distance, and, for the aircraft, height. Crew aboard the aircraft are protected from ordinary local crew hazards but not from an aircraft collision or the final timeout. Crew already delivered remain safe even if the aircraft is subsequently lost.

Here, a `glm::vec3` is a three-component vector with `x`, `y`, and `z`. The world uses `y` for height and the `x,z` plane for ground positions. A `glm::vec2` holds two components. For the mouse these are horizontal and vertical normalized input; for distance checks they usually stand for world `x,z`. Distances are expressed in world units, treated as meters by the test messages. Times are seconds, yaw and camera pitch are stored in degrees, and trigonometric functions receive radians.

The notation `dt` means the amount of simulated time represented by one update. A frame at 120 updates per second has a `dt` near `1/120`. Multiplying a speed by `dt` gives the movement for that update. This is essential to avoid rules that change simply because the program draws more frames, although not every integration in this file is exactly frame-rate independent.

Several small mathematical operations recur throughout the chapter. `clamp(value,minimum,maximum)` returns the minimum when value is below it, the maximum when above it, and value otherwise. `min` chooses the lesser of two inputs and `max` the greater. `abs` measures magnitude without sign. A vector's length is its Euclidean magnitude; normalizing a nonzero vector divides it by that length to preserve direction while making its length one. The code checks for zero before normalizing movement input.

`mix(a,b,w)` means `a*(1-w)+b*w`. With weight zero it returns a; with weight one it returns b. The same operation works on each component of a vector. Here most smoothing weights are `1-exp(-k*dt)`, where `exp` is the exponential function and k is a positive response rate. Over a constant-target interval t, the fraction of the original error remaining is `exp(-k*t)`. The characteristic time is `1/k`: approximately 0.125 seconds for rate eight, 0.167 for six, 0.182 for 5.5, 0.083 for 12, and 0.25 for four. A smaller characteristic time means faster response, not a fixed delay after which the value suddenly snaps to its target.

A dot product multiplies corresponding vector components and sums them. For unit directions it equals the cosine of the angle between them, so a value near one means nearly identical directions and a value near minus one means opposite directions. A cross product returns a perpendicular vector with an order-dependent sign. Taking forward cross world-up gives aircraft-right in this project's coordinate convention. Converting degrees to radians multiplies by `pi/180`; converting radians to degrees multiplies by `180/pi`.

In C++, `if` chooses whether a statement or block runs, `else` selects an alternative, `continue` skips the remainder of the current loop iteration, and `return` leaves a function. Conditions joined by `&&` require both operands, while `||` requires either; they short-circuit, so the second operand need not be evaluated once the result is already known. Prefix `++` and `--` change a counter by one. The references used by range-based loops operate on the original objects, while constant references allow reading without changing them. These details explain why the update can mutate every person's state in place and why comparison loops can stop safely when a mismatch is found.

## Game Types and Storage

### Game Lines 1-4: Inclusion and State Enumerations

Line 1 uses `#pragma once` to avoid processing this header more than once within a translation unit. This does not make its globals or ordinary function definitions safe to duplicate across many independently compiled translation units. The project includes them as part of its existing compilation structure.

`MissionPhase` is a scoped enumeration with `Title`, `Flying`, and `Debrief`. With no explicit assigned values, their underlying values begin at zero in declaration order, but the code uses their names rather than relying on numeric values. `Title` is the pre-mission state, `Flying` enables simulation, and `Debrief` is the finished state. A pause is not a fourth phase: it is a separate Boolean within a mission.

`CrewState` has six scoped values, likewise in declaration order: `Inside`, `Running`, `Waiting`, `Aboard`, `Safe`, and `Lost`. `Inside` means still attached to the harvester's evacuation exit. `Running` means moving to a scheduled destination. `Waiting` means standing at that destination. `Aboard` means counted in the aircraft cabin. `Safe` means unloaded and scored. `Lost` means no longer rescuable. These are mutually exclusive because each person stores one enumeration value, not several independent flags.

The usual successful path is `Inside -> Running -> Waiting -> Aboard -> Safe`, but pickup also accepts `Running`, so waiting is not compulsory. Local hazards can turn `Running` or `Waiting` into `Lost`; swallowing can turn `Inside` into `Lost`; aircraft loss or timeout converts every state other than `Safe` and already `Lost` into `Lost`. A mission reset creates an entirely new population rather than reversing those transitions on existing people.

### Game Lines 5-11: Every Evacuee Field

`Evacuee` represents one crew member. `pos{0}` initializes all three position components to zero, and `destination{0}` does the same for the target position. These are world-space values, not coordinates relative to a moving harvester. `state` initially equals `CrewState::Inside`.

`release` is a floating-point scheduled release time, initially zero. `group` is an integer index identifying the person's evacuation group, initially zero. `slot` is an integer identifying their position within that group, also initially zero. The declaration places `group` and `slot` on one line but initializes each separately. A freshly constructed person therefore has a valid default state, but mission setup must assign their real route position, release schedule, group, and slot before gameplay.

The person has no individual health, score, velocity, animation timer, or inventory. Walking speed is supplied by the update function. The group index selects which release loop will activate the person, while the slot determines the spacing within the gathering cluster. The duplicate release value on the person is used to calculate how much walking time is available when a frame crosses the release threshold.

### Game Lines 12-17: Every Evacuation Group Field

`EvacuationGroup` stores `count`, initially zero, for the number of people assigned to the group. `release`, initially zero, records the group's scheduled release time. `center{0}` is a three-component world-space cluster center, initially the origin. `released`, initially `false`, prevents the release operation from being repeated in later updates.

Before release, reset stores the initial harvester position in `center`; this is temporary. At release, the code replaces it with the actual scheduled gathering point. The group has no list of member pointers. Instead, the update scans the mission's crew vector and selects people whose `group` index matches the group being released.

### Game Lines 18-32: Every Mission Field and Default

`RescueMission` contains the state of one attempt. `phase` initially equals `Title`. `paused`, `planeLost`, and `boostLocked` are initially `false`. `paused` stops the update before any simulation work. `planeLost` records a worm collision with the aircraft. `boostLocked` prevents held Shift from immediately reactivating boost after energy runs low.

The integer fields are `wave = 1`, `rescued = 0`, `aboard = 0`, `lost = 0`, and `best = 0`. `wave` affects the breach timer, not crew count or cabin capacity. `rescued` counts delivered people. `aboard` counts current cabin occupants. `lost` counts people marked lost. `best` is the highest delivered total retained across resets in the current process. This header does not save that record to disk or calculate a separate cumulative score across waves.

The floating-point fields are `elapsed = 0`, `breachAt = 210`, `altitude = 8`, and `boost = 1`. `elapsed` is the attempt's simulation clock. `breachAt` is an absolute elapsed-time threshold. `altitude` is desired terrain-relative height, not necessarily the aircraft's instantaneous measured height. `boost` is a normalized resource from zero to one.

`harvesterYaw = 0` and `harvesterTravel = 0` describe orientation and accumulated horizontal travel. `wormYaw = 90` is a stored fixed orientation; no function in this header changes it. `pickup = 0` and `unload = 0` are normalized dwell progress. Their completion threshold is one. `target = -1` means no current pickup person; nonnegative values are indices into `crew`.

`prompt` starts as an empty `std::string` and is replaced during active updates with instruction text. `base{55, 0, 55}` gives the base's horizontal location; reset later fills in its terrain height. `crew` and `groups` are initially empty standard-library vectors. Their sizes and contents are populated by reset.

These defaults are significant because reset replaces the entire mission object with `RescueMission{}`. Any field that is not explicitly restored afterward returns to exactly these values. For example, pause, plane loss, boost lock, prompts, targets, travel, and old populations disappear, while best and wave are specially preserved or advanced.

### Game Lines 33-39: Every Global and Rock Site

`g_mission` is the global mission object and begins with the defaults above. `g_missionRevision` starts at zero and increments whenever `resetMission` is called. It can let other systems recognize a new attempt; this file does not otherwise read it. `g_mouseHover(0)` begins with both mouse-input axes zero. `g_mouseTurnRate = 0` begins with no stored angular inertia.

`RESCUE_CAPACITY` is a compile-time integer constant of eight. It is the cabin limit used during boarding and by several tests. It is not an adjustable per-wave mission field.

`g_rockSites` is a fixed array of eight horizontal obstacle centers: `(45,-65)`, `(-55,40)`, `(90,30)`, `(-90,-70)`, `(15,95)`, `(-25,-115)`, `(120,-25)`, and `(-130,85)`. The pairs represent `x,z`, not `x,y`. The flight update uses these centers to increase terrain clearance nearby. It does not calculate collisions from rendered rock triangles or assign each rock a separate height.

Globals such as `g_ornPos`, `g_ornVel`, `g_ornYaw`, `g_keys`, `g_harvX`, `g_harvZ`, `g_audio`, and `g_tanks` are used but not declared here. Their definitions and, where relevant, their callbacks and rendering uses belong to other chapters. The distinction matters: this header supplies the rules, not the entire window/input/audio implementation.

## World and Input Helpers

### Game Lines 41-46: Horizontal Distance and Current Harvester Position

`horizontalDistance(a,b)` subtracts the two points' `x` and `z` components, forms a two-component vector, and returns its length. Mathematically this is `sqrt((a.x-b.x)^2 + (a.z-b.z)^2)`. Height differences are deliberately ignored. Pickup, base arrival, and worm proximity therefore have separate horizontal and vertical requirements rather than a single spherical distance check.

`harvesterPosition()` constructs a position from the external `g_harvX` and `g_harvZ`. Its height is freshly queried from `getDuneHeight` at those coordinates. The height is not separately stored in a harvester global. If the terrain function changes, this helper reflects that change on its next call.

### Game Lines 47-60: Analytic Route, Tangent Yaw, and Forward Direction

`harvesterRoute(time)` calculates an absolute route position directly from a time value. The horizontal formulas are `x = -40 + 0.62*time + 15*sin(0.045*time)` and `z = -55 + 22*sin(0.032*time) + 9*sin(0.071*time)`. At zero time the harvester is at `(-40,-55)` horizontally. The linear term generally carries it east, while three sine terms create the wandering path. Height is the terrain height at the calculated position.

These are analytic coordinates: the function does not move a previous position incrementally and does not modify mission state. Asking for time six always gives the same result, regardless of the frame that noticed a release. The helper itself does not clamp time; clamping happens in `updateHarvester`. Scheduled release code deliberately calls this raw route at the release's own time.

`harvesterRouteYaw(time)` derives the horizontal tangent. Differentiating the x formula gives `dx = 0.62 + 0.675*cos(0.045*time)` because `15*0.045 = 0.675`. Differentiating the z formula gives `dz = 0.704*cos(0.032*time) + 0.639*cos(0.071*time)` because `22*0.032 = 0.704` and `9*0.071 = 0.639`.

The returned yaw is the degree conversion of `atan2(-dx,-dz)`. This sign and argument order align a model whose local forward direction is negative Z with the tangent. `atan2` accounts for quadrants rather than dividing one derivative by the other. This is a horizontal heading; it does not tilt the harvester to match the vertical terrain slope.

`harvesterForward(yaw)` converts degrees to radians and returns `(-sin(angle),0,-cos(angle))`. It is a horizontal unit direction. At zero yaw it points toward negative Z. This same convention appears in aircraft flight, although `wormYaw` is a separate stored model-orientation value and should not be interpreted as an aircraft heading calculation.

### Game Lines 61-78: Harvester Updates and Worm Geometry

`updateHarvester(elapsed)` first clamps the supplied elapsed time into `[0, breachAt]`. Negative times select the beginning; times after breach select the breach pose. It reads the previous current position, evaluates the next route position, and adds the horizontal distance between them to `harvesterTravel`. It then writes the new external x/z globals and the route tangent yaw.

Accumulated travel is a sum of straight chords between sampled positions, not an exact integral of curved-route arc length. Its value can differ slightly with update frequency. Calling the helper with nonmonotonic times can also add backward travel; this helper does not reject such calls. Once repeated calls clamp to the same breach pose, the added distance is zero and the pose and accumulated travel stop changing.

`wormGap()` calculates `progress = clamp(elapsed/breachAt,0,1)` and linearly interpolates from 220 to 18. Equivalently, the gap is `220 - 202*progress`. During a normal first wave it closes by `202/210`, approximately 0.962 world units per second. Later waves reach the same endpoint sooner. After breach the gap remains 18. The implementation assumes a positive nonzero breach time, which normal reset guarantees.

`wormPosition()` begins with the current harvester position, subtracts the gap from x, leaves z unchanged, and recomputes terrain height at the worm's new x/z. Thus the worm is always west of the harvester, not necessarily behind the harvester's current tangent direction. It does not independently steer toward a person or aircraft. Its motion follows this geometric relationship.

`wormAttackTime()` returns `elapsed - breachAt`. A negative answer means before breach, zero means the exact breach instant, and a positive answer measures seconds since breach. Most hazard windows below are expressed in this relative time, so shortening a wave automatically shifts them earlier in absolute mission time.

### Game Lines 80-89: Nearest Crew Position

`nearestCrewPosition()` initializes a search radius of 10,000 and a fallback point equal to the current harvester position. It loops over every person by constant reference, skipping states other than `Waiting` and `Running`. For each eligible person it measures horizontal distance from the aircraft, and a strictly smaller distance replaces the current best position.

The helper returns a position, not a crew index, and has no 12-unit pickup radius. If no eligible person is closer than the initial 10,000-unit bound, it returns the harvester fallback. Equal-distance ties retain the first previously selected person because the comparison is strict. It does not change the pickup target or board anyone.

### Game Lines 91-100: Cursor Validation, Normalization, and Deadzone

`setMouseHover(x,y,width,height)` first clears both hover axes. It returns immediately if width or height is nonpositive, either coordinate is nonfinite, or either coordinate lies outside the window. Coordinates exactly zero or exactly width/height are accepted because the boundary comparisons use `<` and `>`, not `<=` and `>=`.

For valid input, horizontal position becomes `2*x/width - 1` and vertical position becomes `2*y/height - 1`. A left or top edge is -1, the center is zero, and a right or bottom edge is +1. The intermediate expressions use the double cursor coordinates before conversion to float for the deadzone function.

The local lambda maps one normalized value to `copysign(max(0,(abs(value)-0.12)/0.88),value)`. The central magnitude of 0.12 produces zero, and the rest is rescaled so magnitude one remains one. The deadzone occupies six percent of window width or height on either side of center, because the normalization doubles pixel displacement. In a 1,000-pixel-wide window, horizontal coordinates 440 through 560 are in that neutral region.

`copysign` keeps the original sign, including a possible negative zero; comparisons and vector lengths still treat that zero as neutral. Each axis is treated separately. There is no radial deadzone. A corner can therefore produce `(1,1)`, whose two-component length is greater than one. Invalid input clears both axes even if only one coordinate was wrong. This function only stores arithmetic input: it does not itself steer, move, advance time, or update turn inertia.

### Game Lines 102-118: Flight Direction and Camera Helpers

`flightForward()` uses the aircraft's current `g_ornYaw` and the same `(-sin(yaw),0,-cos(yaw))` convention as the harvester helper. It does not inspect velocity. Reversing or strafing therefore does not automatically aim the nose in the movement direction.

`flightCameraPitch()` returns `clamp(12 - g_mouseHover.y*22 + g_camOrbitPitch,6,65)`. With a centered mouse and zero orbit adjustment, pitch is 12 degrees. Top-edge hover has y = -1 and gives 34 degrees. Bottom-edge hover has y = +1 and would give -10 before clamping, so its result is six degrees. The separately supplied orbit adjustment can raise the result to the overall 65-degree maximum or lower it to six. This helper calculates a value; it does not implement the callback that changes orbit adjustment.

`flightCameraOffset(mode)` starts with `behind = -flightForward()`. Mode 2 returns 26 units behind and 100 units upward, independent of mouse pitch. Otherwise, mode 1 uses distance 20 and every other integer uses distance 40. Thus unknown modes fall through to the ordinary follow calculation rather than producing an error.

For those non-overview modes, horizontal distance behind is `distance*cos(pitch)` and height is `distance*sin(pitch)`. Mode 1 adds another 2.2 vertical units. The ordinary follow offset has length 40, and the close offset has length 20 after subtracting the extra height. All offsets remain horizontally behind the nose, which keeps camera screen-right consistent with aircraft-right. The helper returns an offset, not an absolute eye position or a view matrix; the runtime uses it to place the camera.

## Starting and Finishing Attempts

### Game Lines 120-140: Reset Ordering and Flight Restoration

`resetMission(start,next=false)` is the attempt initializer. `start` chooses active play versus title, while `next` determines whether to increment the current wave. The default second argument means a one-argument call retries the same wave.

It first increments the mission revision and calls `g_audio.resetMission()`. Audio internals are explained separately. It saves the previous best and either preserves wave or calculates `wave + 1`. Then it replaces the mission with a default-constructed object and restores those two saved values.

The new breach threshold is `max(150,210-(wave-1)*10)`. Wave one gets 210 seconds, wave two 200, wave three 190, wave four 180, wave five 170, wave six 160, and wave seven and later 150. Nothing here prevents callers from changing wave to an unusual number outside normal play; the documented progression describes ordinary positive waves.

The phase becomes `Flying` when `start` is true and `Title` otherwise. The harvester is restored to `harvesterRoute(0)`, its x/z globals are replaced, and its yaw is recomputed from the route tangent. The base's y is replaced with terrain height. The aircraft is placed eight units above that base and receives zero velocity and yaw.

Both hover axes, stored turn rate, aircraft speed, pitch, roll, and camera orbit pitch are zeroed. The chained assignment to speed, pitch, and roll resets all three values. Importantly, the ordinary reset does not clear `g_keys`, does not explicitly clear `g_isBoosting`, and does not reset `g_wingPhase`. The testing helper `fresh` handles those additional globals. A reset should not be described as clearing every application-global variable.

### Game Lines 141-154: Exact Population and Release Schedule

Population construction begins with `remaining = 36`. The `while` loop continues until none remain. For each iteration, the group index is the current number of groups. Desired size is `1 + group%4`, capped by remaining people. Release time is `6 + group*7` seconds.

The group is appended with its count, scheduled release, initial harvester position, and `released=false`. A nested `for` loop creates slots from zero through `count-1`. Each new person's default state remains `Inside`, while release, group index, and slot are assigned explicitly. Both position and destination initially become the starting harvester position. Each person is appended, then the outer loop subtracts that group's count from remaining.

The exact result is 15 groups. Their sizes are `1,2,3,4,1,2,3,4,1,2,3,4,1,2,3`, totaling 36. The release schedule is `6,13,20,27,34,41,48,55,62,69,76,83,90,97,104`. The last group's three people complete the population without truncating its nominal size. No random generator determines these numbers. The schedule is unchanged on later waves, so even the 150-second breach floor remains after the final scheduled release.

The loops assign identity by vector position and group/slot fields, not by a separately allocated identifier. Vector insertion order later matters for nearest-person ties. All people in a group share a release time but receive different destinations when their group is activated.

### Game Lines 155-159: Tank Reset

The external tank vector is cleared, then three aggregate records are inserted. Their positions and accompanying values are `(-12,0,-26),20,1`; `(-19,0,-32),-15,1`; and `(-26,0,-23),10,1`. The game header supplies the literal records but does not declare the tank structure; the scene chapter identifies how its orientation and scale fields are rendered.

These y coordinates are explicitly zero in the stored records. This routine does not query terrain for them, and this header does not give tanks an independent collision, crew population, or mission counter. Clearing the vector means a normal mission reset replaces any previous tank entries with these three.

### Game Lines 161-166: Finishing Without a Separate Win Flag

`finishMission()` changes phase to `Debrief`, sets best to the larger of its existing value and the current delivered count, clears both dwell-progress values, and zeros aircraft velocity. It does not itself mark unresolved crew lost, empty the cabin, change the prompt, or decide whether the result is a success. The calling update branches do their own loss reconciliation before invoking it.

There is no `won` Boolean or `Win` phase. Delivering all 36 without losses is the full-success scenario, but delivering the final survivors after other crew are lost also ends naturally in `Debrief`. Direct callers can invoke `finishMission()` on an unresolved population, as the next-wave test intentionally does; the function alone does not enforce a finished-population invariant.

It also does not reset `g_ornSpeed`, pitch, roll, wing phase, boost status, or turn rate. Because later mission updates return immediately in `Debrief`, those values can remain as the last-frame snapshot even though velocity is now zero. The tests correctly check frozen simulation rather than assuming all display or animation values become their defaults.

## The Active Update

### Game Lines 168-174: Guard, Clock, Harvester, and Winch Input

`updateMission(dt)` takes a reference named `m` to the global mission, avoiding repeated long names and ensuring writes affect the real object. If phase is anything other than `Flying`, or if pause is true, it immediately returns. Title, debrief, and pause therefore freeze all work within this function, including the clock, routes, resources, crew, hazards, and flight animation.

For active play it first adds `dt` to elapsed time, then updates the harvester at the new elapsed time, then calculates attack time. Thresholds in the frame therefore see the end-of-step clock. `winch` is a constant Boolean snapshot of the Space key for this update. No mouse click is required to steer or winch; mouse position and a keyboard Space state are separate inputs.

The function does not validate that `dt` is finite, nonnegative, or reasonably small. Normal application timing and test fixtures provide suitable values. A zero dt still evaluates release and hazard conditions and can finish a mission already placed at its timeout. That behavior is deliberately used in tests. Negative or nonfinite dt is not covered by these tests and should not be advertised as supported.

### Game Lines 175-182: Steering and Exact Angular Smoothing

Steering combines normalized horizontal hover, a +1 contribution from Right, and a -1 contribution from Left, then clamps the result into `[-1,1]`. Both arrows together cancel. An arrow can reinforce or oppose mouse hover, but the clamp limits total strength. Desired angular rate is `turnRate = -steer*80`, so full right gives -80 degrees per second and full left gives +80.

The previous actual rate is `g_mouseTurnRate`. With damping constant eight per second, `turnDecay = exp(-8*dt)`. The new stored rate is `mix(turnRate,oldRate,turnDecay)`, or `turnRate + (oldRate-turnRate)*exp(-8*dt)`. It approaches the requested rate exponentially rather than switching instantly.

Yaw uses the exact integral of that exponential over the step: its change is `turnRate*dt + (oldRate-turnRate)*(1-exp(-8*dt))/8`. This is more accurate than first computing the new rate and multiplying that end-of-step rate by dt. With a constant steering command it agrees with the continuous solution across different step sizes, apart from floating-point error.

`std::remainder(...,360)` wraps yaw using the nearest multiple of 360. Its result is around the signed half-turn range rather than the `[0,360)` interval returned by some wrap conventions. Equivalent headings such as 359 and -1 therefore remain physically identical even if their stored numbers differ.

Roll moves exponentially toward `g_mouseTurnRate*0.28` with rate six per second. At a settled full turn, the target magnitude is 22.4 degrees. This is a visual bank based on turn rate, not a cause of sideways acceleration. Because the roll target itself evolves during the step, the same exact-yaw guarantee should not automatically be claimed for the entire visual roll trajectory.

### Game Lines 183-195: Translation, Boost Lock, and Velocity

The update computes forward from the current yaw and right as `cross(forward,(0,1,0))`. At zero yaw these are negative Z and positive X. D contributes +right, A contributes -right, W contributes +forward, and S contributes -forward. Opposite held keys cancel on their respective axes.

If the resulting input vector has positive length, it is normalized. This prevents diagonal W+D movement from being `sqrt(2)` times faster than a single direction. A zero vector stays zero, avoiding division by zero. Translation remains relative to nose orientation; neither desired movement nor velocity realigns yaw.

Boost lock is evaluated in order. Releasing left Shift clears the lock first. A resource level at or below 0.04 then sets it again. Actual boosting requires held left Shift, an unlocked resource, no winch, and nonzero movement input. Holding Shift alone while stationary is not boosting; holding Space suppresses boost even when Shift and movement are also held.

Resource then changes by `dt*(-0.22)` while boosting or `dt*(0.14)` otherwise, clamped into `[0,1]`. Full-resource continuous use reaches the 0.04 lock threshold in approximately `(1-0.04)/0.22 = 4.36` seconds under small steps. Recharging from zero to one takes approximately `1/0.14 = 7.14` seconds while not boosting. A held Shift cannot automatically unlock after recharge: release is needed, and if that release happens while energy is still at or below 0.04 the next condition relocks immediately. Threshold decisions use the resource before this frame's resource change.

Desired velocity is normalized input times 48 while actually boosting and times 28 otherwise. If Space is held, desired velocity is reduced to ten percent, giving at most 2.8 with ordinary translation. Holding Space therefore permits slow movement rather than forcing a stationary aircraft.

Velocity approaches desired velocity using `1-exp(-dt*k)`, with `k=12` while winching and `k=5.5` otherwise. With a constant target this smoothing matches exponential damping. The aircraft position then advances by the resulting velocity times dt. That positional integration uses the new sampled velocity, not the analytic integral used for yaw, so complete movement is not exactly independent of step size. Changing heading and changing boost targets introduce additional per-step effects.

### Game Lines 196-214: Bounds, Height, Obstacle Clearance, and Animation

The aircraft's x and z are each clamped into `[-160,160]`. This is an axis-aligned square, not a circular map boundary. Velocity is not zeroed or clipped on hitting an edge, so a craft pressing against the boundary can still have a nonzero speed value. `g_ornSpeed` is the length of its velocity after the update, not displacement divided by dt.

Altitude input is `(Q-E)*15*dt`, clamped between four and 55. Q raises desired altitude; E lowers it; both cancel. With Space held, the already clamped desired altitude is then exponentially mixed toward six at rate four. The order means altitude keys can still influence the value while winching, but the winch's pull toward six acts afterward.

`ground` is terrain height under the aircraft's newly updated horizontal position. `clearance` starts as desired altitude. For each of eight rock sites, the code measures horizontal distance and computes `avoid = 1-smoothstep(8,17,distance)`. Specifically, let `u = clamp((distance-8)/9,0,1)`; smoothstep is `u*u*(3-2*u)`, and avoid is one minus that result. At distance eight or less, avoid is one. At 17 or more, it is zero. Between them it changes smoothly with the smoothstep cubic rather than linearly. Clearance becomes the maximum of its current value and `13*avoid`. Thus close rocks request at least 13 units, and the outer region fades that extra request.

There is also harvester avoidance. While attack time is less than 11, and the craft is within strictly eight x units and strictly seven z units of the harvester, clearance becomes at least 12. This is a rectangle, not a radius. The condition includes all pre-breach times because negative attack times are less than 11. It stops at attack 11, one second before the Inside-crew swallowing threshold of 12. It is a height-assistance rule, not a harvester impact/failure test.

Aircraft y approaches `ground+clearance` exponentially at rate eight. A subsequent hard maximum guarantees y is at least `ground+3` immediately. The hard floor protects against abrupt terrain changes; the 12/13-unit assistance is only a smoothed target and does not guarantee instantaneous clearance above every rendered obstacle.

Aircraft pitch approaches `-speed*0.3` at rate six. Faster horizontal motion therefore tilts the display downward more strongly. Wing phase increases by `36*dt` during actual boost or `23*dt` otherwise, including stationary active flight. The phase is not wrapped here. Q/E height changes and y smoothing do not add a vertical component to the translational velocity vector; this is terrain-following arcade flight, not full rigid-body aerodynamics.

### Game Lines 215-233: Releasing Groups at the Scheduled Pose

The update loops over all groups by integer index. It skips a group if already released or if elapsed time is still below its release time. A group can therefore activate exactly at its schedule, and one coarse update can activate several due groups.

For an activating group it evaluates the route and tangent yaw at `group.release`, not the current elapsed time. Forward and right are constructed at that scheduled pose. The exit is `origin + right*6.1 - forward*1.2`. The gathering center is another 14 units along right from the exit, and its y is terrain height. `released` becomes true before processing members, preventing future destination recomputation.

The nested crew scan selects only members of this group still in `Inside`. Those people become `Running` and start at the exit. Their destination is center plus a forward spread of `1.7*(slot-(count-1)*0.5)` and an additional right spread of `1.3*(slot%2)`. Centering the forward offsets around zero distributes slots symmetrically along forward; alternating even/odd slots creates a second small row sideways. Destination y is terrain height at each person's own destination.

For a one-person group, slot zero has neither spread and its destination equals the center. For larger groups, not everyone stands exactly at the center. The exit's initial y comes from the route's origin because horizontal vectors have zero y, but the later crew loop sets actual position height to local terrain before hazards and pickup.

The release loop's work is proportional to groups times crew because each released group scans the whole population. With 15 groups and 36 people this is small. More importantly, setting a group's flag while none of its people remain Inside is permitted; the flag records scheduled activation, not a successful evacuation count.

### Game Lines 234-250: Walking and Terrain Following

`waiting` starts at zero. Despite its name, it will count all unresolved ground/harvester states, including `Inside` and `Running`, not just `Waiting`. The update calculates the current worm position and harvester forward/right before walking through all crew.

An Inside person's current position follows the current moving harvester exit. Their original destination is not updated until release. A Running person forms a displacement to destination and removes its vertical component, so movement is horizontal. Available walking time is `min(dt,max(0,elapsed-c.release))`. This prevents a newly released person from walking the entire dt when only a small final part of the step occurred after release.

At walking speed 3.8, a person reaches their destination when remaining distance is at most `walkDt*3.8 + 0.00001`. The tiny extra tolerance avoids floating-point near-arrival jitter. Arrival snaps position to destination and changes the state to Waiting. Otherwise movement is normalized horizontal displacement times walking distance. Zero-distance people take the arrival branch rather than dividing by zero.

Every person then has their y set to terrain height at their current x/z, regardless of state. That includes Aboard, Safe, and Lost people. This header does not physically attach an Aboard person's stored position to the aircraft or move a Safe person's stored position to base. Their states and counters carry those meanings; their stored positions can remain at old pickup points, which the renderer can choose not to draw for those states.

### Game Lines 251-259: Crew Hazard Windows and Unresolved Count

`outside` means the state is Running or Waiting. Danger radius is 16 before breach, 32 from breach until attack time 12, and 44 at attack time 12 and later. Separately, `activeWorm` is true before breach only after elapsed time is strictly greater than eight, or after breach only when attack time is strictly greater than three and strictly less than 18.

An Inside person is lost when attack time is at least 12, independent of their distance and independent of `activeWorm`. An outside person is lost when the worm is active and their horizontal distance is strictly less than the current radius. At the exact radius they are not lost by that test. The state changes to Lost and the loss counter increments once. On future updates that person is no longer Inside or outside, so the same local loss is not counted again.

The actual outdoor windows are therefore: no pursuit danger through exactly elapsed eight; radius 16 after eight and before breach; no outside danger from breach through exactly attack three; radius 32 after attack three and before 12; radius 44 from attack 12 until just before 18; and no local outside danger at 18 or later. Inside swallowing remains active after 12. The interval between breach and attack three is a real rule gap, not an omitted continuous hazard.

After those possible transitions, the update counts every remaining Inside, Running, or Waiting person into `waiting`. Aboard, Safe, and Lost do not contribute. This local count is used by the natural-completion branch and is decremented immediately if someone boards later in the same update.

### Game Lines 261-277: Prompts and Unloading

The initial prompt is `HARVESTER FLEEING / RESCUE AMBER GROUPS` before breach or `WORM BREACH / CLEAR THE HARVESTER` at and after breach. Actual low-flight status is `g_ornPos.y-ground <= 11`; the desired altitude alone is not enough. Steady means speed strictly below six. At base means horizontal distance strictly below 14.

A full cabin overwrites the prompt with `CABIN FULL / RETURN TO CYAN BASE`. At attack 12 or later, that is overwritten with either `HARVESTER LOST / DELIVER SURVIVORS TO BASE` if anyone is aboard, or `HARVESTER LOST / RESCUE REMAINING GROUPS` otherwise. Prompt order matters: these are assignments, not independent UI messages.

If at base with at least one occupant, the prompt becomes `HOLD SPACE / UNLOAD SURVIVORS`. Only simultaneous Space, low status, and steady status increase unload progress, by `dt/1.2`. Completion at progress one means a continuous eligible interval of about 1.2 seconds. Every Aboard person's state changes to Safe, rescued increases by the entire cabin count, aboard becomes zero, unload resets to zero, and best updates to the maximum banked score.

If any unloading requirement fails, progress resets, rather than pausing and preserving its partial value. Being outside base or having no cabin occupants also resets it. No fractional score is banked before completion. A large step that exceeds the interval unloads once and discards extra progress; it does not calculate repeated unload transactions.

Unloading happens before nearest-person pickup. In an unusual fixture with waiting people at base, an update may unload a previous occupant and then start or complete a new pickup. The code does not declare the base a pickup-free area. Also, later pickup prompts can overwrite the unloading instruction even while unload progress is being processed, so prompt text is not an exclusive state-machine representation.

### Game Lines 278-297: Target Selection and Pickup Dwell

The pickup search starts with index -1 and radius 12, scans every crew index, and skips everyone not Running or Waiting. A candidate replaces the nearest only if its horizontal distance is strictly smaller than the current best. A person exactly 12 units away is not selected, and equal-distance ties favor the first encountered index.

Changing target resets pickup progress immediately. The current target field then becomes the selected index. A valid target and an aboard count below eight allow the pickup block. If measured clearance is too high, the prompt says `TOO HIGH / HOLD SPACE TO LOWER WINCH`; otherwise it says `HOLD SPACE / HOVER TO WINCH CREW`. This text does not separately report excessive speed, although speed still controls eligibility.

With Space, clearance at most 11, and speed below six, pickup adds `dt/0.42`. At one or more, that single person becomes Aboard, aboard increases, unresolved `waiting` decreases, progress resets, and target resets to -1. The next frame must search again. Losing eligibility, having no target, or having a full cabin resets progress.

The 0.42-second requirement is per person. It is not enough to spend that total near multiple changing targets, since each target switch loses partial progress. Boarding does not increase rescued or best. The cabin limit is checked before boarding, so a correct counter never goes above eight.

Only one person can board per call, even with a very large dt. Extra progress is discarded rather than carried to the next person. Thus dwell behavior under coarse steps is quantized, and exact angular smoothing should not be confused with a guarantee that every gameplay outcome is identical under arbitrary dt. The test suite mainly uses fine updates and separately tests the scheduled-release boundary logic.

### Game Lines 298-307: Aircraft Collision, Natural Completion, and Timeout

Aircraft approach collision requires pre-breach attack time, elapsed strictly greater than eight, horizontal distance to the worm strictly below 18, and aircraft y strictly below `worm.y+22`. Breach collision requires attack strictly greater than three and strictly less than 15, horizontal distance strictly below 24, and y strictly below `worm.y+40`.

These heights are relative to terrain at the worm, not necessarily terrain under the aircraft. At coincident x/z they are the same terrain height, which the height-envelope tests use. At separated points on a dune they can differ. Aircraft and crew therefore have different radii and different post-breach end times: aircraft local breach danger stops at 15, outside crew danger stops at 18.

If either aircraft-hit condition is true, every person other than Safe or already Lost becomes Lost and increments the loss total. This includes remote people and cabin occupants, not just those near the crash. Aboard is set to zero, `planeLost` becomes true, the prompt becomes `ORNITHOPTER LOST IN THE WORM'S MAW`, and `finishMission()` enters debrief. Banked Safe people survive and retain their score.

The alternate finishing branch runs only if there was no aircraft hit. It triggers when unresolved `waiting` is zero and aboard is zero, or when attack time is at least 60. Any remaining nonsafe/nonlost people are then marked Lost, aboard is cleared, and finish is called. The natural condition does not require rescued to equal 36: a mixture of delivered and lost people can satisfy it.

Timeout is 60 seconds after breach, making a first-wave total deadline of 270 seconds and a floor-wave deadline of 210 seconds. The function does not set `planeLost` for timeout. Because unloading and pickup precede this branch, a delivery that completes on the update reaching the timeout can bank its occupants before unresolved people are lost. Conversely, any person newly picked up on that same frame without a completed delivery is still unbanked and is lost.

No separate penalties are applied for rock contact, leaving the map, or hitting the harvester in this header. Map edges clamp movement, obstacle proximity requests clearance, and the worm alone supplies explicit aircraft loss. Pausing freezes elapsed time, so deadlines are simulation-time limits rather than unavoidable wall-clock limits.

## Test Harness and Fixtures

### Test Lines 1-13: Dependencies, Namespace, and Result Fields

The test header also uses `#pragma once`. Its comment says to include it after the game header and that these tests never initialize GLFW or call OpenGL. They still use GLFW key constants and the globals supplied by the surrounding program. This is a mission-logic test suite, not an independent replacement for all application initialization.

`<iostream>` supplies console streams and `<limits>` supplies infinity and NaN values. The namespace `arrakis_tests_detail` contains the small harness and reusable setup helpers without putting their short names directly into the global namespace.

`Results.current` is a borrowed `const char*` case-name pointer, initially the empty string. `cases`, `failedCases`, `checks`, and `failures` are integers initially zero. A case is an entire named scenario; a check is one Boolean assertion. Several checks can fail within one case. Consequently failedCases and failures need not be equal.

### Test Lines 15-32: Every Results Method

`check(condition,message)` always increments the check count. If condition is false, it increments failure count and writes `[FAIL]`, the current case name, a colon, the supplied explanation, and a newline to standard error. It does not throw, abort, or return early, allowing a case to collect additional failures.

`run(name,test)` is a function template accepting a callable of any suitable type. In this file those callables are lambdas. It stores the case name, increments cases, snapshots the existing failure count, and calls the callable. If the count has changed afterward it increments failedCases. It prints `[PASS]` or `[FAIL]` followed by the name to standard output according to whether that case added failures.

An earlier failure does not automatically fail every later case, because comparison is against the per-case snapshot. The harness does not catch exceptions, measure test durations, isolate process state, or stop after the first failure. Helpers reset globals between scenarios. A serious setup failure could still lead to later invalid indexing because `check` is not a protective assertion that terminates execution.

### Test Lines 34-40: Near Comparison and State Counter

`near(a,b)` checks whether absolute difference is strictly less than 0.001. It uses absolute tolerance, not a percentage or a relative error. Exactly 0.001 is outside the accepted interval. NaN differences do not satisfy the comparison.

`count(state)` starts at zero, loops over all mission crew by constant reference, adds one for each exact state match, and returns the count. It derives population information from individual states rather than trusting score/cabin/loss counters, which makes later reconciliation meaningful.

### Test Lines 42-55: Fresh State and Fine-Grained Advancement

`fresh(start=true)` clears every Boolean key in the key array, centers hover, zeros stored turn rate, replaces the mission with defaults, clears actual boost status, and resets wing phase. It then calls `resetMission(start)`. Unlike an ordinary retry, this fixture deliberately discards a previous best and wave because the default mission was installed first. The revision counter and audio state are affected by reset in the normal way.

`advance(seconds)` chooses `max(1,ceil(seconds*120))` update steps and calls `updateMission(seconds/steps)` for each. For positive durations this produces steps no longer than approximately 1/120 second. It always calls the updater at least once, even for zero seconds. Tests sometimes directly call the updater instead when an exact coarse interval or boundary is the subject of the scenario.

The helper is not a real-time wait and does not sleep. Advancing 100 seconds in title or pause simply executes many immediately returning simulation calls. Negative or nonfinite input is not its intended use; its integer conversion and update semantics do not provide general validation.

### Test Lines 57-70: Hover Teleport and Staging Point

`hover(position,altitude=6)` is explicitly a teleport fixture, not autopilot. It replaces the passed position's y with terrain height plus the requested altitude, writes aircraft position, clears velocity and speed, centers hover and turn rate, and sets desired altitude. It does not clear keys, reset mission time, or change crew states.

`staging()` returns the first crew member's destination when crew exist, otherwise the current harvester position. It does not find the nearest unresolved member. In the compact fixture the first destination is a useful common pickup area even after that first person becomes Aboard or Safe, since their stored destination remains there.

These helpers isolate state transitions from route navigation. A test that uses hover demonstrates requirements at a chosen location but does not prove the player can fly there quickly enough. The later input-only case addresses that distinct limitation.

### Test Lines 72-99: Compact Ready Crew, Collection, and Delivery

`readyCrew()` begins with `fresh()`. It chooses a terrain-height center at x zero and z -10. Every group is marked released and receives that same center. For each crew index i, the person becomes Waiting and is placed at center plus `(0.6*(i%6),0,0.6*(i/6))`; integer division creates six rows and modulo creates six columns. The person's y is recalculated and destination becomes position.

All 36 therefore form a compact six-by-six grid. Original group identities, slots, and release values remain, but the release flags prevent normal scheduled spawning. This is a deliberate synthetic fixture. It must not be described as the actual spatial layout generated by the harvester's route.

`collect(occupants)` teleports to `staging`, holds Space, and advances `occupants*0.45` seconds. The 0.45 interval gives a small margin over each 0.42-second pickup. It does not directly set aboard or crew states, nor does it release Space afterward. Collection can still be limited by capacity or remaining eligible crew.

`deliver()` teleports to base, holds Space, and advances 1.3 seconds, a margin over the 1.2-second unload dwell. It likewise relies on real mission logic and leaves Space held. Tests that require Space released must explicitly change the key.

### Test Lines 101-113: Every Reconciliation Assertion

`reconciled(r)` computes unresolved people by adding actual Inside, Running, and Waiting state counts. Its first check requires the vector to contain exactly 36 people. Its second requires actual Safe count to equal `rescued`; its third requires actual Aboard count to equal `aboard`; its fourth requires actual Lost count to equal `lost`.

The fifth check requires `rescued + aboard + lost + unresolved == 36`. In combination with the individual state-counter comparisons, this detects missing or double-accounted people. The sixth check requires the cabin count to lie inclusively between zero and eight. If phase is Debrief, a seventh conditional check requires unresolved to be zero and aboard to be zero.

The helper does not require every debrief to have 36 rescues, require best to equal rescued, inspect group identities, or compare stored cabin positions with the aircraft. It tests accounting invariants appropriate to both success and failure. Calls made while Flying produce six assertions; calls during Debrief produce seven.

## Reset, Population, and Pickup Cases

### Test Lines 115-140: Case 1, Reset and Title Do Not Advance

`runMissionTests()` imports the helper namespace locally and constructs a fresh `Results`. Each case is registered with `r.run` and a lambda capturing surrounding variables by reference. The first case calls `fresh(false)` to enter Title rather than active flight.

The checks at lines 121-126 require Title, wave one with breach time near 210, all four score-related counters at zero, all 36 people Inside, altitude near eight, and full boost near one. The combined counter check specifically includes best because `fresh` discards old records before reset.

The case snapshots aircraft position/yaw and harvester/worm positions. It sets the cursor to the right edge at midheight and holds D plus Space, then advances 100 seconds. Assertions at lines 133-138 require elapsed time still zero; aircraft position unchanged within 0.001 and wing phase near zero; unchanged yaw and zero smoothed turn rate; all crew still Inside with zero pickup; and both moving-world actors unchanged within 0.001.

These checks prove the update guard freezes title simulation even under nonneutral steering, movement, and winch input. They do not require `g_mouseHover` itself to stay zero, because cursor arithmetic is allowed to store hover while the simulation is inactive. The case closes with the six active/title reconciliation checks.

### Test Lines 142-178: Case 2, Pause Freezes Mission and Resumes

After `fresh`, the case applies right-edge hover and advances seven seconds. This gives an already moving harvester and a first group in the evacuation process, making pause more meaningful than freezing an untouched initial state. It copies the entire mission, snapshots flight position, wing phase, yaw, stored turn rate, harvester/worm positions, and worm gap, then sets pause and holds W, Shift, and Space.

After 100 simulated seconds of attempted advancement, lines 155-158 separately require an unchanged elapsed clock; unchanged aircraft position and wings; unchanged yaw and stored turn rate; and unchanged boost plus pickup progress. Because pause returns before boost logic, contradictory boost/winch keys do not alter resources.

The first comparison loop initializes `crewUnchanged` from vector-size equality, then proceeds only while that flag remains true and the index is within the copied population. Each person must retain state and position within 0.001. The assertion at line 163 checks that accumulated Boolean. It does not compare every Evacuee field, such as destination, individually.

Lines 164-166 require unchanged harvester and worm positions, yaw, accumulated travel, and gap. A second short-circuit comparison loop checks group vector-size equality, then released flags and centers within 0.001. Line 171 checks its result. The case clears all keys, centers the mouse, unpauses, advances 0.1 second, and requires elapsed time to increase. Reconciliation then confirms the still-active population.

### Test Lines 180-246: Case 3, Scheduled Groups and Fixed Clusters

The initial assertion requires exactly 15 groups. For each group, lines 186-187 check the repeating one-through-four size, release `6+7*i`, and an initially false release flag. The nested crew loop counts members with this group index. Each member must have a slot from zero through `count-1` and the group's release time within near tolerance.

A bitmask records slots by setting `1u << slot` for slots zero through three. The group assertion requires both the correct member count and the mask `(1u<<count)-1`. Together these verify every expected slot is represented exactly once: a duplicate would consume one of the fixed number of members and leave a required slot absent. The accumulated count must total 36 and the last group must contain three.

The case records the last Inside person's initial position and the initial harvester position, then advances 5.99 seconds. It requires all 36 still Inside. A combined movement assertion requires that last person to have moved more than two units, remain less than 15 horizontally from the current harvester, and the harvester itself to have moved more than two. This checks following rather than a fixed spawn position.

Advancing another 0.02 crosses the first release. The first person must be Running and exactly 35 remain Inside. The case records their current position, destination, and group center, then independently constructs the tangent, right vector, and exit at exactly time six. Lines 219-221 require center to equal scheduled exit plus 14 rightward units within horizontal tolerance 0.001, and the one-person destination to equal center horizontally.

Another 0.25 seconds must move that person by approximately `3.8*0.25 = 0.95`, with absolute movement error below 0.01. Another 5.74 seconds leaves elapsed near 12, before the second release at 13, and all 35 other people must still be Inside. Another 1.02 crosses that release: group one must be released and only 33 remain Inside.

Advancing 106 more seconds reaches a time at which all default groups have released and walked, but still before breach. The assertion requires 36 Waiting. Stored first destination and center must remain unchanged in full three-dimensional distance below 0.001. A loop then requires every person's horizontal distance to destination below 0.15 and their y near terrain height. The first and final group centers must differ by more than 20 horizontal units.

Finally, the first waiting position is snapshotted, five more seconds are advanced, and it must stay fixed within 0.001 while the harvester and worm continue. Reconciliation verifies all 36 remain unresolved but correctly counted. This case exercises actual scheduled evacuation rather than the compact fixture.

### Test Lines 248-267: Case 4, Space Dwell and Eight-Person Capacity

The case uses `readyCrew` and teleports to staging. One second without Space must leave aboard zero. It then holds Space for 0.3 seconds, less than the pickup dwell, and requires aboard still zero but pickup progress positive.

Releasing Space for 0.05 seconds must reset progress to near zero. Holding it for 0.5 seconds must produce exactly one occupant and exactly one person in Aboard. Rescued and best must still be zero, proving boarding is not scoring.

After five additional seconds of eligible hovering, the capacity constant must equal eight and the cabin count must equal eight. The remaining 28 people must still be Waiting. This simultaneously checks repeated pickup, capacity enforcement, and preservation of ground crew once full. Reconciliation verifies cabin and population totals.

### Test Lines 269-278: Case 5, Excessive Speed Blocks Pickup

The compact fixture is prepared, the craft is placed at staging, and Space is held. The case deliberately gives the aircraft a velocity of `(20,0,0)` and runs one 0.05-second update. With Space damping at rate 12, it remains fast enough to exceed the six-unit threshold despite slowing.

The first assertion explicitly checks that setup still has speed greater than six. The second requires aboard zero and pickup progress near zero. Without the first assertion, a test could accidentally pass or fail after its injected velocity had decayed below the intended boundary. Reconciliation checks population consistency. This is not a sustained high-speed flight test; it isolates one invalid pickup frame.

### Test Lines 280-294: Case 6, High Hover Must Lower Before Pickup

The aircraft begins at staging 35 units above local terrain. After 0.75 seconds without Space, aboard must be zero and desired altitude still near 35. The case then holds Space for 0.5 seconds.

Actual clearance must still exceed 11 after that initial lowering period, and aboard plus pickup progress must both remain zero. This distinguishes desired-altitude smoothing from actual y smoothing; merely pressing Space does not instantly satisfy clearance.

After two further seconds, measured clearance must be at most 11. At least one person must be aboard, while rescued remains zero. Reconciliation confirms no accounting change was mistaken for scoring. The scenario demonstrates eventual winch lowering, not a direct Q/E altitude-key test.

### Test Lines 296-318: Case 7, Delivery Banks Only at Base

`collect(3)` must create exactly three unbanked occupants. The craft then hovers at base plus 15 x units and advances 1.4 seconds with Space still held by collection. Aboard must remain three, rescued zero, and unload near zero. This tests a point outside the strict 14-unit base radius, not the exact radius boundary.

At base, Space is released and 1.3 seconds passes; rescued must still be zero. Space is held for 0.6 seconds; rescued remains zero but unload becomes positive. Releasing it for 0.02 seconds resets unload to zero.

`deliver()` then supplies a fresh eligible dwell. The next combined check requires rescued three, best three, and no occupants. Exactly three people must be Safe and the phase must remain Flying because 33 unresolved people remain. Reconciliation checks that their states match all counters. This case separately tests location, key requirement, dwell, interruption, score banking, and continuation after a partial delivery.

## Failure, Retry, and Completion Cases

### Test Lines 320-347: Case 8, Swallowing Spares Remote Ground Crew

The case delays every group and every person's release to 1,000 seconds. It sets elapsed to breach plus 11.5 and updates the harvester, then makes person one Running exactly 40 x units from the worm with destination equal to position. People two and three become Waiting at base and base plus two x units. The other 33 remain Inside.

One 0.25 update reaches attack 11.75. No one must be lost and phase must remain Flying: the local person is beyond the 32-unit radius and Inside swallowing has not started. Because destination equals position, the Running local person can become Waiting during the crew loop; the scenario's key property is being outside at 40 units, not continued walking.

Another 0.25 reaches attack exactly 12. The assertion requires attack time near 12, losses 34, and no Inside people. Those losses are the 33 Inside people plus the outside person within the now-44-unit radius. Exactly two Waiting people must survive, phase remain Flying, and rescued stay zero.

After eight seconds, attack is 20 and local outdoor hazards have stopped. The craft is teleported to base and Space is held for one second. It must carry both survivors with losses still 34. `deliver()` must bank two and produce Debrief because nobody else remains unresolved. The seven debrief reconciliation checks prove all 36 are accounted for. The test explicitly disproves the assumption that swallowing automatically kills every waiting person anywhere in the world.

### Test Lines 349-367: Case 9, Carried Crew Survive Swallowing

The compact fixture collects eight, and an assertion confirms that setup. Every non-Aboard person is changed to Inside and given a personal release time of 1,000. Group release flags remain true from the fixture, so these manually Inside people are not rereleased by the scheduled loop.

Space is released; elapsed is placed at breach plus 11.75; one 0.25 update reaches attack 12. Exactly 28 must be lost while eight remain aboard. Phase must still be Flying and rescued zero, demonstrating that cabin survival is not yet a banked rescue.

Reconciliation runs before delivery. `deliver()` then must produce Debrief, rescued eight, and best eight. A second reconciliation runs in the finished state. This separates ordinary crew swallowing from aircraft collision: the aircraft remains remote, so carried people are not automatically consumed with the harvester.

### Test Lines 369-387: Case 10, Collision Preserves Banked Score

The fixture collects eight, delivers them, then collects eight more. An assertion requires rescued eight and aboard eight. Space is released, elapsed becomes breach plus four, and the harvester is updated. The craft is teleported to the worm and a 0.01 update triggers the active breach aircraft envelope.

Phase must become Debrief and cabin count zero. Losses must equal 28, while rescued and best both remain eight. The 28 are every unbanked person, including eight occupants and 20 ground people. The state reconciliation confirms the eight already Safe were not included in losses.

The case snapshots elapsed, advances ten more seconds, and requires elapsed unchanged and losses still 28. That tests both the debrief guard and protection against double-counting finished losses. This particular case does not separately assert `planeLost`; the dedicated envelope case does.

### Test Lines 389-409: Case 11, Extraction Timeout

After banking eight and carrying another eight, the aircraft is placed 20 x units from base, outside unloading radius, and Space is released. Every remaining Waiting person is placed at base plus `(40,0,40)`, remote from the stopped worm. Elapsed becomes breach plus 59.75.

A 0.125 update reaches attack 59.875. Assertions require Flying, eight occupants, zero losses, and 20 Waiting people. Reconciliation verifies that the setup is valid just before timeout.

A second 0.125 reaches exactly 60. The next assertion requires attack time near 60 and Debrief. The final totals must be rescued eight, best eight, losses 28, and cabin zero. Timeout consumes both undelivered cabin occupants and unresolved remote ground crew, but not the banked eight. Final reconciliation verifies the complete accounting. The aircraft need not collide or set `planeLost` for this ending.

### Test Lines 411-440: Case 12, Retry Preserves Best and Clears Attempt State

The fixture collects and delivers eight, releases Space, places elapsed at breach plus 60, and calls `updateMission(0)`. The setup assertion requires Debrief and best eight. Zero dt is enough because the ending condition evaluates the already-set clock.

The case deliberately contaminates pause, boost, pickup, unload, target, velocity, hover, and stored turn rate. Values include boost 0.2, pickup 0.7, unload 0.4, target five, velocity `(10,0,0)`, corner hover, and turn rate -50. It then calls ordinary `resetMission(true)`, not `fresh`, so preserving best is actually exercised.

The six subsequent checks require Flying, unpaused, wave one; best eight with current rescued/aboard/lost zero; elapsed and both dwell values near zero with target -1; full boost, altitude eight, and velocity length below 0.001; neutral hover, zero turn rate, and camera pitch 12; and aircraft position within 0.001 of base plus eight upward units with all 36 Inside.

Reconciliation checks the new population. The scenario does not claim retry clears keys or wing phase. Its assertions are restricted to state the reset actually restores and the camera default derived from those resets.

### Test Lines 442-457: Case 13, Next-Wave Timer Floor

After `fresh`, the test writes best 17 and directly calls `finishMission`, then resets with both arguments true. Wave must become two with breach time near 200. Best must remain 17, phase Flying, and all 36 Inside.

An eight-iteration loop snapshots the current breach time and starts another next wave. Each iteration checks that the new threshold is no greater than the previous one and no less than 150. Afterward wave must equal ten and time be near 150. Reconciliation validates the final fresh population.

This directly exercises reset's progression formula without playing every intermediate wave. It does not prove later waves remain fully rescuable before their shorter breach thresholds; the detailed input-only playability scenario uses wave one only.

### Test Lines 459-490: Case 14, All 36 Through Real Crew State Flow

This case uses normal `fresh` and advances 120 seconds so all real scheduled groups release and walk. It first requires 36 Waiting. It does not use `readyCrew`, but it still uses teleport assistance for navigation.

`banked` starts at zero. The five-flight loop chooses `batch = min(8,36-banked)`, producing batches 8,8,8,8,4. For each person in the batch, a nested search starts with no index and infinite distance, scans actual Waiting crew, and picks the nearest to current aircraft position. If no target is found it breaks that inner collection loop rather than dereferencing -1.

Each selected position is reached with `hover`, Space is held, and 0.45 seconds are advanced. At the end of each batch, cabin must equal expected batch while rescued still equals the previous banked total. Reconciliation checks the load. Delivery then runs, banked increases by the expected batch, and rescued must equal that value with an empty cabin.

The phase assertion expects Flying for every banked total below 36 and Debrief exactly at 36. Reconciliation runs after each base visit. Final assertions require rescued and best 36, zero losses, 36 Safe, and elapsed before breach. This proves full successful state transitions are possible without altering the timer, but its instantaneous relocation does not by itself prove a physically flown route meets the deadline.

## Input-Only Rescue Scenario

Case 15 is substantially larger than the other cases because it constructs a small test-side controller. The controller reads the world and emits Boolean key states. It is not player-facing autopilot shipped in the gameplay header. After its initial `fresh`, it does not teleport the aircraft, directly change velocity, set crew states, edit the clock, or shorten the deadline. All subsequent game-state transitions come from the actual updater.

### Test Lines 492-504: Case 15 Setup and Finite-Vector Helper

The case begins with `fresh` and an assertion that wave is one and the breach threshold is near 210. The comments explicitly rule out `hover`, `collect`, and `deliver` assistance after reset. The mouse remains neutral, so aircraft yaw stays at its initial zero and world-aligned cardinal movement is available through the aircraft-relative keys.

The controller snapshots base and uses a constant dt of `1/120`. `steps`, `travelSteps`, `boostSteps`, `directionsUsed`, and `directionReversals` start at zero. Steps count every update; travelSteps count updates performed inside flight-leg attempts; boostSteps count actual boost frames; directionsUsed is a bitmask; directionReversals count changes between positive and negative commands within a flight leg.

`finiteState`, `clearOfGround`, and `neutralHeading` start true and are accumulated as permanent success flags. Once one becomes false, later good frames cannot restore it. `minimumClearance` starts with actual initial aircraft y minus local terrain height. The `finiteVector` lambda checks all three components with `std::isfinite`, detecting NaN and either infinity.

The lambda objects capture the surrounding test variables as needed. A capture by reference means updates inside a lambda affect those original counters and flags, rather than independent copies. The later `seconds` lambda instead captures constant dt by value, since it only converts a tick count to display time.

### Test Lines 505-529: Step Instrumentation and Every Observed Quantity

Before each game update, `step` records W as bit one, A as bit two, S as bit four, and D as bit eight whenever held. Bitwise OR preserves every direction ever used. It then calls `updateMission(dt)`, increments total steps, and increments boostSteps only if the updater reports actual `g_isBoosting`.

Finite-state checks include every component of aircraft position, velocity, and base; aircraft yaw, pitch, roll, speed, and wing phase; mission elapsed time, breach time, desired altitude, boost, pickup progress, and unload progress; both hover axes; and stored mouse turn rate. A loop also checks every person's position, destination, and release time.

These checks are extensive but not literally every floating-point field in the mission. They do not test finite group centers/release values, harvesterTravel, harvesterYaw, or wormYaw in this lambda. The final assertion's wording says all flight and crew float state, and should be read according to this actual list rather than generalized to every object in the application.

Neutral-heading monitoring requires yaw near zero, stored turn rate near zero, and hover vector length below 0.001 on every substep. This checks that WASD translation never secretly aligns the nose with its movement direction. Ground monitoring computes actual clearance each time, requires it finite and at least `3-0.001`, and updates the minimum encountered clearance.

Because these are cumulative checks performed after every update, they can catch a transient invalid frame even if final state later looks correct. They still sample at update boundaries; they do not test a continuous path between those samples or compare the craft with rendered rock geometry.

### Test Lines 530-550: Active Predicate, Flight-Leg State, and Braking

`active()` requires finiteState, phase Flying, and elapsed strictly before breach. The controller voluntarily stops at breach even though gameplay permits further extraction for 60 seconds. Thus a successful result is a pre-breach playability result, not merely a rescue before the final timeout.

`fly(target)` snapshots the current step count. It starts previous X/Z command signs at zero, per-axis pulse accumulators at zero, and braking false. Its while loop continues only while active and fewer than `12*120` updates have elapsed for this leg, limiting a leg to 12 seconds.

Each iteration calculates a target delta and horizontal distance. Braking becomes permanently true for the rest of the leg once distance is below `4 + speed/12`. The `speed/12` term approximates coasting distance under Space's rate-12 damping; the four-unit margin starts slowing early. Once braking is set it is not unset if the next distance grows slightly.

All keys are cleared before new commands are chosen. Space is held exactly when braking. Left Shift is requested only while not braking, distance exceeds 20, and boost is above 0.08. This gives a controller-specific energy margin above the game's 0.04 lock threshold.

The leg succeeds when horizontal distance is at most two and actual speed is below 0.5. It adds its elapsed updates to travelSteps and returns true. These tolerances are more restrictive than the actual pickup/base radii and speed-six dwell requirement, helping stabilize arrival. Because the return happens before this iteration's `step`, no extra update is needed solely to acknowledge arrival.

### Test Lines 551-569: Boolean-Key Pulse Controller and Axis Projection

If still more than two units away, the controller estimates available speed as 2.8 in braking mode, otherwise 48 if Shift is requested or 28 if not. It selects damping 12 while braking or 5.5 otherwise. The estimated speed can differ from actual boost behavior if the game's lock rejects Shift, but the game remains the authority over motion.

It constructs `backward = -flightForward()` and aircraft right from the cross product. These form the horizontal axes for commands. Dot products project world-space position error and actual velocity onto each axis, so the controller still uses the declared aircraft coordinate system rather than writing x/z velocity directly.

The nested `axis` lambda takes error, velocity, a pulse accumulator by reference, and the negative/positive key constants. It predicts remaining error after coasting as `error - velocity/damping`. If the current absolute error is below one, or predicted error multiplied by current error is at most zero, it clears the accumulator and emits no key. The latter means estimated coast movement is sufficient to reach or cross the target on that axis.

Otherwise, it adds `min(1,abs(predicted)*(braking?2:4)/speed)` to the accumulator. When that accumulator reaches one, it presses the key corresponding to the sign of error and subtracts one. Smaller desired commands therefore become occasional one-frame key pulses. This expresses proportional control through Boolean keys, not through fractional keyboard values, position assignments, or velocity overrides.

For the right axis, negative means A and positive D. For the backward axis, negative means W and positive S. The reversed-looking W/S arguments are correct because the axis is backward, not forward. Diagonal pulses are still normalized by actual game logic. Pulse duty cycle and feedback influence the route, but all acceleration, braking, altitude lowering, boost consumption, and pickup remain ordinary updates.

### Test Lines 570-590: Reversal Accounting, Leg Failure, and Space-Only Steps

Command X is D minus A; command Z is S minus W. Only nonzero commands are compared with their previous nonzero sign. If a previous sign exists and differs, directionReversals increments. Zero-input coasting frames do not erase the remembered sign. Each `fly` call initializes these memories anew, so an outbound D and a later return A are not counted as a reversal within one leg.

The controller executes `step()` after commands and reversal checks. If active becomes false or the leg budget expires before arrival, it adds that leg's steps to travelSteps and returns false. This ensures unsuccessful movement attempts still contribute to reported travel time.

`holdSpace()` clears all keys, enables only Space, and performs one instrumented step. It is used for waiting, boarding, and unloading. It does not directly complete a dwell. The `seconds(ticks)` helper multiplies ticks by dt and rounds to the nearest millisecond using `round(value*1000)/1000`; that affects printed summaries, not simulated time.

### Test Lines 591-617: Return Decisions, Banking Assertions, and Trip Output

The controller initializes completed flights, cluster visits, outbound/boarding timing counters, and a 15-group visit bitmask. Departure is the current total step count. In the main while loop, `outside` counts Running plus Waiting, and `allReleased` means no Inside people remain.

Return is chosen with a full cabin, or with a nonempty partial cabin when all crew have released and no outside people remain. The assertion at line 598 requires either a batch of eight or that final-survivor condition. It prevents intentionally inefficient early partial returns while people still await pickup.

The controller snapshots batch, banked score, and boarding-completion step, then flies to the captured base. A check requires successful arrival; if arrival failed it breaks out instead of entering an endless unloading loop. While still active and occupants remain, it calls holdSpace. The next assertion requires rescued to increase by exactly the original carried batch and aboard to become zero. Reconciliation verifies all individual states afterward.

Each completed trip prints its sequential flight number, outbound duration, boarding/waiting duration, return duration, unloading duration, batch size, total rescued, elapsed mission time, and total trip duration. It increments flights as part of that print expression. Departure then becomes current steps, outbound and boarding counters reset to zero, and `continue` begins the next main iteration.

These durations classify test-controller phases. They are not guaranteed to match exclusive physical activities: Space during braking can already begin pickup or unloading before `fly` reports settled arrival. The printed unloading interval after settled arrival may therefore be less than 1.2 seconds because some progress happened during the return approach. No assertion requires a fixed wall-clock duration from these print labels.

### Test Lines 618-646: Selecting Released Crew and Visiting Every Cluster

If no return is due, a nearest-person search starts with index -1 and infinite distance. It scans only Running or Waiting people and selects strictly smaller horizontal distances. If none exist yet, holdSpace performs a waiting step, boardingSteps increases, and the controller retries. Early scheduled gaps therefore consume legitimate mission time.

For a selected person, the controller records group and chooses a fixed pickup point: destination for Running crew, actual position for Waiting crew. It does not chase the Running person's changing position every frame. It flies to that point, adds the leg to outboundSteps, asserts successful arrival, and breaks on failure.

Successful arrival increments clustersVisited. A group index between zero and 14 sets its bit in visitedGroups. Repeated visits increase clustersVisited repeatedly but do not create new bits, so the final bitmask is necessary to prove coverage of every group rather than merely many visits to a few groups.

After arrival, a loop holds Space while active, cabin is below capacity, fewer than `3*120` updates have passed at the cluster, and the originally selected person's state remains Running or Waiting. It stops when that person is boarded/lost, the cabin fills, the local three-second budget ends, or active fails. Other nearby people can also be picked up during these ordinary updates. Boarding timing accumulates after the loop, and the outer controller reevaluates the world.

This loop does not forcibly wait until an entire group is collected in one visit. A group can require repeated outer-loop selections. The complete-group coverage and full-load return conditions are validated by final assertions rather than assumed from the controller's intent.

### Test Lines 647-666: Every Final Input-Only Assertion and Summary

All keys are cleared at the end of the controller. The first final check requires the accumulated finiteState flag, and the second requires neutralHeading throughout the route. The third requires the cumulative ground-clearance flag. These validate numerical safety, no unintended yaw, and the terrain floor across all sampled substeps.

The fourth requires `directionsUsed == 15`, meaning every W/A/S/D bit was exercised, plus actual boostSteps greater than zero. The fifth requires directionReversals zero. It means no individual `fly` leg changed its nonzero cardinal command sign on an axis; it does not forbid opposite directions on different outbound/return legs.

The sixth requires rescued 36, zero losses, and 36 Safe. The seventh requires Debrief, best 36, and no occupants. The eighth requires elapsed strictly less than 210 and the unchanged breach threshold near 210. These prove a completely banked, lossless first-wave finish before breach rather than a partially filled cabin at the deadline.

The ninth requires exactly five completed flights, at least 15 cluster arrivals, and visitedGroups equal `(1u<<15)-1`. Those conditions establish visits to all 15 released groups and the four full eight-person loads plus final four-person load implied by capacity and total 36. Reconciliation supplies the seven debrief accounting checks.

The final print reports travel seconds, elapsed seconds, rescued out of 36, aboard, lost, flights, clusters, minimum clearance, and approach reversals. The case then closes. Output reports are diagnostics, not extra Boolean assertions. Exact numeric travel durations are determined by execution and should not be invented from this source alone.

## Analytic Route and Hazard Cases

### Test Lines 668-705: Case 16, Route, Heading, Breach Freeze, and Worm Gap

The first part begins with `fresh` and samples route times 0,6,25,60,104,150,210. For each, it independently constructs x and z from the documented sinusoidal formulas and terrain height. Its first assertion requires the route helper to match that expected three-dimensional position within 0.001.

It converts route yaw to radians, constructs negative-Z-model forward, and independently calculates the derivative tangent. The second assertion requires finite yaw and a dot product greater than 0.99999 between heading and normalized tangent. A unit-vector dot product near one means the directions nearly coincide, rather than merely having similar angle numbers modulo 360.

It calls `updateHarvester(t)`. The third assertion requires current position to match expected within 0.001 and yaw difference, wrapped with remainder, to have magnitude below 0.001. That checks both the pure helpers and the state-mutating helper. Three checks are executed at each of the seven sampled times.

After the final sample, the case snapshots breach position, yaw, and travel, then calls updateHarvester at breach plus 50. A combined assertion requires all three unchanged, checking the clamp and zero additional travel beyond the endpoint. It does not compare accumulated travel with an exact analytic arc length.

The second part calls `fresh` again and sets previousGap to 221, larger than the initial expected 220. It samples 0,1,30,75,104,150,209,210,230 by assigning elapsed and updating the harvester. Expected gap is independently calculated as `220-202*clamp(t/breachAt,0,1)`.

At each of nine times, one check requires gap near expected and no greater than the previous sampled gap. Another requires worm x/z to match current harvester minus expected gap in x within 0.001. A third requires stored wormYaw near 90 and worm y near its own terrain height. It then records the gap for the next monotonic comparison.

This is a geometry test using deliberate clock assignments. It proves the worm relation, monotonic sampled closure, terrain following, and fixed stored yaw. It does not move an aircraft through a route or test whether the worm model's independently animated pose matches every geometric point.

### Test Lines 707-748: Case 17, Frame-Rate-Stable Scheduled Release

Empty reference vectors are created for crew and groups. The outer loop runs at 30,60,120 updates per second. Each frequency starts with `fresh`; an inner loop performs 30 simulated seconds of updates. Before each update, elapsed is explicitly set to `(tick-1)/frequency`, then the updater receives `1/frequency`.

These clock assignments intentionally remove accumulated-float drift from this fixture. This test is about release pose and walking time at equal absolute endpoints, not a proof that a long real clock accumulates identically across frame rates. The source comment contrasts it with the previous controller, which never edits the real clock.

The first, 30-Hz run is saved as reference. At later frequencies, every crew member must have the same state, horizontal position within 0.01, and full destination within 0.001. Every group's released flag must match and center must be within 0.001. At time 30, groups scheduled at six,13,20,27 have released; later groups remain Inside. Comparing all 36 therefore covers both released and unreleased populations.

The next part tests a release-straddling coarse frame. Starting from elapsed 5.99, it updates the harvester and runs dt 0.02, ending near 6.01. It saves first-person position/destination and first group center. A second fresh setup starts at 5.75, updates the harvester, and runs dt 0.26, also ending near 6.01.

The final combined assertion requires the two first-person horizontal positions within 0.001 and destination plus center within 0.001 in full distance. Both frames allow only approximately 0.01 second of post-release walking, even though their total frame durations differ drastically. This directly validates `min(dt,elapsed-release)` and use of the scheduled route pose. It does not compare coarse pickup throughput, aircraft displacement, or the other crew hazards across arbitrary frame sizes.

### Test Lines 750-774: Case 18, Pursuit Crew and Aircraft Envelopes

Case 18 contains four distinct setups inside one named `run`. The first delays every group and person release to 1,000, sets elapsed to 100 before the default breach, and updates the harvester. Person zero becomes Running at worm plus 15 x units; person one becomes Waiting at worm plus 17. Destinations equal their positions.

After dt 0.01, the first assertion requires person zero Lost, person one Waiting, and loss count one. That distinguishes the 16-unit pursuit radius. The second requires 34 still Inside and phase Flying, showing ordinary pre-breach pursuit does not swallow Inside people or end a remote aircraft's mission.

The craft is then teleported at the worm with altitude 30. After another 0.01, phase must remain Flying and planeLost false, proving it is above the 22-unit aircraft height envelope. Teleporting at altitude 21 and updating 0.01 must instead produce Debrief, planeLost true, and all 36 Lost. Reconciliation verifies that the earlier local loss was not counted twice when the crash lost everyone else.

The locations are set relative to the worm at setup, and the worm moves slightly in the following update. The chosen distances 15/17 and heights 30/21 have margins, so this is not an exact equality test for 16, 18, or 22. Aircraft y also passes through normal smoothing before the hit test.

### Test Lines 776-795: Case 18, Early Breach Crew and Aircraft Envelopes

A new fresh setup again delays release and places elapsed at breach plus four. People zero and one become Waiting at worm plus 31 and 33 x units. The craft is placed at worm altitude 45, then dt 0.01 runs.

The crew assertion requires the 31-unit person Lost and the 33-unit person still Waiting, distinguishing the active 32-unit outdoor breach radius before swallowing. The phase assertion requires Flying, because the aircraft remains above the 40-unit height envelope.

The aircraft is then placed at worm altitude 39 and another dt 0.01 runs. A combined assertion requires planeLost and Debrief, demonstrating the active breach aircraft envelope at horizontal distance near zero and height below worm terrain plus 40. Reconciliation follows. This segment does not independently assert a specific final loss counter; the reconciliation checks actual Lost states against the counter.

### Test Lines 797-822: Case 18, Exact Start and Retreat Boundaries

The third fresh setup delays release and sets elapsed exactly eight. The first person is manually Waiting at the current worm position. `updateMission(0)` must leave that person Waiting because pursuit activation uses elapsed greater than eight, not greater than or equal. A subsequent 0.01 update must mark them Lost and set loss count one.

The fourth setup resets again, delays releases, and places elapsed at breach plus 18. The first person is Waiting exactly at the worm; the aircraft is remote at base. A zero-dt update must yield 35 losses, one Waiting person, and phase Flying. The 35 Inside people are swallowed because attack is already at least 12, but outside danger has ceased because its upper bound is strictly less than 18.

Reconciliation confirms one unresolved survivor remains. This scenario highlights that the retreat boundary does not revive prior Lost people and does not stop the unconditional Inside-crew loss rule. The closing lambda brace ends the whole four-part case.

## Cursor and Steering Cases

### Test Lines 824-856: Case 19, Cursor Arithmetic and Resize

The case snapshots aircraft position, yaw, and elapsed after fresh reset. Center cursor `(500,250)` in a `1000x500` window must produce hover length below 0.001.

The horizontal sample loop uses x values 440, 460, 500, 540, 560 with centered y. Each must give x hover near zero, including the two exact nominal deadzone edges. The vertical sample loop uses y values 220, 230, 250, 270, 280 with centered x; each y hover must be near zero. These are five assertions per loop, not a single check of one representative center.

At x = 570, horizontal hover must lie strictly between zero and one; at x = 430 it must lie strictly between -1 and zero. At the left window edge, hover must be x = -1 and y = 0; at the right edge, x = 1 and y = 0.

At `(800,100)` the raw normalized coordinates are `(0.6,-0.6)`. After deadzone, the assertion expects `(0.48/0.88,-0.48/0.88)` within near tolerance. Doubling coordinates and dimensions to `(1600,200)` in `2000x1000` must preserve that vector within 0.001. The new center `(1000,500)` must return neutral hover.

The final combined assertion requires aircraft position unchanged, yaw unchanged, elapsed unchanged, and stored turn rate still zero. Repeated arithmetic input changes do not simulate a frame. This case validates normalized behavior after resize, but it does not create or resize an actual GLFW window or test event callback coordinate scaling.

### Test Lines 858-878: Case 20, All Invalid Cursor Scenarios

The case obtains quiet NaN and positive infinity from `std::numeric_limits<double>`. A local `Cursor` record stores two double coordinates and integer width/height. Its array contains 14 invalid inputs, each tested independently after reseeding active hover.

The four outside-window scenarios are x = -0.01, x = 1000.01, y = -0.01, and y = 500.01 in the original `1000x500` dimensions. Two NaN scenarios put NaN in x or y. Four infinity scenarios put positive or negative infinity in either coordinate. Four invalid-size scenarios use width zero, height zero, width -1, or height -1 with zero cursor coordinates.

For each array entry, the case first supplies valid right/top corner hover and requires its vector length above 0.5. It then submits the invalid input and requires both components finite and total length below 0.001. Thus there are two assertions for each of the 14 scenarios: active setup and safe clearing.

The reseeding is important. Without it, an implementation that merely ignored invalid input could appear correct after the first zeroed result. These checks ensure invalid input actively clears previous steering on both axes, without producing NaN. Accepted exact window edges are covered separately by Case 19, so slightly outside values here intentionally test strict rejection.

### Test Lines 880-908: Case 21, Hover Turns Without Clicks and W Follows It

The outer loop runs side -1 and side +1. Each begins fresh, records position, sets left edge for negative side or right edge for positive side, and advances 0.5 second with no keyboard movement or mouse-click input.

The first assertion requires yaw times side below -1 and stored rate times side below -1: right produces negative yaw/rate, left positive. The second requires rate near `-side*80*(1-exp(-8*0.5))`, validating the exponential approach at the half-second endpoint. The third requires horizontal position unchanged within 0.001 and speed near zero. The fourth requires forward.x times side positive and forward.z negative, proving the new nose points toward the hovered side while still generally facing negative Z.

The cursor is then centered and two seconds pass to damp residual turning. The case records settled yaw, forward, and departure position, holds W, and advances 0.5. The next assertion requires displacement length above one, normalized displacement dot settled forward above 0.999, and displacement.x times side positive. The final assertion requires yaw change from the recorded settled value below 0.001.

Both left and right scenarios therefore prove stationary yaw steering, the correct sign convention, no unintended translation from hover, forward movement in the newly chosen heading, and heading retention during centered forward flight. The method does not require any button press for mouse steering.

### Test Lines 910-921: Case 22, Arrow-Key Stationary Backup

For side -1 and +1, each fresh setup records aircraft position and holds the corresponding Left or Right arrow for 0.5 second. A sign assertion requires yaw*side and stored turnRate*side below -1, matching hover steering's sign.

A second assertion requires horizontal position unchanged within 0.001 and speed near zero. This confirms arrows rotate a stationary craft without injecting translational input. It does not separately test simultaneous mouse-plus-arrow saturation, both arrows held together, or Up/Down keys; those behaviors must be inferred only where the game expressions actually define them.

### Test Lines 923-946: Case 23, Every Translation Key at Wrapped Headings

The key array is W, S, A, D. The heading array is 0, 90, -90, 179, -181, 359, 361. The outer loop independently constructs forward `(-sin,0,-cos)` and right `(cos,0,-sin)` for each heading. The inner loop performs all four keys, giving 28 combinations.

Each combination resets, assigns the chosen yaw, and first asserts `flightForward()` matches the independently calculated vector within 0.001. Expected motion is forward for W, reverse forward for S, right for D, and reverse right for A. After recording position and holding the chosen key for 0.5 second, it forms horizontal displacement.

The second assertion requires velocity length above one and normalized velocity aligned with expected by dot product above 0.999, plus displacement length above one and its normalized direction likewise aligned. The third requires wrapped yaw difference from the assigned heading below 0.001 and stored turn rate near zero.

There are three assertion calls per combination, or 84 total within this case. The unusual headings check equivalent angles around wrap boundaries: 179 and -181 represent the same direction; 359 and -1 would as well; 361 is equivalent to one. The yaw storage can wrap during an update without failing because comparison uses remainder. Reverse and strafe are explicitly verified not to turn the nose toward velocity.

### Test Lines 948-962: Case 24, Centering Damps Inertia Rather Than Recentering Nose

The case steers right for 0.5 second, snapshots yaw and turn rate, centers the mouse, then advances 0.25. The first assertion requires the new stored rate near `oldRate*exp(-8*0.25)`. The second requires yaw smaller than its previous value and rate still negative, demonstrating continued turn while inertia decays rather than an instant snap.

Two more seconds must reduce rate magnitude below 0.001. After recording that settled yaw, another 0.25 must change it by less than 0.001. A centered mouse thus means stop requesting turn, not aim the nose back to zero degrees. These four checks cover the release transient, eventual settling, and retained heading.

### Test Lines 964-980: Case 25, Exact One-Second Turning at Three Frequencies

Two three-element arrays store yaw and rate. The frequency array is 30, 60, 120. Each run resets, applies right-edge hover, and calls the updater exactly frequency times with dt `1/frequency`, simulating one second without the `advance` helper.

The rate assertion requires absolute error below 0.002 from `-80*(1-exp(-8))`. The yaw assertion uses the exact one-second integral `-80*(1-(1-exp(-8))/8)` and also requires error below 0.002. Two checks run at each frequency.

After the loop, a final combined assertion requires 30-Hz yaw and 60-Hz yaw each within 0.002 of 120-Hz yaw. This is a seven-check case. It is strong evidence for the analytic angular integration under constant steering, but it does not show equivalent translational trajectories, roll animations, target switching, pickup totals under very coarse steps, or arbitrary changing-input schedules.

## Camera Cases and Test Result

### Test Lines 982-1006: Case 26, Every Camera Behind Heading

After fresh reset, headings 0, 90, -90, 179, -181, 359, 361, 720 are tested. The addition of 720 checks multiple full rotations, because trigonometric direction should be unchanged. For each heading, the case computes flight forward and aircraft right. It then tests vertical cursor positions 0, 250, 500 in a `1000x500` window, and camera modes 0, 1, 2.

That gives eight headings times three vertical inputs times three modes, or 72 combinations. For each combination, the offset and its horizontal projection are calculated. The first check requires all offset components finite and y positive. The second requires nonzero horizontal length and normalized horizontal offset dot forward below -0.999, proving the camera is directly behind rather than merely somewhere elevated.

The case computes `viewRight = normalize(cross(-offset,up))`, treating the direction from camera toward target as `-offset`. The third check requires its dot with aircraft right above 0.999. This ensures D/right movement is screen-right in the corresponding look-at orientation rather than visually reversed.

Exactly one mode-specific fourth check runs per combination. Mode 0 requires offset length near 40. Mode 1 subtracts `(0,2.2,0)` before requiring length near 20. Mode 2 requires y near 100 and horizontal length near 26. The branch structure means each combination receives four checks, for 288 assertion executions.

This verifies helper geometry and screen-right basis, not an OpenGL image, projection matrix, camera-target offset used elsewhere, or a real mouse callback. Mode 2 is checked at all three vertical cursor positions even though its offset deliberately ignores camera pitch.

### Test Lines 1008-1036: Case 27, Vertical Pitch Mapping, Orbit Clamp, and Both Reset Modes

The case begins centered and requires pitch 12. At the top edge, it records pitch and checks hover magnitude near one plus exact mapping to the clamped expression `12-hover.y*22`. At the bottom edge, it performs the corresponding check again. A fourth check requires the lesser of these pitches near 6 and the greater near 34.

With bottom-edge hover still stored, an orbit loop uses -100, 0, 15, 100. For each, it writes `g_camOrbitPitch` and checks pitch against `clamp(12-hover.y*22+orbit,6,65)`. Those inputs exercise extreme low/high clamping and intermediate adjustment. They demonstrate that the full 6-to-65 range includes orbit contribution, while mouse extremes alone span 6-to-34 under zero orbit.

The final loop runs `start=false` and `start=true`. Before each reset it installs right/top hover, stored turn rate -60, and orbit 35. Calling `resetMission(start)` must center both hover axes, clear turn inertia, and restore yaw zero. A second check requires orbit zero and derived camera pitch 12. These are two checks per reset mode.

The scenario proves camera and steering state restoration for both title and playing resets. It does not explicitly check their phase values here because title and flying phase selection are already checked elsewhere. There are 12 assertion executions in this case: four initial mapping checks, four orbit checks, and four reset checks.

### Test Lines 1038-1041: Console Summary and Process Result

After all 27 named cases, the function prints a summary containing successful cases over total cases, total checks, and total failures. Successful cases are calculated as `cases-failedCases`, not as checks minus failures. The checks total depends on repeated loops and, for the input controller, the route's visited clusters and return legs.

It returns zero when failures is zero and one otherwise. A caller can use that result as an executable exit code for automation. The code does not return the number of failures or use different exit codes for different cases. Standard-output case labels and diagnostics plus standard-error assertion messages provide the human-readable detail.

This chapter explains what the suite checks; it is not an execution log or a claim that a build was run while writing the documentation. The build-and-run documentation describes the actual test invocation and any independently observed run results.

## Important Limits

The implementation and tests are deliberately small and deterministic. Understanding their actual boundaries is as important as understanding the successful scenarios.

- Best score is process-local mission memory here. Reset preserves it, but this header provides no persistent save file.
- Aboard and Safe are logical states. Stored crew positions are not updated to aircraft or base coordinates when those states change.
- The unresolved counter called `waiting` includes Inside and Running, not merely Waiting. Natural completion requires no unresolved people and no cabin occupants, not a special victory flag.
- Harvester movement and release destinations use analytic route formulas. Harvester accumulated travel is sampled chord length, and translational position integration is not an exact continuous integral.
- Angular rate and yaw have an explicit exponential solution under a constant steering command. This does not make all controls, animation values, dwell transactions, or hazards invariant under arbitrarily large dt.
- Pickup and unload progress reset on interruption. Pickup additionally resets when nearest target changes. One update boards at most one person, and excess progress is discarded.
- Local crew hazard, local aircraft hazard, Inside swallowing, and final timeout are four distinct rules with different radii and time intervals.
- Clearance assistance is not a collision model for rendered rocks or the harvester. The only explicit aircraft-loss checks in this header are worm envelopes.
- Mouse invalid-input tests cover finite and nonfinite cursor data, but gameplay does not similarly validate every externally supplied dt or every externally mutated global.
- Most transition tests use deliberate fixture mutations or teleportation. The input-only controller is the specific case proving an actual key-driven complete first-wave rescue before the unchanged breach timer.
- The controller's no-reversal assertion applies within each flight leg. Opposite commands on separate return legs are expected and are exercised.
- Headless mission tests do not verify rendered pixels, model orientation in a screenshot, graphics driver behavior, real-time audio playback, window callbacks, or asset loading.
- Later waves have shorter timers, but the exhaustive input-only rescue scenario establishes full pre-breach completion for wave one, not every difficulty wave.

## Coverage Reference

The game explanation covers original lines 1-307, including both enumerations, all three structures, six crew states, three mission phases, every default field, six game globals/constants, all eight rock sites, every helper, both reset loops, tank records, finish behavior, and every update branch. The headings divide the file into source-order ranges so a reader can match each nonblank line to its explanation.

The test explanation covers original lines 1-1041. It explains the namespace and dependencies; all five Results fields; both Results methods; `near`, `count`, `fresh`, `advance`, `hover`, `staging`, `readyCrew`, `collect`, `deliver`, and `reconciled`; all 27 named cases; every test-side loop and helper lambda; the individual scenario assertions and repeated assertion families; and the final summary and return value.

For the long input-only case, source ranges 492-504, 505-529, 530-550, 551-569, 570-590, 591-617, 618-646, 647-666 separate setup, instrumentation, controller decisions, keyboard pulse conversion, movement accounting, delivery, crew visits, and final assertions. For the combined worm-envelope case, ranges 750-774, 776-795, 797-822 distinguish pursuit, early breach, and exact activation/retreat boundaries. These subdivisions preserve complete coverage without hiding complex test logic inside a single unexplained case title.
