param([Parameter(Mandatory=$true)][string]$GameFolder,[Parameter(Mandatory=$true)][string]$Package,[scriptblock]$ProgressCallback)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $GameFolder).Path.TrimEnd('\')
$packageRoot=(Resolve-Path -LiteralPath $Package).Path.TrimEnd('\')
function Report-Update([string]$stage,[long]$completed=0,[long]$total=0) {
    if($ProgressCallback) { & $ProgressCallback $stage $completed $total }
}
Report-Update 'Checking update compatibility...'
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
    try {$path=$process.Path} catch {$path=$null}
    if(!$path -and $process.ProcessName -in @('SETUP_DWG2','DWG2R','START_DWG2','dwg2')) {throw 'Cannot verify a running game/launcher location. Close it before updating.'}
    if($path -and $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Close the game and launcher before updating.'}
}
$manifestPath=Join-Path $packageRoot 'maintenance/update.json'
if(!(Test-Path -LiteralPath $manifestPath)) {$manifestPath=Join-Path $packageRoot 'update.json'}
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
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
$allowed=@('SETUP_DWG2.exe','game/README.txt','maintenance/UpdateProgress.exe','maintenance/legacy-launcher.exe','licenses/host-source.zip','settings.ini','DWG2R.exe','README.txt','licenses/THIRD_PARTY_LICENSE.txt','maintenance/apply-update.ps1','maintenance/apply-portable-update.ps1','maintenance/Apply-DownloadedUpdate.ps1','maintenance/release.json','START_DWG2.exe','READ_ME_FIRST.txt','START_HERE.txt','THIRD_PARTY_LICENSE.txt','TEST_CHECKLIST.md','apply-update.ps1','apply-portable-update.ps1','Apply-DownloadedUpdate.ps1','release.json')
if($manifest.updateKind -eq 'hostRuntime') {$allowed+=@('bin/rexruntime.dll')}
$seen=@{}
$verified=0
foreach($item in $manifest.files) {
    if($item.path -eq 'licenses/v04-host-source.zip') {$allowed+=@('licenses/v04-host-source.zip')}
    if(($allowed -notcontains $item.path -and !$item.path.StartsWith('portable/',[StringComparison]::Ordinal) -and !$item.path.StartsWith('preparation/',[StringComparison]::Ordinal)) -or $seen.ContainsKey($item.path)) {throw 'Unexpected or duplicate update entry.'}
    $source=Safe-Path $packageRoot $item.path;$dest=Safe-Path $target $item.path
    if((Update-Hash $source) -ne $item.sha256) {throw "Update hash mismatch: $($item.path)"}
    $seen[$item.path]=$item.sha256
    $verified++;Report-Update 'Verifying update files...' $verified @($manifest.files).Count
}
$launcherName=if($seen.ContainsKey('SETUP_DWG2.exe')) {'SETUP_DWG2.exe'} elseif($seen.ContainsKey('DWG2R.exe')) {'DWG2R.exe'} else {'START_DWG2.exe'}
$readmeName=if($seen.ContainsKey('README.txt')) {'README.txt'} else {'READ_ME_FIRST.txt'}
if($manifest.compressedPreparation) {
    foreach($name in @($launcherName,$readmeName,'preparation/build.json','preparation/package.json','preparation/tools.zip')) {if(!$seen.ContainsKey($name)) {throw 'Incomplete compressed preparation update.'}}
    $archive=Get-Content -LiteralPath (Join-Path $packageRoot 'preparation/package.json') -Raw | ConvertFrom-Json
    if($archive.format -ne 1 -or $archive.sha256 -ne $seen['preparation/tools.zip']) {throw 'Preparation archive hash differs.'}
} else {
    foreach($name in @($launcherName,$readmeName,'portable/build.json')) {if(!$seen.ContainsKey($name)) {throw 'Incomplete portable update.'}}
    $recipe=Get-Content -LiteralPath (Join-Path $packageRoot 'portable/build.json') -Raw | ConvertFrom-Json
    if($recipe.format -ne 1) {throw 'Unsupported portable recipe.'}
    foreach($file in $recipe.files) {
        $path='portable/'+$file.path
        [void](Safe-Path $packageRoot $path)
        if(!$seen.ContainsKey($path) -or $seen[$path] -ne $file.sha256) {throw "Missing verified preparation component: $path"}
    }
}
# Runtime-only changes preserve the prepared native executable and native ABI ID.
if($manifest.updateKind -eq 'hostRuntime') {
    $native=Safe-Path $target 'bin/dwg2.exe'
    $oldRecipe=Get-Content -LiteralPath (Safe-Path $target 'preparation/build.json') -Raw | ConvertFrom-Json
    $newRecipe=Get-Content -LiteralPath (Safe-Path $packageRoot 'preparation/build.json') -Raw | ConvertFrom-Json
    $stamp=@(Get-Content -LiteralPath (Safe-Path $target 'bin/portable-build.txt'))
    if($manifest.nativeCompatibleBuildIds -notcontains $oldRecipe.buildId -or $manifest.nativeBuildId -ne $newRecipe.buildId -or $oldRecipe.buildId -ne $newRecipe.buildId -or $stamp.Count -ne 3 -or $stamp[0] -ne $newRecipe.buildId -or $stamp[1] -ne $supported -or $stamp[2] -ne (Update-Hash $native)) {throw 'Native build identity differs. No files changed.'}
    if(!$seen.ContainsKey('bin/rexruntime.dll')) {throw 'Missing verified host runtime.'}
}
# A launcher-only package must preserve the exact installed native recipe.
# Never accept a mismatched stamp or rewrite it to skip preparation.
if($manifest.updateKind -eq 'launcherOnly') {
    foreach($entry in $manifest.files) {
        if($entry.path.StartsWith('preparation/',[StringComparison]::Ordinal) -and $entry.path -notin @('preparation/build.json','preparation/package.json','preparation/tools.zip')) {throw 'Unexpected preparation content in a launcher-only update.'}
    }
    foreach($name in @('preparation/build.json','preparation/package.json','preparation/tools.zip')) {
        if(!$seen.ContainsKey($name) -or !(Test-Path -LiteralPath (Join-Path $target $name)) -or
           (Update-Hash (Join-Path $target $name)) -ne $seen[$name]) {throw 'Launcher-only update requires the same prepared game components. No files changed.'}
    }
    $recipe=Get-Content -LiteralPath (Join-Path $target 'preparation/build.json') -Raw | ConvertFrom-Json
    $stampPath=Safe-Path $target 'bin/portable-build.txt'
    if(!(Test-Path -LiteralPath $stampPath)) {throw 'The prepared game stamp is missing. No files changed.'}
    $stamp=@(Get-Content -LiteralPath $stampPath)
    if($stamp.Count -ne 3 -or $stamp[0] -ne $recipe.buildId -or $stamp[1] -ne $supported -or
       $manifest.nativeBuildId -ne $recipe.buildId -or (Update-Hash (Safe-Path $target 'bin/dwg2.exe')) -ne $stamp[2]) {
        throw 'The prepared game does not match this launcher-only update. No files changed.'
    }
}
$legacyTargets=@()
if($manifest.packageLayout -ge 4) {
    foreach($name in @($launcherName,'maintenance/UpdateProgress.exe','game/README.txt')) {if(!$seen.ContainsKey($name)){throw 'Incomplete standardized player package.'}}
    if($launcherName -notin @('DWG2R.exe','SETUP_DWG2.exe') -or ($seen.ContainsKey('DWG2R.exe') -and $seen.ContainsKey('SETUP_DWG2.exe')) -or $seen.ContainsKey('START_DWG2.exe')) {throw 'Standardized packages must contain one canonical launcher.'}
    foreach($name in @('SETUP_DWG2.exe','DWG2R.exe','START_DWG2.exe')) {if($name -ne $launcherName -and (Test-Path -LiteralPath (Safe-Path $target $name))) {$legacyTargets+=$name}}
} elseif($manifest.packageLayout -ge 3 -and !$seen.ContainsKey('START_DWG2.exe')) {throw 'Missing legacy shortcut forwarder.'}
# Check every existing destination is writable and unlocked before changing any.
foreach($item in $manifest.files) {
    $dest=Safe-Path $target $item.path
    $parent=Split-Path $dest -Parent
    while($parent -and $parent.StartsWith($target,[StringComparison]::OrdinalIgnoreCase)) {
        if((Test-Path -LiteralPath $parent) -and !(Test-Path -LiteralPath $parent -PathType Container)) {throw 'An update destination parent is not a directory.'}
        if($parent -eq $target) {break};$parent=Split-Path $parent -Parent
    }
    if($item.path -eq 'settings.ini' -and (Test-Path -LiteralPath $dest)) {continue}
    if(Test-Path -LiteralPath $dest) {
        $handle=[IO.File]::Open($dest,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        $handle.Dispose()
    }
}
foreach($name in $legacyTargets) {
    $handle=[IO.File]::Open((Safe-Path $target $name),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$handle.Dispose()
}
# The current bin directory is deliberately left intact. The launcher builds a
# complete replacement locally, then swaps it only after preparation succeeds.
$backup=Join-Path $target ('.update-backup\'+[Guid]::NewGuid().ToString('N'))
[void](Safe-Path $target ('.update-backup/'+[IO.Path]::GetFileName($backup)+'/receipt.json'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
# Keep existing unified settings, or let the new launcher migrate legacy values.
$preservePlayerSettings=$false
foreach($name in @('settings.ini','display.ini','camera.ini','video.ini','prompts.ini','controls.ini')) {if(Test-Path -LiteralPath (Join-Path $target $name)) {$preservePlayerSettings=$true}}
$changed=New-Object Collections.Generic.List[string]
$processed=0
try {
    foreach($item in $manifest.files) {
        Report-Update ('Updating launcher and files: '+$item.path) $processed @($manifest.files).Count
        $processed++
        if($item.path -eq 'settings.ini' -and $preservePlayerSettings) {continue}
        $dest=Safe-Path $target $item.path;$saved=Join-Path $backup $item.path
        if((Test-Path -LiteralPath $dest) -and (Update-Hash $dest) -eq $item.sha256) {continue}
        New-Item -ItemType Directory -Path (Split-Path $dest -Parent),(Split-Path $saved -Parent) -Force | Out-Null
        if(Test-Path -LiteralPath $dest) {Copy-Item -LiteralPath $dest -Destination $saved}
        $changed.Add($item.path)
        $temporary=$dest+'.update-'+[Guid]::NewGuid().ToString('N')
        try {
            Copy-Item -LiteralPath (Join-Path $packageRoot $item.path) -Destination $temporary
            if(Test-Path -LiteralPath $dest) {[IO.File]::Replace($temporary,$dest,$temporary+'.previous');Remove-Item -LiteralPath ($temporary+'.previous') -Force}
            else {[IO.File]::Move($temporary,$dest)}
        } finally {if(Test-Path -LiteralPath $temporary) {Remove-Item -LiteralPath $temporary -Force}}
    }
    foreach($name in $legacyTargets) {
        $dest=Safe-Path $target $name;$saved=Join-Path $backup $name
        if((Update-Hash $dest) -eq $seen[$launcherName]) {continue}
        Copy-Item -LiteralPath $dest -Destination $saved;$changed.Add($name)
        $temporary=$dest+'.update-'+[Guid]::NewGuid().ToString('N')
        # Keep existing shortcut targets and the launcher icon: an alias contains
        # the exact same launcher bytes, never an old launcher or generic stub.
        try {Copy-Item -LiteralPath (Safe-Path $packageRoot $launcherName) -Destination $temporary;[IO.File]::Replace($temporary,$dest,$temporary+'.previous');Remove-Item -LiteralPath ($temporary+'.previous') -Force}
        finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
    }
    Report-Update 'Launcher files installed.' @($manifest.files).Count @($manifest.files).Count
    if($manifest.packageLayout -ge 2) {
        foreach($name in @('READ_ME_FIRST.txt','START_HERE.txt','TEST_CHECKLIST.md','Apply-DownloadedUpdate.ps1','apply-update.ps1','apply-portable-update.ps1','release.json','update.json','SHA256SUMS.txt')) {
            $dest=Safe-Path $target $name
            if(Test-Path -LiteralPath $dest -PathType Leaf) {
                $saved=Join-Path $backup $name
                Copy-Item -LiteralPath $dest -Destination $saved -Force
                $changed.Add($name)
                Remove-Item -LiteralPath $dest -Force
            }
        }
    }
} catch {
    Report-Update 'Restoring previous files after an update failure...'
    foreach($relative in $changed) {
        $saved=Join-Path $backup $relative;$dest=Safe-Path $target $relative
        if(Test-Path -LiteralPath $saved) {Copy-Item -LiteralPath $saved -Destination $dest -Force}
        elseif(Test-Path -LiteralPath $dest) {Remove-Item -LiteralPath $dest -Force}
    }
    throw
}
@{version=$manifest.version;updateKind=$manifest.updateKind;nativeBuildId=$manifest.nativeBuildId;changed=@($changed);completedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
if($manifest.compressedPreparation) {
    # Remove only a completely verified previous managed tools tree. Unknown or
    # modified files are left alone; game files and settings are never included.
    $legacy=Join-Path $target 'portable'
    if(Test-Path -LiteralPath $legacy) {
        try {
            $resolved=[IO.Path]::GetFullPath($legacy)
            if(!$resolved.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe legacy folder.'}
            $items=@(Get-ChildItem -LiteralPath $legacy -Recurse -Force)
            if((Get-Item -LiteralPath $legacy).Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Linked legacy folder.'}
            foreach($entry in $items) {if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Linked legacy entry.'}}
            $old=Get-Content -LiteralPath (Join-Path $legacy 'build.json') -Raw | ConvertFrom-Json
            $managed=@{};foreach($file in $old.files) {
                if($file.path.Contains(':') -or $file.path.Contains('\') -or $file.path.Split('/') -contains '..') {throw 'Unsafe legacy entry.'}
                $managed[$file.path]=$file.sha256
            }
            $files=@($items | Where-Object {!$_.PSIsContainer})
            if($files.Count -ne $managed.Count+1) {throw 'Unrecognized legacy files.'}
            foreach($file in $files) {
                $name=$file.FullName.Substring($legacy.Length+1).Replace('\','/')
                if($name -eq 'build.json') {continue}
                if(!$managed.ContainsKey($name) -or (Update-Hash $file.FullName) -ne $managed[$name]) {throw 'Modified legacy file.'}
            }
            # The resolved absolute target was checked above and its complete tree is managed.
            Remove-Item -LiteralPath $resolved -Recurse -Force
            Write-Host 'Removed the verified old expanded preparation tools.'
        } catch {Write-Host 'Previous preparation folder was retained because it contains unknown or modified files.'}
    }
}
Write-Host "Updated launcher to $($manifest.version). Compatible executables are reused; first-time preparation uses temporary bundled tools. Imported game, saves, controls and display settings were preserved."
