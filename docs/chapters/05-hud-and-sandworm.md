# Chapter 5: The Rescue HUD and the Procedural Sandworm

## Reading This Chapter

This chapter explains all of `arrakis_hud.h` and `arrakis_worm.h`, including their declarations, constants, helper functions, loops, conditions, rendering calls, failure paths, and cleanup. The HUD is the two-dimensional information layer drawn over the game. The sandworm is a three-dimensional object built from mathematical surfaces rather than loaded from a model file. Their purposes are different, but both turn game state into visible geometry.

All source references below use the original root-level files read for this chapter: `arrakis_hud.h`, lines 1-552, and `arrakis_worm.h`, lines 1-360. Their final project paths are `src/arrakis_hud.h` and `src/arrakis_worm.h`; the original references identify the same code after relocation. The explanations are not substitutes for the source appendix: they describe what the code means and why its operations are arranged this way without reproducing the complete source listings.

For a beginner, three distinctions are important. A value in a C++ structure is ordinary CPU data. A VBO is an OpenGL buffer containing data for the GPU. A shader is a small program that runs as part of drawing on the GPU. Generating a triangle in a CPU vector does not itself put a pixel on the screen; the program must eventually upload the vector and issue a draw call. Likewise, moving a mathematical centerline does not automatically update a worm mesh already stored on the GPU; the changed vertices must be sent again.

## The HUD Header and Its Input Contract

### Header Guard, Includes, and State Structure

**Source: `arrakis_hud.h`, lines 1-28.**

Lines 1-2 start an include guard named `ARRAKIS_HUD_H`. The first inclusion defines the name and processes the contents. Later inclusions in the same translation unit skip the protected contents, avoiding duplicate declarations. The matching end directive is at line 552. The guard is a compile-time device; it is unrelated to whether OpenGL resources have been initialized at runtime.

The includes in lines 4-12 supply the facilities used throughout the header. GLAD supplies OpenGL types, constants, and loaded function entry points. GLM supplies vectors. `<algorithm>` provides clamping, minimum, and maximum. `<array>` provides fixed-size arrays for glyph rows and saved capability state. `<cmath>` supplies finiteness tests, rounding, and trigonometry. `<cstddef>` provides `std::size_t` and `offsetof`, which are important for buffer byte sizes and vertex attribute offsets. `<cstdio>` provides bounded formatted output and diagnostic printing. `<string>` provides owned strings and conversions. `<vector>` provides resizable arrays of geometry and markers.

`RescueHudState`, opened at line 14 and closed at line 28, is a snapshot of information to display. It does not run the mission, advance the timer, move the aircraft, or change crew states. The caller fills it from the game and passes it to the HUD. Its member initializers make an unmodified snapshot internally usable, but they are defaults rather than a second source of gameplay rules.

`width` and `height` default to 1280 and 720. These are framebuffer pixels, not necessarily the logical window dimensions reported by an operating system. On a high-density display, a window can have more framebuffer pixels than screen-coordinate units. The HUD uses these values for its viewport and derives a separate layout canvas from them.

Line 16 specifically explains `waiting`: it includes crew in both Running and Waiting states who are outside the harvester, not workers who are still inside. This matters because the UI later labels that count `SAND`; it is not merely the number of stationary people. Line 17 declares seven integers. `rescued` is the count secured by delivery to base and initially zero. `aboard` is the count in the aircraft, initially zero, and is visually compared with eight seats. `waiting` is the outside count just described. `lost` is the number no longer rescueable. `total` defaults to 36, the mission population displayed beside the secured count. `best` stores the record used on the results screen. `wave` starts at one and identifies the mission or wave. All seven are numbers supplied by the caller; the HUD does not reconcile contradictory counts.

Line 18 declares `remaining`, initially 210 seconds; `altitude`, initially zero and displayed in meters; `speed`, initially zero; and `boost`, initially one. Boost is drawn as a normalized meter, so one means a full displayed bar. Line 19 adds `missionDuration`, initially 210, as the denominator for the pre-breach timer bar; `pursuit`, initially zero, as a normalized threat bar; `wormDistance`, initially 220, as the displayed range; and `extractionRemaining`, initially 60, as the post-breach countdown. A changing countdown and its total duration are different quantities: without the denominator, a progress meter could not tell what fraction remains.

Line 20 supplies `inside`, initially 36, and `releasedGroups`, initially zero. The first means crew still in the harvester; the second means groups released so far. Line 21 supplies `pickupProgress` and `unloadProgress`, both initially zero, and `fps`, initially 60. The two progress values drive the action message and shared action bar. FPS is diagnostic frame-rate information. Line 22 supplies a heading in degrees, initially zero, used only to rotate the player marker on the north-up radar.

Line 23 contains four booleans. `title` initially true requests the introduction overlay. `paused` requests the pause overlay. `ended` requests the result overlay and takes precedence over the title flag there. `muted` changes the telemetry text and color; it does not itself mute audio. The default false values for the last three mean that a default state shows the title screen rather than a paused, ended, or muted mission.

`prompt` in line 24 is an initially empty string. It can override the normal action instruction, and one exact prompt is also recognized when choosing the failure text on the result screen. Line 25 initializes five two-dimensional positions to zero: `player`, `harvester`, `base`, `worm`, and `pickup`. The radar interprets each used vector as ground-plane coordinates. `pickup` is declared but is not referenced by any drawing function in this header; describing it as a visible marker would be inaccurate. `crewMarkers`, in line 26, is a variable-length list of ground-plane crew markers. The header draws every supplied entry without deciding which crew qualify. Finally, `attackTime`, initially -1 at line 27, distinguishes approach from an attack that has begun. The HUD uses only whether it is at least zero; it does not implement the worm's detailed attack phases.

### Rendering Preconditions and Internal Namespace

**Source: `arrakis_hud.h`, lines 30-34 and 454.**

The comments in lines 30-33 define the intended environment: a current OpenGL 3.3 core context, loaded GLAD entry points, framebuffer-pixel dimensions, and one owning context until cleanup. Creating or deleting OpenGL objects without the correct current context is outside that contract. A static CPU resource holder does not make GPU object names portable between arbitrary contexts.

The comments also describe the drawing function's state isolation. It saves and restores the program, VAO, array-buffer binding, viewport, polygon mode, write mask, blend setup, and selected enable flags. It does not switch the caller's framebuffer or texture bindings. This means the HUD is drawn into whichever framebuffer the caller has already bound. It is not a routine that automatically finds the screen framebuffer. Nor is the comment a promise to snapshot every possible OpenGL setting: only the state actually saved by `GlState` receives explicit restoration.

Line 34 opens `arrakis_hud_detail`, and line 454 closes it. Keeping implementation helpers in a named internal namespace prevents common names such as `text`, `bar`, and `Color` from colliding with unrelated names in the surrounding program. The three public lifecycle functions after the namespace form the intended entry points.

## HUD Data, Colors, and Formatting

### Color Palette, Vertex Layout, and Shared Resources

**Source: `arrakis_hud.h`, lines 36-52.**

`Color` is four floating-point components: red, green, blue, and alpha. RGB controls the tint; alpha controls how strongly the fragment contributes when blended over the existing scene. All palette components are expressed on a zero-to-one scale. The constants are `inline constexpr`, so they are compile-time values that can be defined in a header without ordinary multiple-definition problems.

`bone` is `(0.91, 0.89, 0.82, 1.0)`, a warm nearly white main text color. `muted` is `(0.59, 0.61, 0.58, 1.0)`, used for secondary text. `cyan` is `(0.36, 0.85, 0.86, 1.0)`, connecting the aircraft, safe base, and rescue progress. `amber` is `(0.98, 0.65, 0.27, 1.0)`, used for crew and warnings. `white` is pure opaque white. `threat` is `(0.98, 0.36, 0.27, 1.0)`, the warmer red-orange worm and pursuit color. `black` is `(0.018, 0.025, 0.028, 0.84)`: it is a very dark panel background that still permits some scene contribution. `edge` is `(0.42, 0.47, 0.46, 0.25)`, a faint translucent border and empty-bar color.

The HUD's `Vertex` at line 45 is not the scene mesh's position-normal-UV vertex. It has exactly six floats: pixel-space `x` and `y`, followed by `r`, `g`, `b`, and `a`. Every triangle vertex carries its own color. The simple HUD shader needs no normals, texture coordinates, light calculation, or font atlas.

`Resources` in lines 46-51 owns `program`, `vao`, and `vbo`, initially zero to mean no object has been created. `canvas`, initially -1, is the uniform location for the two-dimensional layout size; a uniform location is not an OpenGL object name. `capacity`, initially zero, counts allocated GPU buffer bytes, not vertices. `vertices` is the CPU-side batch of all triangles for the current composition. Its vector capacity is distinct from the GPU byte capacity.

`resources()` in line 52 returns a reference to a function-local static `Resources`. The object is created once and shared by these inline helpers. Returning a reference avoids copying its object names and vector. This is convenient for a single-context HUD, but it is not a collection of per-window or per-context resources. The static object's ordinary destruction also does not replace explicit OpenGL cleanup.

### Finite Values, Counts, Rounded Measurements, and Clocks

**Source: `arrakis_hud.h`, lines 53-66.**

`finite(v, fallback)` returns `v` when `std::isfinite` accepts it; otherwise it returns the fallback, zero unless specified. It screens out both infinities and NaN. This prevents malformed measurements from turning into invalid positions or unreadable timer calculations. It does not clamp large finite values by itself.

`unit(v)` first replaces a non-finite value with zero, then clamps the result between zero and one. Bars can therefore receive values below zero or above one without drawing negative widths or filling beyond their background. `number(v)` converts the greater of zero and the integer argument to a string. Negative counts become `0`; positive counts are not otherwise limited. The parenthesized `(std::max)` syntax also prevents an unfortunate function-like `max` macro from matching the name on platforms where such macros are present.

`decimal(v)` creates a 48-byte local character array, clamps a finite-or-zero value to the interval -99999 through 99999, and formats it with `%.0f`. Despite the function name, the displayed result has zero fractional digits. It rounds according to formatted floating-point output rather than truncating with an integer cast. The clamp limits the text width, and the bounded `snprintf` receives the full buffer size. Returning `out` constructs an owned `std::string`, so no pointer to a dead local array escapes.

`clock(v)` uses a different policy. It clamps to 0-5999, rounds upward with `ceil`, and casts the result to an integer number of seconds. Thus a positive fraction of a remaining second continues to display as one rather than immediately becoming zero. Integer division by 60 obtains minutes; remainder modulo 60 obtains seconds. `%02d:%02d` prints each part with at least two digits, producing values from `00:00` through `99:59`. The 16-byte array comfortably holds this representation. A NaN or infinity becomes `00:00`, because the default finite fallback is zero. These helpers sanitize display, not game state: they never modify the original snapshot.

## Preserving OpenGL State

### The Saved-State Structure and Constructor

**Source: `arrakis_hud.h`, lines 68-90.**

`GlState` is an automatic scope guard. Its constructor reads the caller's settings before the HUD changes them; its destructor restores those settings when the drawing function leaves scope. This pattern is often called resource acquisition is initialization, or RAII, even though here the managed resource is a restoration obligation rather than a newly allocated buffer.

The integer fields in line 69 hold the active shader program, bound vertex array, bound array buffer, four viewport numbers, and two polygon-mode values. The viewport entries are origin x, origin y, width, and height. Line 70 stores four blend factors, separately for RGB and alpha, and two blend equations. Line 71 stores the four color-write booleans, one each for red, green, blue, and alpha. Preserving the mask matters because a previous render pass might intentionally have disabled color writes.

The fixed capability array in lines 72-74 names ten switches. Blending combines HUD fragments with the scene. Depth testing could reject overlay pixels based on three-dimensional depth. Face culling could discard triangles because of their winding. Scissoring could clip the HUD to an old rectangle. Stencil testing could mask it according to a previous pass. Rasterizer discard could prevent all fragments. Color logic operations could replace normal blending with bitwise operations. Sample alpha-to-coverage, sample coverage, and sample alpha-to-one could alter multisample behavior. The corresponding ten-element `enabled` array starts value-initialized and then stores each switch's actual incoming state.

Lines 77-88 query each scalar or array into its matching member using `glGetIntegerv` or `glGetBooleanv`. These queries do not intentionally change the state they inspect. The loop at line 89 visits every capability index from zero to one less than `caps.size()` and records `glIsEnabled(caps[i])`. The shared index ties each enum to the boolean that will later restore it; no capability is inferred from another one.

### Destructor Restoration and Non-Copyability

**Source: `arrakis_hud.h`, lines 91-107.**

The destructor rebinds the old program, VAO, and array buffer in lines 92-94. The explicit casts convert the queried integer object names back to OpenGL's unsigned object type. It then restores all four viewport values. OpenGL core contexts use one polygon mode for front and back faces, so line 97 restores `polygon[0]` through `GL_FRONT_AND_BACK`; the comment at line 96 explains why it does not separately restore two independent face modes.

Lines 98-100 restore the separate RGB and alpha factors, separate equations, and per-channel write mask. The loop in lines 101-103 checks each saved boolean. A true value calls `glEnable` for that capability; a false value calls `glDisable`. This is stronger than blindly disabling blend after the HUD: if the caller had blending enabled, blending remains enabled afterward with the original factors.

