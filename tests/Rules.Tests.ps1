param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$native=(Resolve-Path -LiteralPath $NativeDirectory).Path
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh test directory.'}
function Run([string]$directory,[string]$mode){
    $p=Start-Process -FilePath (Join-Path $directory 'fixture.exe') -ArgumentList '420',$mode -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput (Join-Path $directory 'stdout.log') -PassThru
    $handle=$p.Handle
    if(!$p.WaitForExit(60000)){throw 'Rule fixture timeout.'}
    if($p.ExitCode){throw 'Rule fixture failed.'}
}
$baselines=@{}
foreach($mode in @('letterbox','offscreen')){
    $directory=Join-Path $root ('baseline-'+$mode)
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $native 'fixture.exe') -Destination $directory
    Run $directory $mode
    $baselines[$mode]=[IO.File]::ReadAllBytes((Join-Path $directory 'smoke.bmp'))
}
$stdout=Get-Content -LiteralPath (Join-Path $root 'baseline-letterbox\stdout.log') -Raw
$values=@{}
foreach($key in @('HUD pixel shader hash','Engine UI RVA','Engine UI hash','Engine timestamp','Engine image size')){$values[$key]=[regex]::Match($stdout,([regex]::Escape($key)+'=([^\r\n]+)')).Groups[1].Value}
$results=@()
foreach($case in @('engine','shader','full','full-stale-guards','shader-unmatched')){
    $directory=Join-Path $root $case
    New-Item -ItemType Directory -Path $directory | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $directory $_.Name) -Recurse}
    Copy-Item -LiteralPath (Join-Path $native 'fixture.exe') -Destination $directory
    $rule=if($case -eq 'full-stale-guards'){'full'}elseif($case -eq 'shader-unmatched'){'shader'}else{$case}
    $exe=Join-Path $directory 'KuroAA\KuroAA.Settings.exe'
    foreach($args in @(@('--backend','dlss'),@('--rule',$rule))){$p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru -Wait;if($p.ExitCode){throw 'CLI rule mapping failed.'}}
    $hash=if($case -eq 'shader-unmatched'){'0123456789abcdef'}else{$values['HUD pixel shader hash']}
    # Deliberately leave contradictory legacy guards to verify that AARule is authoritative.
    [IO.File]::WriteAllText((Join-Path $directory 'KuroAA\KuroUI.ini'),"[KuroUI]`nAARule=$rule`nEnableEarlyAA=1`nEarlyUIShaderHash=$hash`nEarlyUIVertexShaderHash=0`nAllowOffscreenTarget=1`nSkipUnmatchedFrames=1`nEngineUIFunctionRVA=$($values['Engine UI RVA'])`nEngineUIFunctionHash=$($values['Engine UI hash'])`nEngineImageTimestamp=$($values['Engine timestamp'])`nEngineImageSize=$($values['Engine image size'])`nTraceDraws=0`n")
    $mode=if($rule -eq 'engine'){'letterbox'}else{'offscreen'}
    Run $directory $mode
    $reference=$baselines[$mode];$image=[IO.File]::ReadAllBytes((Join-Path $directory 'smoke.bmp'))
    $ui=0;$scene=0
    for($y=280;$y -lt 345;$y++){for($x=20;$x -lt 620;$x++){$o=54+($y*640+$x)*4;if($reference[$o] -ne $image[$o] -or $reference[$o+1] -ne $image[$o+1] -or $reference[$o+2] -ne $image[$o+2]){$ui++}}}
    for($y=50;$y -lt 260;$y++){for($x=40;$x -lt 600;$x++){$o=54+($y*640+$x)*4;if($reference[$o] -ne $image[$o] -or $reference[$o+1] -ne $image[$o+1] -or $reference[$o+2] -ne $image[$o+2]){$scene++}}}
    $log=Get-Content -LiteralPath (Join-Path $directory 'KuroAA\KuroUI.log') -Raw
    if($case -eq 'shader-unmatched'){if($ui -ne 0 -or $scene -ne 0){throw 'Unmatched shader profile altered frame.'}}
    elseif($rule -eq 'full'){if($ui -eq 0 -or $scene -eq 0 -or $log -notmatch 'AA rule=full'){throw 'Explicit full-frame rule did not process final image.'}}
    elseif($ui -ne 0 -or $scene -eq 0){throw 'Protected rule failed scene/UI separation.'}
    if($rule -eq 'engine' -and $log -notmatch 'engine frames=[1-9]'){throw 'Engine route did not execute.'}
    if($rule -eq 'shader' -and $log -match 'engine frames=[1-9]'){throw 'Shader rule accidentally used engine route.'}
    $result=[pscustomobject]@{Case=$case;Rule=$rule;UIDifferentPixels=$ui;SceneDifferentPixels=$scene}
    $results+=$result;$result | ConvertTo-Json -Compress | Write-Output
    [IO.File]::WriteAllText((Join-Path $root 'results.json'),($results | ConvertTo-Json -Depth 4))
}
