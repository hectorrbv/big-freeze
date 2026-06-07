# Big Freeze Simulation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an interactive real-time C++/OpenGL visualization of the Big Freeze (ΛCDM heat death of the universe), in the code style of kavan010/black_hole, that runs natively on macOS.

**Architecture:** A single `big_freeze.cpp` (structs `Camera`, `Universe`, `Engine` + `main`) renders a comoving point cloud of galaxies. Cosmology is analytic (FLRW closed form) so scrubbing the cosmic clock (10⁸→10¹⁰⁰ yr) is instant. Pure physics lives in header-only `cosmology.h` and is unit-tested. Rendering uses OpenGL 3.3 core (forward-compat for macOS): point sprites with additive glow, a stretching spacetime grid, a faint star background, and an on-screen HUD.

**Tech Stack:** C++17, OpenGL 3.3 core, GLFW, GLEW, GLM, CMake (+vcpkg and Homebrew), stb_easy_font (vendored, HUD text).

---

## File Structure

```
big_freeze/
├── CMakeLists.txt          # targets: BigFreeze (app) + test_cosmology (CTest)
├── vcpkg.json              # glfw3, glew, glm
├── README.md               # build with vcpkg AND Homebrew (macOS)
├── cosmology.h             # pure physics, header-only, no OpenGL  → unit-tested
├── stb_easy_font.h         # vendored single-header font (HUD text)
├── big_freeze.cpp          # app: Camera, Universe, Engine, main()
├── shaders/
│   ├── points.vert / points.frag   # galaxy point sprites + additive glow
│   ├── grid.vert   / grid.frag     # spacetime grid (scales with a)
│   ├── star.vert   / star.frag     # faint background stars
│   └── hud.vert    / hud.frag      # 2D text overlay
└── tests/
    └── test_cosmology.cpp  # unit tests for the physics model
```

Responsibilities:
- `cosmology.h` — all pure math (scale factor, redshift, CMB temp, era, blackbody color, galaxy lifecycle). No GL, no globals. Testable in isolation.
- `big_freeze.cpp` — windowing, GL resources, input, the per-frame loop, HUD. Reads `cosmology.h`.
- shaders — GPU side. The galaxy lifecycle math in `points.vert` MUST mirror `cosmology.h` (documented inline).

**Key numeric rule (read before coding):** at t≈10¹⁰⁰ yr the scale factor a≈10^(10^89) — it overflows `double` (and `float`). Therefore: compute everything from **`logScaleFactor` (natural log, stays finite in `double`)**; derive `redshift`/`cmbTemperature` via `exp(-logA)` (which underflows to 0 gracefully, never overflows); and send the shader only **bounded** uniforms (`uLog10T`, clamped `uTGalaxy`, capped `uExpansion`, `uReddening∈[0,1]`). Never send raw `t` or raw `a` to a shader.

---

## Task 1: Build skeleton + scale factor (TDD)

**Files:**
- Create: `CMakeLists.txt`
- Create: `vcpkg.json`
- Create: `cosmology.h`
- Test: `tests/test_cosmology.cpp`

- [ ] **Step 1: Create `vcpkg.json`**

```json
{
    "dependencies": [
        "glfw3",
        "glm",
        "glew"
    ]
}
```

- [ ] **Step 2: Create `CMakeLists.txt` (test target only for now)**

```cmake
cmake_minimum_required(VERSION 3.21)
project(BigFreeze CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# --- Unit tests for the pure physics model (no OpenGL) ---
add_executable(test_cosmology tests/test_cosmology.cpp)
target_include_directories(test_cosmology PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME cosmology COMMAND test_cosmology)
```

- [ ] **Step 3: Write the failing test** — `tests/test_cosmology.cpp`

```cpp
#include <cstdio>
#include <cmath>
#include "cosmology.h"

static int g_fail = 0;
#define CHECK(cond) do{ if(!(cond)){ std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_fail; } }while(0)
#define CHECK_NEAR(a,b,tol) do{ double _d=std::fabs((double)(a)-(double)(b)); \
    if(_d>(tol)){ std::printf("FAIL: |%s - %s| = %g > %g (line %d)\n", #a,#b,_d,(double)(tol),__LINE__); ++g_fail; } }while(0)

int main(){
    using namespace cosmo;
    // a(t0) normalized to 1, logA(t0)=0
    CHECK_NEAR(scaleFactor(T0_YEARS), 1.0, 1e-9);
    CHECK_NEAR(logScaleFactor(T0_YEARS), 0.0, 1e-9);
    // monotonic increasing
    CHECK(scaleFactor(1e9) < scaleFactor(5e9));
    CHECK(scaleFactor(5e9) < scaleFactor(T0_YEARS));
    CHECK(scaleFactor(T0_YEARS) < scaleFactor(5e10));
    // past < 1 < future
    CHECK(scaleFactor(1e9) < 1.0);
    CHECK(scaleFactor(5e10) > 1.0);
    // extreme time stays finite in log space (no overflow/NaN)
    CHECK(std::isfinite(logScaleFactor(1e100)));
    CHECK(logScaleFactor(1e100) > 1e80);

    if(g_fail){ std::printf("%d checks failed\n", g_fail); return 1; }
    std::printf("all cosmology tests passed\n");
    return 0;
}
```

