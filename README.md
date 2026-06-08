# Big Freeze 🌌

Real-time visual simulation of the **Big Freeze** — the heat death of the universe (ΛCDM) — in the spirit of [kavan010/black_hole](https://github.com/kavan010/black_hole). Orbit the camera, scrub a cosmic clock from *today* to **10¹⁰⁰ years**, and watch galaxies cool and fade, spacetime stretch, black holes evaporate one by one, and everything sink into an eternal — but still faintly alive — void.

Written in **C++17 + OpenGL 3.3 core**, so it runs natively on macOS (no compute shaders). The cosmology is the **analytic closed-form flat-ΛCDM** scale factor, so scrubbing the whole 10⁸–10¹⁰⁰ year range is instant and exact.

> 🇬🇧 English below · 🇪🇸 [Español más abajo ↓](#big-freeze-español)

## Features

- **Real cosmology** — FLRW flat-ΛCDM scale factor `a(t)`, cosmological redshift `z`, CMB temperature `T ∝ 1/a`, and the cosmic eras (Stelliferous → Degenerate → Black Hole → Dark). Pure, unit-tested math in [`cosmology.h`](cosmology.h).
- **Cosmic web** — ~6000 galaxies clustered into filaments, colored by black-body temperature, with subtle twinkle and rare supernova flashes.
- **Stretching spacetime grid**, faint star background, and cosmological reddening as space expands.
- **Cinematic bloom** — HDR framebuffer + separable Gaussian blur (toggle with `B`).
- **Black-hole era** — black holes outlive the stars and wink out via Hawking evaporation, each with a final flash.
- **The living void** — at heat death the screen doesn't "go dead": a cold residual glow, faint quantum-fluctuation sparkles, and a persistent epilogue make the maximum-entropy end state legible.
- **HUD** — cosmic time, scale factor `a`, redshift `z`, CMB temperature, lit-galaxy count, current era + caption, entropy %, milestone banners, and a self-explaining line for every toggle.

## Controls

| Input | Action |
|-------|--------|
| Drag (left mouse) | Orbit camera |
| Scroll | Zoom |
| `Space` | Play / pause the cosmic clock |
| `←` / `→` | Scrub time |
| `+` / `−` | Playback speed |
| `0` / `Home` | Reset to *today* (a = 1, z = 0) |
| `G` | Spacetime grid |
| `R` | Cosmological redshift |
| `P` | Physical vs comoving frame |
| `B` | Cinematic bloom |
| `Esc` | Quit |

> Tip: `R` (redshift) only has a visible effect **after** the universe has expanded — press `Space` to run time forward, then toggle it.

## Build

Requires C++17, CMake ≥ 3.21, and GLFW + GLEW + GLM + OpenGL.

### macOS (Homebrew)
```bash
brew install cmake glfw glew glm
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix)
cmake --build build
./build/BigFreeze
```

### vcpkg (Windows / Linux / macOS)
```bash
vcpkg install
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/BigFreeze
```

## Tests
```bash
ctest --test-dir build --output-on-failure
```

## How it works

In an expanding universe, galaxies sit at fixed **comoving** coordinates; only the scale factor `a(t)` changes. So the whole simulation is a *pure function of cosmic time*: `a(t)`, redshift, temperature, and each galaxy's brightness/color are computed analytically every frame. That's why scrubbing — forward or backward, across 90+ orders of magnitude of time — is instant and never drifts. The heavy lifting runs in the vertex/fragment shaders; the CPU just advances a clock.

- `cosmology.h` — pure physics (no OpenGL), unit-tested.
- `big_freeze.cpp` — windowing, camera, rendering, HUD.
- `shaders/` — point sprites, spacetime grid, stars, bloom, the void, HUD text.

## Credits

Inspired by [kavan010/black_hole](https://github.com/kavan010/black_hole). Cosmic-era timeline after Adams & Laughlin, *Five Ages of the Universe*. Public-domain font via [`stb_easy_font`](https://github.com/nothings/stb).

---

# Big Freeze (Español)

Simulación visual en tiempo real del **Big Freeze** — la muerte térmica del universo (ΛCDM) — en el estilo de [kavan010/black_hole](https://github.com/kavan010/black_hole). Orbita la cámara, mueve un reloj cósmico de *hoy* hasta **10¹⁰⁰ años**, y observa cómo las galaxias se enfrían y se apagan, el espaciotiempo se estira, los agujeros negros se evaporan uno a uno, y todo se hunde en un vacío eterno — pero aún tenuemente vivo.

Hecho en **C++17 + OpenGL 3.3 core**, así que corre nativo en macOS (sin compute shaders). La cosmología es el factor de escala **analítico de ΛCDM plano (forma cerrada)**, por lo que mover el tiempo por todo el rango 10⁸–10¹⁰⁰ años es instantáneo y exacto.

## Características

- **Cosmología real** — factor de escala FLRW de ΛCDM plano `a(t)`, corrimiento al rojo `z`, temperatura del CMB `T ∝ 1/a`, y las eras cósmicas (Estelífera → Degenerada → Agujeros negros → Oscura). Física pura y con pruebas unitarias en [`cosmology.h`](cosmology.h).
- **Telaraña cósmica** — ~6000 galaxias agrupadas en filamentos, coloreadas por temperatura de cuerpo negro, con parpadeo sutil y destellos de supernova ocasionales.
- **Rejilla de espaciotiempo** que se estira, fondo de estrellas tenue y enrojecimiento cosmológico al expandirse el espacio.
- **Bloom cinemático** — framebuffer HDR + blur gaussiano separable (tecla `B`).
- **Era de agujeros negros** — sobreviven a las estrellas y se apagan por evaporación de Hawking, cada uno con un destello final.
- **El vacío vivo** — en la muerte térmica la pantalla no "se muere": un resplandor frío residual, destellos de fluctuaciones cuánticas y un epílogo persistente hacen legible el estado final de entropía máxima.
- **HUD** — tiempo cósmico, factor de escala `a`, redshift `z`, temperatura del CMB, galaxias encendidas, era actual + descripción, % de entropía, banners de hitos, y una línea que explica cada tecla.

## Controles

| Entrada | Acción |
|---------|--------|
| Arrastrar (clic izq.) | Orbitar la cámara |
| Scroll | Zoom |
| `Espacio` | Play / pausa del reloj cósmico |
| `←` / `→` | Mover el tiempo (scrub) |
| `+` / `−` | Velocidad de reproducción |
| `0` / `Home` | Reiniciar a *hoy* (a = 1, z = 0) |
| `G` | Rejilla de espaciotiempo |
| `R` | Corrimiento al rojo cosmológico |
| `P` | Marco físico vs comóvil |
| `B` | Bloom cinemático |
| `Esc` | Salir |

> Tip: `R` (redshift) solo se nota **después** de que el universo se haya expandido — pulsa `Espacio` para avanzar el tiempo y luego actívalo.

## Compilar

Requiere C++17, CMake ≥ 3.21, y GLFW + GLEW + GLM + OpenGL.

### macOS (Homebrew)
```bash
brew install cmake glfw glew glm
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix)
cmake --build build
./build/BigFreeze
```

### vcpkg (Windows / Linux / macOS)
```bash
vcpkg install
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/ruta/a/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/BigFreeze
```

## Pruebas
```bash
ctest --test-dir build --output-on-failure
```

## Cómo funciona

En un universo en expansión, las galaxias están en coordenadas **comóviles** fijas; lo único que cambia es el factor de escala `a(t)`. Así, toda la simulación es una *función pura del tiempo cósmico*: `a(t)`, redshift, temperatura y el brillo/color de cada galaxia se calculan analíticamente en cada frame. Por eso mover el tiempo — hacia adelante o atrás, a través de 90+ órdenes de magnitud — es instantáneo y nunca se desvía. El trabajo pesado está en los shaders; la CPU solo avanza un reloj.

- `cosmology.h` — física pura (sin OpenGL), con pruebas unitarias.
- `big_freeze.cpp` — ventana, cámara, render, HUD.
- `shaders/` — point sprites, rejilla, estrellas, bloom, el vacío, texto del HUD.

## Créditos

Inspirado en [kavan010/black_hole](https://github.com/kavan010/black_hole). Línea de tiempo de eras cósmicas basada en Adams & Laughlin, *Five Ages of the Universe*. Fuente de dominio público vía [`stb_easy_font`](https://github.com/nothings/stb).

---

*Notas técnicas:* corre en OpenGL 3.3 core (forward-compat), compatible con macOS. Si agregas nuevos archivos de shader, vuelve a correr el configure de CMake para que el `file(GLOB)` los detecte.