The deleted copy constructor and assignment operator in lines 105-106 prevent accidentally copying a guard and creating two restoration obligations for the same old state. The closing brace and semicolon complete the type at line 107. This guard does not save buffer contents, uniforms inside programs, framebuffer bindings, textures, or every obscure rendering option. The HUD does not need to change the framebuffer or textures, and its canvas uniform belongs to its own program. Its buffer uploads likewise target its own VBO. Restoring bindings is not the same operation as undoing those intentional resource updates.

## Turning HUD Shapes Into Triangles

### Shader Compilation With Failure Cleanup

**Source: `arrakis_hud.h`, lines 109-124.**

`shader(type, source)` receives the shader stage, such as vertex or fragment, and a pointer to its source text. It asks OpenGL for a shader object. If the returned name is zero, line 111 returns zero immediately; there is no object to compile or delete. With a valid name, `glShaderSource` supplies one string and a null length pointer, meaning the string is null-terminated. Compilation is then requested.

The local `ok` starts as `GL_FALSE`, and `glGetShaderiv` asks for the compile status. If compilation fails, a zero-initialized 2048-byte log buffer receives the compiler message. `fprintf` sends the `Arrakis HUD shader:` diagnostic to standard error, including a newline. The failed object is deleted before returning zero. If compilation succeeds, line 123 returns its object name to the caller, transferring responsibility for later deletion. The function reports failure but does not throw, exit the application, or continue with an unusable shader.

### Triangle, Rectangle, Outline, Card, and Bar Helpers

**Source: `arrakis_hud.h`, lines 126-147.**

`triangle` takes three two-dimensional positions and a color. It obtains the shared CPU vector by reference and appends exactly three vertices, each with the same RGBA values. These pushes preserve order. With the final `GL_TRIANGLES` draw call, each consecutive triple is one triangle. There is no index buffer and no attempt to share corner vertices between HUD shapes.

`rect` treats `(x,y)` as the upper-left corner and `w,h` as extents in the downward-y canvas. Non-positive width or height makes it return without appending anything. Otherwise it constructs the four corners and covers them using two triangles, split along the diagonal from the upper-left to the lower-right corner. The first triangle runs through top-left, top-right, and bottom-right; the second uses top-left, bottom-right, and bottom-left. Six vertices are appended, even though the mathematical rectangle has only four distinct corners.

`outline` builds a one-unit top edge at `y`, bottom edge at `y+h-1`, left edge at `x`, and right edge at `x+w-1`. This uses rectangle geometry rather than OpenGL line primitives, making thickness explicit in canvas units. The corner areas overlap, so this is not a single stroked polygon. `card` draws its dark background first, its faint outline second, and a two-unit accent strip at the left last. The default accent is `edge`; warning or action cards can supply a stronger color.

`bar` first draws a full-width `edge` background, then draws a foreground with width `w*unit(amount)`. Its default height is four units, although callers often supply three or five. The amount is sanitized and bounded; no explicit condition is needed for zero because a zero-width foreground rectangle is ignored by `rect`. The order of appending determines visual layering when blending later runs. In this batching scheme, creating a card, adding its text, and later drawing an overlay naturally puts later triangles over earlier ones without multiple draw calls.

## The Complete Bitmap Font

### Five-Bit Rows and Every Glyph Entry

**Source: `arrakis_hud.h`, lines 149-204.**

The font is a small hand-coded bitmap font, five columns wide and seven rows high. It needs no image file or texture. `glyph(c)` returns seven unsigned bytes, but only the low five bits of each byte are meaningful. Row zero is the top row. Bit 16 is the leftmost column, followed by 8, 4, 2, and 1 toward the right. A set bit means a filled square. The comment at line 149 anticipates the optimization in the text renderer: adjacent set bits in one row will become one wider rectangle.

Here is the complete numeric vocabulary needed to decode every entry. Zero is `00000`; 1 is `00001`; 2 is `00010`; 4 is `00100`; 6 is `00110`; 7 is `00111`; 8 is `01000`; 10 is `01010`; 12 is `01100`; 13 is `01101`; 14 is `01110`; 15 is `01111`; 16 is `10000`; 17 is `10001`; 18 is `10010`; 19 is `10011`; 20 is `10100`; 21 is `10101`; 23 is `10111`; 24 is `11000`; 25 is `11001`; 27 is `11011`; 30 is `11110`; and 31 is `11111`. The row lists below therefore identify the exact shape of every supported character, not just its name.

| Source Line | Character | Rows From Top to Bottom | How the Shape Is Constructed |
| --- | --- | --- | --- |
| 152 | A | 14, 17, 17, 31, 17, 17, 17 | A centered three-pixel top, two vertical sides, and a full-width middle crossbar. |
| 153 | B | 30, 17, 17, 30, 17, 17, 30 | A left stem with three nearly full horizontal bars and right edges between them. |
| 154 | C | 14, 17, 16, 16, 16, 17, 14 | Rounded top and bottom; the three middle rows retain only the left edge. |
| 155 | D | 30, 17, 17, 17, 17, 17, 30 | A straight left stem and a right edge, with the outer upper and lower right corners omitted. |
| 156 | E | 31, 16, 16, 30, 16, 16, 31 | Full top and bottom bars, a shorter middle bar, and a left stem. |
| 157 | F | 31, 16, 16, 30, 16, 16, 16 | The E pattern without its bottom bar. |
| 158 | G | 14, 17, 16, 23, 17, 17, 15 | A C-like outline with right-side interior pixels and a fuller lower-right closure. |
| 159 | H | 17, 17, 17, 31, 17, 17, 17 | Two vertical stems joined by a complete middle bar. |
| 160 | I | 31, 4, 4, 4, 4, 4, 31 | Full-width top and bottom bars with a centered vertical stem. |
| 161 | J | 7, 2, 2, 2, 18, 18, 12 | A top-right cap, a near-right stem, and a hook returning toward the lower left. |
| 162 | K | 17, 18, 20, 24, 20, 18, 17 | The left stem stays on while the second stroke approaches it and moves away diagonally. |
| 163 | L | 16, 16, 16, 16, 16, 16, 31 | A left stem ending in a complete bottom bar. |
| 164 | M | 17, 27, 21, 21, 17, 17, 17 | Two sides with a pair of inward upper diagonals and centered upper pixels. |
| 165 | N | 17, 25, 21, 19, 17, 17, 17 | Two sides with an upper-left to lower-right internal diagonal in the early rows. |
| 166 | O | 14, 17, 17, 17, 17, 17, 14 | A closed outline with trimmed corners. |
| 167 | P | 30, 17, 17, 30, 16, 16, 16 | A closed upper bowl over a left-only lower stem. |
| 168 | Q | 14, 17, 17, 17, 21, 18, 13 | An O-like bowl altered at its bottom to introduce a diagonal tail. |
| 169 | R | 30, 17, 17, 30, 20, 18, 17 | The P upper bowl followed by a diagonal leg to the right. |
| 170 | S | 15, 16, 16, 14, 1, 1, 30 | An upper right-reaching bar, left upper stroke, middle bar, right lower stroke, and lower left-reaching bar. |
| 171 | T | 31, 4, 4, 4, 4, 4, 4 | A full-width top and centered stem. |
| 172 | U | 17, 17, 17, 17, 17, 17, 14 | Two sides ending in a centered three-pixel base. |
| 173 | V | 17, 17, 17, 17, 17, 10, 4 | Two sides narrow to paired inner pixels and then a centered point. |
| 174 | W | 17, 17, 17, 21, 21, 21, 10 | Two outer strokes with a lower central stroke and a paired inner base. |
| 175 | X | 17, 17, 10, 4, 10, 17, 17 | Two diagonals meet at the center and diverge again. |
| 176 | Y | 17, 17, 10, 4, 4, 4, 4 | Diagonals merge into a centered lower stem. |
| 177 | Z | 31, 1, 2, 4, 8, 16, 31 | Full top and bottom joined by a descending right-to-left diagonal. |
| 178 | 0 | 14, 17, 19, 21, 25, 17, 14 | The closed O outline plus an internal diagonal distinguishing the digit. |
| 179 | 1 | 4, 12, 4, 4, 4, 4, 14 | A central stem, a small upper-left flag, and a three-pixel base. |
| 180 | 2 | 14, 17, 1, 2, 4, 8, 31 | A curved top, a diagonal down toward the left, and a full bottom bar. |
| 181 | 3 | 30, 1, 1, 14, 1, 1, 30 | Three horizontal regions joined by right-side strokes. |
| 182 | 4 | 2, 6, 10, 18, 31, 2, 2 | A diagonal into a full crossbar with a near-right stem continuing below. |
| 183 | 5 | 31, 16, 16, 30, 1, 1, 30 | A full top, left upper stroke, and rounded lower-right bowl. |
| 184 | 6 | 14, 16, 16, 30, 17, 17, 14 | A left-connected upper curve and a closed lower bowl. |
| 185 | 7 | 31, 1, 2, 4, 8, 8, 8 | A full top and diagonal into a near-left lower stem. |
| 186 | 8 | 14, 17, 17, 14, 17, 17, 14 | Two closed bowls meeting in a centered middle bar. |
| 187 | 9 | 14, 17, 17, 15, 1, 1, 14 | A closed upper bowl with a right lower stroke and curved base. |
| 188 | / | 1, 1, 2, 4, 8, 16, 16 | A diagonal from the upper right to the lower left. |
| 189 | + | 0, 4, 4, 31, 4, 4, 0 | A central vertical segment crossing a full-width middle row. |
| 190 | - | 0, 0, 0, 31, 0, 0, 0 | A single full-width middle row. |
| 191 | : | 0, 4, 4, 0, 4, 4, 0 | Two two-pixel-tall centered dots. |
| 192 | . | 0, 0, 0, 0, 0, 6, 6 | A two-by-two lower dot. |
| 193 | , | 0, 0, 0, 0, 0, 4, 8 | A small lower dot with a leftward descending tail. |
| 194 | % | 25, 25, 2, 4, 8, 19, 19 | Paired corner marks separated by a diagonal. |
| 195 | ! | 4, 4, 4, 4, 4, 0, 4 | A centered stem, gap, and terminal dot. |
| 196 | ? | 14, 17, 1, 2, 4, 0, 4 | A curved hook, descending center, gap, and dot. |
| 197 | ( | 2, 4, 8, 8, 8, 4, 2 | A parenthesis bowing left in its middle. |
| 198 | ) | 8, 4, 2, 2, 2, 4, 8 | The horizontally mirrored parenthesis. |
| 199 | = | 0, 0, 31, 0, 31, 0, 0 | Two complete horizontal bars with a blank row between. |
| 200 | _ | 0, 0, 0, 0, 0, 0, 31 | A complete bottom row. |
| 201 | space | 0, 0, 0, 0, 0, 0, 0 | No visible pixels, but the text renderer still advances the cursor. |
| 202 | any unsupported character | 14, 17, 1, 2, 4, 0, 4 | The exact question-mark shape, making unsupported input visibly identifiable. |

