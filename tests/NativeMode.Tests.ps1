param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh native-mode fixture directory.'}
New-Item -ItemType Directory -Path $root | Out-Null
Get-ChildItem -LiteralPath $PackageDirectory -Force | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $root -Recurse}
Copy-Item -LiteralPath (Join-Path $NativeDirectory 'fixture.exe') -Destination $root
# A legacy non-native value must not silently enable another mode.
[IO.File]::WriteAllText((Join-Path $root 'KuroAA/AeonSR.ini'),"[AeonSR]`nEnabled=1`nUpscaler=0`nUpscaleMode=4`nRenderPreset=11`nSpatialJitter=0`nKeepInterface=0`nSharpness=0`n")
[IO.File]::WriteAllText((Join-Path $root 'KuroAA/KuroUI.ini'),"[KuroUI]`nAARule=full`nSkipUnmatchedFrames=0`n")
$process=Start-Process -FilePath (Join-Path $root 'fixture.exe') -ArgumentList '300' -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root 'stdout.log') -RedirectStandardError (Join-Path $root 'stderr.log') -PassThru
if(!$process.WaitForExit(60000) -or $process.ExitCode){throw 'Native-mode GPU fixture failed.'}
$log=Get-Content -LiteralPath (Join-Path $root 'KuroAA/AeonSR.log') -Raw
if($log -notmatch 'CreateFeature 640x360 -> 640x360'){throw 'Legacy mode was not constrained to native dimensions.'}
if($log -match 'CreateFeature (?!640x360 -> 640x360)\d+x\d+ -> 640x360'){throw 'A non-native feature was created.'}
'PASS: legacy mode=4 creates only a 640x360 -> 640x360 native feature.'
