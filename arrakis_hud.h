#ifndef ARRAKIS_HUD_H
#define ARRAKIS_HUD_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

struct RescueHudState {
    int width = 1280, height = 720;
    // waiting counts Running + Waiting crew outside, not workers still inside.
    int rescued = 0, aboard = 0, waiting = 0, lost = 0, total = 36, best = 0, wave = 1;
    float remaining = 210, altitude = 0, speed = 0, boost = 1;
    float missionDuration = 210, pursuit = 0, wormDistance = 220, extractionRemaining = 60;
    int inside = 36, releasedGroups = 0;
    float pickupProgress = 0, unloadProgress = 0, fps = 60;
    float heading = 0;
    bool title = true, paused = false, ended = false;
    std::string prompt;
    glm::vec2 player{0}, harvester{0}, base{0}, worm{0}, pickup{0};
    std::vector<glm::vec2> crewMarkers;
    float attackTime = -1;
};

// Call with a current OpenGL 3.3 core context and loaded GLAD entry points.
// width/height are framebuffer pixels. One context owns the HUD until cleanup.
// Drawing preserves program, VAO/VBO, viewport, raster, color and blend state;
// the caller's framebuffer and textures are not changed.
namespace arrakis_hud_detail {

struct Color { float r, g, b, a; };
inline constexpr Color bone{0.91f, 0.89f, 0.82f, 1.0f};
inline constexpr Color muted{0.59f, 0.61f, 0.58f, 1.0f};
inline constexpr Color cyan{0.36f, 0.85f, 0.86f, 1.0f};
inline constexpr Color amber{0.98f, 0.65f, 0.27f, 1.0f};
inline constexpr Color white{1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr Color threat{0.98f, 0.36f, 0.27f, 1.0f};
inline constexpr Color black{0.018f, 0.025f, 0.028f, 0.84f};
inline constexpr Color edge{0.42f, 0.47f, 0.46f, 0.25f};
struct Vertex { float x, y, r, g, b, a; };
struct Resources {
    GLuint program = 0, vao = 0, vbo = 0;
    GLint canvas = -1;
    std::size_t capacity = 0;
    std::vector<Vertex> vertices;
};
inline Resources& resources() { static Resources r; return r; }
inline float finite(float v, float fallback = 0.0f) { return std::isfinite(v) ? v : fallback; }
inline float unit(float v) { return std::clamp(finite(v), 0.0f, 1.0f); }
inline std::string number(int v) { return std::to_string((std::max)(0, v)); }
inline std::string decimal(float v) {
    char out[48];
    std::snprintf(out, sizeof(out), "%.0f", std::clamp(finite(v), -99999.0f, 99999.0f));
    return out;
}
inline std::string clock(float v) {
    const int seconds = static_cast<int>(std::ceil(std::clamp(finite(v), 0.0f, 5999.0f)));
    char out[16];
    std::snprintf(out, sizeof(out), "%02d:%02d", seconds / 60, seconds % 60);
    return out;
}

struct GlState {
    GLint program, vao, buffer, viewport[4], polygon[2];
    GLint srcRgb, dstRgb, srcAlpha, dstAlpha, equationRgb, equationAlpha;
    GLboolean colorMask[4];
    const std::array<GLenum, 10> caps{{GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE,
        GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_RASTERIZER_DISCARD, GL_COLOR_LOGIC_OP,
        GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE, GL_SAMPLE_ALPHA_TO_ONE}};
    std::array<GLboolean, 10> enabled{};
    GlState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_POLYGON_MODE, polygon);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        for (std::size_t i = 0; i < caps.size(); ++i) enabled[i] = glIsEnabled(caps[i]);
    }
    ~GlState() {
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        // Core contexts have one polygon mode for both faces.
        glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(polygon[0]));
        glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
        glBlendEquationSeparate(equationRgb, equationAlpha);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        for (std::size_t i = 0; i < caps.size(); ++i) {
            if (enabled[i]) glEnable(caps[i]); else glDisable(caps[i]);
        }
    }
    GlState(const GlState&) = delete;
    GlState& operator=(const GlState&) = delete;
};