- [ ] **Step 4: Run the test — verify it FAILS to compile**

```bash
cmake -B build -S . && cmake --build build 2>&1 | tail -20
```
Expected: compile error — `cosmology.h` not found / `cosmo` undefined.

- [ ] **Step 5: Create `cosmology.h` with constants + scale factor**

```cpp
#pragma once
#include <cmath>
#include <algorithm>

namespace cosmo {

constexpr double C            = 299792458.0;        // m/s
constexpr double G            = 6.67430e-11;        // m^3 kg^-1 s^-2
constexpr double SEC_PER_YEAR = 3.15576e7;          // s/yr
constexpr double MPC_IN_KM    = 3.0856775814913673e19;
constexpr double H0_KM_S_MPC  = 70.0;
constexpr double H0           = (H0_KM_S_MPC / MPC_IN_KM) * SEC_PER_YEAR; // 1/yr
constexpr double OMEGA_M      = 0.315;
constexpr double OMEGA_L      = 0.685;
constexpr double T_CMB0       = 2.725;              // K
constexpr double T0_YEARS     = 13.8e9;            // present age (a normalized to 1 here)
constexpr double LN10         = 2.302585092994046;

// ln(sinh(x)) computed stably for large x (avoids exp overflow)
inline double lnSinh(double x){
    if (x > 20.0) return x - 0.6931471805599453; // ln(0.5 e^x) = x - ln2
    return std::log(std::sinh(x));
}

// ln of the raw (un-normalized) FLRW flat-ΛCDM scale factor
inline double logScaleFactorRaw(double t_years){
    const double x = 1.5 * std::sqrt(OMEGA_L) * H0 * t_years;
    return (1.0/3.0)*std::log(OMEGA_M/OMEGA_L) + (2.0/3.0)*lnSinh(x);
}

// ln(a), normalized so a(T0_YEARS) = 1
inline double logScaleFactor(double t_years){
    return logScaleFactorRaw(t_years) - logScaleFactorRaw(T0_YEARS);
}

// a(t). May be +inf for astronomically large t — use logScaleFactor for those.
inline double scaleFactor(double t_years){ return std::exp(logScaleFactor(t_years)); }
inline double log10ScaleFactor(double t_years){ return logScaleFactor(t_years)/LN10; }

} // namespace cosmo
```

- [ ] **Step 6: Run the test — verify it PASSES**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: `all cosmology tests passed`, ctest reports `1/1 Passed`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt vcpkg.json cosmology.h tests/test_cosmology.cpp
git commit -m "feat: scale factor a(t) with TDD + build skeleton"
```

---

## Task 2: Redshift + CMB temperature (TDD)

**Files:**
- Modify: `cosmology.h`
- Test: `tests/test_cosmology.cpp`

- [ ] **Step 1: Add failing tests** — insert before the `if(g_fail)` block in `tests/test_cosmology.cpp`:

```cpp
    // redshift: z(t0)=0; past (a<1) -> z>0
    CHECK_NEAR(redshift(T0_YEARS), 0.0, 1e-9);
    CHECK(redshift(2e9) > 0.0);
    // CMB temperature: T(t0)=T_CMB0; cools toward 0 in the far future (no overflow/NaN)
    CHECK_NEAR(cmbTemperature(T0_YEARS), T_CMB0, 1e-6);
    CHECK(cmbTemperature(1e11) < T_CMB0);
    CHECK(std::isfinite(cmbTemperature(1e100)));
    CHECK_NEAR(cmbTemperature(1e100), 0.0, 1e-9);
```

- [ ] **Step 2: Run — verify FAIL to compile** (`redshift`/`cmbTemperature` undefined)

```bash
cmake --build build 2>&1 | tail -20
```
Expected: error: `redshift`/`cmbTemperature` not members of `cosmo`.

- [ ] **Step 3: Implement in `cosmology.h`** — add after `log10ScaleFactor`:

```cpp
// Derived from exp(-logA): underflows to 0 in the far future, never overflows.
inline double redshift(double t_years){ return std::exp(-logScaleFactor(t_years)) - 1.0; }
inline double cmbTemperature(double t_years){ return T_CMB0 * std::exp(-logScaleFactor(t_years)); }
```

- [ ] **Step 4: Run — verify PASS**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: `all cosmology tests passed`.

- [ ] **Step 5: Commit**

```bash
git add cosmology.h tests/test_cosmology.cpp
git commit -m "feat: redshift and CMB temperature (overflow-safe)"
```

---

## Task 3: Cosmic eras (TDD)

**Files:**
- Modify: `cosmology.h`
- Test: `tests/test_cosmology.cpp`

- [ ] **Step 1: Add failing tests** — insert before the `if(g_fail)` block:

```cpp
    CHECK(era(T0_YEARS)  == Era::Stelliferous);
    CHECK(era(1e13)      == Era::Stelliferous);
    CHECK(era(1e20)      == Era::Degenerate);
    CHECK(era(1e60)      == Era::BlackHole);
    CHECK(era(1e110)     == Era::Dark);
    CHECK(std::string(eraName(Era::BlackHole)) == "Agujeros negros");
```
Add `#include <string>` near the top of the test file.

