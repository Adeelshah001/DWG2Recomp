param([Parameter(Mandatory=$true)][string]$GameFolder,[Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $GameFolder).Path.TrimEnd('\')
$packageRoot=(Resolve-Path -LiteralPath $Package).Path.TrimEnd('\')
function Update-Hash([string]$path) {
    $stream=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose();$stream.Dispose() }
}
function Safe-Path([string]$root,[string]$relative) {
    if ([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative) -or $relative.Contains('\') -or $relative.Contains(':')) { throw 'Invalid package path.' }
    foreach($part in $relative.Split('/')) { if ($part -in @('','.','..')) { throw 'Invalid package path component.' } }
    $full=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if (!$full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escapes its folder.' }
    $walk=$full
    while ($walk -and $walk.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) {
        if ((Test-Path -LiteralPath $walk) -and ((Get-Item -LiteralPath $walk).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked update paths are not supported.' }
        if ($walk -eq $root) { break };$walk=Split-Path $walk -Parent
    }
    return $full
}
foreach($process in (Get-Process -ErrorAction SilentlyContinue)) {
    try {$path=$process.Path} catch {continue}
    if($path -and $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Close the game and launcher before updating.'}
}
$manifest=Get-Content -LiteralPath (Join-Path $packageRoot 'update.json') -Raw | ConvertFrom-Json
if (!$manifest.portableBuild) {throw 'Not a portable launcher update.'}
$supported='0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b'
$xex=Join-Path $target 'game-data\default.xex'
if(Test-Path -LiteralPath $xex) {
    if ((Update-Hash $xex) -ne $supported) {throw 'The installed game is not supported. No files changed.'}
} else {
    $marker=Join-Path $target 'installed.txt'
    if(!(Test-Path -LiteralPath $marker)) {throw 'Choose your existing installed game folder. For a fresh setup, extract the complete ZIP and run START_DWG2.exe.'}
    $state=@(Get-Content -LiteralPath $marker)
    if($state.Count -lt 2 -or $state[1] -ne $supported -or !(Test-Path -LiteralPath (Join-Path $target 'bin\dwg2.exe'))) {throw 'Incompatible existing installation.'}
}
$allowed=@('START_DWG2.exe','READ_ME_FIRST.txt','START_HERE.txt','THIRD_PARTY_LICENSE.txt','TEST_CHECKLIST.md','apply-update.ps1','apply-portable-update.ps1')
$seen=@{}
foreach($item in $manifest.files) {
    if(($allowed -notcontains $item.path -and !$item.path.StartsWith('portable/',[StringComparison]::Ordinal)) -or $seen.ContainsKey($item.path)) {throw 'Unexpected or duplicate update entry.'}
    $source=Safe-Path $packageRoot $item.path;$dest=Safe-Path $target $item.path
    if((Update-Hash $source) -ne $item.sha256) {throw "Update hash mismatch: $($item.path)"}
    $seen[$item.path]=$item.sha256
}
foreach($name in @('START_DWG2.exe','READ_ME_FIRST.txt','portable/build.json')) {if(!$seen.ContainsKey($name)) {throw 'Incomplete portable update.'}}
$recipe=Get-Content -LiteralPath (Join-Path $packageRoot 'portable\build.json') -Raw | ConvertFrom-Json
if($recipe.format -ne 1) {throw 'Unsupported portable recipe.'}
foreach($file in $recipe.files) {
    $path='portable/'+$file.path
    [void](Safe-Path $packageRoot $path)
    if(!$seen.ContainsKey($path) -or $seen[$path] -ne $file.sha256) {throw "Missing verified preparation component: $path"}
}
# The current bin directory is deliberately left intact. The launcher builds a
# complete replacement locally, then swaps it only after preparation succeeds.
$backup=Join-Path $target ('.update-backup\'+[Guid]::NewGuid().ToString('N'))
[void](Safe-Path $target ('.update-backup/'+[IO.Path]::GetFileName($backup)+'/receipt.json'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$changed=New-Object Collections.Generic.List[string]
try {
    foreach($item in $manifest.files) {
        $dest=Safe-Path $target $item.path;$saved=Join-Path $backup $item.path
        New-Item -ItemType Directory -Path (Split-Path $dest -Parent),(Split-Path $saved -Parent) -Force | Out-Null
        if(Test-Path -LiteralPath $dest) {Copy-Item -LiteralPath $dest -Destination $saved}
        $changed.Add($item.path)
        Copy-Item -LiteralPath (Join-Path $packageRoot $item.path) -Destination $dest -Force
    }
} catch {
    foreach($relative in $changed) {
        $saved=Join-Path $backup $relative;$dest=Safe-Path $target $relative
        if(Test-Path -LiteralPath $saved) {Copy-Item -LiteralPath $saved -Destination $dest -Force}
        elseif(Test-Path -LiteralPath $dest) {Remove-Item -LiteralPath $dest -Force}
    }
    throw
}
Write-Host "Updated launcher to $($manifest.version). On its next start it prepares the game with bundled tools. Your current executable, imported game, saves, controls and display settings were preserved."