The `switch` begins at line 151, returns immediately from each case, and closes at line 203 before the function closes at 204. There is no fall-through and no need for `break` after a returned array. There are 26 uppercase letters, ten digits, 14 supported non-letter entries including space, and one default branch. Lowercase letters are handled elsewhere by conversion to uppercase. Apostrophes and square brackets have no case. Consequently the failure sentence's apostrophe in `WORM'S` and the telemetry string `[MUTED]` do not render literally: each unsupported punctuation character appears as `?`. That is actual behavior, not an extra font feature.

### Text Width and Rectangle-Based Rendering

**Source: `arrakis_hud.h`, lines 205-222.**

`textWidth` returns zero for an empty string. For a nonempty string of n bytes, it returns `(6*n-1)*size`. Each character has five columns and a one-column gap; the final character does not need a trailing gap. The explicit floating-point conversion avoids performing the full width expression in an unsigned string-size type. Width counts all characters, including spaces and characters whose painted pixels do not touch both outer columns.

`text` walks the string as unsigned bytes. That avoids negative character values when testing ASCII ranges. A byte between `a` and `z` is shifted by subtracting `a` and adding `A`; other bytes retain their value before the cast to `char`. The resulting character selects the seven glyph rows.

The outer row loop runs from zero through six. The inner column loop deliberately has no increment expression in its header because the body advances by either one unlit position or a whole lit run. `16 >> column` creates the mask for the current left-to-right column. If bitwise AND with the row is zero, the negated test is true; the code increments the column and continues without drawing. If the bit is set, `first=column++` remembers the beginning and advances past the first lit pixel. The `while` loop then advances through consecutive set bits, stopping before column five or before an unlit bit.

One rectangle is emitted at `x+first*size`, `y+row*size`. Its width is the number of pixels in the run multiplied by `size`; its height is one pixel multiplied by `size`. A five-pixel horizontal font stroke therefore requires six triangle vertices, not thirty. After all seven rows, the character origin advances by `6*size`, preserving the one-column inter-character gap. The renderer is proportional only in scale, not in character width: I, M, and space all advance equally. It also has no newline interpretation; multiline behavior comes from wrapping and the separate `lines` helper.

### Word Wrapping, Alignment, and Line Limits

**Source: `arrakis_hud.h`, lines 223-255.**

`wrap` estimates the maximum number of characters that fit with the same width formula used by the font. Solving `(6*n-1)*size <= width` gives `n <= (width+size)/(6*size)`. Line 224 floors that value and takes at least one before converting to `std::size_t`. Actual callers supply positive font sizes; the function is not a general validation routine for zero or negative size.

The function creates an output vector, a pending line, and a pending word. The lambda `append`, capturing these by reference, is a local operation for transferring a completed word into the output. An empty word returns immediately. If a nonempty existing line plus one separating space plus the word would exceed the column count, the existing line is emitted and cleared. A word longer than the available column count is split repeatedly: each leading substring of exactly `columns` characters is emitted, then erased from the pending word. Any remainder is added to the line, with one space first only if the line already contains text. The word is cleared so it cannot be appended twice.

The byte loop treats any byte at or below ASCII space as whitespace. It first flushes the pending word. For a newline specifically, it then emits the current line, even if empty, and clears it. Ordinary spaces, tabs, and other low control bytes do not explicitly force new lines; repeated whitespace is collapsed when words are joined. A non-whitespace byte below 127 is kept, while a byte at least 127 becomes `?`. This is byte-oriented ASCII handling, not UTF-8 decoding, so a multibyte character can become multiple question marks. Unsupported printable ASCII such as an apostrophe survives wrapping but later reaches the glyph fallback.

After the loop, `append()` handles the final word, and a nonempty final line is added. The vector is returned by value. Very long exact-multiple words can leave no remainder, and an explicit newline can add an empty line after such processing; this follows from the separate splitting and newline branches. Normal HUD strings contain manageable words and use explicit newlines mainly to separate introductory instructions.

`centered` subtracts `textWidth` from the supplied region width, takes half of the difference, and offsets the text origin by that amount. It does not clip or shrink text if the difference is negative. This helper is defined but not called elsewhere in this header. `lines` takes the smaller of the content length and a maximum count, whose default is 100, then draws each selected line at `y+i*size*10`. A glyph is seven pixels high, so the ten-pixel step leaves three pixels of vertical separation at the selected scale. Limiting count drops later lines rather than rewriting their contents.

## The North-Up Radar

### Grid, World Mapping, and Marker Shapes

**Source: `arrakis_hud.h`, lines 257-294.**

`radar` first emits a square card. A loop with i equal to one, two, and three draws vertical and horizontal lines at one quarter, one half, and three quarters of `side`. Each grid line stays one unit inside the outer border and spans `side-2`. The result is a four-by-four reference grid, not an automatically scaled terrain map.

The local `point` lambda receives a position, color, and integer marker type. For each position component, it replaces non-finite input with zero, clamps to -300 through +300, adds 300, divides by 600, and scales to `side-12`. Adding `x+6` or `y+6` places the useful map inside a six-unit inset. The negative world limit lands at the inner left or top boundary; the positive limit lands at the inner right or bottom boundary. Out-of-range objects stick to the boundary instead of disappearing. The comment in line 265 emphasizes that this map stays north-up even when the heading-locked chase camera turns.

Type zero is the player triangle. Heading is sanitized and multiplied by 0.01745329252, the degrees-to-radians conversion factor. `forward=(-sin(yaw),-cos(yaw))` points up the radar at zero degrees; `right=(-forward.y,forward.x)` is its perpendicular. The triangle's tip is four units forward from the marker center, while the tail center is three units backward. The two tail corners lie four units to either side of that tail center. Passing these three points to `triangle` turns heading into an unmistakable directional arrow. The negative sine and cosine belong to this screen-map convention; they should not be replaced by a guessed three-dimensional forward vector.

Type one is an eight-by-eight outline centered on the position, used for the base. Type two is a filled six-by-six square, used for the harvester. Type four is a filled three-by-three square, used for crew. Every other type uses two triangles meeting along a vertical diagonal to produce a diamond with tips four units above, below, left, and right. The worm is explicitly passed type three and therefore gets that diamond branch.

Drawing order is base, all crew entries, harvester, worm, and player. Later markers can cover earlier ones if they coincide. Crew iteration uses const references to avoid copying each GLM vector. `pickup` has no corresponding call. Labels are optional: the `if(labels)` block emits `SECTOR / +/-300` in muted color 18 units above the map; `YOU / BASE` in cyan nine units below; and `CREW`, `HARV`, and `WORM` 26 units below in amber, white, and threat color. The last two begin 47 and 92 units to the right. These are legends, not labels individually positioned beside each object.

## Title, Pause, and Results Screens

### Overlay Choice and Responsive Measurements

**Source: `arrakis_hud.h`, lines 296-322.**

`overlay` begins by adding a full-canvas dark rectangle with `(0.01,0.018,0.022,0.78)`. It darkens the already composed HUD and underlying scene. `result=s.ended` takes priority; the local `title` is true only when the mission is not ended and the input title flag is true. Otherwise the overlay is the pause design. This local precedence also resolves snapshots with more than one flag set.

The horizontal margin is 12 below width 480, otherwise 28. Panel width is the smaller of 680 and the available width after both margins. Interior padding is 14 when the panel is narrower than 420 or the canvas shorter than 420, otherwise 28. Content width subtracts that padding twice. Text pixel scale becomes one below height 300; otherwise it becomes 1.5 for a panel below width 380 or a height below 420, and two for roomier screens. Heading scale is two below height 300, 2.5 for a panel below width 420 or height below 420, and four otherwise. These choices reduce spacing and type size selectively instead of uniformly squeezing a desktop design.

The heading is exactly `MISSION CLOSED`, `HARVESTER FLIGHT`, or `FLIGHT PAUSED` according to the selected mode. For a result, the code compares the prompt with the exact string `ORNITHOPTER LOST IN THE WORM'S MAW`. A match chooses `ORNITHOPTER LOST IN THE WORM'S MAW. DELIVERED CREW REMAIN SAFE.`; otherwise it chooses `RESCUE COMPLETE. ONLY CREW DELIVERED TO BASE COUNT. EVERY LIFE COUNTS.`. This is a display-message test, not collision detection. The apostrophe survives the string comparison correctly even though its font glyph is unsupported.

For the title mode, four explicit instruction lines state `FLEEING HARVESTER RELEASES 1-4 CREW AT A TIME.`, `AMBER GROUPS WAIT ON SAND. 8 RESCUE SEATS.`, `MOUSE STEERS. FLY LOW. HOLD SPACE TO WINCH.`, and `RETURN TO CYAN BASE. HOLD SPACE TO BANK RESCUED.`. Adjacent C++ string literals concatenate into one string containing the specified newline separators. Pause mode uses `THE RESCUE IS ON HOLD. YOUR CREW IS COUNTING ON YOU.`. All three possibilities pass through `wrap`, so long lines still break at the current content width.

The key instruction is independently wrapped: `ENTER NEXT MISSION / R RETRY`, `ENTER START`, or `P RESUME`. `lineH=size*10` matches the `lines` helper. Results reserve `lineH*3+12` for their three statistical rows and a gap; other modes reserve zero. Panel height adds twice the inset, 23 for the branding step, seven heading pixels at heading scale, 18 after the heading, the result reserve, all wrapped body and key lines at `lineH`, and 25 for the final separator spacing. Horizontal position centers the panel. Vertical position centers it unless that would be less than eight, in which case the top is eight. The card accent is amber for results and cyan otherwise.

These calculations are responsive but not an absolute overflow guarantee. If a framebuffer is extremely tiny or unusually short, the computed panel can extend below it. There is no final panel-height clamp, scroll area, or scissor-based text clipping in this function. It prioritizes readable sizes and lets framebuffer bounds provide the ultimate clipping.

### Overlay Content and Cursor Advancement

**Source: `arrakis_hud.h`, lines 323-341.**

The mutable `cursor` begins at the panel's top inset. The header `ARRAKIS / SEARCH + RESCUE` is drawn in muted color at scale 1.5, after which the cursor advances 23. The selected heading is then drawn in bone color at `headingSize`, and the cursor advances its seven-row glyph height plus 18.

For results only, the first line reads `SECURED <rescued> / <total>` in cyan. The second reads `BEST <max(best,rescued)> / MISSION <wave>` in bone. Taking the maximum means a just-achieved score is visible as the best even if the caller has not yet synchronized its stored record. The third reads `ABOARD <aboard> / LOST <lost>` in muted color. Each count passes through the nonnegative `number` helper. Cursor increments match `lineH` between rows, then add 12 after the final row.

The wrapped body is drawn in muted color, and the cursor advances by its entire line count times `lineH` plus 12. A one-unit horizontal separator spans `contentW`. Another advance of 13 puts the key instructions below it; these are cyan. This cursor scheme accounts for all the components used in the panel-height expression. The function closes after adding geometry; no rendering or input handling happens inside it.

## Composing the In-Game HUD

### Global Layout Decisions and the Countdown

**Source: `arrakis_hud.h`, lines 343-355.**

`compose(w,h,s)` is the central geometry builder. Its `w,h` are layout-canvas dimensions, not necessarily raw framebuffer dimensions. Margin `m` is ten when width is below 600 or height below 400, otherwise 22. `inner=w-2*m` is the content width between margins. `compact` is true below width 760 or height 440. `shortView` is true below height 360. These are independent tests: a wide but short screen still enters compact mode.

`extraction` checks `attackTime>=0`. It selects `extractionRemaining` instead of ordinary `remaining`, and a fixed duration of 60 instead of `missionDuration`. Before extraction, the duration is sanitized with a fallback of 210 and constrained to at least 0.001, preventing division by zero in the time bar. A non-finite remaining value is treated as zero in the danger test. Danger is true when remaining time is at most 15 or when extraction is active. Thus the entire extraction phase uses warning styling, not only its last fifteen seconds.

The label precedence is `EXTRACTION WINDOW` for extraction, then `BREACH IMMINENT` for low pre-breach time, otherwise `TIME TO BREACH`. Danger uses amber; ordinary time uses bone. The top card height is 78 for compact short view, 112 for other compact views, and 106 for noncompact views. The initial card spans the full inner width. Label choice, clock formatting, and bar clamping are separate: even inconsistent caller data does not make the displayed fraction overfill the bar.

### Full-Width Desktop Header

**Source: `arrakis_hud.h`, lines 356-375.**

The noncompact branch arranges the top card as branding, crew status, and a timer. Branding begins 18 units from the left and 16 from the top with `ARRAKIS` at scale three. `FLEEING HARVESTER` appears at top offset 47, scale 1.5, muted. `SEARCH + RESCUE / <wave>` appears at offset 76, scale 1.5, cyan. The positions are fixed relative to the panel, not inferred from text height.

`scoreX=m+inner*0.30` starts the central status cluster at thirty percent of the inner width. `SECURED <rescued>/<total>` is cyan at scale 2.5 and y offset 15. `ABOARD <aboard>/8` appears at offset 44; `INSIDE <inside>` shares that row 126 units to the right in muted color. A loop draws eight seat slots at x spacing 14, each ten units wide and five high, at y offset 64. Slot i is cyan when `i<s.aboard`, otherwise edge. The comparison uses the original count; a negative count fills none and a count above eight fills all, but the code never creates more than eight slots.

`GROUPS <releasedGroups>` is amber, positioned 126 units to the right of `scoreX` on the seat row. `SAND <waiting> / LOST <lost>` is muted at y offset 82. `SAND` follows the documented Running-plus-Waiting outside count rather than the `inside` count.

`timerX=m+inner-230` anchors the timer cluster near the right. The label is at y offset 15, the clock at 39 and scale three, and a 210-by-five time bar at 74. Its amount is sanitized remaining time divided by the selected duration. At y offset 87, `SECURE CREW AT CYAN BASE` reminds the player that boarding alone is not scoring. The branch ends at line 375 before the alternate compact branch.

### Compact and Very Short Headers

**Source: `arrakis_hud.h`, lines 375-392.**

The compact branch uses a combined `ARRAKIS / FLEEING HARVESTER` title at x offset 12 and y offset ten. Its scale is 1.5 when the inner width is below 330, otherwise two. `row` is y offset 26 for short views, 32 otherwise. The timer label sits on that row; the clock is two units higher and right-aligned using its measured width and a twelve-unit inset.

The timer bar is only 74 units wide and three high, ending at that same right inset. Its y offset is 56 for short views but 48 for other compact views. This intentional rearrangement puts it below the seat row when space is especially short. `statsY` is offset 43 for short views and 54 otherwise. At that position, `SAVED <rescued>/<total>` is cyan on the left and `SEATS <aboard>/8` is right-aligned on the right. The change from `SECURED` to `SAVED` shortens the label; it does not change which numeric field is displayed.

The next left-aligned line combines `SAND <waiting> INSIDE <inside> GROUPS <releasedGroups>`, at offset 61 in short view or 76 otherwise. If the view is not short, an extra line at offset 96 shows `LOST <lost> / WAVE <wave>`. Only in that non-short branch, and only if inner width is at least 360, does the code draw a compact eight-seat diagram. Each slot is eight by four, separated by twelve units, beginning 104 units from the right and sitting at offset 99. These omissions reduce clutter without changing any mission values.

### Wrapped Controls and the Action Card

**Source: `arrakis_hud.h`, lines 394-422.**

The comment in lines 394-395 explains the design principle: controls wrap at readable pixel sizes instead of shrinking an entire 1280-pixel design. The control font scale is always 1.5. The complete instruction string is `MOUSE STEER / W S THRUST / A D STRAFE / Q E ALT / SPACE WINCH / SHIFT BOOST / C CAMERA / M MUTE / P PAUSE / R RETRY`. It identifies mouse steering, forward/back thrust, lateral strafe, altitude controls, winching, boosting, camera choice, mute, pause, and retry. The HUD merely displays those mappings; actual key handling belongs to the game.

Wrapping uses `inner-20`, allowing ten units on both sides. Controls-card height is fifteen units per wrapped line plus 14. `controlsY=h-m-controlsH` anchors its bottom above the bottom margin. The card spans the inner width, and text starts ten units from its left and eight from its top. A narrow screen makes more lines and a taller bottom card, rather than smaller letters.

The action message initially copies `s.prompt`. An empty prompt chooses one of three defaults in descending priority: with at least eight aboard, `SEATS FULL / RETURN TO CYAN BASE`; with a positive smaller aboard count, `RETURN TO BASE / HOLD SPACE TO BANK CREW`; otherwise `FLY LOW TO CREW / HOLD SPACE TO WINCH`. Active unloading overrides either a prompt or a default with `BANKING CREW / HOLD SPACE`. Only when unloading is not positive does positive pickup progress override with `WINCH ACTIVE / HOLD SPACE`. The `if` followed by `else if` gives unloading explicit precedence if both progress values are positive.

`sideRadar` requires all three conditions: short view, inner width at least 280, and enough room that a 64-unit radar beginning at `controlsY-72` is at least six units below the top card. If true, action width reserves 76 units for that radar; otherwise it uses the full inner width. In either case width is capped at 620. Action text scale is 1.5 for compact mode and two for desktop mode. The message wraps within `actionW-28`, reserving fourteen-unit side insets.

If wrapping produces more than two lines, the vector is resized to two. When the retained last line contains at least three characters, its final three characters are replaced by `...`. This communicates truncation without adding width. A shorter retained line receives no ellipsis. Action height is its retained line count times the line step plus 23. Its bottom sits eight units above the controls card. With a side radar, it is right-aligned to the margin; otherwise it is centered across the whole canvas.

The card gets a cyan accent, and its text begins at x inset 14 and y inset ten. The progress selection uses unloading if it is positive, otherwise pickup. The bar is three units high, fourteen units inset from both sides, and nine units above the card's bottom. Its `unit` sanitization handles out-of-range values. The message-selection conditions themselves do not call `finite`; for example, a NaN progress fails the positive comparison, while a positive infinity selects the active message but produces a zero displayed bar through `unit`. This is the precise split between display formatting and raw conditional state.

### Radar Placement, Telemetry, and Room-Dependent Omission

**Source: `arrakis_hud.h`, lines 424-442.**

The normal radar begins below the top card by ten units in compact mode or 32 on desktop. `available` measures space from that radar top to the action card, subtracting six units in compact mode or 52 on desktop. The larger desktop reserve accommodates legends and breathing room. Requested side length is capped at 92 for compact mode and 156 otherwise, but cannot exceed the available vertical space.

If `sideRadar` is true, the special radar is drawn at the left margin, 72 units above the controls top, with side 64 and no labels. Otherwise, a normal radar is drawn only when the computed side is at least 40, at the right margin. Labels are enabled only for a noncompact radar at least 128 units wide. The `else if` means the two radar placements never both execute.

Telemetry appears only when there is no side radar, at least 88 units of vertical room, and either the view is noncompact or inner width is at least 420. Its width is 184 in compact mode and 222 otherwise. `pursuitRoom` checks available height at least 112 and chooses a 112-unit card instead of an 88-unit card. This gate prevents a pursuit row from being added when only the basic telemetry fits.

The first row is `ALT <rounded altitude> M` at y offset twelve. The second is `SPD <rounded speed> / FPS <rounded fps>`, at offset 31. When muted, it appends ` [MUTED]` and uses amber rather than muted color. The square brackets fall back to question-mark glyphs, as described in the font section. `BOOST` appears at offset 52; its five-high cyan bar begins x offset 72 and y offset 55 and spans `tw-86`, leaving a right inset of fourteen.

`WORM RANGE <rounded distance> M` appears at offset 72 in amber. Its distance is made finite and clamped to a minimum of zero before passing through `decimal`, unlike altitude and speed, which may legitimately format negative values. If pursuit room exists, `PURSUIT` appears at offset 92 in amber and a five-high threat-colored bar begins x offset 86 and y offset 95, with width `tw-100`. Both normalized bars use the generic bar helper and therefore bound their foreground to the background.

### Flight Guides and Overlay Order

**Source: `arrakis_hud.h`, lines 443-452.**

Guides are emitted only when the mission is not ended, not on the title screen, and not paused. A local copy of cyan has alpha reduced to 0.35. Nested range loops choose dx and dy from -1 and +1, producing all four combinations. Guide centers lie at 44 or 56 percent of canvas width and height: `0.5+sign*0.06`.

At each position, one seven-by-one rectangle and one one-by-seven rectangle create an L-shaped corner. Positive dx subtracts seven from the horizontal bar's starting x, so it extends inward toward the screen center; negative dx does not. Positive dy similarly subtracts seven from the vertical bar's y. Thus the four marks outline a quiet central aiming or flight region rather than drawing a full crosshair over the scene.

Finally, if any of ended, title, or paused is true, `overlay` is called. Because it is called after all other composition, its dimmer and panel triangles are appended last and cover the normal interface. The underlying HUD is still composed rather than omitted; the overlay darkens it. Closing line 452 ends `compose`, and line 454 ends the internal namespace.

## HUD Initialization, Drawing, and Cleanup

### Shader Sources and Program Linking

**Source: `arrakis_hud.h`, lines 456-491.**

`initRescueHud` introduces a short namespace alias `hud`, retrieves the shared resources, and immediately returns if the program already exists. This makes normal repeated initialization calls cheap. It also means the program field is the initialization sentinel; the function assumes a nonzero existing program belongs to a valid resource set in the current owning context.

The vertex shader consists of adjacent C++ string literals with explicit newline escapes and requests GLSL `#version 330 core`. Attribute location zero is a two-component position; location one is a four-component color. `canvas` is a two-component uniform and `tint` is a four-component output. The main function assigns the input color to `tint` and converts pixel-style layout coordinates to clip coordinates.

