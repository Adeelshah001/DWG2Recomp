param([string]$GameFolder = (Split-Path $PSScriptRoot -Parent), [string]$Package = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
function Get-UpdateHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$target = (Resolve-Path -LiteralPath $GameFolder).Path
$packageRoot = (Resolve-Path -LiteralPath $Package).Path
$manifest = Get-Content -LiteralPath (Join-Path $packageRoot 'update.json') -Raw | ConvertFrom-Json
if ($manifest.portableBuild) {
    & (Join-Path $PSScriptRoot 'apply-portable-update.ps1') -GameFolder $target -Package $packageRoot
    return
}
foreach ($process in (Get-Process -ErrorAction SilentlyContinue)) {
    try { $path = $process.Path } catch { continue }
    if ($path -and $path.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Close the game and launcher before applying this update.' }
}
$gameExe = Join-Path $target 'bin\dwg2.exe'
if (!(Test-Path -LiteralPath $gameExe)) { throw 'Choose the existing installed game folder.' }
$baseMatches = (Get-UpdateHash $gameExe) -eq $manifest.baseExecutableSha256
if (!$baseMatches) {
    $marker = Join-Path $target 'installed.txt'
    $installed = if (Test-Path -LiteralPath $marker) { @(Get-Content -LiteralPath $marker) } else { @() }
    $baseMatches = $installed.Count -ge 2 -and $manifest.compatibleLauncherVersions -contains $installed[0] -and $installed[1] -eq $manifest.gameExecutableSha256
}
if (!$baseMatches) { throw 'This update targets a different game build. No files were changed.' }
$allowed = @('START_DWG2.exe', 'bin/rexruntime.dll', 'bin/rexgpu-xenos.dll', 'READ_ME_FIRST.txt')
$seen = @{}
foreach ($item in $manifest.files) {
    if ($allowed -notcontains $item.path -or $seen.ContainsKey($item.path)) { throw 'Invalid or duplicate update path.' }
    $seen[$item.path] = $true
    $source = Join-Path $packageRoot $item.path
    if ((Get-UpdateHash $source) -ne $item.sha256) { throw "Update hash mismatch: $($item.path)" }
    $dest = Join-Path $target $item.path
    if ((Get-Item -LiteralPath (Split-Path $dest -Parent)).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked update folders are not supported.' }
    if ((Test-Path -LiteralPath $dest) -and ((Get-Item -LiteralPath $dest).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked update destinations are not supported.' }
}
foreach ($required in @('START_DWG2.exe', 'READ_ME_FIRST.txt')) {
    if (!$seen.ContainsKey($required)) { throw 'Incomplete update package.' }
}
$backup = Join-Path $target ('.update-backup\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$changed = @()
try {
    foreach ($item in $manifest.files) {
        $dest = Join-Path $target $item.path
        $saved = Join-Path $backup $item.path
        New-Item -ItemType Directory -Path (Split-Path $saved -Parent) -Force | Out-Null
        if (Test-Path -LiteralPath $dest) { Copy-Item -LiteralPath $dest -Destination $saved }
        $changed += $item.path
        Copy-Item -LiteralPath (Join-Path $packageRoot $item.path) -Destination $dest -Force
    }
} catch {
    foreach ($relative in $changed) {
        $saved = Join-Path $backup $relative
        if (Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination (Join-Path $target $relative) -Force }
    }
    throw
}
Write-Host "Updated to $($manifest.version). Game data, saves, cache and display settings were preserved."
