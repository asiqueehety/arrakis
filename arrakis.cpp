// ============================================================
//  ARRAKIS - Dune Object Showcase
//  Computer Graphics Project
//
//  Objects:
//    1. Spice Harvester
//    2. Ornithopter
//    3. Thumper
//    4. Spice Storage Tank
//    5. Desert Rock Formation
//
//  Controls:
//    0         - Show ALL objects (5-panel viewport)
//    1-5       - Show individual object
//    W         - Toggle wireframe / solid
//    Arrow Keys- Orbit camera left/right/up/down
//    ESC       - Quit
// ============================================================

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>

// ============================================================
// GLOBALS
// ============================================================

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

int g_showObject = 0; // 0=all, 1-5=individual
bool g_wireframe = false;

float g_camYaw = 30.0f;
float g_camPitch = 12.0f;
float g_camDist = 18.0f;

// ============================================================
// CALLBACKS
// ============================================================

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    if (key == GLFW_KEY_ESCAPE)
      glfwSetWindowShouldClose(window, true);

    if (key == GLFW_KEY_0)
      g_showObject = 0;
    if (key == GLFW_KEY_1)
      g_showObject = 1;
    if (key == GLFW_KEY_2)
      g_showObject = 2;
    if (key == GLFW_KEY_3)
      g_showObject = 3;
    if (key == GLFW_KEY_4)
      g_showObject = 4;
    if (key == GLFW_KEY_5)
      g_showObject = 5;

    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
      g_wireframe = !g_wireframe;
      glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);
    }

    const float step = 10.0f;
    if (key == GLFW_KEY_LEFT)
      g_camYaw -= step;
    if (key == GLFW_KEY_RIGHT)
      g_camYaw += step;
    if (key == GLFW_KEY_UP)
      g_camPitch = glm::clamp(g_camPitch + step, -89.0f, 89.0f);
    if (key == GLFW_KEY_DOWN)
      g_camPitch = glm::clamp(g_camPitch - step, -89.0f, 89.0f);
  }
}

// ============================================================
// SHADER HELPERS
// ============================================================

void checkShaderCompile(unsigned int shader, const char *name) {
  int success;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cout << "[ERROR] Shader '" << name << "':\n" << infoLog << std::endl;
  }
}

void checkProgramLink(unsigned int program) {
  int success;
  char infoLog[512];
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, 512, NULL, infoLog);
    std::cout << "[ERROR] Program link:\n" << infoLog << std::endl;
  }
}

// ============================================================
// DRAW ONE CUBE
//   root  - world-space offset matrix (identity for individual mode)
//   local - part-local transform (translate+rotate+scale)
//   color - RGB colour
// ============================================================

static unsigned int g_shaderProgram = 0;
static unsigned int g_VAO = 0;

void drawCube(const glm::mat4 &root, const glm::mat4 &local,
              const glm::vec3 &color) {
  glm::mat4 model = root * local;
  glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "model"), 1,
                     GL_FALSE, glm::value_ptr(model));
  glUniform3fv(glGetUniformLocation(g_shaderProgram, "objectColor"), 1,
               glm::value_ptr(color));
  glBindVertexArray(g_VAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
}

// ============================================================
// COLOUR PALETTE
// ============================================================

const glm::vec3 COL_SAND_DARK(0.42f, 0.28f, 0.12f);
const glm::vec3 COL_SAND_MID(0.55f, 0.38f, 0.18f);
const glm::vec3 COL_SAND_LIGHT(0.70f, 0.52f, 0.28f);
const glm::vec3 COL_SPICE_ORG(0.82f, 0.45f, 0.05f);
const glm::vec3 COL_SPICE_DEEP(0.65f, 0.28f, 0.04f);
const glm::vec3 COL_METAL_DARK(0.12f, 0.12f, 0.13f);
const glm::vec3 COL_METAL_MID(0.25f, 0.25f, 0.26f);
const glm::vec3 COL_METAL_LIGHT(0.40f, 0.40f, 0.42f);
const glm::vec3 COL_RUST(0.38f, 0.20f, 0.08f);
const glm::vec3 COL_GLASS(0.04f, 0.10f, 0.14f);
const glm::vec3 COL_ROCK_DARK(0.30f, 0.18f, 0.10f);
const glm::vec3 COL_ROCK_MID(0.45f, 0.28f, 0.15f);
const glm::vec3 COL_ROCK_LIGHT(0.60f, 0.40f, 0.22f);
const glm::vec3 COL_BLACK(0.04f, 0.04f, 0.04f);
const glm::vec3 COL_TREAD(0.09f, 0.09f, 0.09f);

