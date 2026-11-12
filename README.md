# CS3241 Assignment 5 — Ray Tracing

Assignment instructions: [Lab 5 Ray Tracing.pdf](Lab%205%20Ray%20Tracing.pdf).
The program includes the original four-sphere scene and a stylised solar-system
scene. A passing build/startup check does not establish assignment correctness.
Submit only `main.cpp`, as the PDF directs.

## Camera and scenes

- **Up / Down:** move forward/backward along the camera's viewing direction.
- **Left / Right:** strafe left/right relative to the camera.
- **Hold left mouse button and drag:** look around; pitch is limited to avoid flipping.
- **Scroll up / down:** zoom in/out by changing the field of view.
- **S:** switch scenes and restore that scene's starting camera.
- **R:** cycle internal resolution: **1.0× → 0.9× → 0.7× → 0.5× → 0.3× → 1.0×**.
- **Home:** reset the current scene's camera and zoom.
- **Space:** pause/resume the solar-system animation.
- **F11:** toggle fullscreen; **Q:** quit.

Rendering starts at native window resolution (1.0×). R applies the selected
multiplier to both internal dimensions; the title shows the percentage and pixel
dimensions. The image fills the entire client area at every window size, including
fullscreen, while preserving the vertical field of view and the shape of spheres.
Resolution changes only when explicitly selected or when the window is resized;
moving, looking, and zooming never automatically lower the selected quality.

The second scene has an emissive Sun and eight plain-coloured planets against a
procedural star field and Milky Way band. Relative planet sizes and orbit spacing
are deliberately compressed for readability, rather than astronomically accurate.
The Sun is largest, followed by Jupiter and Saturn, then the ice giants and rocky
planets. Its position also supplies the scene's point light. Scene switching restores
the original scene's materials, lighting, and camera.