For x, `position.x/canvas.x*2-1` maps zero to -1 and the right edge to +1. For y, `1-position.y/canvas.y*2` maps the top to +1 and the bottom to -1, reversing the down-growing canvas convention into OpenGL's up-growing clip convention. The resulting z is zero and w is one. Because no perspective division changes this w, the HUD is a flat overlay. The fragment shader receives interpolated `tint` and writes it directly to its output `fragment`; it does not sample a texture, apply lighting, or discard pixels.

The helper compiles both stages. If either is zero, the `if` in lines 473-477 deletes whichever stage did succeed and returns. Compiling both before checking means a fragment stage can exist even when the vertex stage failed, and this branch handles that case. Program creation has its own zero check; failure deletes both compiled shader objects and returns.

With a program, the code attaches both stages, links, and deletes the stage objects. OpenGL retains what the linked program needs; the application no longer needs the temporary shader handles. The link-status query starts from false. Failure obtains a bounded 2048-byte program log, prints it with the `Arrakis HUD link:` prefix, deletes the unsuccessful program, and returns. No failed program is installed in `Resources`.

### Vertex Array Setup and Binding Preservation

**Source: `arrakis_hud.h`, lines 492-510.**

Before configuring vertex storage, the function queries the old VAO and array-buffer bindings. It generates one HUD VAO and one VBO. If either name is zero, it deletes any object successfully generated, resets both fields to zero, deletes the otherwise linked program, and returns. Since this failure occurs before the new objects are bound, there are no new binding changes to undo in that branch.

After successful object creation, `r.program` receives the linked program and `r.canvas` receives the location of its `canvas` uniform. The VAO and VBO are bound. Both attributes are enabled. Location zero reads two `GL_FLOAT` values starting at byte offset zero, with no normalization and stride `sizeof(hud::Vertex)`. Location one reads four floats starting at `offsetof(hud::Vertex,r)`, the red field, with the same stride. The cast to a pointer type supplies a buffer byte offset to OpenGL; it is not dereferencing an arbitrary CPU pointer.

The old VAO and array-buffer bindings are restored explicitly. Initialization has not called `glUseProgram`, so it does not need to restore an active program. Finally, the CPU vector reserves room for 65536 vertices. `reserve` changes capacity without adding elements; the first composed frame still begins from an empty vector. It is a starting allocation, not a maximum geometry limit. No GPU vertex storage is allocated here; the drawing function manages that dynamically.

### Frame Composition, Render State, Buffer Orphaning, and Draw Call

**Source: `arrakis_hud.h`, lines 512-540.**

`drawRescueHud` first returns for nonpositive framebuffer width or height. This avoids division by zero and is appropriate for a minimized or otherwise undrawable framebuffer. It calls initialization, retrieves resources, and returns if initialization left no program. Only then does it construct `GlState saved`, making restoration automatic for the subsequent draw operations.

The scale is the greater of one and the smaller of width/1280 and height/720. This chooses the limiting dimension for proportional enlargement but forbids scaling below one. A 640-by-360 framebuffer therefore uses a 640-by-360 layout canvas rather than a shrunken 1280-by-720 one. A 2560-by-1440 framebuffer uses scale two and a 1280-by-720 canvas; one canvas unit then covers two framebuffer pixels. An unusually wide screen with only 720 pixels of height stays scale one because height is limiting. `w=width/scale` and `h=height/scale` define the dimensions passed to layout and to the vertex shader.

The vertex vector is cleared without normally releasing its capacity, then `compose` appends the entire frame. All ten capabilities named by the saved guard are disabled. Blend is then reenabled, with both equations set to addition. RGB factors are source alpha and one minus source alpha, giving the familiar translucent overlay. Alpha factors are one and one minus source alpha, so output alpha accumulates as source alpha plus destination alpha times its remaining fraction. This separate alpha setup is not identical to using source alpha for both RGB and alpha.

All color channels are made writable, polygon mode is forced to filled triangles, and the viewport is the entire framebuffer starting at zero. Disabling depth test prevents the HUD from being hidden by scene depth. Disabling the other named capabilities prevents leftover scissor, stencil, wireframe, discard, and similar settings from corrupting the overlay. The function does not clear color or depth buffers. The caller's framebuffer remains bound, and its existing image becomes the background for blending.

The HUD program is selected and its canvas uniform receives layout width and height. Its VAO and VBO are bound. `bytes` is the vector length multiplied by the actual vertex structure size. If bytes exceed GPU capacity, capacity becomes the greater of the required byte size and twice the old capacity. First use therefore grows from zero to at least the required amount; later growth tends to be geometric, reducing repeated reallocations. If the frame uses fewer bytes, capacity is retained.

`glBufferData` is called every frame with that retained capacity, a null source pointer, and `GL_DYNAMIC_DRAW`. The null pointer requests new storage without copying old data. This is called orphaning: the driver can let a previous draw finish reading the old storage while providing new storage for the current frame. It is a performance technique, not a guarantee that every driver will avoid every stall. `glBufferSubData` then copies exactly `bytes` from the CPU vector to offset zero of the new storage.

`glDrawArrays` uses `GL_TRIANGLES`, starts at vertex zero, and consumes the whole vector after conversion to `GLsizei`. All card, glyph, radar, and overlay triangles are rendered in this one call. When the function closes at line 540, `saved` is destroyed and restores the previously described state. CPU vertices remain in the shared vector until next frame's clear or final cleanup; there is no per-shape OpenGL call hidden in the geometry helpers.

### Explicit Cleanup and End of Header

**Source: `arrakis_hud.h`, lines 542-552.**

`cleanupRescueHud` retrieves resources and conditionally deletes the VBO, VAO, and shader program, in that order. Each zero check avoids treating an uncreated resource as a live one. The chained assignment resets all three object fields to zero. The uniform location returns to -1 and the GPU byte capacity to zero. Swapping the CPU vector with a freshly constructed empty vector releases its allocation, unlike `clear`, which normally retains capacity.

After cleanup, a future valid call can initialize a fresh HUD because the program sentinel is zero. Cleanup should run while the owning OpenGL context is current and before that context is destroyed. It does not use a state-restoration guard; deleting an object that is currently bound has OpenGL-defined effects on bindings, so this is a lifecycle operation rather than a promised transparent render operation. Line 550 closes cleanup, and line 552 closes the include guard begun at lines 1-2.

## The Sandworm's Procedural Geometry

The remaining sections explain `arrakis_worm.h` completely. Unlike the HUD, the worm uses the scene's existing mesh upload and material drawing helpers. It has a changing body mesh, separately constructed mouth surfaces, many individual curved teeth combined into one mesh, and an animation pose shared between scene render passes.

### Header Guard, Dependencies, Constants, and Cache Fields

**Source: `arrakis_worm.h`, lines 1-20 and 258.**

Lines 1-2 begin the `ARRAKIS_WORM_H` include guard, closed at line 360. The comments in lines 4-5 describe integration: include the header after the rock-formation drawing definitions and before `drawFullScene`, initialize and clean up while the GL context is current, and draw in both scene passes. This header is not independently self-contained. It includes `<cmath>` and `<vector>` in lines 6-7, but expects GLM, OpenGL declarations, scene vertex and mesh types, materials, terrain height queries, upload routines, drawing helpers, and the shared sphere mesh to have been declared already by the including translation unit.

In the surrounding renderer, `Vertex` contains `pos`, `normal`, and `uv`: a three-dimensional position, a three-dimensional lighting normal, and two texture coordinates. `Mesh` contains VAO, VBO, EBO, index count, and draw mode. The VAO describes vertex attributes; the VBO contains vertex data; the EBO contains the unsigned indices selecting vertices for triangles. `uploadMesh` creates these GPU objects and returns a `Mesh`. `drawMeshPrimitive` supplies the current pass's uniforms and draws the indexed mesh. A `Material` has diffuse and specular colors, shininess, emissive color, and an `isSand` flag. The worm uses these existing types instead of defining replacements.