- [ ] **Step 2: Run — verify FAIL to compile** (`Era`/`era`/`eraName` undefined)

- [ ] **Step 3: Implement in `cosmology.h`** — add:

```cpp
enum class Era { Stelliferous, Degenerate, BlackHole, Dark };
inline Era era(double t_years){
    if (t_years < 1e14)  return Era::Stelliferous;
    if (t_years < 1e40)  return Era::Degenerate;
    if (t_years < 1e100) return Era::BlackHole;
    return Era::Dark;
}
inline const char* eraName(Era e){
    switch(e){
        case Era::Stelliferous: return "Estelifera";
        case Era::Degenerate:   return "Degenerada";
        case Era::BlackHole:    return "Agujeros negros";
        case Era::Dark:         return "Oscura";
    }
    return "?";
}
```

- [ ] **Step 4: Run — verify PASS**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add cosmology.h tests/test_cosmology.cpp
git commit -m "feat: cosmic era classification"
```

---

## Task 4: Blackbody color (TDD)

**Files:**
- Modify: `cosmology.h`
- Test: `tests/test_cosmology.cpp`

- [ ] **Step 1: Add failing tests** — insert before the `if(g_fail)` block:

```cpp
    {
        RGB hot  = blackbodyRGB(40000.0); // very hot -> blue dominates
        RGB cold = blackbodyRGB(1000.0);  // cool -> red dominates
        CHECK(hot.b  > hot.r);
        CHECK(cold.r > cold.b);
        CHECK(cold.r > 0.9f);             // near-saturated red
        // channels stay in [0,1]
        CHECK(hot.r >= 0.0f && hot.r <= 1.0f);
        CHECK(hot.b >= 0.0f && hot.b <= 1.0f);
    }
```

- [ ] **Step 2: Run — verify FAIL to compile** (`RGB`/`blackbodyRGB` undefined)

- [ ] **Step 3: Implement in `cosmology.h`** — add:

```cpp
struct RGB { float r, g, b; };

// Approximate Planckian-locus color (Tanner Helland approximation). tempK in Kelvin.
inline RGB blackbodyRGB(double tempK){
    double t = std::clamp(tempK, 1000.0, 40000.0) / 100.0;
    double r, g, b;
    if (t <= 66.0) r = 255.0;
    else           r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
    if (t <= 66.0) g = 99.4708025861 * std::log(t) - 161.1195681661;
    else           g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
    if (t >= 66.0)      b = 255.0;
    else if (t <= 19.0) b = 0.0;
    else                b = 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
    auto cl = [](double v){ return (float)(std::clamp(v, 0.0, 255.0) / 255.0); };
    return { cl(r), cl(g), cl(b) };
}
```

- [ ] **Step 4: Run — verify PASS**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add cosmology.h tests/test_cosmology.cpp
git commit -m "feat: blackbody (Planckian) color approximation"
```

---

## Task 5: Galaxy lifecycle + render mappings (TDD)

**Files:**
- Modify: `cosmology.h`
- Test: `tests/test_cosmology.cpp`

- [ ] **Step 1: Add failing tests** — insert before the `if(g_fail)` block:

```cpp
    {
        double tForm = 5e8, tau = 3e9;
        // not yet formed
        CHECK_NEAR(galaxyLuminosity(1e8, tForm, tau), 0.0, 1e-9);
        // bright while young, dimmer when old (within stelliferous era)
        CHECK(galaxyLuminosity(1e9, tForm, tau) > galaxyLuminosity(1e12, tForm, tau));
        // effectively dark after the stelliferous era
        CHECK(galaxyLuminosity(2e14, tForm, tau) < 1e-6);
        // bluer (hotter) when young than when old
        CHECK(galaxyTempK(1e9, tForm, tau) > galaxyTempK(8e9, tForm, tau));
    }
    {
        // bounded render mappings
        CHECK(visualStretch(T0_YEARS) >= 1.0);
        CHECK(std::isfinite(visualStretch(1e100)));
        CHECK(reddening(T0_YEARS) >= 0.0 && reddening(T0_YEARS) <= 1.0);
        CHECK_NEAR(reddening(1e100), 1.0, 1e-9);
    }
```

- [ ] **Step 2: Run — verify FAIL to compile**

- [ ] **Step 3: Implement in `cosmology.h`** — add:

```cpp
// --- Stellar lifecycle (CPU mirror of points.vert; keep both in sync) ---
inline double galaxyLuminosity(double t_years, double tForm, double tauSF){
    if (t_years < tForm) return 0.0;
    double age    = t_years - tForm;
    double burst  = std::exp(-age / tauSF);                                  // formation burst fades
    double cutoff = std::clamp(1.0 - (t_years - 1e13)/(1e14 - 1e13), 0.0, 1.0); // dies by ~1e14 yr
    return std::clamp(0.15 + 0.85*burst, 0.0, 1.0) * cutoff;
}
inline double galaxyTempK(double t_years, double tForm, double tauSF){
    double age  = std::max(0.0, t_years - tForm);
    double frac = std::clamp(age / (5.0*tauSF), 0.0, 1.0);
    return 9000.0*(1.0 - frac) + 3000.0*frac; // blue-white -> red as massive stars die
}

// --- Bounded mappings for the renderer (HUD shows true magnitudes) ---
constexpr double GRID_CAP_DECADES = 3.0;
inline double visualStretch(double t_years){
    double d = std::clamp(log10ScaleFactor(t_years), -1.0, GRID_CAP_DECADES);
    return std::pow(10.0, d * 0.25);
}
inline double reddening(double t_years){
    return std::clamp(log10ScaleFactor(t_years) / GRID_CAP_DECADES, 0.0, 1.0);
}
```

