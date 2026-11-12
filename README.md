# CS3241 Assignment 5 — Ray Tracing

Assignment instructions: [Lab 5 Ray Tracing.pdf](Lab%205%20Ray%20Tracing.pdf).
The ray-tracing steps are still placeholders. A passing build/startup check does
not establish assignment correctness. Submit only `main.cpp`, as the PDF directs.

## Linux

Dependencies: `g++ make python3 freeglut3-dev libglu1-mesa-dev`.
For headless startup checks also install `xvfb xauth`.

```sh
make                        # Debug build, zero warnings/errors required
make check CONFIG=Release   # Release build, zero warnings/errors required
make run                    # Build and launch (requires a display)
xvfb-run -a make smoke      # Build and check startup for five seconds
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
