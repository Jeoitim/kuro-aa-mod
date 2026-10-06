param([string]$Destination=(Join-Path $PSScriptRoot '..\external'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($Destination)
$sdk=Join-Path $root 'reshade-sdk'
if(Test-Path -LiteralPath $sdk){throw 'SDK destination exists; refusing to mix versions.'}
New-Item -ItemType Directory -Path $root -Force | Out-Null
git clone --depth 1 --branch v6.8.0 https://github.com/crosire/reshade.git $sdk
if($LASTEXITCODE){throw 'Pinned ReShade SDK fetch failed.'}
Write-Output 'Fetched ReShade 6.8.0 headers. Runtime binaries are included in vendor/.'
