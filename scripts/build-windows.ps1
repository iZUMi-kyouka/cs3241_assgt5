[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$RendererTests
)

$ErrorActionPreference = 'Stop'
$env:VSLANG = '1033'
$repoRoot = Split-Path $PSScriptRoot -Parent
$msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
$compiler = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio 12.0\VC\bin\cl.exe'
if (!(Test-Path $msbuild) -or !(Test-Path $compiler)) {
    throw 'Install Visual Studio 2013 C++ desktop tools (v120) and MSBuild 12.0. No toolset fallback is permitted.'
}

& python (Join-Path $PSScriptRoot 'verify-project.py')
if ($LASTEXITCODE -ne 0) { throw 'Original project verification failed.' }

# Reconstruct the supplied solution's expected directory layout. Application
# builds preserve source bytes; test builds stage a separate harness below.
# Neither mode edits the solution/project/dependencies or compiler settings.
$stage = Join-Path $repoRoot "build\windows-project\$Configuration"
$projectDir = Join-Path $stage 'Lab5 Ray Tracing'
$output = Join-Path $repoRoot "build\Win32\$Configuration"
if ($RendererTests) {
    $stage = Join-Path $repoRoot "build\windows-renderer-tests\$Configuration"
    $projectDir = Join-Path $stage 'Lab5 Ray Tracing'
    $output = Join-Path $repoRoot "build\Win32\$Configuration\renderer-tests"
}
New-Item -ItemType Directory -Force -Path $projectDir, $output | Out-Null
Copy-Item (Join-Path $repoRoot 'Lab5 Ray Tracing.sln') $stage -Force
foreach ($name in @('Lab5 Ray Tracing.vcxproj', 'Lab5 Ray Tracing.vcxproj.filters', 'packages.config')) {
    Copy-Item (Join-Path $repoRoot $name) $projectDir -Force
}
foreach ($name in @('main.cpp', 'vector3D.cpp', 'vector3D.h')) {
    Copy-Item (Join-Path $repoRoot $name) $stage -Force
}
if ($RendererTests) {
    # Compile the regression harness through the same unmodified v120 project.
    # Only this separate staging area's translation unit is replaced.
    Copy-Item (Join-Path $stage 'main.cpp') (Join-Path $stage 'application-main.cpp') -Force
    # The project compiles ../main.cpp. Put the wrapper there and adapt its one
    # application include; leave the repository's test and application unchanged.
    $harness = Get-Content (Join-Path $repoRoot 'tests\renderer-check.cpp') -Raw
    $harness = $harness.Replace('#include "../main.cpp"', '#include "application-main.cpp"')
    [System.IO.File]::WriteAllText((Join-Path $stage 'main.cpp'), $harness)
}
Copy-Item (Join-Path $repoRoot 'packages') $stage -Recurse -Force
$logPath = Join-Path $output 'msbuild.log'
$solution = Join-Path $stage 'Lab5 Ray Tracing.sln'

# v120 has no /std switch and MSBuild 12 has no binary logger. Preserve /W3,
# all original Debug/Release flags, SDK defaults, runtime selection, and Win32.
# Rebuild and inspect the complete text log; no warning suppression or retargeting.
& $msbuild $solution /nologo /m /t:Rebuild /verbosity:normal `
    "/p:Configuration=$Configuration" /p:Platform=Win32 /p:VisualStudioVersion=12.0 `
    "/p:OutDir=$output\" "/p:IntDir=$output\obj\" `
    /fl "/flp:LogFile=$logPath;Verbosity=diagnostic;Encoding=UTF-8"
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed ($LASTEXITCODE). See $logPath"
}
if (!(Test-Path $logPath)) { throw 'MSBuild did not produce its diagnostic log.' }
$warnings = Select-String -Path $logPath -Pattern '\bwarning(?:\s+[A-Za-z]+\d+)?\s*:'
if ($warnings) {
    $warnings | ForEach-Object { Write-Host $_.Line }
    throw "Compiler/linker/MSBuild warning detected. See $logPath"
}
if (!(Test-Path (Join-Path $output 'Lab5 Ray Tracing.exe'))) {
    throw 'Expected executable is missing.'
}
Write-Host 'Verified: original v120 Win32 solution rebuilt with zero warnings or errors.'
