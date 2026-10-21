<#
.SYNOPSIS
Creates game-free launcher/source and existing-install update ZIPs.
.EXAMPLE
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -Version 0.1-alpha -OutputFolder C:\Releases\DWG2-v0.1-alpha
.DESCRIPTION
Builds the launcher, includes explicitly listed source files, and creates a
launcher-only update by default. RuntimeFolder optionally adds exactly the two
reviewed host DLLs to the update. It never copies game files or an installation.
The launcher version must already match Version. OutputFolder must be new.
#>
param([string]$Version = '0.1-alpha', [Parameter(Mandatory=$true)][string]$OutputFolder, [string]$RuntimeFolder)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+(?:\.\d+)?-(?:alpha(?:\.\d+)?|local-preview)$') { throw 'Use an alpha or local-preview version.' }
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$output = [IO.Path]::GetFullPath($OutputFolder)
if ($output.Equals($repo,[StringComparison]::OrdinalIgnoreCase) -or $output.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Keep release output outside the source checkout.' }
if (Test-Path -LiteralPath $output) { throw 'Choose a new output folder to avoid replacing an existing release.' }
$launcherSource = [IO.File]::ReadAllText((Join-Path $repo 'launcher\Launcher.cs'))
if (!$launcherSource.Contains('Version = "'+$Version+'"')) { throw 'Launcher source version differs from release version.' }
function Hash([string]$Path) {
    $stream=[IO.File]::OpenRead($Path); $sha=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$sources=@(
    '.gitattributes','.gitignore','README.md','BUILDING.md','THIRD_PARTY_LICENSE.txt',
    'READ_ME_FIRST.txt','RELEASE_GUIDE.md','WISHLIST.md','TEST_CHECKLIST.md',
    'config/compiler.json','config/dwg2.toml','config/engine-link.json','config/gpu-link.json',
    'launcher/Launcher.cs','scripts/build-launcher.ps1','scripts/build-game.ps1',
    'scripts/build-local.py','scripts/apply-update.ps1','scripts/package-release.ps1',
    'src/main.cpp','src/generator.cpp','src/window_controls.h','src/dwg2_metrics.h',
    'src/window_sdl.cpp.patch','src/sdl_input_driver.cpp.patch','src/presenter.cpp.patch',
    'src/d3d12_presenter.cpp.patch','src/debug_overlay.cpp.patch','src/xboxkrnl_video.cpp.patch',
    'src/input_config.h','src/input_host.h','src/input_ui.h','src/resolution_control.h',
    'src/resolved_ranges.h','src/d3d12_live_resolution.inc','src/mnk_input_driver.cpp.patch',
    'src/d3d12_command_processor.cpp.patch','src/d3d12_command_processor.h.patch','src/texture_cache.h.patch'
)
foreach ($name in $sources) {
    $item=Get-Item -LiteralPath (Join-Path $repo $name)
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked release sources are not supported.' }
}
New-Item -ItemType Directory -Path $output | Out-Null
$full=Join-Path $output "DWG2Recomp-v$Version-windows-x64"
$update=Join-Path $output "DWG2Recomp-update-v$Version"
New-Item -ItemType Directory -Path $full,$update | Out-Null
foreach ($name in $sources) {
    $dest=Join-Path $full $name
    New-Item -ItemType Directory -Path (Split-Path $dest -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $dest
}
& (Join-Path $repo 'scripts\build-launcher.ps1') -Output (Join-Path $full 'START_DWG2.exe')
foreach ($name in @('START_DWG2.exe','READ_ME_FIRST.txt','THIRD_PARTY_LICENSE.txt')) {
    Copy-Item -LiteralPath (Join-Path $full $name) -Destination (Join-Path $update $name)
}
Copy-Item -LiteralPath (Join-Path $repo 'scripts\apply-update.ps1') -Destination (Join-Path $update 'apply-update.ps1')
$payload=@('START_DWG2.exe','READ_ME_FIRST.txt')
if ($RuntimeFolder) {
    New-Item -ItemType Directory -Path (Join-Path $update 'bin') | Out-Null
    foreach ($name in @('rexruntime.dll','rexgpu-xenos.dll')) {
        $binary=Get-Item -LiteralPath (Join-Path $RuntimeFolder $name)
        if ($binary.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked runtime binaries are not supported.' }
        Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $update ('bin\'+$name))
        $payload+=('bin/'+$name)
    }
}
$manifest=[ordered]@{
    version=$Version
    baseExecutableSha256='75994f07b80a7aaba9a5e280615a7e706402d039800b7e60b18a27ba154ffd21'
    compatibleLauncherVersions=@('0.2.0-preview','0.2-local-preview','0.1-alpha',$Version)
    gameExecutableSha256='0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b'
    files=@($payload | ForEach-Object { @{path=$_;sha256=(Hash (Join-Path $update $_))} })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $update 'update.json') -Encoding UTF8
$commit=(& git -C $repo rev-parse HEAD)
if ($LASTEXITCODE) { throw 'Unable to identify source commit.' }
$release=[ordered]@{version=$Version;sourceCommit=$commit;gameFilesIncluded=$false;localToolchainRequired=$true;files=@()}
foreach ($file in (Get-ChildItem -LiteralPath $full -Recurse -File | Sort-Object FullName)) {
    $release.files+=@{path=$file.FullName.Substring($full.Length+1).Replace('\','/');sha256=(Hash $file.FullName)}
}
$release | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $full 'release.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$checksums=@()
foreach ($folder in @($full,$update)) {
    $zip=$folder+'.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($folder,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
    $checksums+=(Hash $zip)+'  '+[IO.Path]::GetFileName($zip)
}
[IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS.txt'),$checksums)
Write-Host "Release files created in $output. Review the ZIP entries before publishing."