// const glm::vec3 COL_SAND_DARK(0.f, 0.f, 0.f);
// const glm::vec3 COL_SAND_MID(0.f, 0.f, 0.f);
// const glm::vec3 COL_SAND_LIGHT(0.f, 0.f, 0.f);
// const glm::vec3 COL_SPICE_ORG(0.f, 0.f, 0.f);
// const glm::vec3 COL_SPICE_DEEP(0.f, 0.f, 0.f);
// const glm::vec3 COL_METAL_DARK(0.f, 0.f, 0.f);
// const glm::vec3 COL_METAL_MID(0.f, 0.f, 0.f);
// const glm::vec3 COL_METAL_LIGHT(0.f, 0.f, 0.f);
// const glm::vec3 COL_RUST(0.f, 0.f, 0.f);
// const glm::vec3 COL_GLASS(0.f, 0.f, 0.f);
// const glm::vec3 COL_ROCK_DARK(0.f, 0.f, 0.f);
// const glm::vec3 COL_ROCK_MID(0.f, 0.f, 0.f);
// const glm::vec3 COL_ROCK_LIGHT(0.f, 0.f, 0.f);
// const glm::vec3 COL_BLACK(0.f, 0.f, 0.f);
// const glm::vec3 COL_TREAD(0.f, 0.f, 0.f);

// ============================================================
// OBJECT 1 — SPICE HARVESTER
// ============================================================

void drawHarvester(const glm::mat4 &root) {
  // Chassis
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.55f, 0.f}),
                      {5.0f, 0.65f, 2.8f}),
           COL_SAND_DARK);

  // Left tread
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.28f, 1.75f}),
                      {5.4f, 0.55f, 0.55f}),
           COL_TREAD);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.58f, 1.75f}),
                      {5.4f, 0.12f, 0.42f}),
           COL_METAL_DARK);

  // Right tread
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.28f, -1.75f}),
                      {5.4f, 0.55f, 0.55f}),
           COL_TREAD);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.58f, -1.75f}),
                      {5.4f, 0.12f, 0.42f}),
           COL_METAL_DARK);

  // Main body / hopper
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.3f, 1.55f, 0.f}),
                      {3.8f, 1.40f, 2.2f}),
           COL_SAND_MID);

  // Upper deck
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.2f, 2.42f, 0.f}),
                      {2.8f, 0.30f, 2.0f}),
           COL_SAND_DARK);

  // Cockpit
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {1.35f, 2.18f, 0.f}),
                      {1.10f, 0.68f, 1.20f}),
           COL_GLASS);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {1.35f, 2.58f, 0.f}),
                      {1.14f, 0.12f, 1.24f}),
           COL_METAL_DARK);

  // Front harvesting jaw
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {2.55f, 0.65f, 0.f}),
                      {0.80f, 0.60f, 2.50f}),
           COL_METAL_MID);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {2.88f, 0.65f, 0.f}),
                      {0.18f, 0.45f, 2.20f}),
           COL_SPICE_DEEP);

  // Side intake scoops
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.8f, 1.0f, 1.25f}),
                      {1.80f, 0.45f, 0.28f}),
           COL_RUST);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.8f, 1.0f, -1.25f}),
                      {1.80f, 0.45f, 0.28f}),
           COL_RUST);

  // Rear processing block
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.20f, 1.80f, 0.f}),
                      {0.90f, 1.50f, 1.80f}),
           COL_METAL_MID);

  // Exhaust stack 1
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.30f, 3.20f, 0.50f}),
                      {0.25f, 1.10f, 0.25f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.30f, 3.82f, 0.50f}),
                      {0.38f, 0.14f, 0.38f}),
           COL_BLACK);

  // Exhaust stack 2
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.30f, 3.20f, -0.50f}),
                      {0.25f, 1.10f, 0.25f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.30f, 3.82f, -0.50f}),
                      {0.38f, 0.14f, 0.38f}),
           COL_BLACK);

  // Spice glow vent
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.5f, 2.58f, 0.f}),
                      {1.20f, 0.18f, 0.80f}),
           COL_SPICE_ORG);
}