- [ ] **Step 4: Run — verify PASS**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add cosmology.h tests/test_cosmology.cpp
git commit -m "feat: galaxy lifecycle + bounded render mappings"
```

The physics model is now complete and fully tested. Remaining tasks are the OpenGL app (verified by build + a `--smoke` gate + a manual visual checklist).

---

## Task 6: GL window + context (macOS-safe) + smoke gate

**Files:**
- Create: `big_freeze.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the app target to `CMakeLists.txt`** — append:

```cmake
# --- Application (OpenGL) ---
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(BigFreeze big_freeze.cpp)
target_include_directories(BigFreeze PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BigFreeze PRIVATE glfw GLEW::GLEW glm::glm OpenGL::GL)

# Copy shaders next to the executable
file(GLOB SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
                  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
add_custom_command(TARGET BigFreeze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BigFreeze>/shaders)
foreach(shader ${SHADERS})
    add_custom_command(TARGET BigFreeze POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${shader}
        $<TARGET_FILE_DIR:BigFreeze>/shaders)
endforeach()
```

- [ ] **Step 2: Create `big_freeze.cpp` (window + context + loop + smoke)**

```cpp
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
```

- [ ] **Step 3: Build**

```bash
cmake -B build -S . && cmake --build build 2>&1 | tail -20
```
Expected: builds `BigFreeze` and `test_cosmology` with no errors.

- [ ] **Step 4: Smoke test (run in a graphical session)**

```bash
./build/BigFreeze --smoke
```
Expected: prints `[INFO] GL 3.3 ...` then `[SMOKE] ok`, exit code 0.

- [ ] **Step 5: Manual visual check** — run `./build/BigFreeze`; expect a dark blue-black window; Esc closes it.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt big_freeze.cpp
git commit -m "feat: GL 3.3 core window (macOS forward-compat) + smoke gate"
```

---

## Task 7: Shader helpers + orbit camera + static galaxy cloud

**Files:**
- Modify: `big_freeze.cpp`
- Create: `shaders/points.vert`, `shaders/points.frag`

This task draws a fixed white point cloud you can orbit — proves the camera, VAO, and shader pipeline.

- [ ] **Step 1: Create `shaders/points.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3 aPos;     // comoving position
uniform mat4 uView;
uniform mat4 uProj;
void main() {
    gl_Position = uProj * uView * vec4(aPos, 1.0);
    gl_PointSize = 3.0;
}
```

- [ ] **Step 2: Create `shaders/points.frag`**

```glsl
#version 330 core
out vec4 FragColor;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    if (dot(d, d) > 0.25) discard;     // round point
    FragColor = vec4(1.0);
}
```

- [ ] **Step 3: Add includes + helpers + Camera to `big_freeze.cpp`** — after the existing includes add:

```cpp
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
```

Add these free functions above `struct Engine`:

```cpp
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
```

- [ ] **Step 4: Extend `Engine`** — add members (top of struct):

```cpp
    Camera camera;
    GLuint pointProgram = 0, pointVAO = 0, pointVBO = 0;
    int galaxyCount = 0;
```

Add a temporary cloud builder + GL setup. Inside `init()`, before `return true;`:

```cpp
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
```

Add input callbacks. After `glfwMakeContextCurrent(window);` in `init()`:

```cpp
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
```

- [ ] **Step 5: Replace `render()`** with:

```cpp
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
```

- [ ] **Step 6: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok` (shaders compiled, no GL errors).

- [ ] **Step 7: Manual visual check** — run `./build/BigFreeze`: a spherical cloud of ~4000 white dots; left-drag orbits, scroll zooms.

- [ ] **Step 8: Commit**

```bash
git add big_freeze.cpp shaders/points.vert shaders/points.frag
git commit -m "feat: shader helpers, orbit camera, static galaxy cloud"
```

---

## Task 8: Universe struct — clustered comoving cloud + per-galaxy attributes

**Files:**
- Modify: `big_freeze.cpp`

Replaces the temporary cloud with a real `Universe`: clustered positions (cosmic web) + per-galaxy `tForm`, `tauSF`, `mass`, `isBH`.

- [ ] **Step 1: Add `struct Universe` above `struct Engine`**

```cpp
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
};
```

- [ ] **Step 2: In `Engine`** replace the `int galaxyCount = 0;` member with:

```cpp
    Universe universe;
```

- [ ] **Step 3: In `init()`** replace the entire temporary-cloud block (from `// temporary: random comoving cloud` through the `glEnable(GL_PROGRAM_POINT_SIZE);` line) with:

```cpp
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
```

- [ ] **Step 4: In `render()`** change the draw count from `galaxyCount` to `universe.count`.

- [ ] **Step 5: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 6: Manual visual check** — run it: the cloud now shows clumps/filaments (cosmic web), not a uniform ball.

- [ ] **Step 7: Commit**

