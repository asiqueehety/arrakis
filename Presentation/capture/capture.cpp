// Inspection captures reuse the project's actual geometry, shaders and draw routines.
// Only framing and staged demonstration state are changed; application source is untouched.
#define main arrakis_application_main
#include "../../src/arrakis.cpp"
#undef main

#include <filesystem>
#include <functional>

namespace {
const glm::vec3 sun = glm::normalize(glm::vec3(-0.35f, 0.12f, -1.0f));
const glm::vec3 sunColor(1.15f, 1.04f, 0.85f);
const glm::vec3 ambient(0.34f, 0.38f, 0.44f);
glm::vec3 anchor;

void setVec(GLuint program, const char* key, const glm::vec3& value) {
    glUniform3fv(glGetUniformLocation(program, key), 1, glm::value_ptr(value));
}
void setMatrix(GLuint program, const char* key, const glm::mat4& value) {
    glUniformMatrix4fv(glGetUniformLocation(program, key), 1, GL_FALSE, glm::value_ptr(value));
}
void floor() {
    Material sand;
    sand.diffuse = COL_DUNE_SAND;
    sand.specular = glm::vec3(0.25f);
    sand.shininess = 16;
    sand.isSand = 1;
    drawPart(g_meshCube, glm::translate(glm::mat4(1), anchor), {0, -0.05f, 0},
             {0, 0, 0}, {140, 0.1f, 140}, sand);
}
void clearRescueWorld() {
    resetMission(true);
    g_mission.elapsed = g_mission.breachAt + 30;
    g_mission.base = anchor + glm::vec3(10000, 0, 10000);
    for (auto& crew : g_mission.crew) crew.state = CrewState::Safe;
    for (auto& group : g_mission.groups) group.released = false;
    g_mission.pickup = 0;
    g_mission.target = -1;
}
void capture(const std::filesystem::path& path, glm::vec3 eye, glm::vec3 target,
             const std::function<void()>& draw, bool sky = false, bool wire = false,
             bool collapse = false) {
    auto light = glm::ortho(-80.0f, 80.0f, -80.0f, 80.0f, 1.0f, 400.0f) *
                 glm::lookAt(target + sun * 160.0f, target, glm::vec3(0, 1, 0));
    g_isShadowPass = true;
    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowFBO);
    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_progShadow);
    setMatrix(g_progShadow, "lightSpaceMatrix", light);
    auto wormCenter = collapse ? wormPosition() : anchor;
    float collapseAmount = collapse ? glm::smoothstep(7.0f, 14.0f, wormAttackTime()) : 0;
    setVec(g_progShadow, "wormCenter", wormCenter);
    glUniform1f(glGetUniformLocation(g_progShadow, "collapse"), collapseAmount);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.5f, 4.0f);
    draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_scrW, g_scrH);
    glClearColor(0.063f, 0.094f, 0.118f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
    auto projection = glm::perspective(glm::radians(58.0f), float(g_scrW) / g_scrH, 0.1f, 1200.0f);
    if (sky) {
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glUseProgram(g_progSky);
        setVec(g_progSky, "lightDir", sun);
        setVec(g_progSky, "sunColor", sunColor);
        setMatrix(g_progSky, "invViewProj", glm::inverse(projection * view));
        glBindVertexArray(g_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }
    g_isShadowPass = false;
    glUseProgram(g_progScene);
    setMatrix(g_progScene, "view", view);
    setMatrix(g_progScene, "projection", projection);
    setMatrix(g_progScene, "lightSpaceMatrix", light);
    setVec(g_progScene, "lightDir", sun);
    setVec(g_progScene, "sunColor", sunColor);
    setVec(g_progScene, "ambientColor", ambient);
    setVec(g_progScene, "viewPos", eye);
    setVec(g_progScene, "fogColor", glm::vec3(0.90f, 0.74f, 0.47f));
    setVec(g_progScene, "wormCenter", wormCenter);
    glUniform1f(glGetUniformLocation(g_progScene, "fogDensity"), 0.0022f);
    glUniform1f(glGetUniformLocation(g_progScene, "collapse"), collapseAmount);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_shadowTex);
    glUniform1i(glGetUniformLocation(g_progScene, "shadowMap"), 0);
    if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    draw();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glFinish();
    auto error = glGetError();
    if (error != GL_NO_ERROR || !captureScreenshot(path.string()))
        throw std::runtime_error("Capture failed: " + path.string());
    std::cout << "Captured " << path.filename().string() << '\n';
}
}

