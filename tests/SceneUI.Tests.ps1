param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$native=(Resolve-Path -LiteralPath $NativeDirectory).Path
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh output directory.'}
New-Item -ItemType Directory -Path $root | Out-Null
function Run-Fixture([string]$directory) {
    $process=Start-Process -FilePath (Join-Path $directory 'smoke_ui.exe') -ArgumentList '600' -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log') -PassThru
    $handle=$process.Handle
    if(!$process.WaitForExit(50000)){throw 'Fixture timed out.'}
    if($process.ExitCode -ne 0){throw "GPU fixture failed: $($process.ExitCode)"}
}
$baseline=Join-Path $root 'baseline'
New-Item -ItemType Directory -Path $baseline | Out-Null
Copy-Item -LiteralPath (Join-Path $native 'smoke_ui.exe') -Destination $baseline
Run-Fixture $baseline
$raw=[IO.File]::ReadAllBytes((Join-Path $baseline 'smoke.bmp'))
$signature=Select-String -LiteralPath (Join-Path $baseline 'stdout.log') -Pattern '^HUD pixel shader hash=([0-9a-f]+)$'
if(!$signature){throw 'Fixture shader signature not found.'}
$fixtureHash=$signature.Matches[0].Groups[1].Value
$results=@()
foreach($backend in @('dlss','fsr','xess','tfaa')) {
    $testRoot=Join-Path $root $backend
    New-Item -ItemType Directory -Path $testRoot | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $testRoot $_.Name) -Recurse}
    Copy-Item -LiteralPath (Join-Path $native 'smoke_ui.exe') -Destination $testRoot
    Copy-Item -LiteralPath (Join-Path $native 'KuroUI.addon64') -Destination $testRoot -Force
    $manager=Start-Process -FilePath (Join-Path $testRoot 'KuroMod.Manager.exe') -ArgumentList '--backend',$backend -WindowStyle Hidden -PassThru
    $handle=$manager.Handle; $manager.WaitForExit()
    if($manager.ExitCode -ne 0){throw 'Backend selection failed.'}
    [IO.File]::WriteAllText((Join-Path $testRoot 'KuroUI.ini'),"[KuroUI]`nEnableEarlyAA=1`nEarlyUIShaderHash=$fixtureHash`nAllowOffscreenTarget=0`nCaptureCandidates=0`n")
    Run-Fixture $testRoot
    $output=[IO.File]::ReadAllBytes((Join-Path $testRoot 'smoke.bmp'))
    if($output.Length -ne $raw.Length){throw 'Image dimensions changed.'}
    $uiDifferent=0; $sceneDifferent=0
    for($y=280;$y -lt 345;$y++){for($x=20;$x -lt 620;$x++){
        $offset=54+($y*640+$x)*4
        if($raw[$offset] -ne $output[$offset] -or $raw[$offset+1] -ne $output[$offset+1] -or $raw[$offset+2] -ne $output[$offset+2]){$uiDifferent++}
    }}
    for($y=50;$y -lt 260;$y++){for($x=40;$x -lt 600;$x++){
        $offset=54+($y*640+$x)*4
        if($raw[$offset] -ne $output[$offset] -or $raw[$offset+1] -ne $output[$offset+1] -or $raw[$offset+2] -ne $output[$offset+2]){$sceneDifferent++}
    }}
    $log=[IO.File]::ReadAllText((Join-Path $testRoot 'KuroUI.log'))
    $result=[pscustomobject]@{Backend=$backend;UIDifferentPixels=$uiDifferent;SceneDifferentPixels=$sceneDifferent;EarlyExecuted=($log -match 'early frames=[1-9]')}
    $results+=$result; $result | ConvertTo-Json -Compress | Write-Output
    [IO.File]::WriteAllText((Join-Path $root 'results.json'),($results | ConvertTo-Json -Depth 4))
    if($uiDifferent -ne 0 -or $sceneDifferent -eq 0 -or !$result.EarlyExecuted){throw "Scene-only AA contract failed: $backend"}
}
