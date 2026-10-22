param([Parameter(Mandatory=$true)][string]$GameFolder)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=(Resolve-Path -LiteralPath $GameFolder).Path
$payload=$PSScriptRoot
function Hash-File([string]$path) {
    $algorithm=[Security.Cryptography.SHA256]::Create()
    $stream=[IO.File]::OpenRead($path)
    try {return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}
    finally {$stream.Dispose();$algorithm.Dispose()}
}
foreach($name in @('','.local-build','.update-backup','bin','preparation')) {
    $path=if($name){Join-Path $root $name}else{$root}
    if((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'The installation/update folders must be ordinary folders, not links.'
    }
}
if(Get-Process -Name dwg2,START_DWG2 -ErrorAction SilentlyContinue) {throw 'Close the game and launcher before applying this update.'}
$manifest=Get-Content -LiteralPath (Join-Path $payload 'update.json') -Raw | ConvertFrom-Json
if($manifest.format -ne 1 -or !$manifest.localRuntimeUpdate) {throw 'Unsupported update manifest.'}
$allowed=@('bin/rexruntime.dll','bin/rexgpu-xenos.dll','preparation-replacements/lib/librexruntime.dll.a','build.json')
if($manifest.files.Count -ne $allowed.Count) {throw 'Incomplete update.'}
$seen=@{}
foreach($item in $manifest.files) {
    if($allowed -cnotcontains $item.path -or $seen.ContainsKey($item.path)) {throw 'Unexpected or duplicate update file.'}
    $seen[$item.path]=$item.sha256
    $source=Join-Path $payload $item.path
    if((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Update payload links are not supported.'}
    if((Hash-File $source) -ne $item.sha256) {throw "Damaged update file: $($item.path)"}
}
$recipe=Get-Content -LiteralPath (Join-Path $root 'preparation/build.json') -Raw | ConvertFrom-Json
if($manifest.compatibleBuildIds -cnotcontains $recipe.buildId) {throw 'This update requires the supported v02 player installation.'}
$newRecipe=Get-Content -LiteralPath (Join-Path $payload 'build.json') -Raw | ConvertFrom-Json
if($newRecipe.buildId -ne $manifest.buildId) {throw 'Build recipe does not match the update.'}
$marker=Get-Content -LiteralPath (Join-Path $root 'bin/portable-build.txt')
if($marker.Count -ne 3 -or $marker[1] -ne '0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b' -or
   (Hash-File (Join-Path $root 'bin/dwg2.exe')) -ne $marker[2]) {throw 'The existing native executable does not match its installation record.'}
$oldPackage=Get-Content -LiteralPath (Join-Path $root 'preparation/package.json') -Raw | ConvertFrom-Json
if((Hash-File (Join-Path $root 'preparation/tools.zip')) -ne $oldPackage.sha256) {throw 'The existing preparation archive is damaged.'}
$id='local-runtime-'+[Guid]::NewGuid().ToString('N')
$stage=Join-Path $root ('.local-build/'+$id)
$backup=Join-Path $root ('.update-backup/'+$id)
$targets=@('bin/rexruntime.dll','bin/rexgpu-xenos.dll','bin/portable-build.txt','preparation/build.json','preparation/package.json','preparation/tools.zip')
[void](New-Item -ItemType Directory -Path $stage)
[void](New-Item -ItemType Directory -Path $backup)
$started=$false
try {
    foreach($name in $targets) {
        $old=Join-Path $root $name
        if((Get-Item -LiteralPath $old).Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Installation files must not be links.'}
        $saved=Join-Path $backup $name
        [void](New-Item -ItemType Directory -Force -Path (Split-Path $saved))
        Copy-Item -LiteralPath $old -Destination $saved
        $dest=Join-Path $stage $name
        [void](New-Item -ItemType Directory -Force -Path (Split-Path $dest))
    }
    Copy-Item -LiteralPath (Join-Path $payload 'bin/rexruntime.dll') -Destination (Join-Path $stage 'bin/rexruntime.dll')
    Copy-Item -LiteralPath (Join-Path $payload 'bin/rexgpu-xenos.dll') -Destination (Join-Path $stage 'bin/rexgpu-xenos.dll')
    Copy-Item -LiteralPath (Join-Path $payload 'build.json') -Destination (Join-Path $stage 'preparation/build.json')
    $archivePath=Join-Path $stage 'preparation/tools.zip'
    Copy-Item -LiteralPath (Join-Path $root 'preparation/tools.zip') -Destination $archivePath
    $zip=[IO.Compression.ZipFile]::Open($archivePath,[IO.Compression.ZipArchiveMode]::Update)
    try {
        $replacements=@{
            'runtime/rexruntime.dll'=(Join-Path $payload 'bin/rexruntime.dll');
            'runtime/rexgpu-xenos.dll'=(Join-Path $payload 'bin/rexgpu-xenos.dll');
            'lib/librexruntime.dll.a'=(Join-Path $payload 'preparation-replacements/lib/librexruntime.dll.a');
            'build.json'=(Join-Path $payload 'build.json')
        }
        foreach($entryName in $replacements.Keys) {
            $entry=$zip.GetEntry($entryName)
            if(!$entry){throw "Missing preparation entry: $entryName"}
            $entry.Delete()
            $entry=$zip.CreateEntry($entryName,[IO.Compression.CompressionLevel]::Optimal)
            $inputFile=[IO.File]::OpenRead($replacements[$entryName]);$outputFile=$entry.Open()
            try {$inputFile.CopyTo($outputFile)} finally {$inputFile.Dispose();$outputFile.Dispose()}
        }
    } finally {$zip.Dispose()}
    $zip=[IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $expanded=[long]0
        foreach($entry in $zip.Entries){$expanded+=$entry.Length}
        foreach($entryName in @('runtime/rexruntime.dll','runtime/rexgpu-xenos.dll','lib/librexruntime.dll.a')) {
            $entry=$zip.GetEntry($entryName);$stream=$entry.Open();$algorithm=[Security.Cryptography.SHA256]::Create()
            try {$actual=([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}
            finally {$stream.Dispose();$algorithm.Dispose()}
            $expected=($newRecipe.files | Where-Object {$_.path -ceq $entryName}).sha256
            if(!$expected -or $actual -ne $expected){throw 'Repacked preparation verification failed.'}
        }
    } finally {$zip.Dispose()}
    $encoding=New-Object Text.UTF8Encoding($false)
    $metadata=@{format=1;sha256=(Hash-File $archivePath);expandedBytes=$expanded} | ConvertTo-Json
    [IO.File]::WriteAllText((Join-Path $stage 'preparation/package.json'),$metadata,$encoding)
    [IO.File]::WriteAllLines((Join-Path $stage 'bin/portable-build.txt'),[string[]]@($newRecipe.buildId,$marker[1],$marker[2]),$encoding)
    $started=$true
    foreach($name in $targets){[IO.File]::Replace((Join-Path $stage $name),(Join-Path $root $name),[NullString]::Value)}
    [IO.File]::WriteAllText((Join-Path $backup 'RESTORE.txt'),"Close the game and launcher, then restore the six files in this folder to the same relative paths in your installation. Game files, saves and settings were not changed.",$encoding)
    Write-Host "Local update installed. Rollback files: $backup"
} catch {
    if($started){foreach($name in $targets){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $root $name) -Force}}
    throw
} finally {
    $resolved=[IO.Path]::GetFullPath($stage)
    $expectedPrefix=[IO.Path]::GetFullPath((Join-Path $root '.local-build'))+[IO.Path]::DirectorySeparatorChar
    if(!$resolved.StartsWith($expectedPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe temporary cleanup path.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