```bash
git add big_freeze.cpp
git commit -m "feat: Universe with clustered cosmic web + per-galaxy attributes"
```

---

## Task 9: Glow point sprites + blackbody color (additive blending)

**Files:**
- Modify: `shaders/points.vert`, `shaders/points.frag`, `big_freeze.cpp`

Galaxies now glow with blackbody color and brightness from a (still fixed) cosmic time. The GLSL lifecycle MUST mirror `cosmology.h`.

- [ ] **Step 1: Replace `shaders/points.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aTForm;
layout(location = 2) in float aTauSF;
layout(location = 3) in float aMass;
layout(location = 4) in float aIsBH;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uTGalaxy;   // min(t, 1e15) yr  (stellar lifecycle)
uniform float uReddening; // [0,1] cosmological reddening

out vec3  vColor;
out float vAlpha;

// mirror of cosmo::galaxyLuminosity
float luminosity(float t, float tForm, float tauSF) {
    if (t < tForm) return 0.0;
    float burst  = exp(-(t - tForm) / tauSF);
    float cutoff = clamp(1.0 - (t - 1e13) / (1e14 - 1e13), 0.0, 1.0);
    return clamp(0.15 + 0.85*burst, 0.0, 1.0) * cutoff;
}
// mirror of cosmo::galaxyTempK
float tempK(float t, float tForm, float tauSF) {
    float age  = max(0.0, t - tForm);
    float frac = clamp(age / (5.0*tauSF), 0.0, 1.0);
    return 9000.0*(1.0 - frac) + 3000.0*frac;
}
// compact blackbody (matches cosmo::blackbodyRGB closely enough for display)
vec3 blackbody(float k) {
    float t = clamp(k, 1000.0, 40000.0) / 100.0;
    float r = (t <= 66.0) ? 255.0 : 329.698727446 * pow(t-60.0, -0.1332047592);
    float g = (t <= 66.0) ? (99.4708025861*log(t) - 161.1195681661)
                          : (288.1221695283 * pow(t-60.0, -0.0755148492));
    float b = (t >= 66.0) ? 255.0 : (t <= 19.0 ? 0.0 : 138.5177312231*log(t-10.0) - 305.0447927307);
    return clamp(vec3(r,g,b)/255.0, 0.0, 1.0);
}

void main() {
    gl_Position = uProj * uView * vec4(aPos, 1.0);

    float L = luminosity(uTGalaxy, aTForm, aTauSF);
    vec3  c = blackbody(tempK(uTGalaxy, aTForm, aTauSF));
    c = mix(c, vec3(0.55, 0.12, 0.05), uReddening); // shift toward deep red as space expands

    if (aIsBH > 0.5) { c = vec3(0.15, 0.05, 0.25); L = max(L, 0.05); } // BH faint glow (refined in Task 11+)

    vColor = c;
    vAlpha = L;
    gl_PointSize = clamp(2.0 + 9.0 * L * aMass, 1.0, 24.0);
}
```

- [ ] **Step 2: Replace `shaders/points.frag`**

```glsl
#version 330 core
in vec3  vColor;
in float vAlpha;
out vec4 FragColor;
void main() {
    float d = length(gl_PointCoord - vec2(0.5)) * 2.0; // 0 center .. 1 edge
    float glow = exp(-4.0 * d * d);                    // soft gaussian falloff
    FragColor = vec4(vColor * glow, glow * vAlpha);
}
```

- [ ] **Step 3: Enable additive blending in `Engine::init()`** — after `glEnable(GL_PROGRAM_POINT_SIZE);` add:

```cpp
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
        glDepthMask(GL_FALSE);             // glow points don't occlude each other
```

- [ ] **Step 4: Add a temporary fixed time + new uniforms in `render()`** — replace the point-draw block with:

```cpp
        glUseProgram(pointProgram);
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uView"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
        double tNow = cosmo::T0_YEARS; // fixed for now; Task 10 makes it dynamic
        glUniform1f(glGetUniformLocation(pointProgram, "uTGalaxy"),  (float)std::min(tNow, 1e15));
        glUniform1f(glGetUniformLocation(pointProgram, "uReddening"), (float)cosmo::reddening(tNow));
        glBindVertexArray(pointVAO);
        glDrawArrays(GL_POINTS, 0, universe.count);
        glBindVertexArray(0);
```
Add `#include <algorithm>` to the includes if not present.

- [ ] **Step 5: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 6: Manual visual check** — galaxies now glow with warm/blue-white colors and varied sizes; brighter where clustered.

- [ ] **Step 7: Commit**

```bash
git add shaders/points.vert shaders/points.frag big_freeze.cpp
git commit -m "feat: glowing blackbody point sprites (additive)"
```

---

## Task 10: Cosmic clock — play/pause/scrub/speed + dynamic expansion

**Files:**
- Modify: `big_freeze.cpp`

Drives the simulation by `log10(t)` and feeds dynamic uniforms; adds comoving/physical modes.

- [ ] **Step 1: Add clock state + constants to `Engine`** (members):

```cpp
    // cosmic clock in log10(years)
    double logT = std::log10(cosmo::T0_YEARS); // start "today"
    double logTmin = 8.0, logTmax = 106.0;
    double speed = 1.5;     // decades per second when playing
    bool   playing = false;
    bool   physicalMode = false; // P: physical expansion vs comoving
    double lastFrameTime = 0.0;
```

