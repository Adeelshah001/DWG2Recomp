<# Maintainer assembly; players need no Python or separately installed tools. #>
param([string]$Version='0.2.1-alpha',[Parameter(Mandatory=$true)][string]$OutputFolder,[Parameter(Mandatory=$true)][string]$PortableFolder,[Parameter(Mandatory=$true)][string]$HostSourceRelink,[string]$Python='python')
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+(?:\.\d+)?-alpha(?:\.\d+)?$') {throw 'Use an alpha version.'}
& $Python (Join-Path $PSScriptRoot 'package-complete.py') --version $Version --output $OutputFolder --portable $PortableFolder --host-source-relink $HostSourceRelink
if($LASTEXITCODE) {throw 'Complete package assembly failed.'}