// ============================================================
// OBJECT 2 — ORNITHOPTER
// ============================================================

void drawOrnithopter(const glm::mat4 &root) {
  // Fuselage
  drawCube(root, glm::scale(glm::mat4(1.f), {3.20f, 0.55f, 0.70f}),
           COL_SAND_MID);

  // Nose
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {1.90f, 0.f, 0.f}),
                      {0.60f, 0.40f, 0.55f}),
           COL_SAND_DARK);

  // Cockpit
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.70f, 0.42f, 0.f}),
                      {1.00f, 0.50f, 0.60f}),
           COL_GLASS);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.70f, 0.42f, 0.f}),
                      {1.04f, 0.54f, 0.14f}),
           COL_METAL_DARK);

  // Upper-front wing LEFT
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {0.20f, 0.20f, 2.20f});
    m = glm::rotate(m, glm::radians(-8.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {2.20f, 0.10f, 3.50f});
    drawCube(root, m, COL_SAND_LIGHT);
  }
  // Lower-rear wing LEFT
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {-0.40f, -0.15f, 1.90f});
    m = glm::rotate(m, glm::radians(6.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {1.60f, 0.08f, 2.80f});
    drawCube(root, m, COL_SAND_MID);
  }
  // Upper-front wing RIGHT
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {0.20f, 0.20f, -2.20f});
    m = glm::rotate(m, glm::radians(8.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {2.20f, 0.10f, 3.50f});
    drawCube(root, m, COL_SAND_LIGHT);
  }
  // Lower-rear wing RIGHT
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {-0.40f, -0.15f, -1.90f});
    m = glm::rotate(m, glm::radians(-6.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {1.60f, 0.08f, 2.80f});
    drawCube(root, m, COL_SAND_MID);
  }

  // Tail boom
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-2.20f, 0.10f, 0.f}),
                      {1.80f, 0.20f, 0.20f}),
           COL_METAL_DARK);

  // Horizontal stabiliser
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-3.05f, 0.14f, 0.f}),
                      {0.60f, 0.08f, 2.00f}),
           COL_SAND_DARK);

  // Vertical fin
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-3.00f, 0.45f, 0.f}),
                      {0.50f, 0.70f, 0.10f}),
           COL_SAND_DARK);

  // Engine pods
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, -0.40f, 0.60f}),
                      {1.20f, 0.28f, 0.28f}),
           COL_METAL_MID);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.65f, -0.40f, 0.60f}),
                      {0.14f, 0.34f, 0.34f}),
           COL_BLACK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, -0.40f, -0.60f}),
                      {1.20f, 0.28f, 0.28f}),
           COL_METAL_MID);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.65f, -0.40f, -0.60f}),
                      {0.14f, 0.34f, 0.34f}),
           COL_BLACK);
}

// ============================================================
// OBJECT 3 — THUMPER
// ============================================================

void drawThumper(const glm::mat4 &root) {
  // Ground anchor plate
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.10f, 0.f}),
                      {2.00f, 0.20f, 2.00f}),
           COL_METAL_DARK);

  // Four corner spikes
  float sx[] = {0.70f, 0.70f, -0.70f, -0.70f};
  float sz[] = {0.70f, -0.70f, 0.70f, -0.70f};
  for (int i = 0; i < 4; i++) {
    drawCube(root,
             glm::scale(glm::translate(glm::mat4(1.f), {sx[i], -0.25f, sz[i]}),
                        {0.14f, 0.60f, 0.14f}),
             COL_METAL_LIGHT);
  }

  // Main column
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 1.80f, 0.f}),
                      {0.42f, 3.40f, 0.42f}),
           COL_METAL_MID);

  // Mid vibration collar
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 1.50f, 0.f}),
                      {0.90f, 0.30f, 0.90f}),
           COL_RUST);

  // Upper vibration collar
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.80f, 0.f}),
                      {0.75f, 0.25f, 0.75f}),
           COL_RUST);

  // Piston head
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 3.60f, 0.f}),
                      {0.80f, 0.40f, 0.80f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 3.84f, 0.f}),
                      {0.55f, 0.14f, 0.55f}),
           COL_SPICE_DEEP);

  // Guy wire anchor pegs
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.22f, 0.90f}),
                      {0.18f, 0.20f, 0.18f}),
           COL_METAL_LIGHT);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 0.22f, -0.90f}),
                      {0.18f, 0.20f, 0.18f}),
           COL_METAL_LIGHT);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.90f, 0.22f, 0.f}),
                      {0.18f, 0.20f, 0.18f}),
           COL_METAL_LIGHT);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.90f, 0.22f, 0.f}),
                      {0.18f, 0.20f, 0.18f}),
           COL_METAL_LIGHT);
}

