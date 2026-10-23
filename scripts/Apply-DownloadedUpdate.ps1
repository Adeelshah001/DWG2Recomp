param([Parameter(Mandatory=$true)][string]$GameFolder,
      [Parameter(Mandatory=$true)][string]$Package,
      [Parameter(Mandatory=$true)][int]$LauncherProcessId)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $GameFolder).Path
$payload=(Resolve-Path -LiteralPath $Package).Path
$log=Join-Path $target 'logs\update-install.log'
try {
    # Never stop the game or launcher. In particular, do not interrupt a save.
    Wait-Process -Id $LauncherProcessId -ErrorAction SilentlyContinue
    & (Join-Path $payload 'apply-update.ps1') -GameFolder $target -Package $payload *>> $log
    if($LASTEXITCODE -and $LASTEXITCODE -ne 0){throw 'The player updater failed.'}
    try {
        $updates=[IO.Path]::GetFullPath((Join-Path $target '.updates'))
        $owned=[IO.Path]::GetDirectoryName($payload)
        if([IO.Path]::GetDirectoryName($owned) -ceq $updates -and [IO.Path]::GetFileName($owned) -cmatch '^[0-9a-f]{32}$') {
            $resolved=(Resolve-Path -LiteralPath $owned).Path
            if(!$resolved.StartsWith($updates+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected staging path.'}
            if((Get-Item -LiteralPath $owned).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked staging folder.'}
            foreach($entry in (Get-ChildItem -LiteralPath $owned -Recurse -Force)) {
                if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked staging entry.'}
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    } catch {Add-Content -LiteralPath $log -Value ('Update installed; staging cleanup skipped: '+$_.Exception.Message)}
    Start-Process -FilePath (Join-Path $target 'START_DWG2.exe') -WorkingDirectory $target -WindowStyle Hidden
} catch {
    Add-Content -LiteralPath $log -Value ($_.Exception.ToString())
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show('The update could not be installed. Your existing installation is retained. See logs\update-install.log.','DWG2 Update') | Out-Null
}