inline GLuint shader(GLenum type, const char* source) {
    const GLuint id = glCreateShader(type);
    if (!id) return 0;
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Arrakis HUD shader: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

inline void triangle(float ax, float ay, float bx, float by, float cx, float cy, Color c) {
    auto& v = resources().vertices;
    v.push_back({ax, ay, c.r, c.g, c.b, c.a});
    v.push_back({bx, by, c.r, c.g, c.b, c.a});
    v.push_back({cx, cy, c.r, c.g, c.b, c.a});
}
inline void rect(float x, float y, float w, float h, Color c) {
    if (w <= 0 || h <= 0) return;
    triangle(x, y, x + w, y, x + w, y + h, c);
    triangle(x, y, x + w, y + h, x, y + h, c);
}
inline void outline(float x, float y, float w, float h, Color c) {
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}
inline void card(float x, float y, float w, float h, Color accent = edge) {
    rect(x, y, w, h, black); outline(x, y, w, h, edge);
    rect(x, y, 2, h, accent);
}
inline void bar(float x, float y, float w, float amount, Color c, float h = 4) {
    rect(x, y, w, h, edge); rect(x, y, w * unit(amount), h, c);
}

// Seven rows of five bits. Adjacent lit pixels are emitted as one rectangle.
inline std::array<unsigned char, 7> glyph(char c) {
    switch (c) {
    case 'A': return {{14,17,17,31,17,17,17}};
    case 'B': return {{30,17,17,30,17,17,30}};
    case 'C': return {{14,17,16,16,16,17,14}};
    case 'D': return {{30,17,17,17,17,17,30}};
    case 'E': return {{31,16,16,30,16,16,31}};
    case 'F': return {{31,16,16,30,16,16,16}};
    case 'G': return {{14,17,16,23,17,17,15}};
    case 'H': return {{17,17,17,31,17,17,17}};
    case 'I': return {{31,4,4,4,4,4,31}};
    case 'J': return {{7,2,2,2,18,18,12}};
    case 'K': return {{17,18,20,24,20,18,17}};
    case 'L': return {{16,16,16,16,16,16,31}};
    case 'M': return {{17,27,21,21,17,17,17}};
    case 'N': return {{17,25,21,19,17,17,17}};
    case 'O': return {{14,17,17,17,17,17,14}};
    case 'P': return {{30,17,17,30,16,16,16}};
    case 'Q': return {{14,17,17,17,21,18,13}};
    case 'R': return {{30,17,17,30,20,18,17}};
    case 'S': return {{15,16,16,14,1,1,30}};
    case 'T': return {{31,4,4,4,4,4,4}};
    case 'U': return {{17,17,17,17,17,17,14}};
    case 'V': return {{17,17,17,17,17,10,4}};
    case 'W': return {{17,17,17,21,21,21,10}};
    case 'X': return {{17,17,10,4,10,17,17}};
    case 'Y': return {{17,17,10,4,4,4,4}};
    case 'Z': return {{31,1,2,4,8,16,31}};
    case '0': return {{14,17,19,21,25,17,14}};
    case '1': return {{4,12,4,4,4,4,14}};
    case '2': return {{14,17,1,2,4,8,31}};
    case '3': return {{30,1,1,14,1,1,30}};
    case '4': return {{2,6,10,18,31,2,2}};
    case '5': return {{31,16,16,30,1,1,30}};
    case '6': return {{14,16,16,30,17,17,14}};
    case '7': return {{31,1,2,4,8,8,8}};
    case '8': return {{14,17,17,14,17,17,14}};
    case '9': return {{14,17,17,15,1,1,14}};
    case '/': return {{1,1,2,4,8,16,16}};
    case '+': return {{0,4,4,31,4,4,0}};
    case '-': return {{0,0,0,31,0,0,0}};
    case ':': return {{0,4,4,0,4,4,0}};
    case '.': return {{0,0,0,0,0,6,6}};
    case ',': return {{0,0,0,0,0,4,8}};
    case '%': return {{25,25,2,4,8,19,19}};
    case '!': return {{4,4,4,4,4,0,4}};
    case '?': return {{14,17,1,2,4,0,4}};
    case '(': return {{2,4,8,8,8,4,2}};
    case ')': return {{8,4,2,2,2,4,8}};
    case '=': return {{0,0,31,0,31,0,0}};
    case '_': return {{0,0,0,0,0,0,31}};
    case ' ': return {{0,0,0,0,0,0,0}};
    default: return {{14,17,1,2,4,0,4}};
    }
}
inline float textWidth(const std::string& s, float size) {
    return s.empty() ? 0.0f : (static_cast<float>(s.size()) * 6 - 1) * size;
}
inline void text(float x, float y, const std::string& s, float size, Color c = bone) {
    for (unsigned char ch : s) {
        const char upper = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch);
        const auto rows = glyph(upper);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5;) {
                if (!(rows[row] & (16 >> column))) { ++column; continue; }
                const int first = column++;
                while (column < 5 && (rows[row] & (16 >> column))) ++column;
                rect(x + first * size, y + row * size, (column - first) * size, size, c);
            }
        }
        x += 6 * size;
    }
}
inline std::vector<std::string> wrap(const std::string& s, float width, float size) {
    const std::size_t columns = static_cast<std::size_t>((std::max)(1.0f, std::floor((width + size) / (6 * size))));
    std::vector<std::string> lines;
    std::string line, word;
    auto append = [&]() {
        if (word.empty()) return;
        if (!line.empty() && line.size() + 1 + word.size() > columns) {
            lines.push_back(line); line.clear();
        }
        while (word.size() > columns) {
            lines.push_back(word.substr(0, columns)); word.erase(0, columns);
        }
        if (!word.empty()) { if (!line.empty()) line += ' '; line += word; }
        word.clear();
    };
    for (unsigned char c : s) {
        if (c <= ' ') {
            append();
            if (c == '\n') { lines.push_back(line); line.clear(); }
        } else { word += static_cast<char>(c < 127 ? c : '?'); }
    }
    append();
    if (!line.empty()) lines.push_back(line);
    return lines;
}
inline void centered(float x, float y, float width, const std::string& s, float size, Color c = bone) {
    text(x + (width - textWidth(s, size)) * 0.5f, y, s, size, c);
}
inline void lines(float x, float y, const std::vector<std::string>& content, float size,
                  Color c = bone, std::size_t count = 100) {
    const std::size_t n = (std::min)(content.size(), count);
    for (std::size_t i = 0; i < n; ++i) text(x, y + i * size * 10, content[i], size, c);
}