Line 9 opens `arrakis_worm_detail`, and line 258 closes it. `pi` at line 11 is the single-precision constant 3.14159265358979323846, used in all angular construction. `bodyRows=192` means 192 longitudinal intervals, not 192 total vertex rings. `bodySlices=96` means 96 angular intervals around each ring, not 96 stored vertices per ring. Both axes need an extra endpoint, so the full body grid has 193 rings of 97 vertices each.

Line 14 declares five static `Mesh` objects. `body` is the flexible skin; `lip` is the thick exterior rim; `funnel` is the narrowing visible mouth interior; `throat` is the deep dark continuation and recessed closure; `teeth` is the aggregate tooth geometry. `bodyVertices` at line 15 is the CPU copy retained for deformation and uploading. Other geometry is built once and does not require a persistent CPU vector here.

`ready`, initially false, indicates whether initialization has been performed. It is not an OpenGL error-status query. `poseValid`, also false, says whether the remembered input tuple corresponds to the currently uploaded body pose. `lastCenter` starts at the zero vector. `lastAttack` and `lastTime` start at zero; `lastHeading` and `lastProgress` also start at zero. Those values are meaningful as a cache only after `poseValid` becomes true. A deliberately separate validity flag avoids guessing that a zero-valued input tuple must mean an upload has already happened.

The `static` namespace variables have internal linkage, so separate translation units including this header would have separate data instances. The actual project includes this implementation into its scene translation unit rather than treating it as a general multi-context library. No cache member contains a shader, framebuffer, or separate shadow pose.

### Smooth Transitions and Mesh Destruction

**Source: `arrakis_worm.h`, lines 22-32.**

`smooth(a,b,t)` converts a quantity into a smooth progress value across the interval from a to b. It first computes `(t-a)/(b-a)` and clamps it to zero through one. It then returns `u*u*(3-2*u)`, the cubic smoothstep polynomial. The polynomial is zero at u zero, one at u one, and has zero first derivative at both ends. This makes staged rises, lunges, retreats, and tapers start and stop gently rather than introducing abrupt velocity changes.

The helper assumes distinct interval endpoints. It has no branch for `a==b`, so such a call would divide by zero. Every interval used in this header has distinct endpoints. It also does not itself screen out non-finite t; the public drawing function checks its animation inputs before calling the pose logic.

`destroy(mesh)` conditionally deletes the EBO, VBO, and VAO, in that order. Each test checks the object name, allowing cleanup of a default or partially populated mesh without issuing an intentional delete for an absent resource. `mesh=Mesh{}` then resets not only the three handles but also its index count and default draw mode according to the scene's `Mesh` member initializers. This is a complete logical reset of the record, not merely the deletion of GPU storage. As with all OpenGL deletion, the correct current context is required.

## Shared Surface Construction

### Rectangular Grid Indices and Triangle Winding

**Source: `arrakis_worm.h`, lines 34-43.**

`gridIndices(indices,rows,slices)` builds triangle connectivity for a rectangular grid whose stored ring width is `slices+1`. It reserves `rows*slices*6` index slots. There are `rows*slices` cells, each split into two triangles, each triangle containing three indices. `reserve` preallocates capacity without inserting values. This routine appends rather than clearing its argument; its callers pass newly created empty vectors.

The j loop visits row intervals zero through `rows-1`. The nested i loop visits angular intervals zero through `slices-1`. `a=j*(slices+1)+i` locates the current cell's first corner. `b=a+1` is the next vertex around the same ring. `c=a+slices+1` is the corresponding vertex in the next ring. `d=c+1` is the next-ring next-angle corner. The inserted sequence `a,c,d,a,d,b` creates two triangles sharing the a-to-d diagonal.

This consistent winding controls which side of each surface is considered the front and the sign of cross-product normals. The formula uses every adjacent pair of stored angles, including the final pair ending at the duplicated seam vertex. It does not wrap the index modulo `slices`, because the extra vertex explicitly represents the angular endpoint. The same connectivity supports both a straight revolved profile and the much more complicated bent body, provided their vertices use this ring-major layout.

For the body, 192 by 96 produces 18432 cells, 36864 triangles, and 110592 indices. These connectivity numbers never change during animation. Deformation changes positions and normals, not the number or order of triangles. The closing braces in lines 41-43 finish the inner loop, outer loop, and function respectively.

### Triangle-Derived Surface Normals and the Duplicated Seam

**Source: `arrakis_worm.h`, lines 45-61.**

`surfaceNormals` begins by setting every vertex normal to zero. It then visits the index vector in steps of three, matching the triangle groups created by `gridIndices`. References a, b, and c select the actual vertex records for those indices. The vector `cross(b.pos-a.pos,c.pos-a.pos)` is perpendicular to the triangle and has length proportional to twice its area. Its direction depends on the index winding. The same unnormalized vector is added to the normal accumulator at all three corners.

Because the face vector is not normalized before accumulation, larger triangles contribute more strongly than smaller ones. This is area-weighted averaging. When the accumulators are eventually normalized, neighboring faces produce smooth vertex lighting rather than isolated flat face normals. It is useful for the gently roughened mouth surfaces.

The UV seam has two stored vertices at the same nominal angular position: the first angle is zero and the last angle is one full turn. Their separate UV values permit a continuous parameter interval from zero to one, but separate accumulation would otherwise give each side only its own adjoining faces. The j loop at line 56 advances by `slices+1`, jumping to the beginning of each ring. It adds the first and final vertex's accumulated normals and assigns that sum to both. This welds their shading, not their indices or positions.

The comment in line 55 specifies why this should not be used indiscriminately on tooth surfaces: teeth are separate objects, and averaging unrelated surfaces together would blur their lighting. In fact, the tooth builder supplies its own normals and does not call this function. A final range loop normalizes every accumulated vector to unit length. There is no zero-length fallback here; callers are expected to supply usable nondegenerate surfaces. The later throat floor is added after this computation specifically to give it independent upward normals.

### Revolving a Profile Into a Lathe Surface

**Source: `arrakis_worm.h`, lines 63-84.**

The comments in lines 63-64 explain the geometric ordering: the profile follows the exterior upward toward the lip, then proceeds down the inner funnel. With the same triangle winding, an increasing-height exterior has normals pointing away from the axis, while a descending interior has normals pointing into the mouth cavity. The normal direction comes from traversal direction and the cross product, not from a separate branch that labels a surface as inside or outside.

`lathe(profile,roughness,deepFloor)` receives radius-height points in `glm::vec2` records. The x component is radius and the y component is vertical position in the local mouth coordinate system. Revolving those points around the y axis creates a surface of revolution, analogous to shaping clay on a spinning wheel. `deepFloor` defaults to false. Each lathe uses 160 angular slices, a higher angular resolution than the body's 96 slices.

The function allocates local vertex and index vectors and reserves `profile.size()*161` vertices. It assumes a profile with at least two points; both the interpolation denominator `profile.size()-1` and the longitudinal cell count depend on that. The supplied lip, funnel, and throat profiles satisfy the assumption. The outer loop visits every profile point; the inner loop visits angles i zero through 160 inclusive, creating the duplicate angular seam.

`u=float(i)/slices` is normalized angular position. `a=2*pi*u` maps it to one complete revolution. The roughness formula multiplies three things: the supplied roughness amplitude, a longitudinal sine envelope, and a weighted angular wave sum. The envelope `sin(pi*j/(profile.size()-1))` is nominally zero at each end of the profile and positive in the middle, so neighboring surfaces retain their intended radius at attachment rings while interior rings gain irregularity. Floating-point trigonometry can leave tiny endpoint residuals rather than mathematically exact zero.

The angular wave sum is `0.55*sin(11*a+0.7*j) + 0.30*sin(23*a-0.9*j) + 0.15*sin(47*a+1.1*j)`. Eleven, 23, and 47 are distinct integer frequencies around a full turn. Their weights total one, and their j-dependent phases prevent identical bumps in every profile row. The signs and phase rates vary the directions in which the patterns shift. For amplitude 0.16, the magnitude is bounded by approximately 0.16 before the envelope; for 0.08, by approximately 0.08. This is deterministic procedural roughness, not random noise that changes every frame.

`r=profile[j].x+rough` perturbs radius only. Each vertex position is `(r*cos(a), profile[j].y, r*sin(a))`; its initial normal is zero; its UV is `(u,j/(profile.size()-1))`. The UV's first coordinate tracks the revolution and the second tracks profile row fraction, not physical arclength. After building all rings, `gridIndices` connects `profile.size()-1` longitudinal intervals, and `surfaceNormals` computes normals including the actual roughened shape.

### The Recessed Floor, Its Extra Vertices, and Upload

**Source: `arrakis_worm.h`, lines 85-97.**

The optional `deepFloor` block closes only the deepest throat, not the large mouth opening. It records the current vertex count as the center index and appends a center at x and z zero and the final profile point's y. That center has normal `(0,1,0)` and UV `(0.5,0.5)`.

The angular loop again includes zero through 160. Each iteration copies a vertex from the final lathe ring, overrides its normal to point upward, and appends the copy. Copying rather than reusing the existing ring preserves a hard normal transition between the narrowing throat wall and its floor. The copied UV remains the wall-ring UV; this is not a fully remapped radial disk texture layout. Here the material is mainly dark color, so geometric closure is its main purpose.

For i below 160, the code inserts a triangle from the center to the next copied ring vertex and then the current copied ring vertex. The indices use `center+i+2` and `center+i+1` because the center itself occupies the first newly appended slot. At the time some indices are inserted, the next ring copy will be appended in a later iteration; this is valid because all referenced vertices exist before final upload. The final i equals 160 adds the duplicate endpoint but no extra triangle. This makes exactly 160 floor triangles with upward winding.

The function returns `uploadMesh(v,indices)`. That creates GPU buffers while the temporary vectors still exist; they can be destroyed after their data has been uploaded. There is no stored CPU copy of these lathe vertices in the worm namespace. Both the false-floor path and true-floor path converge on the same upload return.

## The Thousand Curved Teeth

### Row Counts, Radii, Depths, and Deterministic Variation

**Source: `arrakis_worm.h`, lines 99-120.**

`makeTeeth` constructs all teeth in one mesh. There are six concentric rows, five cross-section sides per tooth, and six sampled cross-section rings along each tooth. The row counts are 192, 184, 176, 168, 152, and 128, totaling exactly 1000 teeth. The attachment radii are 13.1, 11.3, 9.4, 7.7, 6.0, and 4.5. The corresponding depths are -0.8, -2.6, -4.5, -6.3, -8.2, and -10.1. These pairs match explicit points in the funnel profile, so each row starts on an appropriate narrowing section of the inner mouth.

The vertex reserve is `1000*(steps*sides+1)`: 30 ring vertices and one apex per tooth, giving 31000 vertices. The index reserve is `1000*((steps-1)*sides*6+sides*3)`: five bridges of five quadrilateral sides, each split into two triangles, plus five final triangles at the apex. This is 165 indices per tooth and 165000 indices overall. Reserving the exact expected quantities avoids repeated vector growth during construction.

The outer loop selects a row and the inner loop visits each tooth number below that row's count. `seed=float(tooth*17+row*131)` produces a deterministic number from its identifiers. `variation=0.5+0.5*sin(seed*1.713)` maps a sine into zero through one. There is no random-number engine, random seed file, or per-frame regeneration: the same row and tooth produce the same result each time meshes are initialized.

The tooth angle is `2*pi*(tooth+0.37*row+0.18*sin(seed))/counts[row]`. The tooth number spaces teeth around the row. The 0.37-row term offsets consecutive rows so their teeth do not form perfectly aligned radial columns. The final sine term supplies a small angular jitter measured in fractions of nominal tooth spacing. `radial=(cos(a),0,sin(a))` points out from the axis toward the attachment. `sideways=(-sin(a),0,cos(a))` is a perpendicular horizontal direction that allows a tooth to curl sideways independently of inward reach.

`reach=(radii[row]-1.8)*(0.55+0.17*variation)` sets inward extent. The multiplier ranges from 0.55 to 0.72; subtracting 1.8 preserves a central region instead of forcing every tip to the axis. The possible reach ranges by row are 6.215-8.136, 5.225-6.840, 4.180-5.472, 3.245-4.248, 2.310-3.024, and 1.485-1.944. As the mouth narrows, the deeper teeth become shorter in horizontal reach.

`drop=2.1+1.1*variation` ranges from 2.1 to 3.2 downward units. `curl=0.20+0.30*sin(seed*0.91)` ranges from -0.10 to +0.50, so some teeth bend slightly the opposite way but most have a positive-side tendency. `width=(0.075+0.035*variation)*(1-0.06*row)` sets the starting cross-section radius. Its base range is 0.075-0.110, multiplied by 1.00, 0.94, 0.88, 0.82, 0.76, and 0.70 as rows deepen. The local variable `base` remembers where this tooth's vertices begin in the shared vector, ensuring later indices refer to this tooth rather than a previous one.

### Curved Centerline and a Moving Cross-Section Frame

**Source: `arrakis_worm.h`, lines 121-137.**

The k loop samples six rings with `t=k/steps`. Since k runs zero through five and the denominator is six, the sampled values are 0, 1/6, 2/6, 3/6, 4/6, and 5/6. The endpoint at one is deliberately not a small ring; it will be a single pointed apex later.