- [ ] **Step 2: Add a key callback in `init()`** — after the scroll callback:

```cpp
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
                case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, true); break;
            }});
```

- [ ] **Step 3: Advance the clock each frame** — at the top of `render()` (before `glClear`):

```cpp
        double now = glfwGetTime();
        double dt = (lastFrameTime == 0.0) ? 0.0 : (now - lastFrameTime);
        lastFrameTime = now;
        if (playing) {
            logT += speed * dt;
            if (logT >= logTmax) { logT = logTmax; playing = false; }
        }
        double t = std::pow(10.0, logT);
```

- [ ] **Step 4: Feed dynamic uniforms** — replace the fixed `tNow` lines in `render()` with:

```cpp
        glUniform1f(glGetUniformLocation(pointProgram, "uTGalaxy"),  (float)std::min(t, 1e15));
        glUniform1f(glGetUniformLocation(pointProgram, "uReddening"), (float)cosmo::reddening(t));
        // physical vs comoving point spacing
        float spacing = physicalMode ? (float)cosmo::visualStretch(t) : 1.0f;
        glUniform1f(glGetUniformLocation(pointProgram, "uSpacing"), spacing);
```

- [ ] **Step 5: Apply spacing in `shaders/points.vert`** — add `uniform float uSpacing;` and change the position line to:

```glsl
    gl_Position = uProj * uView * vec4(aPos * uSpacing, 1.0);
```

- [ ] **Step 6: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 7: Manual visual check** — Space plays: galaxies redden and fade as time advances toward the freeze; ←/→ scrub instantly; +/− change speed; P spreads the cloud (physical mode).

- [ ] **Step 8: Commit**

```bash
git add big_freeze.cpp shaders/points.vert
git commit -m "feat: cosmic clock (play/pause/scrub/speed) + comoving/physical modes"
```

---

## Task 11: Spacetime grid layer (toggle G)

**Files:**
- Create: `shaders/grid.vert`, `shaders/grid.frag`
- Modify: `big_freeze.cpp`

- [ ] **Step 1: Create `shaders/grid.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4  uView;
uniform mat4  uProj;
uniform float uStretch;
void main() { gl_Position = uProj * uView * vec4(aPos * uStretch, 1.0); }
```

- [ ] **Step 2: Create `shaders/grid.frag`**

```glsl
#version 330 core
out vec4 FragColor;
uniform float uFade;
void main() { FragColor = vec4(0.30, 0.42, 0.80, 0.18 * uFade); }
```

- [ ] **Step 3: Add grid members to `Engine`**

```cpp
    GLuint gridProgram = 0, gridVAO = 0, gridVBO = 0;
    int gridVertexCount = 0;
    bool showGrid = true;
```

- [ ] **Step 4: Build the grid in `init()`** — after the point-cloud setup:

```cpp
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
```

- [ ] **Step 5: Toggle G** — in the key callback `switch`, add:

```cpp
                case GLFW_KEY_G: if (action==GLFW_PRESS) e->showGrid = !e->showGrid; break;
```

- [ ] **Step 6: Draw the grid in `render()`** — after clearing, BEFORE the points (so points glow over it):

```cpp
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
```

- [ ] **Step 7: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 8: Manual visual check** — a faint blue 3D grid; it stretches as time advances and fades late; G toggles it.

- [ ] **Step 9: Commit**

```bash
git add shaders/grid.vert shaders/grid.frag big_freeze.cpp
git commit -m "feat: stretching spacetime grid layer (toggle G)"
```

---

## Task 12: Star background + redshift toggle (R)

**Files:**
- Create: `shaders/star.vert`, `shaders/star.frag`
- Modify: `big_freeze.cpp`

- [ ] **Step 1: Create `shaders/star.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3 aDir;   // direction on a far shell
uniform mat4 uView;
uniform mat4 uProj;
void main() {
    mat4 v = uView; v[3] = vec4(0,0,0,1);   // strip translation -> sky dome
    gl_Position = uProj * v * vec4(aDir * 900.0, 1.0);
    gl_PointSize = 1.5;
}
```

- [ ] **Step 2: Create `shaders/star.frag`**

```glsl
#version 330 core
out vec4 FragColor;
uniform float uFade;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    if (dot(d,d) > 0.25) discard;
    FragColor = vec4(vec3(0.8, 0.85, 1.0) * uFade, uFade);
}
```

- [ ] **Step 3: Add star members to `Engine`**

```cpp
    GLuint starProgram = 0, starVAO = 0, starVBO = 0;
    int starCount = 0;
    bool redshiftOn = true;
```

- [ ] **Step 4: Build stars in `init()`**

```cpp
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
```

- [ ] **Step 5: Toggle R** — in the key callback `switch`, add:

```cpp
                case GLFW_KEY_R: if (action==GLFW_PRESS) e->redshiftOn = !e->redshiftOn; break;
```

- [ ] **Step 6: Draw stars first in `render()`** (immediately after `glClear`, before the grid):

