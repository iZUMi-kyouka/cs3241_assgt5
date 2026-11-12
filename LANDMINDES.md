# Setup errors and established fixes

Recorded during initial setup on 2026-11-12. The filename intentionally follows
the user's requested `LANDMINDES` spelling. Read this before repeating setup work.

## Windows toolchain and installation

**Assignment 4 is not this assignment's compiler configuration.** Its project uses
VS2022/v143 and additional architectures. Assignment 5's original project requires
VS2013/v120, Win32 only, and `/W3`; it does not set a language-standard switch.
Linux explicitly targets C++11-era source compatibility. Never silently retarget
Windows to make the hosted build easier. C++11 on modern Linux still does not
prove compatibility with VS2013's incomplete implementation.

**The VS2013 web installer downloads but cannot install.** Both original Windows
CI configurations failed with installer exit **1603**. Installer logs revealed
HTTP **404** responses from old payload URLs, including the targets of
`LinkId=319051`, `319090`, and `318558`. This is not a C++ compilation failure.
Retrying the same bootstrapper, installing runtime redistributables alone, or
changing compiler flags will not fix it.

The replacement in `scripts/install-v120.ps1` uses Microsoft's complete RTM
desktop ISO, mounts it, and invokes its installer with `/NoWeb`. The ISO and its
Microsoft-signed installer are SHA-256 pinned. The official offline URL is:

`https://download.microsoft.com/download/7/2/E/72E0F986-D247-4289-B9DC-C4FB07374894/VS2013_RTM_DskExp_ENU.iso`

ISO SHA-256:
`1403f579199948e7c9d4046e17d1eb173cad64a23e59208cf078437fb278f376`

The ISO was downloaded and hashed locally. Actions run `43144522490` successfully
installed the native v120 desktop tools and reached compilation on both Windows
configurations. That candidate was then rejected for C4996 (see below); installation
success alone is not a passing build/runtime check.

**MSBuild 12.0 does not support `/bl`.** Retain full diagnostic text logging rather
than copying assignment 4's modern binary-logger command. Reject warning messages
after every Rebuild, including linker and MSBuild diagnostics.

## Project layout and byte preservation

**The extracted files do not match their own relative references.** The solution
expects `Lab5 Ray Tracing/Lab5 Ray Tracing.vcxproj`; that project expects
`../main.cpp`, `../vector3D.cpp`, `../vector3D.h`, and `../packages/`. All were
initially extracted into the repository root. The Windows build script recreates
this hierarchy in `build/windows-project/<configuration>/` using unmodified file
copies. Do not rewrite the original project paths or point source includes at
machine-specific directories.

**Git newline conversion breaks checksum verification.** `packages.config` is an
original CRLF file included in the SHA-256 manifest. Git's automatic text
normalisation initially put LF bytes into the index. `.gitattributes` now marks
`packages.config`, original solution/project/filter files, and `/packages/**` as
`-text`. A fresh local clone passes verification. Preserve this policy rather
than updating hashes to converted files.

## C++ warnings and portability

**The untouched starter does not compile warning-free on current Linux tools.**
Necessary fixes, without implementing assignment steps:

- `GL\glut.h` is a Windows-style include. Use `GL/glut.h` on Windows and the
  system `<GL/glut.h>` on Linux.
- `DBL_MAX` needs the explicit `<cfloat>` include.
- `__int64` is a Microsoft extension. `auto` preserves the actual chrono count
  type on both platforms without assuming its underlying integer typedef.
- Unused intersection/recursion placeholders, `mint`, and keyboard coordinates
  trigger diagnostics. Acknowledge the unfinished placeholders with `(void)` and
  leave unused callback parameter names out; do not disable warning categories.
- Deleting through the polymorphic `RtObject` base needs a virtual destructor.
- `Vector3` has a user-provided assignment operator but relied on an implicit
  copy constructor. GCC reports `-Wdeprecated-copy`; explicitly defaulting that
  constructor preserves copy semantics and removes the warning.
- Two `};;` sequences in `vector3D.h` trigger Clang `-Wextra-semi` and hosted
  GCC 13 `-Wpedantic`. Local GCC 15 did not diagnose them in the first build.
  Remove the redundant semicolons; do not assume a newer GCC's silence proves
  all configured compilers are warning-free.

**MSVC C4996 on `getenv` is a real warning under the original `/W3`.** The first
offline-install candidate compiled but was correctly rejected for the diagnostic
helper's `std::getenv`. Do not define `_CRT_SECURE_NO_WARNINGS`, disable C4996, or
relax the log gate. Windows now uses `GetEnvironmentVariableA`; Linux retains
`std::getenv` behind its platform branch.

GCC and Clang both passed local Debug/Release builds after the initial helper fixes. Local
Xvfb startup checks passed. A deliberate temporary `#warning` injection was
rejected by `make check`, confirming warnings cause failure. Do not commit the
probe or add suppression flags.

## OpenGL runtime and screenshots

**Staying alive does not prove an error-free OpenGL frame.** The starter called
`glClear` with `GL_DOUBLEBUFFER`, which is a display-mode/query constant, not a
valid clear-mask bit. Remove it from `glClear`; retain double buffering in GLUT
initialisation. `CS3241_CHECK_GL=1` enables `glGetError` checking after each frame.
Local Debug/Release now pass these frame checks.

**The assignment is still a skeleton.** Sphere intersection still always returns
a miss and scene 1 still has no settings. Both screenshots currently show the
background. Do not interpret that as a broken window/toolchain or implement the
assignment merely to make screenshots look finished.

**Windows screenshot evidence must come from the running native executable.**
The test waits for rendering and an error-free frame, finds its visible window
by process ID, brings it forward, and captures its 600 × 400 client area with
Windows desktop capture. It also sends S, captures scene 1, then sends Q and
requires exit 0. The JSON report records the executable/image hashes and native
platform. Screenshots are runtime evidence, not golden-image correctness tests.

## GitHub Actions and log retrieval

**Old action versions add deprecation annotations.** Initial `checkout@v4` and
`setup-python@v5` runs emitted Node 20 deprecation warnings. The workflow now
uses the same current action versions as assignment 4: checkout `v7.0.1`,
setup-python `v7.1.0`, upload-artifact `v7.0.2`.

**Run-level logs may be unavailable while other matrix jobs are running.** Use
`gh api repos/iZUMi-kyouka/cs3241_assgt5/actions/jobs/<job-id>/logs` for a completed
job instead of repeatedly retrying `gh run view --log` on an unfinished run.
PowerShell logs can contain terminal escape sequences; `gh api` supports
`--allow-escape-sequences`, while `gh run view` does not. Save logs to a local
file and strip escapes for inspection when needed.

**The push gate intentionally waits.** `.githooks/pre-push` pushes the exact
candidate commit to a temporary validation branch, watches Actions, removes the
branch, and pushes main only after success. A failing candidate is blocked. Do
not disable this hook or the server's four required checks to get a green-looking
main branch. Fix the failure and validate the new candidate.
