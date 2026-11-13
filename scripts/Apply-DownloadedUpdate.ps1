param([Parameter(Mandatory=$true)][string]$GameFolder,
      [Parameter(Mandatory=$true)][string]$Package,
      [Parameter(Mandatory=$true)][int]$LauncherProcessId,
      [switch]$NoLaunch,
      [switch]$ClosePromptHandled)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $GameFolder).Path
$payload=(Resolve-Path -LiteralPath $Package).Path
$log=Join-Path $target 'logs\update-install.log'
$updateForm=$null
$displayProcess=$null;$displayFile=$null;$displayState=$null
function Show-UpdateProgress([string]$stage,[long]$completed=0,[long]$total=0,[string]$state='working') {
    if($script:displayState) {
        $record=@{state=$state;message=$stage;completed=$completed;total=$total}
        $temporary=$script:displayState+'.tmp'
        for($attempt=0;$attempt -lt 4;$attempt++) {try {
            [IO.File]::WriteAllText($temporary,($record|ConvertTo-Json -Compress))
            if(Test-Path -LiteralPath $script:displayState) {$previous=$script:displayState+'.previous';[IO.File]::Replace($temporary,$script:displayState,$previous);if(Test-Path -LiteralPath $previous){Remove-Item -LiteralPath $previous -Force}}
            else {[IO.File]::Move($temporary,$script:displayState)}
            break
        } catch {if($attempt -lt 3){Start-Sleep -Milliseconds 25}}}
        # Progress display contention must never fail or roll back installation.
    }
    if(!$script:updateForm) {return}
    $script:updateLabel.Text=$stage
    if($total -gt 0) {
        $script:updateBar.Style='Continuous'
        $script:updateBar.Value=[int][Math]::Min(100,[Math]::Max(0,100.0*$completed/$total))
    } else {$script:updateBar.Style='Marquee'}
    [Windows.Forms.Application]::DoEvents()
}
try {
    New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
    # Older launchers cannot display the new close/install choice themselves.
    # Their staged helper supplies the choice; the new launcher handles it in C#.
    if(!$NoLaunch -and !$ClosePromptHandled) {
        Add-Type -AssemblyName System.Windows.Forms
        $choice=[Windows.Forms.MessageBox]::Show('The update is ready. Close the game and launcher to install now? Save your progress before choosing Yes. No installs after you quit.','DWG2 Update',[Windows.Forms.MessageBoxButtons]::YesNo,[Windows.Forms.MessageBoxIcon]::Question,[Windows.Forms.MessageBoxDefaultButton]::Button2)
        if($choice -eq [Windows.Forms.DialogResult]::Yes) {
            Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class DWG2UpdateClose {[DllImport("user32.dll")]public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);}'
            foreach($game in (Get-Process -Name dwg2 -ErrorAction SilentlyContinue)) {
                try {$path=$game.Path} catch {$path=$null}
                if($path -and $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase) -and $game.MainWindowHandle -ne [IntPtr]::Zero) {
                    [void][DWG2UpdateClose]::PostMessage($game.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
                }
            }
        }
    }
    # Request only normal close after consent; never force-stop a process.
    $displaySource=Join-Path $payload 'maintenance/UpdateProgress.exe'
    if(!$NoLaunch -and (Test-Path -LiteralPath $displaySource)) {
        $script:displayState=Join-Path $target ('logs/update-progress-'+[Guid]::NewGuid().ToString('N')+'.json')
        # Launch only after the game/old launcher have exited; no helper window
        # interrupts gameplay when the player chooses Later.
    } elseif(!$NoLaunch) {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        $script:updateForm=New-Object Windows.Forms.Form
        $updateForm.Text='DWG2 — Launcher update';$updateForm.ClientSize=New-Object Drawing.Size(560,150)
        $updateForm.StartPosition='CenterScreen';$updateForm.FormBorderStyle='FixedDialog'
        $updateForm.MaximizeBox=$false;$updateForm.MinimizeBox=$false;$updateForm.ControlBox=$false
        $script:updateLabel=New-Object Windows.Forms.Label
        $updateLabel.Location=New-Object Drawing.Point(24,25);$updateLabel.Size=New-Object Drawing.Size(510,45)
        $script:updateBar=New-Object Windows.Forms.ProgressBar
        $updateBar.Location=New-Object Drawing.Point(24,90);$updateBar.Size=New-Object Drawing.Size(510,24)
        $updateForm.Controls.Add($updateLabel);$updateForm.Controls.Add($updateBar)
        # When the player chose Later, keep the helper unobtrusive until play ends.
    }
    while(Get-Process -Id $LauncherProcessId -ErrorAction SilentlyContinue) {
        Show-UpdateProgress 'Waiting for the game and launcher to close normally...'
        Start-Sleep -Milliseconds 150
    }
    if($updateForm) {$updateForm.TopMost=$true;$updateForm.Show();$updateForm.Activate()}
    Show-UpdateProgress 'Checking that the game has closed...'
    # The compatibility forwarder may exit just after the launcher it started.
    # Wait briefly for it as well; never terminate another process.
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    do {
        $busy=@(Get-Process -ErrorAction SilentlyContinue | Where-Object {
            try {$path=$_.Path} catch {$path=$null}
            ($path -and $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) -or
            (!$path -and $_.ProcessName -in @('SETUP_DWG2','DWG2R','START_DWG2','dwg2'))
        })
        if(!$busy.Count) {break}
        if([DateTime]::UtcNow -ge $deadline) {throw 'The game or launcher is still running. Close it normally and retry the update.'}
        Start-Sleep -Milliseconds 200
        Show-UpdateProgress 'Waiting for the game to close normally...'
    } while($true)
    if($displayState) {
        $displayFile=Join-Path ([IO.Path]::GetTempPath()) ('DWG2UpdateProgress-'+[Guid]::NewGuid().ToString('N')+'.exe')
        Copy-Item -LiteralPath $displaySource -Destination $displayFile
        Show-UpdateProgress 'Updating your current installation...'
        $displayProcess=Start-Process -FilePath $displayFile -ArgumentList @(('"'+$displayState+'"'),$PID) -PassThru -WindowStyle Normal
    }
    New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
    Add-Content -Encoding UTF8 -LiteralPath $log -Value ('Installing verified update: '+[DateTime]::UtcNow.ToString('o'))
    $apply=Join-Path $payload 'maintenance/apply-update.ps1'
    if(!(Test-Path -LiteralPath $apply)) {$apply=Join-Path $payload 'apply-update.ps1'}
    $callback={param($stage,$completed,$total) Show-UpdateProgress $stage $completed $total}
    & $apply -GameFolder $target -Package $payload -ProgressCallback $callback *>&1 | ForEach-Object {Add-Content -Encoding UTF8 -LiteralPath $log -Value ([string]$_)}
    if($LASTEXITCODE -and $LASTEXITCODE -ne 0){throw 'The player updater failed.'}
    try {
        $updates=[IO.Path]::GetFullPath((Join-Path $target '.updates'))
        $owned=$payload
        while($owned.StartsWith($updates+'\',[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetDirectoryName($owned) -cne $updates) {$owned=[IO.Path]::GetDirectoryName($owned)}
        if([IO.Path]::GetDirectoryName($owned) -ceq $updates -and [IO.Path]::GetFileName($owned) -cmatch '^[0-9a-f]{32}$') {
            $resolved=(Resolve-Path -LiteralPath $owned).Path
            if(!$resolved.StartsWith($updates+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected staging path.'}
            if((Get-Item -LiteralPath $owned).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked staging folder.'}
            foreach($entry in (Get-ChildItem -LiteralPath $owned -Recurse -Force)) {
                if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked staging entry.'}
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    } catch {Add-Content -Encoding UTF8 -LiteralPath $log -Value ('Update installed; staging cleanup skipped: '+$_.Exception.Message)}
    $launcher=Join-Path $target 'DWG2R.exe'
    if(!(Test-Path -LiteralPath $launcher)) {$launcher=Join-Path $target 'SETUP_DWG2.exe'}
    if(!(Test-Path -LiteralPath $launcher)) {$launcher=Join-Path $target 'START_DWG2.exe'}
    Add-Content -Encoding UTF8 -LiteralPath $log -Value 'Installation completed.'
    # Staging may have been cleaned above. Read the installed release identity.
    $installedRelease=Join-Path $target 'maintenance/release.json'
    if(!(Test-Path -LiteralPath $installedRelease)) {$installedRelease=Join-Path $target 'release.json'}
    try {if(Test-Path -LiteralPath $installedRelease) {
        $releaseState=Get-Content -LiteralPath $installedRelease -Raw | ConvertFrom-Json
        if($releaseState.version -match '^v?\d+(\.\d+){0,2}(-(alpha|test)(\.\d+)?)?$') {
            $receipt=Join-Path $target 'logs/update-completed.json'
            @{version=$releaseState.version;completedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $receipt -Encoding UTF8
        }
    }} catch {Add-Content -Encoding UTF8 -LiteralPath $log -Value 'Update installed; completion notification could not be recorded.'}

    if(!$NoLaunch) {
        Show-UpdateProgress 'Starting updated launcher...'
        $ready=$false
        try {
            $launcherProc=Start-Process -FilePath $launcher -WorkingDirectory $target -PassThru
            $gamePath=Join-Path $target 'bin/dwg2.exe'
            Show-UpdateProgress 'Preparing game and waiting for its window...'
            $deadline=[DateTime]::UtcNow.AddSeconds(240)
            do {
                foreach($game in (Get-Process -Name dwg2 -ErrorAction SilentlyContinue)) {
                    try {
                        if($game.Path -eq $gamePath -and $game.MainWindowHandle -ne [IntPtr]::Zero -and !$game.HasExited) {
                            $ready=$true
                            break
                        }
                    } catch {}
                }
                if($ready -or $launcherProc.HasExited) {break}
                Start-Sleep -Milliseconds 300
                if($updateForm) {[Windows.Forms.Application]::DoEvents()}
            } while([DateTime]::UtcNow -lt $deadline)
        } catch {Add-Content -Encoding UTF8 -LiteralPath $log -Value $_.Exception.Message}
        if($ready) {
            Show-UpdateProgress 'Game is running. Update complete.' 1 1 'done'
        } else {
            $message='Update installed. The game window did not open. See logs/update-install.log.'
            Add-Content -Encoding UTF8 -LiteralPath $log -Value $message
            Show-UpdateProgress $message 0 0 'failed'
            if($updateForm) {[Windows.Forms.MessageBox]::Show($updateForm,$message,'DWG2 Update') | Out-Null}
        }
    }
    if($displayState) {
        if($displayProcess) {[void]$displayProcess.WaitForExit(5000);if($displayProcess.HasExited -and (Test-Path -LiteralPath $displayFile)){Remove-Item -LiteralPath $displayFile -Force}}
    }
    if($updateForm) {$updateForm.Close();$updateForm.Dispose();$script:updateForm=$null}
} catch {
    if($displayState) {Show-UpdateProgress 'Update could not finish. See logs/update-install.log.' 0 0 'failed'}
    if($updateForm) {$updateForm.Close();$updateForm.Dispose();$script:updateForm=$null}
    Add-Content -Encoding UTF8 -LiteralPath $log -Value ($_.Exception.ToString())
    if($NoLaunch) {throw}
    Add-Type -AssemblyName System.Windows.Forms
    if(!$displayProcess){[Windows.Forms.MessageBox]::Show('The update could not be installed. Your existing installation is retained. See logs\update-install.log.','DWG2 Update') | Out-Null}
    exit 1
}