inline void radar(float x, float y, float side, const RescueHudState& s, bool labels) {
    card(x, y, side, side);
    for (int i = 1; i < 4; ++i) {
        rect(x + side * i / 4, y + 1, 1, side - 2, edge);
        rect(x + 1, y + side * i / 4, side - 2, 1, edge);
    }
    auto point = [&](glm::vec2 p, Color c, int type) {
        const float px = x + 6 + (std::clamp(finite(p.x), -300.0f, 300.0f) + 300) / 600 * (side - 12);
        // Radar stays north-up even when the heading-locked chase camera turns.
        const float py = y + 6 + (std::clamp(finite(p.y), -300.0f, 300.0f) + 300) / 600 * (side - 12);
        if (type == 0) {
            float yaw = finite(s.heading) * 0.01745329252f;
            glm::vec2 forward(-std::sin(yaw), -std::cos(yaw)), right(-forward.y, forward.x);
            glm::vec2 tip = glm::vec2(px, py) + forward * 4.0f;
            glm::vec2 tail = glm::vec2(px, py) - forward * 3.0f;
            triangle(tip.x, tip.y, tail.x - right.x * 4, tail.y - right.y * 4,
                     tail.x + right.x * 4, tail.y + right.y * 4, c);
        }
        else if (type == 1) outline(px - 4, py - 4, 8, 8, c);
        else if (type == 2) { rect(px - 3, py - 3, 6, 6, c); }
        else if (type == 4) { rect(px - 1.5f, py - 1.5f, 3, 3, c); }
        else {
            triangle(px, py - 4, px - 4, py, px, py + 4, c);
            triangle(px, py - 4, px, py + 4, px + 4, py, c);
        }
    };
    point(s.base, cyan, 1);
    for (const glm::vec2& crew : s.crewMarkers) point(crew, amber, 4);
    point(s.harvester, white, 2);
    point(s.worm, threat, 3); point(s.player, cyan, 0);
    if (labels) {
        text(x, y - 18, "SECTOR / +/-300", 1.5f, muted);
        text(x, y + side + 9, "YOU / BASE", 1.5f, cyan);
        text(x, y + side + 26, "CREW", 1.5f, amber);
        text(x + 47, y + side + 26, "HARV", 1.5f, white);
        text(x + 92, y + side + 26, "WORM", 1.5f, threat);
    }
}