// ============================================================
// OBJECT 4 — SPICE STORAGE TANK
// ============================================================

void drawSpiceTank(const glm::mat4 &root) {
  // Four support legs
  float lx[] = {0.70f, 0.70f, -0.70f, -0.70f};
  float lz[] = {0.70f, -0.70f, 0.70f, -0.70f};
  for (int i = 0; i < 4; i++) {
    drawCube(root,
             glm::scale(glm::translate(glm::mat4(1.f), {lx[i], 0.50f, lz[i]}),
                        {0.20f, 1.00f, 0.20f}),
             COL_METAL_DARK);
  }

  // Cross braces
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.70f, 0.50f, 0.f}),
                      {0.10f, 0.12f, 1.40f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.70f, 0.50f, 0.f}),
                      {0.10f, 0.12f, 1.40f}),
           COL_METAL_DARK);

  // Main tank body
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.20f, 0.f}),
                      {1.70f, 3.00f, 1.70f}),
           COL_SAND_MID);

  // Weld seam bands
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 1.60f, 0.f}),
                      {1.74f, 0.10f, 1.74f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.80f, 0.f}),
                      {1.74f, 0.10f, 1.74f}),
           COL_METAL_DARK);

  // Domed top cap
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 3.82f, 0.f}),
                      {1.50f, 0.55f, 1.50f}),
           COL_SAND_DARK);

  // Fill hatch
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 4.14f, 0.f}),
                      {0.60f, 0.20f, 0.60f}),
           COL_SPICE_ORG);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 4.28f, 0.f}),
                      {0.20f, 0.10f, 0.60f}),
           COL_METAL_LIGHT);

  // Pressure gauges
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.40f, 0.92f}),
                      {0.30f, 0.40f, 0.14f}),
           COL_METAL_LIGHT);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.40f, 0.98f}),
                      {0.20f, 0.30f, 0.06f}),
           COL_GLASS);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.40f, -0.92f}),
                      {0.30f, 0.40f, 0.14f}),
           COL_METAL_LIGHT);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.f, 2.40f, -0.98f}),
                      {0.20f, 0.30f, 0.06f}),
           COL_GLASS);

  // Valve / pipe cluster
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.92f, 1.10f, 0.f}),
                      {0.14f, 0.55f, 0.80f}),
           COL_RUST);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {1.05f, 0.82f, 0.f}),
                      {0.40f, 0.14f, 0.14f}),
           COL_RUST);

  // Ladder rungs
  for (int r = 0; r < 5; r++) {
    float ry = 1.20f + r * 0.55f;
    drawCube(root,
             glm::scale(glm::translate(glm::mat4(1.f), {0.f, ry, 0.88f}),
                        {0.55f, 0.06f, 0.06f}),
             COL_METAL_LIGHT);
  }
  // Ladder rails
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {0.22f, 2.40f, 0.88f}),
                      {0.05f, 3.00f, 0.05f}),
           COL_METAL_DARK);
  drawCube(root,
           glm::scale(glm::translate(glm::mat4(1.f), {-0.22f, 2.40f, 0.88f}),
                      {0.05f, 3.00f, 0.05f}),
           COL_METAL_DARK);
}

// ============================================================
// OBJECT 5 — DESERT ROCK FORMATION
// ============================================================

