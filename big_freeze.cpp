#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
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
#include <cstdio>
#include <algorithm>
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

// Per-galaxy vertex layout: pos(3) tForm(1) tauSF(1) mass(1) isBH(1) = 7 floats
struct Universe {
    vector<float> data;
    int count = 0;

    void generate(int nGalaxies = 6000, int nClusters = 40, float boxR = 35.0f) {
        unsigned seed = 99173u;
        auto rnd = [&seed](){ seed = seed*1664525u + 1013904223u; return (seed>>8)*(1.0f/16777216.0f); };
        // cluster seeds (cosmic web nodes)
        vector<vec3> nodes;
        for (int c = 0; c < nClusters; ++c) {
            float r = boxR * cbrtf(rnd());
            float th = rnd()*6.2831853f, ph = acosf(2*rnd()-1);
            nodes.push_back(r*vec3(sinf(ph)*cosf(th), sinf(ph)*sinf(th), cosf(ph)));
        }
        for (int i = 0; i < nGalaxies; ++i) {
            vec3 p;
            if (rnd() < 0.85f) {                  // 85% clustered around a node
                vec3 n = nodes[(int)(rnd()*nClusters) % nClusters];
                float spread = 4.0f * cbrtf(rnd());
                float th = rnd()*6.2831853f, ph = acosf(2*rnd()-1);
                p = n + spread*vec3(sinf(ph)*cosf(th), sinf(ph)*sinf(th), cosf(ph));
            } else {                               // 15% field galaxies
                float r = boxR * cbrtf(rnd());
                float th = rnd()*6.2831853f, ph = acosf(2*rnd()-1);
                p = r*vec3(sinf(ph)*cosf(th), sinf(ph)*sinf(th), cosf(ph));
            }
            float tForm = (0.2f + 1.0f*rnd()) * 1e9f;        // 0.2–1.2 Gyr
            float tauSF = (1.0f + 5.0f*rnd())  * 1e9f;        // 1–6 Gyr
            float mass  = 0.3f + 1.7f*rnd();
            float isBH  = (rnd() < 0.05f) ? 1.0f : 0.0f;      // 5% are black holes
            data.insert(data.end(), { p.x, p.y, p.z, tForm, tauSF, mass, isBH });
        }
        count = nGalaxies;
    }

    int litCount(double t_years) const {
        double tg = std::min(t_years, 1e15);
        int n = 0;
        for (int i = 0; i < count; ++i) {
            float isBH = data[i*7 + 6];
            if (isBH > 0.5f) continue;
            double tForm = data[i*7 + 3], tauSF = data[i*7 + 4];
            if (cosmo::galaxyLuminosity(tg, tForm, tauSF) > 0.02) ++n;
        }
        return n;
    }
};

static const char* eraCaption(cosmo::Era e) {
    switch (e) {
        case cosmo::Era::Stelliferous: return "Estrellas brillando; el universo aun es joven";
        case cosmo::Era::Degenerate:   return "Las estrellas murieron; quedan enanas y restos";
        case cosmo::Era::BlackHole:    return "Solo los agujeros negros emiten luz (Hawking)";
        case cosmo::Era::Dark:         return "Oscuridad y frio maximos; entropia maxima";
    }
    return "";
}