inline void overlay(float w, float h, const RescueHudState& s) {
    rect(0, 0, w, h, {0.01f, 0.018f, 0.022f, 0.78f});
    const bool result = s.ended;
    const bool title = !result && s.title;
    const float margin = w < 480 ? 12.0f : 28.0f;
    const float panelW = (std::min)(680.0f, w - margin * 2);
    const float inset = panelW < 420 || h < 420 ? 14.0f : 28.0f;
    const float contentW = panelW - inset * 2;
    const float size = h < 300 ? 1.0f : panelW < 380 || h < 420 ? 1.5f : 2.0f;
    const float headingSize = h < 300 ? 2.0f : panelW < 420 || h < 420 ? 2.5f : 4.0f;
    const std::string heading = result ? "MISSION CLOSED" : title ? "HARVESTER FLIGHT" : "FLIGHT PAUSED";
    const auto body = wrap(result ? (s.prompt == "ORNITHOPTER LOST IN THE WORM'S MAW" ?
        "ORNITHOPTER LOST IN THE WORM'S MAW. DELIVERED CREW REMAIN SAFE." :
        "RESCUE COMPLETE. ONLY CREW DELIVERED TO BASE COUNT. EVERY LIFE COUNTS.") :
        title ? "FLEEING HARVESTER RELEASES 1-4 CREW AT A TIME.\n"
                "AMBER GROUPS WAIT ON SAND. 8 RESCUE SEATS.\n"
                "MOUSE STEERS. FLY LOW. HOLD SPACE TO WINCH.\n"
                "RETURN TO CYAN BASE. HOLD SPACE TO BANK RESCUED." :
                "THE RESCUE IS ON HOLD. YOUR CREW IS COUNTING ON YOU.", contentW, size);
    const auto keys = wrap(result ? "ENTER NEXT MISSION / R RETRY" : title ? "ENTER START" : "P RESUME", contentW, size);
    const float lineH = size * 10;
    const float statsH = result ? lineH * 3 + 12 : 0;
    const float panelH = inset * 2 + 23 + headingSize * 7 + 18 + statsH +
                         static_cast<float>(body.size() + keys.size()) * lineH + 25;
    const float x = (w - panelW) * 0.5f;
    const float y = (std::max)(8.0f, (h - panelH) * 0.5f);
    card(x, y, panelW, panelH, result ? amber : cyan);
    float cursor = y + inset;
    text(x + inset, cursor, "ARRAKIS / SEARCH + RESCUE", 1.5f, muted);
    cursor += 23;
    text(x + inset, cursor, heading, headingSize, bone);
    cursor += headingSize * 7 + 18;
    if (result) {
        text(x + inset, cursor, "SECURED " + number(s.rescued) + " / " + number(s.total), size, cyan);
        cursor += lineH;
        text(x + inset, cursor, "BEST " + number((std::max)(s.best, s.rescued)) + " / MISSION " + number(s.wave), size, bone);
        cursor += lineH;
        text(x + inset, cursor, "ABOARD " + number(s.aboard) + " / LOST " + number(s.lost), size, muted);
        cursor += lineH + 12;
    }
    lines(x + inset, cursor, body, size, muted);
    cursor += static_cast<float>(body.size()) * lineH + 12;
    rect(x + inset, cursor, contentW, 1, edge);
    cursor += 13;
    lines(x + inset, cursor, keys, size, cyan);
}

