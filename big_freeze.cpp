#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "cosmology.h"

using namespace glm;
using namespace std;

static string readFile(const string& path) {
    ifstream f(path);
    if (!f) { cerr << "[ERR] cannot open " << path << "\n"; return ""; }
    stringstream ss; ss << f.rdbuf(); return ss.str();
}
static GLuint compileShader(GLenum type, const string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log); cerr << "[SHADER] " << log << "\n"; }
    return s;
}
static GLuint makeProgram(const string& vertPath, const string& fragPath) {
    GLuint v = compileShader(GL_VERTEX_SHADER, readFile(vertPath));
    GLuint f = compileShader(GL_FRAGMENT_SHADER, readFile(fragPath));
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log); cerr << "[LINK] " << log << "\n"; }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

struct Camera {
    vec3 target = vec3(0.0f);
    float radius = 60.0f, minRadius = 5.0f, maxRadius = 400.0f;
    float azimuth = 0.6f, elevation = float(M_PI) / 2.4f;
    float orbitSpeed = 0.008f, zoomSpeed = 4.0f;
    bool dragging = false;
    double lastX = 0, lastY = 0;

    vec3 position() const {
        float e = glm::clamp(elevation, 0.05f, float(M_PI) - 0.05f);
        return target + radius * vec3(sin(e)*cos(azimuth), cos(e), sin(e)*sin(azimuth));
    }
    mat4 view() const { return lookAt(position(), target, vec3(0,1,0)); }
    void onMouseMove(double x, double y) {
        if (dragging) {
            azimuth   += float(x - lastX) * orbitSpeed;
            elevation -= float(y - lastY) * orbitSpeed;
            elevation  = glm::clamp(elevation, 0.05f, float(M_PI) - 0.05f);
        }
        lastX = x; lastY = y;
    }
    void onScroll(double dy) { radius = glm::clamp(radius - float(dy)*zoomSpeed, minRadius, maxRadius); }
};

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 1100, HEIGHT = 720;
    bool smoke = false; // --smoke: init, render one frame, check GL error, exit

    Camera camera;
    GLuint pointProgram = 0, pointVAO = 0, pointVBO = 0;
    int galaxyCount = 0;

    bool init() {
        if (!glfwInit()) { cerr << "[ERR] glfwInit failed\n"; return false; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
        glfwWindowHint(GLFW_SAMPLES, 4);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Big Freeze", nullptr, nullptr);
        if (!window) { cerr << "[ERR] window creation failed\n"; glfwTerminate(); return false; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y){
            ((Engine*)glfwGetWindowUserPointer(w))->camera.onMouseMove(x, y); });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int b, int a, int){
            if (b == GLFW_MOUSE_BUTTON_LEFT) {
                Engine* e = (Engine*)glfwGetWindowUserPointer(w);
                e->camera.dragging = (a == GLFW_PRESS);
                glfwGetCursorPos(w, &e->camera.lastX, &e->camera.lastY);
            }});
        glfwSetScrollCallback(window, [](GLFWwindow* w, double, double dy){
            ((Engine*)glfwGetWindowUserPointer(w))->camera.onScroll(dy); });

        glewExperimental = GL_TRUE;
        GLenum gerr = glewInit();
        if (gerr != GLEW_OK) { cerr << "[ERR] glewInit: " << glewGetErrorString(gerr) << "\n"; return false; }
        glGetError(); // clear benign GL_INVALID_ENUM from GLEW on core profiles

        cout << "[INFO] GL " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << "\n";
        glViewport(0, 0, WIDTH, HEIGHT);
        glEnable(GL_DEPTH_TEST);

        pointProgram = makeProgram("shaders/points.vert", "shaders/points.frag");

        // temporary: random comoving cloud (replaced by Universe in Task 8)
        vector<float> pts;
        unsigned seed = 1234567u;
        auto rnd = [&seed](){ seed = seed*1664525u + 1013904223u; return (seed>>8)*(1.0f/16777216.0f); };
        for (int i = 0; i < 4000; ++i) {
            float r = 35.0f * cbrtf(rnd());
            float th = rnd()*6.2831853f, ph = acosf(2*rnd()-1);
            pts.push_back(r*sinf(ph)*cosf(th));
            pts.push_back(r*sinf(ph)*sinf(th));
            pts.push_back(r*cosf(ph));
        }
        galaxyCount = (int)pts.size()/3;
        glGenVertexArrays(1, &pointVAO);
        glGenBuffers(1, &pointVBO);
        glBindVertexArray(pointVAO);
        glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
        glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(float), pts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glEnable(GL_PROGRAM_POINT_SIZE);

        return true;
    }

    void render() {
        glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 proj = perspective(radians(50.0f), (float)WIDTH/HEIGHT, 0.1f, 2000.0f);
        mat4 view = camera.view();

        glUseProgram(pointProgram);
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uView"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
        glBindVertexArray(pointVAO);
        glDrawArrays(GL_POINTS, 0, galaxyCount);
        glBindVertexArray(0);
    }

    int run() {
        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);
            render();
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        return 0;
    }

    int runSmoke() {
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
        GLenum e = glGetError();
        if (e != GL_NO_ERROR) { cerr << "[SMOKE] GL error 0x" << std::hex << e << "\n"; return 1; }
        cout << "[SMOKE] ok\n";
        return 0;
    }

    void shutdown() { if (window) glfwDestroyWindow(window); glfwTerminate(); }
};

int main(int argc, char** argv) {
    Engine engine;
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--smoke") == 0) engine.smoke = true;

    if (!engine.init()) return 1;
    int rc = engine.smoke ? engine.runSmoke() : engine.run();
    engine.shutdown();
    return rc;
}
