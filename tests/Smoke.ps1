param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$fixturePath=(Resolve-Path -LiteralPath $Fixture).Path
$results=@()
foreach($backend in @('dlss','fsr','xess')) {
    $testRoot=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $backend
    if(Test-Path -LiteralPath $testRoot) { throw "Test output exists: $backend" }
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $testRoot $_.Name) -Recurse }
    Copy-Item -LiteralPath $fixturePath -Destination (Join-Path $testRoot 'smoke_d3d11.exe')
    $manager=Start-Process -FilePath (Join-Path $testRoot 'KuroAA\KuroAA.Settings.exe') -ArgumentList '--backend',$backend -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
    $handle=$manager.Handle; $manager.WaitForExit()
    if($manager.ExitCode -ne 0) { throw "Backend configuration failed: $backend" }
    # This fixture tests runtimes, not the CLE game's shader signature.
    [IO.File]::WriteAllText((Join-Path $testRoot 'KuroAA\KuroUI.ini'),"[KuroUI]`nEnableEarlyAA=0`nCaptureCandidates=0`n")
    $process=Start-Process -FilePath (Join-Path $testRoot 'smoke_d3d11.exe') -ArgumentList '1200' -WorkingDirectory $testRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $testRoot 'stdout.log') -RedirectStandardError (Join-Path $testRoot 'stderr.log') -PassThru
    $handle=$process.Handle
    if(!$process.WaitForExit(60000)) { throw "GPU fixture timed out: $backend (PID $($process.Id))" }
    $rendered=@(Select-String -LiteralPath (Join-Path $testRoot 'stdout.log') -Pattern 'Rendered .*mean RGB=' | ForEach-Object Line)
    $log=if(Test-Path -LiteralPath (Join-Path $testRoot 'KuroAA\AeonSR.log')) { [IO.File]::ReadAllText((Join-Path $testRoot 'KuroAA\AeonSR.log')) } else { '' }
    $compilerLog=[IO.File]::ReadAllText((Join-Path $testRoot 'ReShade.log'))
    $ready=$log -match 'upscaler\s+(DLSS|FSR[^:\r\n]*|XeSS): ready'
    $result=[pscustomobject]@{ Backend=$backend; ExitCode=$process.ExitCode; Nonblank=($rendered.Count -gt 0); RuntimeReady=$ready; ShaderCompiled=($compilerLog -match "Successfully compiled .*Runtime.fx") }
    $results+=$result
    $result | ConvertTo-Json -Compress | Write-Output
    [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'results.json'),($results | ConvertTo-Json -Depth 4))
}
if(@($results | Where-Object { $_.ExitCode -ne 0 -or !$_.Nonblank -or !$_.RuntimeReady -or !$_.ShaderCompiled }).Count -gt 0) { throw 'One or more runtime smoke checks failed.' }
