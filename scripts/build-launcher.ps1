param([string]$Output = (Join-Path $PSScriptRoot '..\DWG2R.exe'),[string]$IconPath,[string]$SourceDirectory = (Join-Path $PSScriptRoot '..\launcher'))
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x is required.' }
if (!$IconPath) {
    $localConfig = Join-Path $PSScriptRoot '..\toolchain.local.json'
    if (Test-Path -LiteralPath $localConfig) { $IconPath = (Get-Content -LiteralPath $localConfig -Raw | ConvertFrom-Json).launcherIcon }
}
$iconArguments = @()
if ($IconPath) {
    if (!(Test-Path -LiteralPath $IconPath -PathType Leaf)) { throw 'The launcher icon file is missing.' }
    $iconArguments = @('/win32icon:' + [IO.Path]::GetFullPath($IconPath))
}
$sources = @('Launcher.cs','PortableBuild.cs','SetupProgress.cs','GameSession.cs','UpdateService.cs','DesktopShortcut.cs','PlayerSettings.cs') | ForEach-Object { Join-Path $SourceDirectory $_ }
$defaults = Join-Path $SourceDirectory "SettingsDefaults.ini"
if (!(Test-Path -LiteralPath $defaults)) { $defaults = Join-Path $PSScriptRoot "..\settings.default.ini" }
& $compiler "/resource:$defaults,SettingsDefaults.ini" /nologo /codepage:65001 /target:winexe /optimize+ /platform:x64 /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll @iconArguments "/out:$Output" @sources
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
