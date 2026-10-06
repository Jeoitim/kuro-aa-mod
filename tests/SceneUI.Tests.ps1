param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$fixture=Join-Path (Resolve-Path -LiteralPath $NativeDirectory).Path 'smoke_ui.exe'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh test directory.'}
function Run-Fixture([string]$directory,[bool]$offscreen=$false){
    $arguments=if($offscreen){@('600','offscreen')}else{@('600')}
    $p=Start-Process -FilePath (Join-Path $directory 'smoke_ui.exe') -ArgumentList $arguments -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log') -PassThru
    $handle=$p.Handle
    if(!$p.WaitForExit(60000)){throw 'GPU fixture timed out.'}
    if($p.ExitCode){throw 'GPU fixture failed.'}
}
$baseline=Join-Path $root 'baseline'
New-Item -ItemType Directory -Path $baseline -Force | Out-Null
Copy-Item -LiteralPath $fixture -Destination $baseline
Run-Fixture $baseline
$raw=[IO.File]::ReadAllBytes((Join-Path $baseline 'smoke.bmp'))
$match=Select-String -LiteralPath (Join-Path $baseline 'stdout.log') -Pattern '^HUD pixel shader hash=([0-9a-f]+)$'
$signature=$match.Matches[0].Groups[1].Value
$results=@()
foreach($case in @('dlss','fsr','xess','offscreen','unmatched')){
    $directory=Join-Path $root $case
    New-Item -ItemType Directory -Path $directory | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $directory $_.Name) -Recurse}
    Copy-Item -LiteralPath $fixture -Destination $directory
    $backend=if($case -eq 'offscreen' -or $case -eq 'unmatched'){'dlss'}else{$case}
    $p=Start-Process -FilePath (Join-Path $directory 'KuroAA\KuroAA.Settings.exe') -ArgumentList '--backend',$backend -WindowStyle Hidden -PassThru -Wait
    if($p.ExitCode){throw 'Backend selection failed.'}
    $hash=if($case -eq 'unmatched'){'0123456789abcdef'}else{$signature}
    [IO.File]::WriteAllText((Join-Path $directory 'KuroAA\KuroUI.ini'),"[KuroUI]`nEnableEarlyAA=1`nEarlyUIShaderHash=$hash`nAllowOffscreenTarget=1`nSkipUnmatchedFrames=1`nTraceDraws=0`nCaptureCandidates=0`n")
    Run-Fixture $directory ($case -eq 'offscreen')
    $image=[IO.File]::ReadAllBytes((Join-Path $directory 'smoke.bmp'))
    $ui=0;$scene=0
    for($y=280;$y -lt 345;$y++){for($x=20;$x -lt 620;$x++){$offset=54+($y*640+$x)*4;if($raw[$offset] -ne $image[$offset] -or $raw[$offset+1] -ne $image[$offset+1] -or $raw[$offset+2] -ne $image[$offset+2]){$ui++}}}
    for($y=50;$y -lt 260;$y++){for($x=40;$x -lt 600;$x++){$offset=54+($y*640+$x)*4;if($raw[$offset] -ne $image[$offset] -or $raw[$offset+1] -ne $image[$offset+1] -or $raw[$offset+2] -ne $image[$offset+2]){$scene++}}}
    $log=Get-Content -LiteralPath (Join-Path $directory 'KuroAA\KuroUI.log') -Raw
    $early=$log -match 'early frames=[1-9]'
    if($case -eq 'unmatched'){
        if((Get-FileHash -LiteralPath (Join-Path $baseline 'smoke.bmp')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $directory 'smoke.bmp')).Hash){throw 'Unknown signature altered the frame.'}
    }elseif($ui -ne 0 -or $scene -eq 0 -or !$early){throw "Scene/UI contract failed: $case"}
    $result=[pscustomobject]@{Case=$case;UIDifferentPixels=$ui;SceneDifferentPixels=$scene;EarlyExecuted=$early}
    $results+=$result;$result | ConvertTo-Json -Compress | Write-Output
    [IO.File]::WriteAllText((Join-Path $root 'results.json'),($results | ConvertTo-Json -Depth 4))
}