void drawRock(const glm::mat4 &root) {
  // Boulder 1 — large base slab
  {
    glm::mat4 m =
        glm::rotate(glm::mat4(1.f), glm::radians(15.f), {0.f, 1.f, 0.f});
    m = glm::translate(m, {0.f, 0.65f, 0.f});
    m = glm::rotate(m, glm::radians(-6.f), {0.f, 0.f, 1.f});
    m = glm::scale(m, {3.20f, 1.30f, 2.10f});
    drawCube(root, m, COL_ROCK_MID);
  }
  // Boulder 2 — tall leaning slab
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {-0.60f, 1.60f, 0.30f});
    m = glm::rotate(m, glm::radians(22.f), {0.f, 1.f, 0.f});
    m = glm::rotate(m, glm::radians(-14.f), {0.f, 0.f, 1.f});
    m = glm::scale(m, {1.50f, 2.20f, 1.00f});
    drawCube(root, m, COL_ROCK_DARK);
  }
  // Boulder 3 — smaller rounded top
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {0.90f, 1.40f, -0.40f});
    m = glm::rotate(m, glm::radians(-18.f), {0.f, 1.f, 0.f});
    m = glm::rotate(m, glm::radians(10.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {1.20f, 1.10f, 1.30f});
    drawCube(root, m, COL_ROCK_LIGHT);
  }
  // Boulder 4 — low flat slab right
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {1.70f, 0.35f, 0.80f});
    m = glm::rotate(m, glm::radians(35.f), {0.f, 1.f, 0.f});
    m = glm::scale(m, {1.80f, 0.55f, 1.10f});
    drawCube(root, m, COL_ROCK_MID);
  }
  // Boulder 5 — angled shard
  {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {-1.30f, 0.70f, -0.80f});
    m = glm::rotate(m, glm::radians(-28.f), {0.f, 1.f, 0.f});
    m = glm::rotate(m, glm::radians(18.f), {1.f, 0.f, 0.f});
    m = glm::scale(m, {0.90f, 1.60f, 0.80f});
    drawCube(root, m, COL_ROCK_DARK);
  }
  // Pebble cluster
  float px[] = {2.20f, -2.10f, 1.80f, -1.60f};
  float pz[] = {-0.50f, 0.60f, 1.30f, -1.20f};
  float ps[] = {0.45f, 0.35f, 0.30f, 0.40f};
  for (int i = 0; i < 4; i++) {
    glm::mat4 m = glm::translate(glm::mat4(1.f), {px[i], 0.18f, pz[i]});
    m = glm::rotate(m, glm::radians(i * 30.f), {0.f, 1.f, 0.f});
    m = glm::scale(m, {ps[i] * 1.5f, ps[i] * 0.8f, ps[i] * 1.2f});
    drawCube(root, m, COL_ROCK_LIGHT);
  }
}

// ============================================================
// MAIN
// ============================================================