inline void compose(float w, float h, const RescueHudState& s) {
    const float m = w < 600 || h < 400 ? 10.0f : 22.0f;
    const float inner = w - 2 * m;
    const bool compact = w < 760 || h < 440;
    const bool shortView = h < 360;
    const bool extraction = s.attackTime >= 0;
    const float remaining = extraction ? s.extractionRemaining : s.remaining;
    const float duration = extraction ? 60.0f : (std::max)(0.001f, finite(s.missionDuration, 210.0f));
    const bool danger = finite(remaining) <= 15 || extraction;
    const std::string timerLabel = extraction ? "EXTRACTION WINDOW" : danger ? "BREACH IMMINENT" : "TIME TO BREACH";
    const Color timerColor = danger ? amber : bone;
    const float topH = compact ? (shortView ? 78.0f : 112.0f) : 106.0f;
    card(m, m, inner, topH);
    if (!compact) {
        text(m + 18, m + 16, "ARRAKIS", 3.0f);
        text(m + 18, m + 47, "FLEEING HARVESTER", 1.5f, muted);
        text(m + 18, m + 76, "SEARCH + RESCUE / " + number(s.wave), 1.5f, cyan);
        const float scoreX = m + inner * 0.30f;
        text(scoreX, m + 15, "SECURED " + number(s.rescued) + "/" + number(s.total), 2.5f, cyan);
        text(scoreX, m + 44, "ABOARD " + number(s.aboard) + "/8", 1.5f);
        text(scoreX + 126, m + 44, "INSIDE " + number(s.inside), 1.5f, muted);
        for (int i = 0; i < 8; ++i) {
            const Color c = i < s.aboard ? cyan : edge;
            rect(scoreX + i * 14, m + 64, 10, 5, c);
        }
        text(scoreX + 126, m + 64, "GROUPS " + number(s.releasedGroups), 1.5f, amber);
        text(scoreX, m + 82, "SAND " + number(s.waiting) + " / LOST " + number(s.lost), 1.5f, muted);
        const float timerX = m + inner - 230;
        text(timerX, m + 15, timerLabel, 1.5f, timerColor);
        text(timerX, m + 39, clock(remaining), 3.0f, timerColor);
        bar(timerX, m + 74, 210, finite(remaining) / duration, timerColor, 5);
        text(timerX, m + 87, "SECURE CREW AT CYAN BASE", 1.5f, muted);
    } else {
        text(m + 12, m + 10, "ARRAKIS / FLEEING HARVESTER", inner < 330 ? 1.5f : 2.0f);
        const float row = m + (shortView ? 26 : 32);
        text(m + 12, row, timerLabel, 1.5f, timerColor);
        text(m + inner - 12 - textWidth(clock(remaining), 2), row - 2, clock(remaining), 2, timerColor);
        bar(m + inner - 86, m + (shortView ? 56 : 48), 74, finite(remaining) / duration, timerColor, 3);
        const float statsY = m + (shortView ? 43 : 54);
        text(m + 12, statsY, "SAVED " + number(s.rescued) + "/" + number(s.total), 1.5f, cyan);
        const std::string seats = "SEATS " + number(s.aboard) + "/8";
        text(m + inner - 12 - textWidth(seats, 1.5f), statsY, seats, 1.5f);
        text(m + 12, m + (shortView ? 61 : 76), "SAND " + number(s.waiting) + " INSIDE " + number(s.inside) +
             " GROUPS " + number(s.releasedGroups), 1.5f, muted);
        if (!shortView) {
            text(m + 12, m + 96, "LOST " + number(s.lost) + " / WAVE " + number(s.wave), 1.5f, muted);
            if (inner >= 360) for (int i = 0; i < 8; ++i)
                rect(m + inner - 104 + i * 12, m + 99, 8, 4, i < s.aboard ? cyan : edge);
        }
    }

    // Controls wrap at readable pixel sizes instead of scaling a 1280px screen down.
    const float controlSize = 1.5f;
    const auto controls = wrap("MOUSE STEER / W S THRUST / A D STRAFE / Q E ALT / SPACE WINCH / SHIFT BOOST / C CAMERA / P PAUSE / R RETRY", inner - 20, controlSize);
    const float controlsH = static_cast<float>(controls.size()) * controlSize * 10 + 14;
    const float controlsY = h - m - controlsH;
    card(m, controlsY, inner, controlsH);
    lines(m + 10, controlsY + 8, controls, controlSize, muted);

    std::string action = s.prompt;
    if (action.empty()) action = s.aboard >= 8 ? "SEATS FULL / RETURN TO CYAN BASE" :
        s.aboard > 0 ? "RETURN TO BASE / HOLD SPACE TO BANK CREW" : "FLY LOW TO CREW / HOLD SPACE TO WINCH";
    if (s.unloadProgress > 0) action = "BANKING CREW / HOLD SPACE";
    else if (s.pickupProgress > 0) action = "WINCH ACTIVE / HOLD SPACE";
    const bool sideRadar = shortView && inner >= 280 && controlsY - 72 >= m + topH + 6;
    const float actionW = (std::min)(inner - (sideRadar ? 76.0f : 0.0f), 620.0f);
    const float actionSize = compact ? 1.5f : 2.0f;
    auto actionLines = wrap(action, actionW - 28, actionSize);
    if (actionLines.size() > 2) {
        actionLines.resize(2);
        std::string& last = actionLines.back();
        if (last.size() >= 3) last.replace(last.size() - 3, 3, "...");
    }
    const float actionH = static_cast<float>(actionLines.size()) * actionSize * 10 + 23;
    const float actionY = controlsY - actionH - 8;
    const float actionX = sideRadar ? w - m - actionW : (w - actionW) * 0.5f;
    card(actionX, actionY, actionW, actionH, cyan);
    lines(actionX + 14, actionY + 10, actionLines, actionSize);
    const float progress = s.unloadProgress > 0 ? s.unloadProgress : s.pickupProgress;
    bar(actionX + 14, actionY + actionH - 9, actionW - 28, progress, cyan, 3);

    const float radarY = m + topH + (compact ? 10 : 32);
    const float available = actionY - radarY - (compact ? 6 : 52);
    const float side = (std::min)(compact ? 92.0f : 156.0f, available);
    if (sideRadar) radar(m, controlsY - 72, 64, s, false);
    else if (side >= 40) radar(w - m - side, radarY, side, s, !compact && side >= 128);
    if (!sideRadar && available >= 88 && (!compact || inner >= 420)) {
        const float tw = compact ? 184.0f : 222.0f;
        const bool pursuitRoom = available >= 112;
        card(m, radarY, tw, pursuitRoom ? 112.0f : 88.0f);
        text(m + 12, radarY + 12, "ALT " + decimal(s.altitude) + " M", 1.5f);
        text(m + 12, radarY + 31, "SPD " + decimal(s.speed) + " / FPS " + decimal(s.fps), 1.5f, muted);
        text(m + 12, radarY + 52, "BOOST", 1.5f, muted);
        bar(m + 72, radarY + 55, tw - 86, s.boost, cyan, 5);
        text(m + 12, radarY + 72, "WORM RANGE " + decimal((std::max)(0.0f, finite(s.wormDistance))) + " M", 1.5f, amber);
        if (pursuitRoom) {
            text(m + 12, radarY + 92, "PURSUIT", 1.5f, amber);
            bar(m + 86, radarY + 95, tw - 100, s.pursuit, threat, 5);
        }
    }
    if (!s.ended && !s.title && !s.paused) {
        Color guide = cyan; guide.a = 0.35f;
        for (int dx : {-1, 1}) for (int dy : {-1, 1}) {
            float x = w * (0.5f + dx * 0.06f), y = h * (0.5f + dy * 0.06f);
            rect(x - (dx > 0 ? 7 : 0), y, 7, 1, guide);
            rect(x, y - (dy > 0 ? 7 : 0), 1, 7, guide);
        }
    }
    if (s.ended || s.title || s.paused) overlay(w, h, s);
}

} // namespace arrakis_hud_detail

