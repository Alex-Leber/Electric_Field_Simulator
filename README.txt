<div align="center">

# Electric Field Simulator

**An interactive, real-time 3D visualization of electrostatic fields written in C, compiled to WebAssembly, running natively in your browser.**

[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![raylib](https://img.shields.io/badge/raylib-5.5-000000?style=for-the-badge)](https://www.raylib.com/)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-654FF0?style=for-the-badge&logo=webassembly&logoColor=white)](https://webassembly.org/)
[![Emscripten](https://img.shields.io/badge/Emscripten-323330?style=for-the-badge)](https://emscripten.org/)
[![Deployed on Vercel](https://img.shields.io/badge/Deploy-Vercel-000000?style=for-the-badge&logo=vercel)](https://vercel.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-3DA639?style=for-the-badge)](LICENSE)

### [ ▶ **Try the Live Demo** ](https://electricfieldsim.com)
<!-- ↑ replace with your actual Vercel URL -->

<img src="assets/demo.gif" width="80%" alt="Electric Field Simulator demo"/>
<!-- ↑ record a short screen capture, save it as assets/demo.gif, and commit it -->

</div>

---

## Overview

Drop point charges into a 3D scene and watch the electric field render itself in real time. Field lines are traced live by numerically integrating the net field of every charge, so the picture updates the instant you add, move, or delete a charge. Fly around the scene freely to inspect the structure from any angle. Students will benefit from the hands-on learning experience of visualizing complex charge configurations in real time.
The entire simulation, physics, rendering, and UI, is a single C program built on [raylib](https://www.raylib.com/), compiled to **WebAssembly** via **Emscripten** and rendered through **WebGL 2**. The `.wasm` runs directly in the browser.

## Features

- **Live field-line tracing** — lines are integrated through the superposed Coulomb field every frame.
- **Interactive charge editing** — place, drag, and delete charges; type exact charge magnitudes (positive or negative).
- **Free-fly camera** — full 6-DOF movement with mouse-look for inspecting the field in 3D.
- **Tunable resolution** — adjust field-line density and integration length.
- **Field line shading** — segments blend from source to sink.
- **Responsive** — auto-resizes to the browser window, with 4× MSAA for clean edges.
- **Native performance in the browser** — C + WebAssembly, up to 100 simultaneous charges.

## Controls

Press **`F`** to toggle between the two modes.

### Camera Mode (fly around)
| Input | Action |
|-------|--------|
| **Mouse** | Look around |
| **W / A / S / D** | Move forward / left / back / right |
| **Space** | Ascend |
| **Left Shift** | Descend |

### Edit Mode (build the scene)
| Input | Action |
|-------|--------|
| **Left-click empty space** | Start typing a value, then **Enter** to place a charge |
| **Left-click + drag** | Move an existing charge |
| **Right-click** | Delete a charge |
| **↑ / ↓** | Increase / decrease field-line length (integration steps) |
| **← / →** | Increase / decrease field-line density |


## How It Works

Every rendered frame recomputes the field from scratch. The net electric field at any point $\vec{r}$ is the superposition of the Coulomb contributions from all $N$ charges:

$$\vec{E}(\vec{r}) \;=\; \sum_{i=1}^{N} q_i \, \frac{\vec{r} - \vec{r}_i}{\lVert \vec{r} - \vec{r}_i \rVert^{3}}$$

Field lines are then produced in three stages:

1. **Seeding** — each positive charge emits lines from a small spherical shell of seed points around it. The number of seeds scales with the density setting (azimuthal × polar sampling), so higher density = more lines.
2. **Integration** — each line marches forward in fixed steps, always following the *direction* of the local net field $\hat{E}$. This is a numerical streamline integration of the vector field.
3. **Termination** — a line ends when it reaches a negative charge (a sink), the field vanishes, or it leaves the bounding region.

Each segment is tinted along a blue→red gradient based on its relative proximity to the nearest positive vs. negative charge, drawn with **additive blending** and a tail fade so dense bundles glow rather than clip. Charges themselves are drawn as shaded spheres with wireframe halos and live magnitude labels.

## Tech Stack

| Layer | Choice |
|-------|--------|
| **Language** | C (C99) |
| **Graphics / windowing** | [raylib 5.5](https://www.raylib.com/) + `rlgl` immediate-mode layer |
| **Compile target** | WebAssembly via [Emscripten](https://emscripten.org/) |
| **Rendering backend** | WebGL 2 / OpenGL ES 2, GLFW3 (provided by Emscripten) |
| **Hosting** | Vercel (static) |

## Build & Run Locally

**Prerequisites**
- The [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (`emcc` on your `PATH`)
- A WebAssembly build of `libraylib.a` (5.5) — the `make raylib` target rebuilds it from source
- `make` and Python 3

```bash
# 1. Clone
git clone https://github.com/Alex-Leber/Electric_Field_Simulator.git
cd Electric_Field_Simulator/src

# 2. Activate Emscripten (in this shell)
source ../emsdk/emsdk_env.sh

# 3. Build + serve
make run          # compiles main.c -> index.js/.wasm/.data and serves on :8000
```

Then open **http://localhost:8000**. (Click the canvas once to capture the mouse.)

| Command | Does |
|---------|------|
| `make` | Compile `main.c` → `index.js` + `index.wasm` + `index.data` |
| `make serve` | Serve the folder over HTTP on port 8000 |
| `make run` | Build, then serve |
| `make clean` | Remove build artifacts |
| `make raylib` | Rebuild `libraylib.a` from `../raylib/src` with the current emsdk |

> **Note:** fonts are bundled into `index.data` at build time via `--preload-file`, so a rebuild is required after changing any asset.

## Deployment

The app is fully static — deployment is just serving the build output. On Vercel: import the repo, set **Framework Preset → Other**, leave the **Build Command empty**, and point the **Root Directory** at `src`. Every push to `main` re-publishes the committed `index.html`, `index.js`, `index.wasm`, and `index.data`. (Vercel serves `.wasm` with the correct `application/wasm` MIME type automatically.)

## Project Structure

```
src/
├── main.c            # simulation, rendering, and UI (the whole app)
├── libraylib.a       # raylib 5.5, built for WebAssembly
├── raylib.h / raymath.h / rlgl.h
├── Fonts/Roboto/     # UI fonts (baked into index.data at build)
├── Makefile          # emscripten build + local server
├── shell.html        # emscripten HTML shell template
└── index.html        # deployed page (loads index.js)
```

## License

Released under the **MIT License** — see [LICENSE](LICENSE).

<div align="center">
<sub>Built with C, raylib, and a lot of vectors.</sub>
</div>
