# Big Freeze — Simulación visual (diseño)

- **Fecha:** 2026-06-07
- **Estado:** Aprobado en brainstorming, pendiente de revisión final del usuario
- **Inspiración:** [kavan010/black_hole](https://github.com/kavan010/black_hole) — mismo estilo de código (C++17, un `.cpp` por ejecutable, OpenGL + GLFW + GLEW + GLM, structs con métodos inline, constantes físicas reales, comentarios didácticos).

## 1. Objetivo

Simulación visual interactiva en tiempo real del **Big Freeze** (muerte térmica del universo): la expansión eterna de un universo ΛCDM hasta el frío y la oscuridad máximos. El usuario observa, con una cámara orbital y control de la línea de tiempo cósmica (de hoy hasta ~10¹⁰⁰ años), cómo:

1. El espaciotiempo se estira (factor de escala a(t)).
2. Las galaxias se enfrían (color de cuerpo negro: azul → rojo) y se apagan (fin de la era estelífera).
3. La temperatura del fondo cae (T_CMB ∝ 1/a) y la entropía tiende al máximo.
4. Los agujeros negros quedan como las últimas "luces" hasta evaporarse (Hawking) → oscuridad total.

Debe **correr nativamente en macOS** (OpenGL 4.1 máx, sin compute shaders), además de Windows/Linux.

## 2. Decisiones de diseño (acordadas)

| Tema | Decisión |
|------|----------|
| Núcleo visual | Universo en expansión y enfriamiento (nube de galaxias 3D) |
| Plataforma | OpenGL 3.3 core, forward-compat (corre en macOS); GPU sin compute shaders |
| Enfoque de simulación | **FLRW analítico**: galaxias fijas en coords comóviles; expansión = a(t); apariencia = función pura de t |
| Física | Cosmología real simplificada (ΛCDM plano), constantes reales |
| Estructura de código | Un solo `big_freeze.cpp` + shaders; mate cosmológica en `cosmology.h` (testeable) |
| Interactividad | Completa: cámara orbital + línea de tiempo (play/pausa/scrub/velocidad) + HUD |
| Estilo de render | Glow aditivo (point sprites) + rejilla de espaciotiempo estirándose (capa activable) |

**Por qué FLRW analítico y no transform feedback / N-body:** en cosmología las galaxias permanecen en coordenadas comóviles fijas; solo cambia a(t). Así la expansión es un escalado uniforme y el color/brillo de cada galaxia es una función pura de t. Resultado: el scrub de la línea de tiempo (adelante y atrás, en todo el rango 10⁸–10¹⁰⁰ años) es **instantáneo y exacto**, sin estado por frame, y corre en GL 3.3 puro. La telaraña cósmica se "hornea" una vez al iniciar (clustering en la generación de posiciones), así que igual se ve viva. Transform feedback / N-body / lentes gravitacionales quedan como v2.

## 3. Modelo físico (`cosmology.h`)

Universo ΛCDM plano (Ω_m + Ω_Λ = 1). Funciones puras, sin dependencias de OpenGL, unidades en SI/años según convenga.

Constantes:
- `c = 299792458` m/s, `G = 6.67430e-11`
- `H0 ≈ 70` km/s/Mpc (convertir a 1/año)
- `Omega_m = 0.315`, `Omega_L = 0.685`
- `T_CMB0 = 2.725` K
- `t0 ≈ 13.8e9` años (edad actual, donde a = 1)

Factor de escala — **forma cerrada exacta para ΛCDM plano** (permite scrub analítico, sin integración):

```
a(t) = (Omega_m / Omega_L)^(1/3) * sinh( (3/2) * sqrt(Omega_L) * H0 * t )^(2/3)
```

con t medido desde el Big Bang. Para t grande, a(t) ∝ exp(sqrt(Ω_Λ)·H0·t) → expansión de De Sitter (el "freeze").

Derivados:
- Estiramiento relativo a hoy: `S(t) = a(t) / a(t0)`
- Redshift de luz emitida en t y observada hoy: `1 + z = a(t0) / a(t)` (para el HUD/efecto de reddening usamos S(t))
- Temperatura del fondo: `T(t) = T_CMB0 / S(t)`
- Era (umbrales aprox., basados en Adams & Laughlin, *Five Ages of the Universe*):
  - **Estelífera:** hoy → ~10¹⁴ años
  - **Degenerada:** ~10¹⁴ – 10⁴⁰ años
  - **Agujeros negros:** ~10⁴⁰ – 10¹⁰⁰ años
  - **Oscura:** > 10¹⁰⁰ años

Apariencia por galaxia (función pura de t y de atributos por-galaxia generados al init):
- Cada galaxia tiene `t_form`, `tau_SF` (escala de formación estelar), `mass`, semilla aleatoria.
- Luminosidad: `L(t)` decae tras la era estelífera; se apaga hacia ~10¹⁴ años.
- Temperatura efectiva del cúmulo estelar `T_eff(t)`: interpola azul → rojo conforme mueren primero las estrellas masivas (azules, de vida corta).
- Color final = locus de Planck (cuerpo negro) de `T_eff`, enrojecido además por el redshift cosmológico (factor de S(t)) — físicamente motivado.

Agujeros negros (subconjunto de puntos marcados):
- Glow tenue tipo Hawking; vida `t_evap ∝ M³` (estelares ~10⁶⁷ años, supermasivos ~10¹⁰⁰ años).
- Se apagan en `t_evap` con un destello final → contribuyen al clímax de oscuridad.

Color de cuerpo negro: función `blackbodyRGB(tempK)` (aproximación del locus de Planck), reutilizable en CPU (HUD) y portada al shader.

## 4. Arquitectura

```
big_freeze/
├── CMakeLists.txt          # targets: BigFreeze (app) + test_cosmology (pruebas)
├── vcpkg.json              # glfw3, glew, glm  (igual que el repo)
├── README.md               # build con vcpkg Y con Homebrew (macOS)
├── cosmology.h             # física pura, header-only, sin OpenGL  → testeable
├── big_freeze.cpp          # app: structs Camera, Universe, Engine + main()
├── shaders/
│   ├── points.vert / points.frag   # point sprites con glow aditivo
│   ├── grid.vert   / grid.frag     # rejilla de espaciotiempo (escala con a)
│   └── star.vert   / star.frag     # fondo de estrellas tenues
└── tests/
    └── test_cosmology.cpp  # pruebas unitarias del modelo físico
```

**Structs (estilo del repo, inline en `big_freeze.cpp`):**
- `Camera` — órbita azimuth/elevation/radius alrededor del origen; mouse drag + scroll zoom (calcado del repo).
- `Universe` — genera al init la nube de galaxias en coords comóviles con clustering (algunas semillas atractoras → telaraña cósmica); guarda atributos por-galaxia (pos, t_form, tau, mass, esBH); sube todo a un VBO una sola vez.
- `Engine` — ventana GLFW, contexto GL 3.3 core forward-compat, GLEW, compilación de shaders, VAOs (puntos / rejilla / fondo), loop de render, HUD, manejo de entrada, estado de tiempo cósmico.

**Stack y build:** GLFW + GLEW + GLM, CMake ≥ 3.21. Soporta vcpkg (`vcpkg install`) como el repo, y Homebrew en macOS (`brew install glfw glew glm`). Shaders copiados al directorio de salida vía `add_custom_command` POST_BUILD (igual que el repo).

**Notas técnicas macOS (críticas):**
- Window hints: `CONTEXT_VERSION_MAJOR=3, MINOR=3, OPENGL_PROFILE=CORE, OPENGL_FORWARD_COMPAT=GL_TRUE`.
- `glewExperimental = GL_TRUE` antes de `glewInit()`.
- Point sprites en core profile: habilitar `GL_PROGRAM_POINT_SIZE`, fijar `gl_PointSize` en el vertex shader, disco suave con `gl_PointCoord` en el fragment shader.
- Glow aditivo: `glEnable(GL_BLEND)` + `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`; depth test sin escritura para los puntos.

## 5. Pipeline de render (por frame)

1. Actualizar tiempo cósmico `t` (si play: avanzar en escala log; si scrub: leer slider/teclas).
2. Calcular en CPU: `a(t)`, `S(t)`, `z`, `T(t)`, era. Estos van a uniforms + HUD.
3. Subir uniforms: matrices de cámara (view, proj), `t`, `a`, `S`, flags de capas.
4. Dibujar fondo de estrellas (quad o puntos lejanos, muy tenue).
5. Dibujar rejilla de espaciotiempo si está activa: `GL_LINES`, posiciones escaladas por a(t) (o por S en modo comóvil) → se ve estirarse.
6. Dibujar galaxias: VAO de puntos. Vertex shader coloca `pos_comoving` (o `a(t)·pos` en modo físico), calcula `gl_PointSize` (∝ brillo, atenuado por distancia) y pasa color/alfa derivados de `t` y atributos por-galaxia. Fragment shader: disco suave (glow) con blending aditivo.
7. Dibujar HUD (texto): t, a(t), z, T_CMB, # galaxias encendidas, etiqueta de era.

## 6. Línea de tiempo y eras (narrativa)

Slider/estado en `log10(t)` desde ~10⁸ años hasta ~10¹⁰⁶ años. Transiciones de era cambian la etiqueta del HUD y el comportamiento visual:
- **Estelífera** → galaxias brillantes; las azules (masivas) se enrojecen y mueren primero.
- **Degenerada** → casi todo apagado; quedan restos tenues.
- **Agujeros negros** → los AN son las últimas luces (glow Hawking) hasta evaporarse en destellos.
- **Oscura** → pantalla negra, T→0, entropía máxima. Clímax: todo apagado.

## 7. Capas visuales activables (estilo 3)

- Glow aditivo de galaxias (siempre).
- Rejilla de espaciotiempo (tecla **G**) — estirándose con a(t).
- Corrimiento al rojo on/off (tecla **R**) — aplica el reddening por S(t).
- Modo físico vs comóvil (tecla **P**) — en físico las galaxias se separan; en comóvil quedan fijas y el énfasis es la rejilla + color.
- Fondo de estrellas tenues.

## 8. Controles y HUD

- **Cámara:** mouse drag = orbitar; scroll = zoom (calcado del repo).
- **Teclas:** Espacio = play/pausa; ← / → = scrub tiempo; + / − = velocidad; G / R / P = toggles de capa; Esc = salir.
- **HUD:** tiempo cósmico (años, notación científica), a(t), redshift z, T_CMB (K), galaxias encendidas, etiqueta de era.

## 9. Pruebas y verificación

- **Unitarias** (`tests/test_cosmology.cpp`, ejecutable CMake aparte; TDD del modelo físico):
  - `a(t0) ≈ 1` (con tolerancia).
  - `z(t0) ≈ 0`.
  - `a(t)` monótona creciente; `a(t) → ∞` cuando t crece.
  - `T(t) = T_CMB0 / S(t)`; `T → 0` cuando t crece.
  - Clasificación de era correcta en los umbrales.
  - `blackbodyRGB`: monotonía de canales en rangos conocidos (caliente=azulado, frío=rojizo).
- **Render:** verificación visual ejecutando `BigFreeze` (no automatizable de forma simple); checklist manual de cada capa y de las transiciones de era.

## 10. Fuera de alcance (YAGNI v1)

N-body / gravedad real, transform feedback, bloom multi-pass, lentes gravitacionales, audio, grabación de video, UI de paneles. Reservado para v2.

## 11. Riesgos y mitigaciones

- **Rango dinámico de 10¹⁰⁰:** se resuelve usando marco comóvil por defecto (nada vuela fuera de pantalla) + escala log de tiempo + mostrar a(t) numéricamente y vía estiramiento de la rejilla, en vez de movimiento literal.
- **OpenGL en macOS:** ceñirse a 3.3 core forward-compat; nada de compute shaders ni features > 4.1.
- **Precisión numérica:** `sinh` con argumentos grandes desborda `float`; calcular a(t) en `double` y normalizar a S(t) antes de pasar a `float` del shader.
- **Toolchain en Mac:** documentar ruta Homebrew además de vcpkg para evitar fricción.