Planets follow elliptical orbits with the Sun at a focus, moving faster near the
Sun. Approximate real eccentricities and orbital inclinations are used, while
periods follow Kepler's third law for the compressed displayed distances: Earth
orbits in 24 seconds, Mercury in about 10 seconds, and Neptune in about 138 seconds.
Orbital planes are tilted toward the viewer so motion is visible. Orbital spacing
keeps the displayed spheres from colliding. This is a stylised Kepler model, not
an astronomical ephemeris or a gravitational simulation. The orbital model follows
[NASA's explanation of Kepler's laws](https://science.nasa.gov/solar-system/orbits-and-keplers-laws/)
and [JPL's elliptic-orbit equations and planetary elements](https://ssd.jpl.nasa.gov/planets/approx_pos.html).

## Linux

Dependencies: `g++ make python3 freeglut3-dev libglu1-mesa-dev`.
For headless startup checks also install `xvfb xauth`.

```sh
make                        # Debug build, zero warnings/errors required
make check CONFIG=Release   # Release build, zero warnings/errors required
make run                    # Build and launch (requires a display)
xvfb-run -a make smoke      # Build and check startup for five seconds
make renderer-test          # CPU camera/orbit/projection regressions
make clean                  # Remove generated Linux outputs
```

Both `main.cpp` and `vector3D.cpp` are compiled on every build. Outputs are
`build/linux/Debug/cs3241_assgt5` and `build/linux/Release/cs3241_assgt5`.
`CXX=clang++` selects Clang instead of GCC. `CONFIG` accepts only `Debug` or
`Release`. There is no non-strict build mode.

## Windows and configuration parity

Unlike assignment 4, this starter specifies **Visual Studio 2013 v120**, **Win32**
only, `/W3`, Unicode, and Debug/Release configurations. The original `.sln`,
`.vcxproj`, package targets, and Windows dependency binaries remain byte-for-byte
unchanged and are checked against a SHA-256 manifest before either platform builds.
Windows builds use these actual files through **MSBuild 12.0**, not reconstructed
compiler commands, CMake, or a retargeted modern Visual Studio project.

The extracted files do not match the solution's directory references. The Windows
script copies them into the original expected layout under `build/windows-project/`.
It changes only output/intermediate directories through MSBuild properties.

```powershell
# With VS2013 C++ desktop tools and Python installed:
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
python scripts/smoke-test.py 'build/Win32/Debug/Lab5 Ray Tracing.exe'
```

Windows logs and executable/DLL outputs are under `build/Win32/<configuration>/`.
MSBuild 12.0 does not support binary logs; full diagnostic text logs are retained.
Every build is a Rebuild and fails on compiler, linker, or MSBuild warnings in its
log, retaining the professor's `/W3` rather than changing the warning level.

VS2013 has no `/std` switch and implements a partial C++11-era language/library
feature set. Linux explicitly uses `-std=c++11`; do not introduce C++14 or newer
features. GCC is not an exact substitute for v120: Windows CI is the compatibility
authority, including VS2013's missing C++11 features and Microsoft extensions.

| Setting | Original Windows project | Linux equivalent |
| --- | --- | --- |
| Warnings | `/W3`, full-log zero-warning gate | `-Wall -Wextra -Wpedantic -Werror` |
| Linker warnings | Full-log zero-warning gate | `-Wl,--fatal-warnings` |
| Compiler defaults | `/GS`, `/Oy-`, `/fp:precise`, `/EHsc` | `-fstack-protector -fno-omit-frame-pointer -fno-fast-math -fexceptions` |
| Debug | Disabled optimisation, debug runtime/information, `_DEBUG` | `-O0 -g -D_DEBUG` |
| Release | MaxSpeed, whole-program optimisation, function linking, intrinsics, COMDAT/reference optimisation | `-O2 -g -flto -ffunction-sections -finline-functions -Wl,--gc-sections` |
| Release assertions | `NDEBUG` | `-DNDEBUG` |
| Common macros | `_CONSOLE`, `_LIB`, Unicode, plus Windows platform macros | `_CONSOLE`, `_LIB`, `UNICODE`, `_UNICODE`; no false `WIN32` definition |
| Architecture/runtime | Win32 and MSVC's selected debug/release CRT | Native Linux architecture and system runtime |

Compiler switches, warning sets, debug runtimes, and optimisation passes cannot be
identical across MSVC and GCC. The table states the practical equivalents; Linux
success alone never proves the supplied Windows configuration passes.

## Regression tests

The committed `tests/renderer-check.cpp` checks camera movement and orthonormal
axes, zoom limits, scene restoration, the original 600 × 400 projection pixel by
pixel, all resolution dimensions (including a 1 × 1 client), orbital periods,
periodicity, faster perihelion motion, and nonintersecting orbital shells.
Checks remain active in Release builds; they do not rely on `assert`/`NDEBUG`.

`scripts/interactive-test.py` drives the actual application window, captures real
client-area screenshots, and rejects runtime diagnostics. It checks arrow and
mouse controls, release behavior, scroll zoom, Home reset, S switching, moving
planets, pause/resume, all five R scales and wraparound, odd-sized resize, round
spheres, no letterboxing, fullscreen entry/exit, unchanged quality while moving,
and clean Q exit. Reports and screenshots are written under `build/`.

Run the complete Linux suite with Pillow, Xvfb, Openbox, xdotool, and x11-utils
installed (in addition to the build dependencies):

```sh
python3 -m pip install Pillow==11.3.0
xvfb-run -a -s '-screen 0 1280x900x24' bash -e -c '
  openbox >/tmp/cs3241-openbox.log 2>&1 &
  wm_pid=$!
  trap "kill $wm_pid 2>/dev/null || true" EXIT
  make test CONFIG=Debug
  make test CONFIG=Release
'
```

On native Windows, run the existing application build and smoke test, then:

```powershell
python scripts/interactive-test.py 'build/Win32/Debug/Lab5 Ray Tracing.exe' --output build/Win32/Debug/interactive
./scripts/build-windows.ps1 -Configuration Debug -RendererTests
& 'build/Win32/Debug/renderer-tests/Lab5 Ray Tracing.exe'
```

Repeat with `Release`. The harness builds in a separate staging directory using
the original unmodified solution, project, v120 compiler, and configuration flags.
The application and regression executables have separate output directories.
All four CI jobs run these tests and upload the desktop screenshots and reports.

Source compatibility fixes leave assignment behavior unfinished: portable GLUT
includes and timestamp types, explicit `<cfloat>`, safe virtual destruction,
acknowledged unused placeholders, and an explicit default copy constructor for
the supplied vector class. The vector constructor has the same copy behavior as
before and only addresses modern compiler diagnostics. Redundant semicolons in
the same header were also removed for Clang. The submission still uses
the professor's vector library.

The starter also passed `GL_DOUBLEBUFFER` to `glClear`, which is not a valid
clear-mask bit. This was removed. Setting `CS3241_CHECK_GL=1` enables frame-level
`glGetError` checks; the startup test enables this on both platforms.

## GitHub checks

`.github/workflows/cross-platform-build.yml` checks Debug and Release on Windows
and Linux. Windows installs the original v120 desktop toolchain from Microsoft's
SHA-256-pinned offline ISO and installer; a missing toolchain or installation error fails the job
rather than silently retargeting. Both platforms also check GLUT startup.
Windows executable, bundled DLLs, build logs, and installer diagnostics are saved
as Actions artifacts. Each Windows job runs the actual application on the runner's
Windows desktop, waits for a completed error-free frame, captures the 600 × 400
client area, switches scenes with S, captures scene 1, and quits with Q (exit 0).
Artifacts contain `screenshots/scene-0.png`, `scene-1.png`, `runtime-check.json`,
and stdout/stderr logs. Black captures, missing frames/windows, runtime diagnostics,
and nonzero exits fail the job. This checks startup and rendering infrastructure;
it does not verify the unfinished assignment's ray-tracing algorithm.

Like assignment 4, `make install-hooks` installs a pre-push hook that submits each
outgoing commit to a temporary `ci-validation/*` branch and waits for all four
checks before pushing the requested branch. It requires authenticated GitHub CLI
(`gh`). Main's repository rules require these checks and block force pushes.
