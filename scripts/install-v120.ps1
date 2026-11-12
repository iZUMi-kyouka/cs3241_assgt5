$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$logDirectory = Join-Path $repoRoot 'build\install'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$installer = Join-Path $logDirectory 'wdexpress_full.exe'
# Microsoft's VS2013 Express for Windows Desktop bootstrapper (not a runtime-only
# redistributable). Pin the installer and verify its Authenticode publisher.
$url = 'https://download.microsoft.com/download/7/2/E/72E0F986-D247-4289-B9DC-C4FB07374894/wdexpress_full.exe'
Invoke-WebRequest $url -OutFile $installer
$expected = '0d03ba8588d86509dddadbf9f3a363fd407e9fa6561414d5ec6eb78a96e2d118'
if ((Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'VS2013 installer SHA-256 mismatch.'
}
$signature = Get-AuthenticodeSignature $installer
if ($signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw 'VS2013 installer is not signed by Microsoft.'
}
$log = Join-Path $logDirectory 'vs2013.log'
$process = Start-Process $installer -ArgumentList @('/Quiet', '/NoRestart', '/Log', $log) -Wait -PassThru
if ($process.ExitCode -notin @(0, 3010)) {
    throw "VS2013 installation failed: $($process.ExitCode). See $log"
}
$compiler = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio 12.0\VC\bin\cl.exe'
$msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
if (!(Test-Path $compiler) -or !(Test-Path $msbuild)) {
    throw 'Installation did not supply the required v120 compiler and MSBuild 12.0.'
}