```cpp
        {
            glUseProgram(starProgram);
            glUniformMatrix4fv(glGetUniformLocation(starProgram, "uView"), 1, GL_FALSE, value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(starProgram, "uProj"), 1, GL_FALSE, value_ptr(proj));
            glUniform1f(glGetUniformLocation(starProgram, "uFade"), 0.6f * (1.0f - (float)cosmo::reddening(t)));
            glBindVertexArray(starVAO);
            glDrawArrays(GL_POINTS, 0, starCount);
            glBindVertexArray(0);
        }
```

- [ ] **Step 7: Gate reddening on the toggle** — change the points `uReddening` line in `render()` to:

```cpp
        glUniform1f(glGetUniformLocation(pointProgram, "uReddening"),
                    redshiftOn ? (float)cosmo::reddening(t) : 0.0f);
```

- [ ] **Step 8: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 9: Manual visual check** — faint background stars behind the galaxies, fading into the dark over time; R toggles galaxy reddening on/off.

- [ ] **Step 10: Commit**

```bash
git add shaders/star.vert shaders/star.frag big_freeze.cpp
git commit -m "feat: star background + redshift toggle (R)"
```

---

## Task 13: On-screen HUD (stb_easy_font)

**Files:**
- Create: `stb_easy_font.h` (vendored), `shaders/hud.vert`, `shaders/hud.frag`
- Modify: `big_freeze.cpp`, `CMakeLists.txt`

- [ ] **Step 1: Vendor the font header**

```bash
curl -L -o stb_easy_font.h https://raw.githubusercontent.com/nothings/stb/master/stb_easy_font.h
```
Expected: a ~200-line single-header file at repo root.

- [ ] **Step 2: Create `shaders/hud.vert`** (pixel coords → NDC via ortho)

```glsl
#version 330 core
layout(location = 0) in vec2 aPos;   // pixel coordinates
uniform vec2 uScreen;                // (width, height)
void main() {
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0,
                    1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
```

- [ ] **Step 3: Create `shaders/hud.frag`**

```glsl
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 0.92); }
```

- [ ] **Step 4: Include the font + add HUD resources** — at the very top of `big_freeze.cpp`, before other includes:

```cpp
#define STB_EASY_FONT_IMPLEMENTATION   // (header has no impl guard; this is harmless if unused)
#include "stb_easy_font.h"
```
Note: `stb_easy_font.h` is implementation-in-header (no separate guard needed); include it exactly once (we do).

Add `#include <cstdio>` to includes. Add `Engine` members:

```cpp
    GLuint hudProgram = 0, hudVAO = 0, hudVBO = 0, hudEBO = 0;
```

- [ ] **Step 5: Set up HUD GL in `init()`**

```cpp
        hudProgram = makeProgram("shaders/hud.vert", "shaders/hud.frag");
        glGenVertexArrays(1, &hudVAO);
        glGenBuffers(1, &hudVBO);
        glGenBuffers(1, &hudEBO);
```

- [ ] **Step 6: Add a `drawText` helper to `Engine`** (converts stb quads → triangles)

```cpp
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
```

- [ ] **Step 7: Draw the HUD at the end of `render()`** (after the points, so text sits on top)

```cpp
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
            std::snprintf(line, sizeof(line), "Era: %s", eraStr);                       drawText(14, 82, line, vec3(1.0f,0.8f,0.4f));
            std::snprintf(line, sizeof(line), "[espacio] play  [<- ->] scrub  [+ -] vel  [G]rid [R]edshift [P]hys");
            drawText(14, (float)HEIGHT-22, line, vec3(0.5f,0.55f,0.65f));
        }
```
Add `#include <cmath>` (for `std::isfinite`) if not present.

- [ ] **Step 8: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 9: Manual visual check** — top-left HUD shows t, a, z, T_cmb, Era; a help line at the bottom. Values update live while playing.

- [ ] **Step 10: Commit**

```bash
git add stb_easy_font.h shaders/hud.vert shaders/hud.frag big_freeze.cpp CMakeLists.txt
git commit -m "feat: on-screen HUD (time, a, z, T, era) via stb_easy_font"
```

---

## Task 14: Black hole era + final darkness (narrative climax)

**Files:**
- Modify: `shaders/points.vert`, `big_freeze.cpp`

Black holes outlive the stars and wink out via Hawking evaporation; the last light fades to black.

- [ ] **Step 1: Add black hole evaporation to `shaders/points.vert`** — add a uniform and replace the BH branch.

Add near the other uniforms:
```glsl
uniform float uLog10T;    // log10(cosmic time / yr), 8..106
```
Replace the line `if (aIsBH > 0.5) { c = vec3(0.15, 0.05, 0.25); L = max(L, 0.05); }` with:
```glsl
    if (aIsBH > 0.5) {
        // evaporation time ~ M^3; map mass to log10(t_evap) in [40, 100]
        float logEvap = 40.0 + 60.0 * clamp((aMass - 0.3) / 1.7, 0.0, 1.0);
        float pre  = smoothstep(13.0, 15.0, uLog10T);            // BH "lights" appear after stars die
        float gone = smoothstep(logEvap - 0.5, logEvap, uLog10T); // fade out near evaporation
        float flash = exp(-pow((uLog10T - (logEvap - 0.25)) * 6.0, 2.0)); // final Hawking flash
        L = pre * (0.18 * (1.0 - gone) + 0.9 * flash);
        c = mix(vec3(0.5, 0.2, 0.8), vec3(1.0, 0.95, 0.8), flash); // purple glow -> white flash
    }
```