The centerline position p combines three independent directions. Its radial component is `radii[row]-reach*t`, which moves inward linearly. Its sideways component is `curl*t*t`, which starts with zero lateral slope and increasingly bends to one side. Its vertical component is `depths[row]+0.65*sin(pi*t)-drop*t*t`. The sine briefly lifts the tooth above a purely downward parabola, while the quadratic term bends it down toward the final tip. At t zero the center is exactly at its attachment radius and depth. At t one the sine is zero, the inward displacement is reach, sideways displacement is curl, and vertical displacement is negative drop.

The tangent formula is the derivative of this centerline with respect to t, normalized to unit length. The radial derivative is `-reach*radial`; the sideways derivative is `2*curl*t*sideways`; and the vertical derivative is `0.65*pi*cos(pi*t)-2*drop*t`. This makes the local cross-section track the changing tooth direction rather than remaining parallel to a fixed plane.

The vector b is the normalized cross product of tangent with sideways. It is perpendicular to the tangent. The vector n is `cross(b,tangent)`, completing a perpendicular pair for the cross-section. Here n is a local frame direction, not yet a particular vertex normal. Because the inward radial derivative is nonzero for all supplied rows, the tangent does not collapse into the sideways direction and this construction has a usable frame for the generated teeth.

Cross-section radius is `width*pow(1-t,0.85)`. At the attachment it is width, and it decreases toward zero. The exponent 0.85 controls how the narrowing develops along the shaft; it is not a linear cone. The five-side loop chooses angles `2*pi*s/sides`, then creates a normal direction `n*cos(angle)+b*sin(angle)`. Multiplying that direction by the radius and adding p gives the surface vertex. The same direction is stored as its lighting normal. UV is `(s/sides,t)`.

Only five angular vertices are stored per ring, so there is no sixth duplicated seam vertex. Connectivity later wraps the fifth side back to the first with modulo. The supplied normals are moving-frame radial normals, an efficient approximation for a tapered curved tooth; they do not include a derivative correction for every taper slope in the way the flexible body's finite-difference normals do. The UV seam is also a wrapped five-vertex polygon rather than the explicitly duplicated zero/one seam of the lathe.

### Shaft Triangles and One True Apex

**Source: `arrakis_worm.h`, lines 138-160.**

The next nested loops connect adjacent sampled rings. k ranges zero through four, covering five bridges; s ranges zero through four, covering the five pentagonal sides. `a0=base+k*sides+s` is the current corner, while `b0=base+k*sides+(s+1)%sides` is the next corner around the ring. Modulo returns to side zero after side four. Adding `sides` selects corresponding corners in the next ring. The six inserted indices form two triangles covering the side quadrilateral.

The comment at line 145 emphasizes the apex design. `tip` is the next vertex index. `tipPos` evaluates the same centerline at t one: radial radius becomes `radii[row]-reach`, sideways displacement becomes curl, and depth becomes `depths[row]-drop`. `tipNormal` uses the normalized endpoint tangent. Its vertical term is `-0.65*pi-2*drop`, because cosine at pi is -1. The tip UV is `(0.5,1)`.

The final side loop takes each adjacent pair from the sixth sampled ring and joins them to the single apex. It contributes five triangles. There is no flat tiny cap at the tip and no set of five coincident endpoint vertices. There is also no cap at the attachment base; that open end starts against the funnel surface. Each tooth therefore has 31 vertices, 50 shaft triangles, and five tip triangles. Every tooth's `base` and `tip` keep its connectivity independent of the other 999 teeth.

The nested row and tooth loops close at lines 157-158. `uploadMesh` receives the aggregate vectors once and returns the teeth mesh at line 159; line 160 closes the function. This is one GPU mesh with 55000 triangles, not a thousand separate draw calls. The teeth do not deform individually after construction: the common head matrix moves, tilts, and radially scales all of them together.

## Worm Pose and Attack Phases

### Pose Fields and Terrain-Aware Head Position

**Source: `arrakis_worm.h`, lines 162-189.**

`Pose` contains `head`, a three-dimensional translation for the local mouth origin; `tilt`, an angle in radians used to tip the mouth and bend the body; and `aperture`, a radial scale for the mouth. There is no yaw field because heading is applied separately by a root matrix. The structure is an animation result, not persistent game state.

`pose(center,attack,time,headingDegrees,approachProgress)` accepts a worm center, an attack-phase time, the general animation time, and optional heading and approach progress defaulting to zero. The first three smooth transitions are rise over attack seconds 0-6, lunge over 6-12, and retreat over 13.5-18. These overlap only at endpoints and create distinct stages. Negative attack time gives zero for all three and represents approach.

`ground=getDuneHeight(center.x,center.z)` samples terrain beneath the center. The center's y value is not used as the head's terrain reference. `approachHeight=10+4*smooth(0,1,approachProgress)` raises the approaching head from ten to fourteen units above local terrain as progress advances. For negative attack time, this is the chosen height. For nonnegative attack time, the alternative is `14+22*rise-27*lunge`: the head begins fourteen above ground, rises to 36, and lunges down to nine.

The next expression adds a 0.25-amplitude sine bob using `time*1.6`, multiplied by rise and by `1-lunge`. Bobbing fades in as the rise develops and fades out during the lunge. It does not oscillate the approach height because rise is zero before attack. `glm::mix(height,-24,retreat)` then pulls the head below terrain during retreat. The final target is 24 units below the sampled ground, not absolute world y -24.

Forward reach is `12*lunge*(1-retreat)`. It grows to twelve during the lunge and disappears during retreat. Head is initially constructed at `(center.x,ground+height,center.z+reach)` in the unyawed deformation coordinate system. The later root matrix rotates that local +z reach toward the requested heading. This separation lets body and mouth geometry share a simple local bending plane.

Heading is converted from degrees to radians. A terrain target is sampled eighteen units from the center in the direction `(sin(heading),cos(heading))` on the xz plane, and one unit is added to that height. `aim=atan2(18-reach,targetY-head.y)` is intentionally written with horizontal remaining distance as the first argument and vertical difference as the second. An ordinary yaw calculation would use a different interpretation; here the angle is measured away from the upward y direction into local +z. A target below the head therefore can produce a tilt greater than ninety degrees, tipping the mouth into a downward bite.

Tilt is `radians(70)*(1-rise)+aim*lunge*(1-retreat)`. During approach it is seventy degrees. The rise removes that initial tilt, leaving an upright pose at attack six. The lunge adds the terrain-aware aim. Retreat fades the aim out. `swallow=smooth(12,13.5,attack)*(1-retreat)` narrows the mouth after the lunge and releases that narrowing during retreat. Aperture is `1-0.18*swallow`, so the minimum intended radial scale is 0.82. Returning `{head,tilt,aperture}` uses aggregate initialization of the three fields.

At attack zero, height is fourteen and tilt seventy degrees. At six, height is 36 plus possible bob, reach is zero, and tilt is zero. At twelve, height is nine, reach is twelve, and tilt is the full aim, with swallow just starting. At 13.5, radial aperture is 0.82 and retreat has just begun. At eighteen, the formulas reach a submerged, untilted, reopened pose, but the public draw function rejects attack times at least eighteen, so that exact endpoint is not drawn. These are animation values rather than a separate collision model.

### The Head Matrix and Order of Operations

**Source: `arrakis_worm.h`, lines 191-195.**

`headMatrix` begins with an identity matrix translated to `p.head`, then applies an x-axis rotation by tilt, then an xz radial scale `(aperture,1,aperture)`. With the renderer's usual column-vector convention, the resulting product is translation times rotation times scale. A local mouth vertex is radially scaled first, rotated second, and placed at the head third.

Scaling x and z but not y narrows the mouth and teeth arrangement without directly shortening its throat depths. An x-axis positive rotation transforms local up `(0,1,0)` into `(0,cos(tilt),sin(tilt))`, exactly the axis used by body deformation. This agreement is what lets the body end meet the tilted lip. The function returns a matrix and does not bind a shader, change OpenGL state, or modify mesh vertices.

## Deforming the Flexible Body

### Neck Axis, Circular-Arc Centerline, and Buried Bezier Segment

**Source: `arrakis_worm.h`, lines 197-213.**

`deformBody(center,p)` changes the retained CPU vertex vector. It does not upload data by itself. It starts with the mouth's unit axis `(0,cos(tilt),sin(tilt))`. Neck length is 64 and `join` is 0.4. The body meets the rim at `end=p.head-1.4*axis`, placing its final centerline ring at the local mouth y of -1.4. That matches the first lip-profile point's height.

The `neckPoint(s)` lambda describes a constant-curvature neck rather than joining the head to the ground with a sharp corner. `half=0.5*tilt*(1-s)` is half the remaining angular sweep, and `mid=0.5*tilt*(1+s)` is the corresponding midpoint angle. `sinc` is `sin(half)/half` when half exceeds 0.0001 and one otherwise. The one branch avoids division by a nearly zero value and matches the limiting value of the expression for the normal nonnegative tilt range used here.

The comparison is specifically `half>0.0001`, not an absolute-value test. A negative tilt passed to this helper would take the one branch rather than evaluate its nonzero sinc, so it should not be described as a universally robust signed-arc implementation. The current approach and bite poses produce the nonnegative bending behavior the helper is designed for. This is a concrete implementation condition, not a source correction.

`span=64*(1-s)*sinc` is the straight chord distance associated with the remaining arc length. Subtracting `(0,span*cos(mid),span*sin(mid))` from end gives the point on the circular arc. At s one, span is zero and the point is end. At s zero, it is the neck's lower endpoint. As tilt tends to zero, sinc tends to one and this becomes a straight upright 64-unit segment. The comment about a bend radius over twenty meters expresses the intent to keep a large smooth bend even during a downward bite; the geometry is not a thin hinged cylinder.

The lower body begins at `p0=(center.x,terrainHeight-140,center.z)`, deeply underground. `p3=neckPoint(0)` is the connection to the visible curved neck. `buriedLength=(p3.y-p0.y)/3` defines vertical control offsets. `p1=p0+(0,buriedLength,0)` and `p2=p3-(0,buriedLength,0)` give a cubic Bezier segment vertical tangents at its ends while bridging any z displacement between the buried base and the curved neck's lower point. Only the upper part needs to be visually convincing; the deep base keeps the object from looking like a floating short tube when terrain or pose changes.

### Ring Centers, Tangents, Radius, Ridges, and Surface Grain

**Source: `arrakis_worm.h`, lines 214-241.**

The j loop visits all 193 body rings. `t=j/bodyRows` is the normalized longitudinal grid parameter. The code declares a center c and tangent for that ring and chooses one of two centerline formulas.

When t is below the join value 0.4, `s=t/join` stretches the buried part onto a zero-to-one Bezier parameter, and `u=1-s`. Center c is the cubic combination `u^3*p0 + 3*u^2*s*p1 + 3*u*s^2*p2 + s^3*p3`. Those coefficients form the familiar cubic Bezier curve. The tangent uses its derivative with the common factor of three removed: `u^2*(p1-p0)+2*u*s*(p2-p1)+s^2*(p3-p2)`, then normalizes it. Removing that common positive factor does not change direction.

When t is at least 0.4, `s=(t-join)/(1-join)` maps the rest of the body onto the neck arc. Center c comes from `neckPoint(s)`. Tangent is `(0,cos(tilt*s),sin(tilt*s))`, turning continuously from upward at the lower neck to the mouth's full tilt at the top. At the join, both constructions meet p3 and have an upward tangent, giving positional and directional continuity. Parameter speeds and sampling densities need not be identical; this is not a proof of equal arclength sampling for the whole body.

`across=(0,-tangent.z,tangent.y)` gives a perpendicular direction in the yz plane. The other cross-section direction is the x axis. Because the centerline remains in a fixed-x plane and tangent is normalized, these form a usable radial frame without constructing a full arbitrary three-dimensional transport frame.

`taper=1-smooth(0.85,1,t)` stays one through most of the body and falls to zero over the final fifteen percent of its parameter range. `ridge=0.70*pow(0.5+0.5*cos(2*pi*32*t),3)` creates 32 repeated ring ridges along t. The shifted cosine is in zero through one, cubing sharpens its peaks, and the coefficient limits ridge height to 0.70. The base body radius is consequently between 13.9 and 14.6 before the mouth blend and grain.

`radius=mix(13.9+ridge,15*aperture,1-taper)` transitions from ridged skin to the mouth's 15-unit outer attachment radius, scaled by aperture. Far from the head, the mix weight is zero. At the final ring it is one, giving exactly the intended mouth-join radius in the mathematical formula. During swallowing, that final radius becomes 12.3 because 15 times 0.82 is 12.3. The skin therefore follows the narrowing mouth rather than remaining wide behind a shrunken lip.

For every ring, the i loop visits 97 angular positions including the duplicate seam. `a=2*pi*i/bodySlices` is the angle. `radial=(cos(a),0,0)+sin(a)*across` combines the x direction and the transverse yz direction to generate a circle perpendicular to the current tangent. `grain=taper*(0.11*sin(13*a+9*t)+0.07*sin(29*a-17*t))` adds smaller irregularities with two angular frequencies and longitudinal phase changes. Its possible magnitude is at most about 0.18 when taper is one, and it fades away at the lip so the surfaces meet cleanly.