int main() {
  if (!glfwInit()) {
    std::cout << "[ERROR] Failed to initialise GLFW" << std::endl;
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                       "Arrakis Objects  |  0=All  1=Harvester  2=Ornithopter  "
                       "3=Thumper  4=Tank  5=Rock  W=Wire  Arrows=Orbit",
                       NULL, NULL);

  if (!window) {
    std::cout << "[ERROR] Failed to create window" << std::endl;
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, key_callback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "[ERROR] Failed to initialise GLAD" << std::endl;
    glfwTerminate();
    return -1;
  }

  std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "Keys: 0=All  1-5=Object  W=Wireframe  Arrows=Orbit  ESC=Quit"
            << std::endl;

  glEnable(GL_DEPTH_TEST);

  // --- Unit cube ---
  float vertices[] = {
      // Back
      -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
      -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
      // Front
      -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
      // Left
      -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
      // Right
      0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
      -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
      // Bottom
      -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f,
      0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f,
      // Top
      -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f};

  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  g_VAO = VAO;

  // --- Shaders ---
  const char *vertSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

  const char *fragSrc = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 objectColor;
        void main() {
            FragColor = vec4(objectColor, 1.0);
        }
    )";

  unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vert, 1, &vertSrc, NULL);
  glCompileShader(vert);
  checkShaderCompile(vert, "vertex");

  unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag, 1, &fragSrc, NULL);
  glCompileShader(frag);
  checkShaderCompile(frag, "fragment");

  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vert);
  glAttachShader(shaderProgram, frag);
  glLinkProgram(shaderProgram);
  checkProgramLink(shaderProgram);
  glDeleteShader(vert);
  glDeleteShader(frag);

  g_shaderProgram = shaderProgram;

  // World-space roots for ALL mode panels
  struct PanelInfo {
    int x, y, w, h;
    void (*drawFn)(const glm::mat4 &);
    glm::vec3 target;
    float dist;
  };

  int W = SCR_WIDTH;
  int H = SCR_HEIGHT;

  PanelInfo panels[] = {
      {0, 0, W / 2, H, drawHarvester, {0.f, 1.8f, 0.f}, 14.f},
      {W / 2, H / 2, W / 4, H / 2, drawOrnithopter, {0.f, 0.3f, 0.f}, 13.f},
      {W / 2, 0, W / 8, H / 2, drawThumper, {0.f, 2.f, 0.f}, 8.f},
      {W * 5 / 8, 0, W / 8, H / 2, drawSpiceTank, {0.f, 2.2f, 0.f}, 9.f},
      {W * 3 / 4, 0, W / 4, H, drawRock, {0.f, 1.f, 0.f}, 9.f},
  };

  // Camera targets and distances for individual mode
  // At FOV=60 degrees, half-angle=30 deg, tan(30)=0.577
  // visible half-width at dist d = d*0.577*(aspect). Distances tuned
  // so each object fills ~80% of screen without any part being clipped.
  glm::vec3 indivTarget[] = {
      {0.f, 1.8f, 0.f}, // 1 Harvester  (5.4 wide, 3.9 tall)
      {0.f, 0.3f, 0.f}, // 2 Ornithopter(wings +-3.95 on Z)
      {0.f, 2.f, 0.f},  // 3 Thumper    (0.9 wide, 3.9 tall)
      {0.f, 2.2f, 0.f}, // 4 SpiceTank  (1.7 wide, 4.4 tall)
      {0.f, 1.f, 0.f},  // 5 Rock       (~3.5 wide spread)
  };
  float indivDist[] = {13.f, 10.f, 7.f, 8.f, 9.f};

  glm::mat4 identityRoot = glm::mat4(1.0f);

  // ---- RENDER LOOP ----
  while (!glfwWindowShouldClose(window)) {
    glClearColor(0.06f, 0.05f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);

    float yR = glm::radians(g_camYaw);
    float pR = glm::radians(g_camPitch);

    if (g_showObject == 0) {
      // ALL MODE — 5 viewport panels
      for (auto &p : panels) {
        glViewport(p.x, p.y, p.w, p.h);
        glScissor(p.x, p.y, p.w, p.h);
        glEnable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(
            glm::radians(60.0f), (float)p.w / (float)p.h, 0.1f, 200.0f);
        glm::vec3 cp = p.target + glm::vec3(p.dist * cosf(pR) * sinf(yR),
                                            p.dist * sinf(pR),
                                            p.dist * cosf(pR) * cosf(yR));
        glm::mat4 view = glm::lookAt(cp, p.target, {0.f, 1.f, 0.f});

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(proj));

        p.drawFn(identityRoot);
      }
      glDisable(GL_SCISSOR_TEST);
      glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    } else {
      int idx = g_showObject - 1;
      glm::vec3 tgt = indivTarget[idx];
      float dist = indivDist[idx];
      glm::vec3 cp =
          tgt + glm::vec3(dist * cosf(pR) * sinf(yR), dist * sinf(pR),
                          dist * cosf(pR) * cosf(yR));
      glm::mat4 view = glm::lookAt(cp, tgt, {0.f, 1.f, 0.f});
      glm::mat4 proj =
          glm::perspective(glm::radians(60.0f),
                           (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1,
                         GL_FALSE, glm::value_ptr(view));
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1,
                         GL_FALSE, glm::value_ptr(proj));

      switch (g_showObject) {
      case 1:
        drawHarvester(identityRoot);
        break;
      case 2:
        drawOrnithopter(identityRoot);
        break;
      case 3:
        drawThumper(identityRoot);
        break;
      case 4:
        drawSpiceTank(identityRoot);
        break;
      case 5:
        drawRock(identityRoot);
        break;
      }
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
  glfwTerminate();
  return 0;
}