int main(int argc, char** argv) {
    if (argc != 2 || !std::filesystem::is_directory(argv[1])) return 2;
    if (!glfwInit()) return 3;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    g_scrW = 1280; g_scrH = 720;
    g_window = glfwCreateWindow(g_scrW, g_scrH, "Arrakis inspection captures", nullptr, nullptr);
    if (!g_window) { glfwTerminate(); return 4; }
    glfwMakeContextCurrent(g_window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 5;
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    g_meshCube = createCube(); g_meshCylinder = createCylinder(24);
    g_meshSphere = createSphere(16, 24); g_meshWing = createWingBlade();
    g_meshTerrain = createDuneTerrain(260, 260, 900);
    g_meshRing = createRing(); g_meshRock = createErodedRock();
    initSandwormMeshes();
    g_progScene = buildProgram(SCENE_VERT, SCENE_FRAG);
    g_progShadow = buildProgram(SHADOW_VERT, SHADOW_FRAG);
    g_progSky = buildProgram(SKY_VERT, SKY_FRAG);
    setupShadowFramebuffer(); setupSkyVAO();
    anchor = glm::vec3(55, getDuneHeight(55, 55), 55);
    const std::filesystem::path out(argv[1]);
    try {
        g_wingPhase = 0.6f;
        auto aircraft = [&]() { floor(); drawMovieOrnithopter(glm::translate(glm::mat4(1), anchor + glm::vec3(0, 4, 0)), g_wingPhase, 0, 0, g_isBoosting); };
        capture(out / "aircraft.bmp", anchor + glm::vec3(13, 11, -20), anchor + glm::vec3(0, 4, 0), aircraft);
        capture(out / "aircraft_wire.bmp", anchor + glm::vec3(13, 11, -20), anchor + glm::vec3(0, 4, 0), aircraft, false, true);
        g_isBoosting = true;
        capture(out / "boost.bmp", anchor + glm::vec3(10, 8, 18), anchor + glm::vec3(0, 4, 0), aircraft);
        g_isBoosting = false;
        auto harvester = [&]() { floor(); drawSpiceHarvester(glm::translate(glm::mat4(1), anchor), 1.2f); };
        capture(out / "harvester.bmp", anchor + glm::vec3(-15, 11, -20), anchor + glm::vec3(0, 3, 0), harvester);
        capture(out / "tank.bmp", anchor + glm::vec3(6, 4.5f, -9), anchor + glm::vec3(0, 1.4f, 0), [&]() { floor(); drawMovieSpiceTank(glm::translate(glm::mat4(1), anchor)); });
        capture(out / "rocks.bmp", anchor + glm::vec3(12, 8, -15), anchor + glm::vec3(0, 3, 0), [&]() { floor(); drawDesertRockFormation(glm::translate(glm::mat4(1), anchor), 0); });
        clearRescueWorld();
        g_mission.base = anchor;
        capture(out / "base.bmp", anchor + glm::vec3(18, 18, -25), anchor, [&]() { floor(); drawRescueWorld(1); });
        clearRescueWorld();
        g_mission.crew[0].state = CrewState::Waiting; g_mission.crew[0].pos = anchor;
        capture(out / "worker.bmp", anchor + glm::vec3(3, 2.6f, 5), anchor + glm::vec3(0, 1.2f, 0), [&]() { floor(); drawRescueWorld(1); });
        g_mission.groups[0].released = true; g_mission.groups[0].center = anchor;
        g_mission.crew[0].pos = anchor + glm::vec3(1, 0, 0);
        capture(out / "beacon.bmp", anchor + glm::vec3(8, 7, 12), anchor + glm::vec3(0, 1, 0), [&]() { floor(); drawRescueWorld(1); });
        g_mission.groups[0].released = false;
        g_mission.crew[0].pos = anchor; g_mission.target = 0; g_mission.pickup = 0.4f;
        g_ornPos = anchor + glm::vec3(0, 8, 0);
        capture(out / "winch.bmp", anchor + glm::vec3(13, 9, 20), anchor + glm::vec3(0, 4, 0), [&]() { floor(); drawRescueWorld(1); drawMovieOrnithopter(glm::translate(glm::mat4(1), g_ornPos), g_wingPhase, 0, 0, false); });
        auto worm = [&]() { floor(); drawSandworm(anchor, 7, 157, 0, 1); };
        auto pose = arrakis_worm_detail::pose(anchor, 7, 157, 0, 1);
        auto axis = glm::vec3(0, std::cos(pose.tilt), std::sin(pose.tilt));
        capture(out / "mouth.bmp", pose.head + axis * 43.0f + glm::vec3(7, 0, 0), pose.head, worm);
        const float stages[] = {-30, 3, 9, 16};
        const char* names[] = {"worm_approach", "worm_rise", "worm_attack", "worm_retreat"};
        for (int i = 0; i < 4; ++i)
            capture(out / (std::string(names[i]) + ".bmp"), anchor + glm::vec3(65, 45, 90), anchor + glm::vec3(0, 17, 0), [&]() { floor(); drawSandworm(anchor, stages[i], 150 + stages[i], 0, 1); });
        resetMission(true);
        g_wingPhase = 0.6f;
        for (int mode = 0; mode < 3; ++mode) {
            glm::vec3 eye = g_ornPos + flightCameraOffset(mode);
            eye.y = std::max(eye.y, getDuneHeight(eye.x, eye.z) + 2);
            capture(out / ("camera_" + std::to_string(mode) + ".bmp"), eye, g_ornPos, [&]() { drawFullScene(0); }, true);
        }
        g_mission.elapsed = g_mission.breachAt + 14;
        auto crater = wormPosition();
        capture(out / "crater.bmp", crater + glm::vec3(25, 85, 100), crater,
                [&]() { drawFullScene(g_mission.elapsed); }, true, false, true);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; glfwDestroyWindow(g_window); glfwTerminate(); return 6;
    }
    // Process-local graphics resources are reclaimed with the inspection context.
    glfwDestroyWindow(g_window); glfwTerminate(); return 0;
}