The vector index `j*(bodySlices+1)+i` accesses the existing body vertex rather than appending a new one. Position becomes `c+(radius+grain)*radial`. UV becomes `(i/bodySlices,t*32)`, giving one wrap around the circumference and 32 repetitions along the body. The repeated longitudinal coordinate works with the renderer's procedural appearance; it is not a statement that the header loads a worm texture. Normals are deliberately left for a second pass because they depend on neighboring updated positions. Lines 240-241 finish the angular and longitudinal loops.

### Finite-Difference Normals and Seam Copying

**Source: `arrakis_worm.h`, lines 242-256.**

The comment at line 242 explains the motivation for another normal method: a radial direction alone cannot account for changing ridges, taper, and bend. The next pair of loops recomputes normals from positional differences after every vertex position has been updated. The outer loop includes all body rings. The inner loop covers only the 96 unique angles, leaving the duplicated endpoint for an explicit copy afterward.

`prev=(i+bodySlices-1)%bodySlices` and `next=(i+1)%bodySlices` choose wrapped angular neighbors. At angle zero, the previous neighbor is 95 rather than an invalid -1; at 95, the next neighbor is zero. This uses the unique-angle set and avoids differentiating between the duplicated endpoint and its same-position counterpart.

The lower row neighbor is zero at the first row, otherwise j-1. The upper row neighbor is j at the final row, otherwise j+1. Interior rings therefore use differences across both sides, while end rings use one-sided differences. `ds` is the positional difference between higher and lower rows at the same angle; `da` is the positional difference between next and previous angles in the same row. Their cross product, `cross(ds,da)`, is normalized and assigned as the lighting normal.

The order matters. For a straight upward tube at its positive-x point, ds points up and da points roughly positive z, so ds crossed with da points outward positive x. Reversing the order would reverse the surface normal. Since these differences come from the full deformed positions, their directions include local grain, ring-ridge slope, radius change, and centerline bending. Dividing by the small parameter steps is unnecessary for the final direction because positive scalar factors vanish under normalization.

After each row's unique normals are finished, the last stored seam vertex receives the first vertex's normal exactly. This guarantees matching seam shading even if tiny trigonometric roundoff makes the nominally equal endpoint positions slightly different. This method is distinct from the lathe's accumulated triangle normals. There is no zero-vector check here either; it relies on the configured mesh and deformation producing valid neighbor directions. The final brace closes `deformBody` at line 256, and the internal namespace closes at 258.

## Creating and Releasing All Worm Meshes

### Cleanup and What It Resets

**Source: `arrakis_worm.h`, lines 260-265.**

`cleanupSandwormMeshes` brings the detail names into scope and calls `destroy` for body, lip, funnel, throat, and teeth. This releases fifteen possible GPU objects, three per mesh, and resets every mesh record. It then clears `bodyVertices` and sets both `ready` and `poseValid` false with a chained assignment.

Clearing the vector removes its active elements but normally retains its allocated capacity. Unlike the HUD's empty-vector swap, this cleanup does not explicitly shrink the retained CPU storage. The remembered center and animation floats are also not reset, but their cache is invalidated by `poseValid=false`, so stale values cannot suppress the next required upload. The routine can be called again on default mesh records because `destroy` tests each handle. It should run before destroying the context, and it makes no promise to restore all current draw bindings.

### Body Allocation, Initial Pose, and Dynamic Storage

**Source: `arrakis_worm.h`, lines 267-279.**

`initSandwormMeshes` returns immediately when ready is true, avoiding duplicate creation. It resizes the CPU body vector to `(192+1)*(96+1)=18721` elements. `resize`, unlike `reserve`, creates the elements that the deformation routine indexes. It then deforms this initial body at center zero using attack six and animation time zero. In that pose, rise is one, lunge and retreat are zero, tilt is zero, and the mouth is fully open. Terrain still contributes to the initial head's y; this is not necessarily a body centered vertically at world zero.

An empty local index vector receives the fixed grid connectivity, and `uploadMesh` creates the body VAO, VBO, and EBO. The scene upload helper initially supplies static buffer storage for ordinary meshes. The following block specifically replaces the body VBO's storage with `GL_DYNAMIC_DRAW`, using the full current CPU vector as data, because the body will change over time.

The code queries the current `GL_ARRAY_BUFFER_BINDING`, binds the body VBO, allocates dynamic storage using `bodyVertices.size()*sizeof(Vertex)`, and restores the queried binding. This is a narrow preservation block around the reallocation. It does not make the preceding `uploadMesh` state-transparent: that shared helper has its own binding behavior, including ending with VAO zero and leaving its array buffer bound. A reader should not assume this initialization has the HUD's broad `GlState` contract. Byte size is determined by `sizeof(Vertex)` so it remains correct if compiler alignment changes the record layout.

### Exact Lip, Funnel, and Throat Profiles

**Source: `arrakis_worm.h`, lines 281-292.**

The lip has eight radius-height points and roughness 0.16. In profile order these are `(15.0,-1.4)`, `(15.6,-0.9)`, `(15.9,-0.2)`, `(15.5,0.5)`, `(14.8,0.8)`, `(14.0,0.5)`, `(13.4,-0.1)`, and `(13.1,-0.8)`. The first attaches to the final body ring. Radius increases to a nominal maximum 15.9 around the outside, height peaks at 0.8, and the profile curls inward and down to the funnel's first point. This makes a thick rounded lip rather than a paper-thin circular edge. Interior roughness can shift radii slightly from these nominal values.

The funnel has fourteen points and roughness 0.08. Its exact ordered profile is `(13.1,-0.8)`, `(12.8,-1.3)`, `(12.0,-2.0)`, `(11.3,-2.6)`, `(10.5,-3.5)`, `(9.4,-4.5)`, `(8.6,-5.4)`, `(7.7,-6.3)`, `(6.8,-7.3)`, `(6.0,-8.2)`, `(5.2,-9.2)`, `(4.5,-10.1)`, `(3.6,-11.4)`, and `(2.9,-13.0)`. Both radius and height decrease, creating a deep narrowing cavity with inward-facing wall normals. The six tooth rows use the funnel points at radii 13.1, 11.3, 9.4, 7.7, 6.0, and 4.5 exactly. The lip-to-funnel and funnel-to-throat endpoints therefore agree by design.

The throat has five points, zero roughness, and the true-floor option enabled. Its profile is `(2.9,-13.0)`, `(2.6,-15.0)`, `(2.2,-19.0)`, `(1.7,-24.0)`, and `(0.8,-27.0)`. It continues down another fourteen local units from the funnel's end and closes only at the narrow 0.8-radius bottom. There is no disc spanning the 13.1-radius mouth entrance. A dark material and genuine depth together make the opening look hollow rather than merely painted black.

At 160 angular slices, the lip's eight rings contain 1288 vertices and 2240 triangles. The funnel's fourteen rings contain 2254 vertices and 4160 triangles. The throat wall's five rings contain 805 vertices and 1280 triangles; adding a center and 161 copied ring vertices yields 967 vertices and 1440 triangles including the floor. These are exact topology counts for the current constants.

`teeth=makeTeeth()` completes the five meshes. `ready=true` marks initialization complete, while `poseValid=false` ensures the first real drawing input will trigger an upload even if it happens to match some remembered defaults. The function does not test individual upload results or recover from allocation failure; ready means the setup code finished, not that every GL operation was independently verified successful. The closed function at line 292 is the end of resource construction.

Together, the five meshes contain 54230 stored vertices and 99704 triangles: 36864 body, 2240 lip, 4160 funnel, 1440 throat, and 55000 tooth triangles. Duplicate seams and the independent floor rim count as stored vertices even when some positions coincide. The sand wake uses the renderer's existing sphere mesh and is not included in these totals.

## Drawing, Cache Reuse, and Materials

### Input Validation and Exact Pose-Cache Comparison

**Source: `arrakis_worm.h`, lines 294-316.**

`drawSandworm` accepts the center, attack time, animation time, optional heading degrees, and optional approach progress. It brings the detail namespace into local scope. The initial compound condition returns when resources are not ready; attack time or animation time is non-finite; any x, y, or z center component is non-finite; heading or approach progress is non-finite; or attack time is at least eighteen. Short-circuit evaluation stops at the first true disqualifier. Negative attack times are deliberately allowed because they display approach and its wake.

Although center y does not determine the terrain-based head height, the function still validates it and includes it in the cache. That makes the entire public position tuple consistently checked. It rejects non-finite progress before clamping it to zero through one. Pose calculation therefore sees a bounded approach progress. The pose is computed on every accepted draw, even when the body upload is cached, because the head matrix and wake still need current animation results.

The cache condition is true if the stored pose is invalid or any center component, attack time, animation time, heading, or clamped approach progress differs exactly from its remembered value. There is no epsilon threshold. A very small finite input change can cause a deformation and upload, while two passes using identical floats share the uploaded skin. Because time is included even when its bob contribution is currently zero, the cache may recompute geometry for some time changes that do not materially alter the body. It favors straightforward correctness over a phase-specific minimal key.

When the condition is true, `deformBody` updates all CPU positions and normals. The block saves the array-buffer binding, binds the body's VBO, and uses `glBufferSubData` to overwrite the full fixed-size vertex storage starting at byte zero. It does not orphan the buffer here, unlike the HUD's per-frame `glBufferData` strategy. The index buffer remains unchanged. The previous array-buffer binding is restored, then all input values are remembered and `poseValid` becomes true.

The comment at line 303 explains the render-pass motivation. A shadow pass and a normal scene pass should display the same pose. Only the flexible skin requires streaming; lip, funnel, throat, and teeth keep static vertex data and animate through model matrices. A second pass still makes the draw calls, but skips repeated CPU deformation and GPU data transfer when the input tuple matches.

The cache does not include a terrain version or terrain sample results. If `getDuneHeight` changed while all input floats stayed identical, the body cache could remain valid despite a changed height field, whereas the freshly computed head pose would reflect the query. In the current use, these terrain queries form a stable underlying height function; the tuple cache is not a general dependency-tracking system. Heading is included because it affects the terrain-aware target height, even though the final yaw is also performed by a matrix.

### Five Material Records and Their Inheritance

**Source: `arrakis_worm.h`, lines 317-332.**

`skin` begins with the scene's default `Material`, then receives diffuse `(0.43,0.27,0.12)`, specular `(0.055,0.04,0.025)`, and shininess eight. These values describe a subdued brown exterior with weak, broad highlights. Default emissive color remains zero and default `isSand` remains zero. Brown coloration alone does not activate the renderer's terrain-sand shader branch.

`rim` copies skin, retaining its specular color, shininess, emissive setting, and non-sand flag, but raises diffuse to `(0.49,0.31,0.14)`. This makes the lip somewhat lighter without a wholly different reflection model. `inside` also copies skin, then uses darker diffuse `(0.15,0.075,0.029)` and equal specular channels 0.018. The one-argument GLM vector constructor fills all three channels, so this is `(0.018,0.018,0.018)`, not only a red value.

`dark` copies inside and changes diffuse to `(0.009,0.004,0.002)` with specular zero. It still inherits shininess eight, but zero specular makes that exponent irrelevant to a specular contribution. This nearly black throat material creates depth while keeping the actual geometry. `enamel` copies skin, uses diffuse `(0.58,0.43,0.24)`, equal specular channels 0.07, and shininess eighteen. The teeth are brighter and have more concentrated highlights than skin but are not pure white.

These material records are reconstructed on each accepted draw; they are cheap local descriptions rather than persistent GPU material objects. Their colors influence the normal scene pass through the shared renderer. In the shadow pass the relevant geometry and transforms still matter, while the pass primarily writes depth and uses the renderer's shadow uniforms rather than displaying these RGB colors directly.

### Rotation About the Worm Center and Five Draw Calls

**Source: `arrakis_worm.h`, lines 333-341.**

The root matrix starts with translation to center, then a y-axis rotation by the heading in radians, then translation by negative center. Its product is `T(center)*R_y(heading)*T(-center)`. This rotates geometry about the worm's center rather than about world origin. It is necessary because the flexible body vertices have already been generated in coordinates positioned around center; applying an ordinary translation again would move them twice.

Positive heading rotates local +z toward positive x under GLM's usual right-handed convention. The pose target samples terrain along that same `(sin(heading),cos(heading))` ground direction, so aiming and rendering agree. `head=root*headMatrix(p)` applies the local mouth aperture, tilt, and placement first, then the overall heading about center. This gives one shared mouth transform for all four static mouth pieces.

The five calls draw body with root and skin, lip with head and rim, funnel with head and inside, throat with head and dark, and teeth with head and enamel. The order is explicit and the mesh-material pairings are exact. They use the caller's active scene or shadow program through `drawMeshPrimitive`; `drawSandworm` does not create or choose its own shader. There is no separate translucent mouth pass, no tooth-by-tooth loop during drawing, and no dynamic mouth VBO upload.

Unlike `drawRescueHud`, this function has no broad OpenGL state guard. The common drawing helper binds each mesh VAO and leaves its normal renderer-side state behavior in place. The narrow array-buffer restoration inside the cache-update block is the only explicit binding preservation performed here. Calling the worm as part of both normal scene passes follows the renderer's existing conventions rather than treating it as an isolated overlay library.

## The Moving Sand Wake

### Wake Timing, Material, Positions, and Animated Scales

**Source: `arrakis_worm.h`, lines 342-358.**