- [ ] **Step 2: Feed `uLog10T` in `render()`** — alongside the other point uniforms:

```cpp
        glUniform1f(glGetUniformLocation(pointProgram, "uLog10T"), (float)logT);
```

- [ ] **Step 3: Build + smoke**

```bash
cmake --build build && ./build/BigFreeze --smoke
```
Expected: `[SMOKE] ok`.

- [ ] **Step 4: Manual visual check** — scrub past ~10¹⁴ yr: galaxies are dark, purple BH glows remain; scrub toward 10⁴⁰–10¹⁰⁰ yr: BHs flash white then vanish; by the Dark era the screen is essentially black with only a HUD. Confirm the era label tracks the transitions.

- [ ] **Step 5: Commit**

```bash
git add shaders/points.vert big_freeze.cpp
git commit -m "feat: black hole Hawking era + fade to total darkness"
```

---

## Task 15: README + final polish

**Files:**
- Create: `README.md`
- Verify: `.gitignore` already contains `build/` and `.superpowers/`

- [ ] **Step 1: Create `README.md`**

```markdown
# Big Freeze

Simulación visual en tiempo real del Big Freeze (muerte térmica del universo, ΛCDM),
en el estilo de [kavan010/black_hole](https://github.com/kavan010/black_hole).
Cámara orbital, línea de tiempo cósmica (hoy → 10¹⁰⁰ años), galaxias que se enfrían
y apagan, rejilla de espaciotiempo que se estira, y el clímax: la oscuridad total.

## Controles
- Arrastrar (clic izq.) = orbitar · scroll = zoom
- Espacio = play/pausa · ←/→ = scrub del tiempo · +/− = velocidad
- G = rejilla · R = redshift · P = físico/comóvil · Esc = salir

## Requisitos
C++17, CMake ≥ 3.21, y GLFW + GLEW + GLM + OpenGL.

### macOS (Homebrew)
```bash
brew install cmake glfw glew glm
cmake -B build -S .
cmake --build build
./build/BigFreeze
```

### vcpkg (Windows/Linux/macOS)
```bash
vcpkg install
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/BigFreeze
```

## Pruebas
```bash
ctest --test-dir build --output-on-failure
```

## Notas
- Corre en OpenGL 3.3 core (forward-compat), así que funciona en macOS (sin compute shaders).
- La física es FLRW analítica (forma cerrada de ΛCDM plano): scrub instantáneo en todo el rango.
- `cosmology.h` es física pura y tiene pruebas unitarias (`tests/test_cosmology.cpp`).
```

- [ ] **Step 2: Verify the full build is clean from scratch**

```bash
rm -rf build && cmake -B build -S . && cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: both targets build; `1/1 Passed` for cosmology tests.

- [ ] **Step 3: Final manual verification checklist** — run `./build/BigFreeze` and confirm:
  - [ ] Cosmic web of glowing galaxies; orbit + zoom work.
  - [ ] Space = play; galaxies redden + fade as t advances; HUD updates.
  - [ ] ←/→ scrub jumps instantly both directions.
  - [ ] G toggles the stretching grid; R toggles reddening; P toggles physical spread.
  - [ ] Era label cycles Estelífera → Degenerada → Agujeros negros → Oscura.
  - [ ] Black holes flash and vanish; the Dark era is (almost) pure black.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: README with build (Homebrew + vcpkg), controls, notes"
```

---

## Self-Review (completed by plan author)

- **Spec coverage:** §2 decisions → Tasks 1–15; FLRW analytic (§3) → Tasks 1–2; redshift/temp/era/blackbody/lifecycle (§3) → Tasks 2–5; architecture/files (§4) → Tasks 1,6,8,13,15; render pipeline (§5) → Tasks 7–13; timeline/eras (§6) → Tasks 10,14; visual layers (§7) → Tasks 9,11,12 + toggles in 10,12; controls/HUD (§8) → Tasks 10,13; tests (§9) → Tasks 1–5 + manual checklist Task 15; YAGNI (§10) → nothing out-of-scope added; risks (§11): overflow handled (Tasks 1–2,5 + numeric rule), macOS GL (Task 6), comoving-default dynamic range (Task 10), Homebrew+vcpkg (Tasks 6,15). No gaps.
- **Placeholder scan:** no TBD/TODO; every code step shows complete code.
- **Type consistency:** `cosmo::` API (`logScaleFactor`, `scaleFactor`, `log10ScaleFactor`, `redshift`, `cmbTemperature`, `era`, `eraName`, `blackbodyRGB`, `RGB`, `galaxyLuminosity`, `galaxyTempK`, `visualStretch`, `reddening`) used consistently. Vertex layout (7 floats: pos, tForm, tauSF, mass, isBH) consistent between `Universe::generate` (Task 8) and `points.vert` attributes (Tasks 8–9). Uniform names (`uView`,`uProj`,`uTGalaxy`,`uReddening`,`uSpacing`,`uStretch`,`uFade`,`uScreen`,`uColor`,`uLog10T`) consistent between shader declarations and `glGetUniformLocation` calls.
