# Repository Guidelines

## Project Structure & Module Organization
The Qt/VTK front-end lives in `src` (widgets, app entry point) with matching interfaces under `header`. Shared fiber IO and rendering code is maintained in the static-library workspace (`static/src`, `static/include`, `static/header`), which outputs binaries to `static/lib`. Root-level builds go to `build`, while static-library artifacts stay in `static/build`; keep generated files out of version control. Runtime data and demo tractography dump into `data` (large `.trk` files, JSON exports), and `sample` stores reference UI assets and mock interactions. Reserve `lib` for prebuilt third-party binaries only.

## Build, Test, and Development Commands
Use `static\build.bat` to configure and compile the reusable OpenGL/DTI helpers (Debug by default). Afterwards run `build.bat` from the repo root to generate the Qt solution and compile the viewer; the script wraps `cmake -S . -B build` and `cmake --build build --config Debug`. Launch the debugger build via `build\Exe\Debug\DTIFiberViewer.exe`, or switch to `Release` in the build script for benchmarking. For ad-hoc configuration, `cmake -S . -B build -G "Visual Studio 17 2022"` followed by `cmake --build build --config Release` mirrors the scripted pipeline.

## Coding Style & Naming Conventions
All C++ targets use C++17, 4-space indentation, and braces on the same line as declarations (see `src\mainwindow.cpp`). Prefer PascalCase for Qt-facing classes (`MainWindow`, `GLFiberWidget`), camelCase for member functions, and snake_case for private data or utility statics. Group includes as: standard library, third-party (Qt/VTK), then project headers. Keep OpenGL headers (`glad/glad.h`) first to satisfy loader ordering, and use `// TODO(username, yyyy-mm-dd)` for short-lived follow-ups.

## Testing Guidelines
Automated unit tests are not yet wired in; validate changes by rebuilding both the static library and the viewer, then loading `data\AF_L.trk` to verify render and interaction paths. When touching parsing code, export sample statistics via the File ▸ Export command and diff against `data\tractography_export.json`. If you introduce automated tests (e.g., Qt Test or GoogleTest), place sources under `static/tests` or `tests` and add targets to the top-level `CMakeLists.txt`; document new commands in this guide.

## Commit & Pull Request Guidelines
Existing history favors concise Mandarin, verb-led summaries (e.g., `优化大量trk数据显示`); stay consistent and describe the observable behavior change, not the implementation detail. Reference related issues or plans (`TRK_IMPLEMENTATION_PLAN.md`) in the commit body when relevant. Pull requests should outline the problem, solution, and validation steps, plus screenshots or GIFs for UI changes and before/after frame timings for rendering tweaks. Mention any large data files touched, confirming they remain in `data` and below size limits.

## Data Handling & Configuration
Never commit new raw medical datasets; share access instructions instead. Large `.trk` files already tracked are test fixtures—avoid duplicating them and prefer down-sampled subsets when feasible. Record local configuration overrides (Qt, VTK paths) in `config.cmake` but do not hard-code machine-specific absolute paths in CMake or source files. If you need secrets or licenses, load them from environment variables rather than the repository.

## Recent Rendering Updates (2025-10-24)
- Fiber shading now mirrors DSI Studio: a 64³ occlusion volume with mean-filtered min/max maps drives per-vertex shadow factors while preserving a 0.05 brightness floor.
- Two dynamic point lights are anchored at the bounding-box diagonals; their intensity/ambient terms adapt to the dataset size and can be toggled or scaled via UI sliders.
- Tool bar includes “光照” and “阴影” sliders (0–100) that call `setLightingStrength`, `setLightingEnabled`, and `setShadowStrength`, enabling quick tuning without reloading data.