struct Milestone { double logT; const char* label; };
static const Milestone MILESTONES[] = {
    { 14.0,  "Se apaga la ultima estrella" },
    { 40.0,  "La materia se disuelve (decaimiento de protones)" },
    { 67.0,  "Se evaporan los agujeros negros estelares" },
    { 100.0, "Se evapora el ultimo agujero negro" },
};

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 1100, HEIGHT = 720;
    int fbWidth = WIDTH, fbHeight = HEIGHT; // framebuffer pixels (2× on Retina)
    bool smoke = false; // --smoke: init, render one frame, check GL error, exit

    Camera camera;
    GLuint pointProgram = 0, pointVAO = 0, pointVBO = 0;
    Universe universe;

    GLuint gridProgram = 0, gridVAO = 0, gridVBO = 0;
    int gridVertexCount = 0;
    bool showGrid = true;

    GLuint starProgram = 0, starVAO = 0, starVBO = 0;
    int starCount = 0;
    bool redshiftOn = true;

    GLuint hudProgram = 0, hudVAO = 0, hudVBO = 0, hudEBO = 0;

    // cosmic clock in log10(years)
    double logT = std::log10(cosmo::T0_YEARS); // start "today"
    double logTmin = 8.0, logTmax = 106.0;
    static constexpr double DEFAULT_SPEED = 0.6;
    double speed = DEFAULT_SPEED; // decades per second when playing
    bool   playing = false;
    bool   physicalMode = false; // P: physical expansion vs comoving
    double lastFrameTime = 0.0;

    // Bloom post-process
    GLuint sceneFBO = 0, sceneTex = 0, sceneDepth = 0;
    GLuint blurFBO[2] = {0,0}, blurTex[2] = {0,0};
    GLuint quadVAO = 0, quadVBO = 0;
    GLuint brightProgram = 0, blurProgram = 0, compositeProgram = 0;
    bool bloomOn = true;
    int bloomW = 0, bloomH = 0;

    void createBloomTargets(int w, int h) {
        if (w <= 0 || h <= 0) return;
        bloomW = w; bloomH = h;
        if (sceneTex)  glDeleteTextures(1, &sceneTex);
        if (sceneDepth)glDeleteRenderbuffers(1, &sceneDepth);
        if (blurTex[0])glDeleteTextures(2, blurTex);
        if (!sceneFBO) glGenFramebuffers(1, &sceneFBO);
        if (!blurFBO[0]) glGenFramebuffers(2, blurFBO);
        // scene color (HDR) + depth
        glGenTextures(1, &sceneTex);
        glBindTexture(GL_TEXTURE_2D, sceneTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenRenderbuffers(1, &sceneDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);
        // ping-pong blur targets (HDR, no depth)
        glGenTextures(2, blurTex);
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, blurTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex[i], 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

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

        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int){
            if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
            Engine* e = (Engine*)glfwGetWindowUserPointer(w);
            switch (key) {
                case GLFW_KEY_SPACE: if (action==GLFW_PRESS) e->playing = !e->playing; break;
                case GLFW_KEY_RIGHT: e->logT = std::min(e->logTmax, e->logT + 0.15); break;
                case GLFW_KEY_LEFT:  e->logT = std::max(e->logTmin, e->logT - 0.15); break;
                case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD:      e->speed *= 1.3; break;
                case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: e->speed /= 1.3; break;
                case GLFW_KEY_P: if (action==GLFW_PRESS) e->physicalMode = !e->physicalMode; break;
                case GLFW_KEY_G: if (action==GLFW_PRESS) e->showGrid = !e->showGrid; break;
                case GLFW_KEY_R: if (action==GLFW_PRESS) e->redshiftOn = !e->redshiftOn; break;
                case GLFW_KEY_B: if (action==GLFW_PRESS) e->bloomOn = !e->bloomOn; break;
                case GLFW_KEY_0: case GLFW_KEY_KP_0: case GLFW_KEY_HOME:
                    if (action==GLFW_PRESS) {                       // reset to a good viewing point ("today")
                        e->logT    = std::log10(cosmo::T0_YEARS);  // a = 1, z = 0, bright stelliferous era
                        e->speed   = Engine::DEFAULT_SPEED;
                        e->playing = false;
                        e->camera  = Camera();                     // recenter the orbit camera
                    }
                    break;
                case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, true); break;
            }});

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height){
            Engine* e = (Engine*)glfwGetWindowUserPointer(w);
            e->fbWidth = width; e->fbHeight = height;
            glViewport(0, 0, width, height);
            e->createBloomTargets(width, height);
        });
        glfwSetWindowSizeCallback(window, [](GLFWwindow* w, int width, int height){
            Engine* e = (Engine*)glfwGetWindowUserPointer(w);
            e->WIDTH = width; e->HEIGHT = height;
        });

        glewExperimental = GL_TRUE;
        GLenum gerr = glewInit();
        if (gerr != GLEW_OK) { cerr << "[ERR] glewInit: " << glewGetErrorString(gerr) << "\n"; return false; }
        glGetError(); // clear benign GL_INVALID_ENUM from GLEW on core profiles

        cout << "[INFO] GL " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << "\n";
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glEnable(GL_DEPTH_TEST);

        pointProgram = makeProgram("shaders/points.vert", "shaders/points.frag");

        universe.generate();
        glGenVertexArrays(1, &pointVAO);
        glGenBuffers(1, &pointVBO);
        glBindVertexArray(pointVAO);
        glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
        glBufferData(GL_ARRAY_BUFFER, universe.data.size()*sizeof(float), universe.data.data(), GL_STATIC_DRAW);
        const GLsizei stride = 7*sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                   // pos
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));   // tForm
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4*sizeof(float)));   // tauSF
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(5*sizeof(float)));   // mass
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));   // isBH
        for (int i = 0; i < 5; ++i) glEnableVertexAttribArray(i);
        glBindVertexArray(0);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
        glDepthMask(GL_FALSE);             // glow points don't occlude each other

        gridProgram = makeProgram("shaders/grid.vert", "shaders/grid.frag");
        {
            vector<float> g; const int N = 20; const float S = 35.0f, step = 2.0f*S/N;
            auto line = [&](vec3 a, vec3 b){ g.insert(g.end(),{a.x,a.y,a.z,b.x,b.y,b.z}); };
            for (int i = 0; i <= N; ++i)
                for (int j = 0; j <= N; ++j) {
                    float x = -S + i*step, y = -S + j*step;
                    line(vec3(x, y, -S), vec3(x, y, S)); // z-lines
                    line(vec3(x, -S, y), vec3(x, S, y)); // y-lines
                    line(vec3(-S, x, y), vec3(S, x, y)); // x-lines
                }
            gridVertexCount = (int)g.size()/3;
            glGenVertexArrays(1, &gridVAO); glGenBuffers(1, &gridVBO);
            glBindVertexArray(gridVAO);
            glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
            glBufferData(GL_ARRAY_BUFFER, g.size()*sizeof(float), g.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
        }

        starProgram = makeProgram("shaders/star.vert", "shaders/star.frag");
        {
            vector<float> s; unsigned seed = 7777u;
            auto rnd = [&seed](){ seed = seed*1664525u + 1013904223u; return (seed>>8)*(1.0f/16777216.0f); };
            for (int i = 0; i < 1500; ++i) {
                float th = rnd()*6.2831853f, ph = acosf(2*rnd()-1);
                s.insert(s.end(), { sinf(ph)*cosf(th), sinf(ph)*sinf(th), cosf(ph) });
            }
            starCount = (int)s.size()/3;
            glGenVertexArrays(1, &starVAO); glGenBuffers(1, &starVBO);
            glBindVertexArray(starVAO);
            glBindBuffer(GL_ARRAY_BUFFER, starVBO);
            glBufferData(GL_ARRAY_BUFFER, s.size()*sizeof(float), s.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
        }

        hudProgram = makeProgram("shaders/hud.vert", "shaders/hud.frag");
        glGenVertexArrays(1, &hudVAO);
        glGenBuffers(1, &hudVBO);
        glGenBuffers(1, &hudEBO);

        brightProgram    = makeProgram("shaders/fullscreen.vert", "shaders/bright.frag");
        blurProgram      = makeProgram("shaders/fullscreen.vert", "shaders/blur.frag");
        compositeProgram = makeProgram("shaders/fullscreen.vert", "shaders/composite.frag");
        {
            float quad[] = { -1,-1,  1,-1,  -1,1,  1,1 }; // triangle strip
            glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
        }
        createBloomTargets(fbWidth, fbHeight);

        return true;
    }

    void drawText(float x, float y, const char* s, vec3 color) {
        static char buf[200000];
        int quads = stb_easy_font_print(x, y, (char*)s, nullptr, buf, sizeof(buf));
        // stb vertex: float x,y,z; unsigned char c[4]  -> 16-byte stride; 4 verts per quad
        vector<float> verts; verts.reserve(quads*4*2);
        vector<unsigned int> idx; idx.reserve(quads*6);
        const int stride = 16;
        for (int q = 0; q < quads; ++q) {
            unsigned int base = (unsigned int)(verts.size()/2);
            for (int v = 0; v < 4; ++v) {
                float* fv = (float*)(buf + (q*4 + v)*stride);
                verts.push_back(fv[0]); verts.push_back(fv[1]);
            }
            idx.insert(idx.end(), { base+0, base+1, base+2, base+0, base+2, base+3 });
        }
        glUseProgram(hudProgram);
        glUniform2f(glGetUniformLocation(hudProgram, "uScreen"), (float)WIDTH, (float)HEIGHT);
        glUniform3f(glGetUniformLocation(hudProgram, "uColor"), color.r, color.g, color.b);
        glBindVertexArray(hudVAO);
        glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hudEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned int), idx.data(), GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void drawScene(const mat4& view, const mat4& proj, double t) {
        double now = glfwGetTime();
        {
            glUseProgram(starProgram);
            glUniformMatrix4fv(glGetUniformLocation(starProgram, "uView"), 1, GL_FALSE, value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(starProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
            glUniform1f(glGetUniformLocation(starProgram, "uFade"), 0.6f * (1.0f - (float)cosmo::reddening(t)));
            glBindVertexArray(starVAO);
            glDrawArrays(GL_POINTS, 0, starCount);
            glBindVertexArray(0);
        }

        if (showGrid) {
            glUseProgram(gridProgram);
            glUniformMatrix4fv(glGetUniformLocation(gridProgram, "uView"), 1, GL_FALSE, value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(gridProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
            glUniform1f(glGetUniformLocation(gridProgram, "uStretch"), (float)cosmo::visualStretch(t));
            glUniform1f(glGetUniformLocation(gridProgram, "uFade"), 1.0f - (float)cosmo::reddening(t)*0.7f);
            glBindVertexArray(gridVAO);
            glDrawArrays(GL_LINES, 0, gridVertexCount);
            glBindVertexArray(0);
        }

        glUseProgram(pointProgram);
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uView"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
        glUniform1f(glGetUniformLocation(pointProgram, "uTGalaxy"),  (float)std::min(t, 1e15));
        glUniform1f(glGetUniformLocation(pointProgram, "uReddening"),
                    redshiftOn ? (float)cosmo::reddening(t) : 0.0f);
        // physical vs comoving point spacing
        float spacing = physicalMode ? (float)cosmo::visualStretch(t) : 1.0f;
        glUniform1f(glGetUniformLocation(pointProgram, "uSpacing"), spacing);
        glUniform1f(glGetUniformLocation(pointProgram, "uLog10T"), (float)logT);
        glUniform1f(glGetUniformLocation(pointProgram, "uWallTime"), (float)now);
        glBindVertexArray(pointVAO);
        glDrawArrays(GL_POINTS, 0, universe.count);
        glBindVertexArray(0);
    }

    void render() {
        double now = glfwGetTime();
        double dt = (lastFrameTime == 0.0) ? 0.0 : (now - lastFrameTime);
        lastFrameTime = now;
        if (playing) {
            logT += speed * dt;
            if (logT >= logTmax) { logT = logTmax; playing = false; }
        }
        double t = std::pow(10.0, logT);

        mat4 proj = perspective(radians(50.0f), (float)fbWidth/fbHeight, 0.1f, 2000.0f);
        mat4 view = camera.view();

        if (bloomOn && sceneFBO) {
            // 1. scene -> HDR FBO
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
            glViewport(0, 0, bloomW, bloomH);
            glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawScene(view, proj, t);
            // 2. bright pass: sceneTex -> blurFBO[0]
            glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
            glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[0]);
            glViewport(0, 0, bloomW, bloomH);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(brightProgram);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);
            glUniform1i(glGetUniformLocation(brightProgram, "uScene"), 0);
            glUniform1f(glGetUniformLocation(brightProgram, "uThreshold"), 0.35f);
            glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            // 3. ping-pong gaussian blur (blurFBO[0] has bright; blur into [1],[0] alternately)
            glUseProgram(blurProgram);
            glUniform2f(glGetUniformLocation(blurProgram, "uTexel"), 1.0f/bloomW, 1.0f/bloomH);
            bool horizontal = true; int src = 0;
            const int passes = 10; // 5 full blurs
            for (int i = 0; i < passes; i++) {
                int dst = 1 - src;
                glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[dst]);
                glClear(GL_COLOR_BUFFER_BIT);
                glUniform1i(glGetUniformLocation(blurProgram, "uHorizontal"), horizontal ? 1 : 0);
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blurTex[src]);
                glUniform1i(glGetUniformLocation(blurProgram, "uTex"), 0);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                horizontal = !horizontal; src = dst;
            }
            // blurred bloom now in blurTex[src]
            // 4. composite scene + bloom -> default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(compositeProgram);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);
            glUniform1i(glGetUniformLocation(compositeProgram, "uScene"), 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, blurTex[src]);
            glUniform1i(glGetUniformLocation(compositeProgram, "uBloom"), 1);
            glUniform1f(glGetUniformLocation(compositeProgram, "uIntensity"), 1.1f);
            glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            // restore state for next frame's 3D scene + HUD
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
            glActiveTexture(GL_TEXTURE0);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawScene(view, proj, t);
        }

        // HUD overlay
        {
            char line[256];
            double a = cosmo::scaleFactor(t);
            double l10a = cosmo::log10ScaleFactor(t);
            double z = cosmo::redshift(t);
            double T = cosmo::cmbTemperature(t);
            const char* eraStr = cosmo::eraName(cosmo::era(t));
            vec3 col(0.7f, 0.85f, 1.0f);
            std::snprintf(line, sizeof(line), "t = 10^%.2f anos", logT);              drawText(14, 18, line, col);
            if (std::isfinite(a) && a < 1e6) std::snprintf(line, sizeof(line), "a = %.3g", a);
            else                              std::snprintf(line, sizeof(line), "a ~ 10^%.0f", l10a);
            drawText(14, 34, line, col);
            std::snprintf(line, sizeof(line), "z = %.3g", z);                          drawText(14, 50, line, col);
            std::snprintf(line, sizeof(line), "T_cmb = %.3g K", T);                     drawText(14, 66, line, col);
            std::snprintf(line, sizeof(line), "Galaxias: %d", universe.litCount(t));    drawText(14, 82, line, col);
            std::snprintf(line, sizeof(line), "Era: %s", eraStr);                       drawText(14, 98, line, vec3(1.0f,0.8f,0.4f));
            drawText(14, 114, eraCaption(cosmo::era(t)), vec3(0.55f, 0.65f, 0.8f));
            std::snprintf(line, sizeof(line), "Entropia: %3.0f%%", 100.0 * cosmo::entropyFraction(t));
            drawText(14, 130, line, col);
            for (const Milestone& m : MILESTONES) {
                double d = std::fabs(logT - m.logT);
                if (d < 1.0) {
                    float k = (float)(1.0 - d);                 // 1 at the event, 0 a decade away
                    float w = 6.0f * (float)std::strlen(m.label); // approx text width (px)
                    drawText((float)WIDTH * 0.5f - w * 0.5f, 44.0f, m.label, vec3(1.0f, 0.85f, 0.4f) * k);
                }
            }
            std::snprintf(line, sizeof(line), "[espacio] play  [<- ->] scrub  [+ -] vel  [0]reinicia  [G]rid [R]edshift [P]hys [B]loom");
            drawText(14, (float)HEIGHT-22, line, vec3(0.5f,0.55f,0.65f));
        }
    }

    int run() {
        while (!glfwWindowShouldClose(window)) {
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
