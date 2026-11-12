$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$logDirectory = Join-Path $repoRoot 'build\install'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$mediaDirectory = Join-Path $repoRoot 'build\toolchains'
New-Item -ItemType Directory -Force -Path $mediaDirectory | Out-Null
$iso = Join-Path $mediaDirectory 'VS2013_RTM_DskExp_ENU.iso'
# The web bootstrapper's payload URLs now return 404. Use Microsoft's complete
# offline desktop ISO, retaining the original RTM v120 toolset and Windows SDK.
$url = 'https://download.microsoft.com/download/7/2/E/72E0F986-D247-4289-B9DC-C4FB07374894/VS2013_RTM_DskExp_ENU.iso'
& curl.exe --fail --location --retry 3 $url --output $iso
if ($LASTEXITCODE -ne 0) { throw 'VS2013 ISO download failed.' }
$expected = '1403f579199948e7c9d4046e17d1eb173cad64a23e59208cf078437fb278f376'
if ((Get-FileHash $iso -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'VS2013 ISO SHA-256 mismatch.'
}
$disk = Mount-DiskImage -ImagePath $iso -PassThru
try {
    $drive = ($disk | Get-Volume).DriveLetter
    $installer = "${drive}:\wdexpress_full.exe"
    $expectedInstaller = '0d03ba8588d86509dddadbf9f3a363fd407e9fa6561414d5ec6eb78a96e2d118'
    if ((Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedInstaller) {
        throw 'VS2013 installer SHA-256 mismatch.'
    }
    $signature = Get-AuthenticodeSignature $installer
    if ($signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw 'VS2013 installer is not signed by Microsoft.'
    }
    $log = Join-Path $logDirectory 'vs2013.log'
    $process = Start-Process $installer -ArgumentList @('/Quiet', '/NoRestart', '/NoWeb', '/Log', $log) -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) {
        throw "VS2013 installation failed: $($process.ExitCode). See $log"
    }
} finally {
    Dismount-DiskImage -ImagePath $iso | Out-Null
}
$compiler = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio 12.0\VC\bin\cl.exe'
$msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
if (!(Test-Path $compiler) -or !(Test-Path $msbuild)) {
    throw 'Installation did not supply the required v120 compiler and MSBuild 12.0.'
}
