#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstring>
#include "cosmology.h"

using namespace glm;
using namespace std;

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 1100, HEIGHT = 720;
    bool smoke = false; // --smoke: init, render one frame, check GL error, exit

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

        glewExperimental = GL_TRUE;
        GLenum gerr = glewInit();
        if (gerr != GLEW_OK) { cerr << "[ERR] glewInit: " << glewGetErrorString(gerr) << "\n"; return false; }
        glGetError(); // clear benign GL_INVALID_ENUM from GLEW on core profiles

        cout << "[INFO] GL " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << "\n";
        glViewport(0, 0, WIDTH, HEIGHT);
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void render() {
        glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
