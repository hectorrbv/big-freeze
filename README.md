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
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix)
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
- Si agregas nuevos archivos de shader, vuelve a correr el configure de CMake (`cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix)`) para que el GLOB los detecte.