inline void initRescueHud() {
    namespace hud = arrakis_hud_detail;
    auto& r = hud::resources();
    if (r.program) return;
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 position;\n"
        "layout(location=1) in vec4 color;\n"
        "uniform vec2 canvas; out vec4 tint;\n"
        "void main(){ tint=color; gl_Position=vec4(position.x/canvas.x*2.0-1.0,"
        "1.0-position.y/canvas.y*2.0,0.0,1.0); }\n";
    const char* fs =
        "#version 330 core\n"
        "in vec4 tint; out vec4 fragment;\n"
        "void main(){ fragment=tint; }\n";
    const GLuint vertexShader = hud::shader(GL_VERTEX_SHADER, vs);
    const GLuint fragmentShader = hud::shader(GL_FRAGMENT_SHADER, fs);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return;
    }
    const GLuint program = glCreateProgram();
    if (!program) { glDeleteShader(vertexShader); glDeleteShader(fragmentShader); return; }
    glAttachShader(program, vertexShader); glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader); glDeleteShader(fragmentShader);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Arrakis HUD link: %s\n", log);
        glDeleteProgram(program);
        return;
    }
    GLint oldVao = 0, oldBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldBuffer);
    glGenVertexArrays(1, &r.vao); glGenBuffers(1, &r.vbo);
    if (!r.vao || !r.vbo) {
        if (r.vao) glDeleteVertexArrays(1, &r.vao);
        if (r.vbo) glDeleteBuffers(1, &r.vbo);
        r.vao = r.vbo = 0; glDeleteProgram(program); return;
    }
    r.program = program;
    r.canvas = glGetUniformLocation(r.program, "canvas");
    glBindVertexArray(r.vao); glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hud::Vertex), nullptr);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(hud::Vertex), reinterpret_cast<const void*>(offsetof(hud::Vertex, r)));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldBuffer));
    r.vertices.reserve(65536);
}