`wake=1-smooth(0,6,attackTime)` is one throughout negative-time approach and fades to zero during the initial rise. The wake block executes only when wake exceeds 0.001. This suppresses tiny nearly vanished spheres as the rising worm emerges, and no wake is drawn once the rise has completed. It is not an additional mesh allocation or a particle system in this header.

`sand` copies skin, changes diffuse to `(0.68,0.46,0.22)`, and sets equal specular channels 0.015. It retains shininess eight, emissive zero, and `isSand=0`; the name describes the intended visual mound, not a request to apply the terrain's special sand flag.

The loop draws four mounds for i zero through three. An initial local position is `(center.x,center.y,center.z-14-14*i)`. Thus the centers are fourteen, 28, 42, and 56 units behind local +z movement. The code transforms this point with root and homogeneous w one to find its yawed world x and z. A point uses w one so translation participates; a direction would use w zero. This temporary world position is used for a terrain query, not directly as the final draw position.

`local.y` becomes terrain height at the yawed world xz minus 0.8. Because root's only net rotation is about y, world y equals local y after the paired center translations; replacing local y with this ground value therefore places the mound correctly even though the original center y was present in the first point. The -0.8 sinks each sphere into the dune so its visible top looks like disturbed sand rather than a sphere resting on the surface.

`mound=translate(root,local)` composes the root with the mound translation. Scaling then uses x extent `(12-i)*wake`, y extent `(3+0.35*sin(time*2-i))*wake`, and z extent `10*wake`. The four x scales are twelve, eleven, ten, and nine times wake. Vertical scale oscillates between 2.65 and 3.35 times wake, with each mound phase offset by i. The z scale is uniformly ten times wake. When wake approaches zero, all three dimensions shrink together.

The final call draws the already-existing `g_meshSphere` with that mound matrix and sand material. Four transformed spheres create a stylized raised wake using shared geometry. Their actual geometric radii depend on the renderer's sphere primitive definition; these numbers are model scale factors, not an independently asserted sphere radius. The positions rotate with heading, stay terrain-aware, and pulse with animation time. Lines 356-358 close the loop, conditional wake block, and drawing function.

### End of Header and Lifecycle Summary

**Source: `arrakis_worm.h`, line 360.**

The final preprocessor directive closes the guard begun at lines 1-2. The complete lifecycle is initialization while the context is current, repeated validated calls in both render passes, and cleanup before context destruction. CPU construction happens once for static mouth geometry; CPU skin deformation and VBO updates happen only when the cached pose inputs change. All five worm meshes are explicitly destroyed, while the shared sphere belongs to the surrounding renderer and is not deleted by worm cleanup.

## Numerical and Design Checks

### Following One HUD Frame From State to Pixels

The input snapshot remains const throughout composition. On a 1280-by-720 framebuffer, scale is one and the noncompact desktop layout applies. Its margin is 22, inner width is 1236, top card height is 106, and normal radar top is 160. The control string wraps according to its 1.5-unit pixel font, and the bottom cards' positions depend on the resulting line counts. These dependencies explain why the radar is measured after the action card rather than assigned an unconditional fixed y-to-bottom region.

On a 640-by-360 framebuffer, scale is still one, margin is ten, and compact is true. Because shortView tests strictly less than 360, height exactly 360 is compact but not short. Its top card is therefore 112 high rather than 78. On height 359, shortView becomes true and the special side-radar conditions become eligible. Strict threshold details matter when checking screenshots at boundary sizes; `w<760`, `h<440`, and `h<360` do not include their equality values.

On a 2560-by-1440 framebuffer, scale two produces the same 1280-by-720 layout dimensions as the desktop example while increasing physical glyph and line thickness. This is why the width and height uniform must use w and h rather than the raw framebuffer dimensions: the viewport already maps the clip-space result to the actual framebuffer. Using raw framebuffer dimensions in the shader after scaled composition would incorrectly compress the HUD into part of the image.

The HUD also keeps display roles separate. `number` handles negative integer counts, `clock` handles bounded upward-rounded time, `decimal` handles bounded zero-decimal measurements, `unit` handles bar fractions, and `finite` handles individual floating-point values. Not all conditions sanitize inputs before comparing them. The explicit finite checks and fallback policies described above should therefore not be generalized into a claim that every imaginable corrupted snapshot is completely layout-safe.

### Following One Worm Pose Through Both Passes

Suppose the caller draws a finite, initialized worm at attack six with time zero. Rise is one, lunge zero, retreat zero, aperture one, and tilt zero. The head lies 36 units above terrain. The body final ring is 1.4 units below that head along the upward axis and has radius fifteen, matching the lip's first profile ring. The neck is a straight 64-unit centerline segment in this pose because the arc sweep is zero. The buried Bezier segment joins it to a base 140 units below terrain.

If poseValid is false, the first pass deforms 18721 vertices, computes their normals, and uploads the complete body VBO. The other four meshes are already static. Five mesh draw calls render the worm; the wake is zero at attack six, so no mound spheres are drawn. If the second pass receives exactly the same center, attack, time, heading, and clamped progress, it calculates the pose again but skips body deformation and upload. It still issues the same five mesh draws with the appropriate caller-selected shader pass.

On a later attack value, changing the body skin and multiplying static mouth vertices by the current head matrix produce coordinated motion. The root rotates the full assembly toward heading. The body normals are recomputed from its bent surface, while the static mouth and tooth normals are transformed by the scene shader's normal-transform machinery. When attack reaches eighteen, the validation branch returns before any material setup or draw calls. Visibility termination is therefore explicit, not merely the result of the worm becoming dark or moving beneath dunes.

### Limitations That Should Not Be Mistaken for Features

The HUD has a fixed ASCII bitmap vocabulary rather than Unicode typography, no mouse-click buttons, no input event handling, no texture atlas, and no automatic count reconciliation. The layouts adapt by wrapping, reducing sizes, changing placements, and omitting secondary panels, but they do not implement scrolling for impossibly small framebuffers. The unsupported apostrophe and brackets are real fallback behavior. The declared pickup position and centered-text helper are unused in this header.

The worm does not load `sandworm.jpg`, import a model, regenerate a random set of teeth every frame, or animate each tooth separately. It has five owned meshes and reuses a scene sphere for the wake. It uses a fixed topology, deterministic sinusoidal irregularity, a circular-arc visible neck, a buried cubic Bezier transition, and a tuple cache. There is no general OpenGL state guard, no upload error recovery, no terrain-version cache key, and no automatic initialization inside `drawSandworm`. These boundaries help distinguish what the code really guarantees from what a reader might infer from the visual result.

## Complete Source Coverage Index

The intervals below are an audit of the explanation, not an additional source listing. Every nonblank line in the original two headers is included in an explained region. Structural lines such as opening and closing braces, namespace boundaries, return statements, comments, and end guards belong to the cited region containing the construction they complete. Blank separators do not carry operations.

| Original Header | Line Range | Explained Region |
| --- | --- | --- |
| `arrakis_hud.h` | 1-28 | Include guard, all includes, every `RescueHudState` member, defaults, and the outside-crew comment. |
| `arrakis_hud.h` | 30-34 | Current-context contract, framebuffer coordinates, preservation contract, and internal namespace opening. |
| `arrakis_hud.h` | 36-52 | Every palette constant, both internal structs, and the static resource accessor. |
| `arrakis_hud.h` | 53-66 | Finiteness, normalized clamping, count strings, rounded values, and countdown formatting. |
| `arrakis_hud.h` | 68-90 | Every saved-state field, capability entry, GL query, and constructor loop. |
| `arrakis_hud.h` | 91-107 | All restoration calls, branches, destructor behavior, and deleted copy operations. |
| `arrakis_hud.h` | 109-124 | Shader creation, compilation, status, diagnostics, failure deletion, and successful return. |
| `arrakis_hud.h` | 126-147 | Triangle, rectangle, border, card, and progress-bar construction. |
| `arrakis_hud.h` | 149-204 | Font comment, all 50 explicit glyph cases, default case, and five-bit encoding. |
| `arrakis_hud.h` | 205-222 | Text width, case conversion, each font scan branch, run merging, and character advance. |
| `arrakis_hud.h` | 223-247 | Width-to-column expression, lambda, word splitting, whitespace/newline behavior, and returned lines. |
| `arrakis_hud.h` | 248-255 | Centered placement, line-count cap, and line-loop spacing. |
| `arrakis_hud.h` | 257-294 | Radar card, grid, coordinate clamp/map, all marker branches, marker order, and every legend. |
| `arrakis_hud.h` | 296-322 | Overlay dimming, precedence, responsive dimensions, every body/key string, and panel placement. |
| `arrakis_hud.h` | 323-341 | Branding, heading, all result rows, cursor increments, body, separator, and keys. |
| `arrakis_hud.h` | 343-355 | Margin, modes, extraction timer, duration defense, warning decisions, and top-card size. |
| `arrakis_hud.h` | 356-375 | All desktop header labels, coordinates, seat loop, counts, countdown, and bar. |
| `arrakis_hud.h` | 375-392 | Compact branch, right alignment, short-view placements, omitted rows, and compact seats. |
| `arrakis_hud.h` | 394-422 | Controls string and wrap, action precedence, side-radar test, truncation, card, and shared progress. |
| `arrakis_hud.h` | 424-442 | Radar sizing and labels, telemetry gates, every measurement/string, boost, and pursuit. |
| `arrakis_hud.h` | 443-452 | Flight-only guide condition, four corner loops, offsets, and final overlay call. |
| `arrakis_hud.h` | 454 | Internal namespace closure. |
| `arrakis_hud.h` | 456-491 | Public initializer, GLSL expressions, shader and program creation, all compilation/link failure paths. |
| `arrakis_hud.h` | 492-510 | Binding queries, VAO/VBO allocation failure, uniform location, vertex attributes, restore, and CPU reserve. |
| `arrakis_hud.h` | 512-540 | Dimension checks, lazy init, scope guard, scale, composition, drawing state, buffer growth/orphaning, upload, draw. |
| `arrakis_hud.h` | 542-552 | Conditional resource deletion, resets, CPU capacity release, function end, and guard end. |
| `arrakis_worm.h` | 1-20 | Guard, comments, dependencies, namespace, all constants, meshes, CPU vector, flags, and cached values. |
| `arrakis_worm.h` | 22-32 | Smoothstep expression and conditional mesh deletion/reset. |
| `arrakis_worm.h` | 34-43 | Grid capacity, both loops, all corner-index expressions, triangle order, and closure. |
| `arrakis_worm.h` | 45-61 | Normal reset, triangle accumulation, seam loop, normalization, and assumptions. |
| `arrakis_worm.h` | 63-84 | Winding comments, lathe parameters, sampling loops, complete roughness formula, positions, UVs, connectivity, normals. |
| `arrakis_worm.h` | 85-97 | Deep-floor branch, center, copied rim, triangle fan, normals, upload, and function closure. |
| `arrakis_worm.h` | 99-120 | All tooth constants and arrays, exact capacity expressions, row/tooth loops, seed, angles, variation, dimensions, base. |
| `arrakis_worm.h` | 121-137 | Ring loop, centerline, derivative, frame vectors, taper exponent, side loop, vertex data, and loop closure. |
| `arrakis_worm.h` | 138-160 | Side connections, modulo, apex comment, exact tip position/normal/UV, final triangles, loops, and upload return. |
| `arrakis_worm.h` | 162-189 | Every pose field, all timed transitions, terrain samples, heights, bobbing, reach, aim, tilt, swallowing, and return. |
| `arrakis_worm.h` | 191-195 | Head translation, rotation, aperture scale, returned product, and closure. |
| `arrakis_worm.h` | 197-213 | Mouth axis, neck constants, end, arc lambda and small-angle branch, buried endpoints and control points. |
| `arrakis_worm.h` | 214-241 | All body rings, Bezier/arc branch, tangent, radial frame, taper, ridge, blend, grain, position and UV expressions. |
| `arrakis_worm.h` | 242-256 | Normal comment, both derivative loops, wrapped neighbors, endpoint branches, differences, cross product, seam, closure. |
| `arrakis_worm.h` | 258 | Internal namespace closure. |
| `arrakis_worm.h` | 260-265 | Public cleanup, all five destroys, CPU clear, and flag invalidation. |
| `arrakis_worm.h` | 267-279 | Idempotence, body size, initial pose, grid/upload, dynamic reallocation, and narrow binding preservation. |
| `arrakis_worm.h` | 281-292 | Every exact profile point, roughness, throat floor, teeth creation, ready/cache flags, and closure. |
| `arrakis_worm.h` | 294-316 | Public parameters, every validation term, clamp, pose, exact cache condition, deformation, upload, restore, and cache updates. |
| `arrakis_worm.h` | 317-332 | All five materials, every diffuse/specular/shininess number, and inherited defaults. |
| `arrakis_worm.h` | 333-341 | Root pivot transforms, heading conversion, head product, and all five mesh-material draws. |
| `arrakis_worm.h` | 342-358 | Wake transition and threshold, sand material, four-mound loop, terrain alignment, all scale factors, sphere draws, and closures. |
| `arrakis_worm.h` | 360 | Guard end. |

The chapter covers 552 original HUD lines and 360 original worm lines, or 912 total source lines including blank separators. The HUD contains 533 nonblank lines and the worm contains 341, giving 874 nonblank lines in total. A mechanical comparison of the relocated headers against this coverage index found no uncovered nonblank lines in either file. The coverage index assigns every nonblank source line to explanatory prose or the exact glyph table. No complete code listing is embedded here; the complete source appendix can be appended separately without replacing these explanations.
