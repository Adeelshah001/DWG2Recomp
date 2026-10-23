param([string]$Output = (Join-Path $PSScriptRoot '..\START_DWG2.exe'))
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x is required.' }
& $compiler /nologo /target:winexe /optimize+ /platform:x64 /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll "/out:$Output" (Join-Path $PSScriptRoot '..\launcher\Launcher.cs') (Join-Path $PSScriptRoot '..\launcher\PortableBuild.cs') (Join-Path $PSScriptRoot '..\launcher\SetupProgress.cs') (Join-Path $PSScriptRoot '..\launcher\GameSession.cs') (Join-Path $PSScriptRoot '..\launcher\UpdateService.cs')
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