inline void drawRescueHud(const RescueHudState& state) {
    namespace hud = arrakis_hud_detail;
    if (state.width <= 0 || state.height <= 0) return;
    initRescueHud();
    auto& r = hud::resources();
    if (!r.program) return;
    hud::GlState saved;
    // Keep small-window text legible; only large framebuffers scale the canvas up.
    const float scale = (std::max)(1.0f, (std::min)(state.width / 1280.0f, state.height / 720.0f));
    const float w = state.width / scale, h = state.height / scale;
    r.vertices.clear();
    hud::compose(w, h, state);
    for (GLenum cap : saved.caps) glDisable(cap);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, state.width, state.height);
    glUseProgram(r.program);
    glUniform2f(r.canvas, w, h);
    glBindVertexArray(r.vao); glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    const std::size_t bytes = r.vertices.size() * sizeof(hud::Vertex);
    if (bytes > r.capacity) r.capacity = (std::max)(bytes, r.capacity * 2);
    // Orphan the dynamic buffer so the previous frame can finish without a stall.
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(r.capacity), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), r.vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(r.vertices.size()));
}

inline void cleanupRescueHud() {
    auto& r = arrakis_hud_detail::resources();
    if (r.vbo) glDeleteBuffers(1, &r.vbo);
    if (r.vao) glDeleteVertexArrays(1, &r.vao);
    if (r.program) glDeleteProgram(r.program);
    r.vbo = r.vao = r.program = 0;
    r.canvas = -1; r.capacity = 0;
    std::vector<arrakis_hud_detail::Vertex>().swap(r.vertices);
}

#endif // ARRAKIS_HUD_H
