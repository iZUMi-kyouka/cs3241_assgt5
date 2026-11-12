# Project and priorities

This repository is `iZUMi-kyouka/cs3241_assgt5`, CS3241 Lab 5 (ray tracing).
Read `README.md`, `LANDMINDES.md`, and `Lab 5 Ray Tracing.pdf` before changing
assignment behavior. The ray-tracing steps remain unfinished. Build/startup
success and screenshots do not establish assignment correctness.

The grader uses native Windows. Preserve the supplied Visual Studio 2013 **v120**
toolset, **Win32** platform, **Debug/Release** configurations, `/W3`, Unicode,
original macros, CRT/SDK defaults, and optimisation/link settings. Assignment 4
uses a different toolset; copy its validation approach, not its compiler settings.

## Build authority and source changes

- Windows must build the supplied `.sln` and `.vcxproj` through MSBuild 12.0.
  Do not retarget to v143 or replace these builds with CMake/compiler commands.
- `scripts/build-windows.ps1` stages the expected original directory structure.
  It changes only output/intermediate paths. Original project and dependency
  files are protected by `scripts/original-files.sha256.json` and its verifier.
- Do not change verifier hashes to hide altered originals. Do not replace the
  bundled Windows libraries, DLLs, headers, or NuGet targets without explicit scope.
- VS2013 has no `/std` switch and incomplete C++11 support. Linux uses
  `-std=c++11`; keep source compatible with v120, not merely modern GCC/Clang.
  Do not introduce C++14/17/20 features.
- The PDF requires submission of **only `main.cpp`**. Implement assignment work
  there. The local `vector3D.h` has only warning-clean helper adjustments; do not
  make the submission depend on functionality absent from the professor's helper.
- Preserve platform-specific GLUT includes, portable timestamp types, the virtual
  object destructor, and the opt-in `CS3241_CHECK_GL` frame diagnostics.
- Preserve the student's intersection, Phong, and recursive-reflection algorithms
  when changing UI. Scene 0 retains the four original spheres; scene 1 contains
  the Sun and eight planets with compressed relative sizes and a procedural sky.
- Keep arrow-key movement, left-drag look, scroll zoom, S scene switching, Home
  camera reset, Space orbit pause, and F11 fullscreen. S must not become a movement
  key. R cycles internal scales 1.0, 0.9, 0.7, 0.5, 0.3; this explicit user request
  supersedes the earlier always-native preference. Fill the entire client area
  without distorting aspect ratio. Never automatically lower quality while moving.
- Solar planets use accelerated elliptical Kepler orbits on compressed displayed
  distances, with Earth taking 24 seconds. Keep real-time progression independent
  of rendering speed, and preserve a static original scene 0.

## C++ formatting

Use Egyptian braces: opening braces stay on the same line as functions, classes,
and control statements, with `} else {` on one line. `.clang-format` is authoritative:
four spaces, no indentation tabs, and no one-line function/control-statement bodies.
The user's Vim formats C/C++ files on save; Space f formats manually. Preserve
C++11 compatibility and include ordering when formatting.

## Validation

Linux prerequisites: `g++ make python3 freeglut3-dev libglu1-mesa-dev`; headless
checks also need `xvfb xauth`. Use the public Make targets from the repository root:

```sh
make check
make check CONFIG=Release
xvfb-run -a make smoke
xvfb-run -a make smoke CONFIG=Release
```

Every Make build recompiles both C++ files with fatal compiler/linker warnings.
Do not suppress warnings, introduce a permissive `STRICT=0` mode, or weaken the
checks. Fix the source. Compiler flags have practical platform equivalents, not
identical semantics; Windows CI remains the compatibility authority.

Windows commands, on a machine with VS2013 desktop tools and Python:

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
python -m pip install Pillow==11.3.0
python scripts/smoke-test.py 'build/Win32/Debug/Lab5 Ray Tracing.exe' --screenshot-dir build/Win32/Debug/screenshots
```

Windows performs a Rebuild, preserves `/W3`, and rejects compiler/linker/MSBuild
warnings in the full diagnostic log. Do not substitute a newer native Windows
toolset, Wine, MinGW, or a cross-compile for the required v120 CI check. Native
Windows compilation need not run on this Linux workstation.

The GitHub workflow tests four required jobs: `Windows Win32 Debug`,
`Windows Win32 Release`, `Linux Debug`, and `Linux Release`. Windows must launch
the actual executable on the runner's Windows desktop, pass frame-level OpenGL
checks, capture both 600 × 400 scenes, switch with S, and quit with Q (exit 0).
Preserve the real screenshots, executable/DLLs, full logs, and JSON runtime report
as artifacts. Never fabricate screenshots or relax capture failures to pass CI.
Only report native Windows verification after both Windows jobs actually pass.

For C++/build/startup changes, run both local configurations and startup checks.
Do not implement ray-tracing tasks simply to make a build or startup check pass.
Visual/rendering changes additionally need inspection of the resulting images.
Run the committed CPU and desktop regression suites with `make test` for each
Linux configuration under Xvfb plus Openbox (see README). Both native Windows
configurations also run `scripts/interactive-test.py` and build the renderer
harness through `scripts/build-windows.ps1 -RendererTests`. Keep Release checks
enabled without relying on `assert`. Upload screenshots/reports as CI artifacts.

## Git and maintenance

`make install-hooks` enables `.githooks/pre-push` using authenticated GitHub CLI.
It validates outgoing commits on temporary `ci-validation/*` branches before the
requested push. Main requires all four checks and blocks force pushes without
bypass actors. Do not skip the hook or main's protection; `--no-verify` inside the
hook is only for its temporary validation refs and cleanup to prevent recursion.

Keep generated outputs in ignored `build/`; preserve `.gitattributes`, especially
byte-preserving rules for original project/package files. Read `LANDMINDES.md`
before investigating an already documented failure, and add newly encountered
errors with their cause, concrete fix, and verification status. Keep outstanding
failures distinct from verified fixes.
