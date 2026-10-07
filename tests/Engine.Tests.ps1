param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh test directory.'}
$native=(Resolve-Path -LiteralPath $NativeDirectory).Path
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
function Run([string]$directory,[string]$mode){
    $p=Start-Process -FilePath (Join-Path $directory 'fixture.exe') -ArgumentList '420',$mode -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput (Join-Path $directory 'stdout.log') -PassThru
    $handle=$p.Handle
    if(!$p.WaitForExit(50000)){throw 'Engine fixture timed out.'}
    if($p.ExitCode){throw 'Engine fixture failed.'}
}
$baseline=Join-Path $root 'baseline'
New-Item -ItemType Directory -Path $baseline -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $native 'fixture.exe') -Destination $baseline
Run $baseline 'letterbox'
$raw=[IO.File]::ReadAllBytes((Join-Path $baseline 'smoke.bmp'))
$stdout=Get-Content -LiteralPath (Join-Path $baseline 'stdout.log') -Raw
$values=@{}
foreach($key in @('Engine UI RVA','Engine UI hash','Engine timestamp','Engine image size')){
    $values[$key]=[regex]::Match($stdout,([regex]::Escape($key)+'=([^\r\n]+)')).Groups[1].Value
}
$results=@()
foreach($case in @('dlss','fsr','xess','invalid-fingerprint','ui-first','ui-only','disabled-boundary','ui-aux','ui-first-aux')){
    $directory=Join-Path $root $case
    New-Item -ItemType Directory -Path $directory | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $directory $_.Name) -Recurse}
    Copy-Item -LiteralPath (Join-Path $native 'fixture.exe') -Destination $directory
    Copy-Item -LiteralPath (Join-Path $native 'KuroUI.addon64') -Destination (Join-Path $directory 'KuroAA\KuroUI.addon64') -Force
    $backend=if($case -in @('dlss','fsr','xess')){$case}else{'dlss'}
    $p=Start-Process -FilePath (Join-Path $directory 'KuroAA\KuroAA.Settings.exe') -ArgumentList '--backend',$backend -WindowStyle Hidden -PassThru -Wait
    if($p.ExitCode){throw 'Backend selection failed.'}
    $hash=if($case -eq 'invalid-fingerprint'){'123456789abcdef0'}else{$values['Engine UI hash']}
    $enable=if($case -eq 'disabled-boundary'){0}else{1}
    [IO.File]::WriteAllText((Join-Path $directory 'KuroAA\KuroUI.ini'),"[KuroUI]`nEnableEarlyAA=$enable`nEarlyUIShaderHash=0`nEarlyUIVertexShaderHash=0`nAllowOffscreenTarget=1`nSkipUnmatchedFrames=1`nEngineUIFunctionRVA=$($values['Engine UI RVA'])`nEngineUIFunctionHash=$hash`nEngineImageTimestamp=$($values['Engine timestamp'])`nEngineImageSize=$($values['Engine image size'])`nTraceDraws=0`n")
    $mode=if($case -in @('ui-first','ui-only','ui-aux','ui-first-aux')){$case}else{'letterbox'}
    $reference=$raw
    if($mode -ne 'letterbox'){
        $referenceDir=Join-Path $root ('baseline-'+$mode)
        New-Item -ItemType Directory -Path $referenceDir | Out-Null
        Copy-Item -LiteralPath (Join-Path $native 'fixture.exe') -Destination $referenceDir
        Run $referenceDir $mode
        $reference=[IO.File]::ReadAllBytes((Join-Path $referenceDir 'smoke.bmp'))
    }
    Run $directory $mode
    $image=[IO.File]::ReadAllBytes((Join-Path $directory 'smoke.bmp'))
    $ui=0;$scene=0
    for($y=280;$y -lt 345;$y++){for($x=20;$x -lt 620;$x++){$o=54+($y*640+$x)*4;if($reference[$o] -ne $image[$o] -or $reference[$o+1] -ne $image[$o+1] -or $reference[$o+2] -ne $image[$o+2]){$ui++}}}
    for($y=50;$y -lt 260;$y++){for($x=40;$x -lt 600;$x++){$o=54+($y*640+$x)*4;if($reference[$o] -ne $image[$o] -or $reference[$o+1] -ne $image[$o+1] -or $reference[$o+2] -ne $image[$o+2]){$scene++}}}
    $log=Get-Content -LiteralPath (Join-Path $directory 'KuroAA\KuroUI.log') -Raw
    $executed=$log -match 'engine frames=[1-9]'
    if($case -in @('invalid-fingerprint','ui-first','ui-only','disabled-boundary','ui-first-aux')){
        if($ui -ne 0 -or $scene -ne 0 -or $executed){throw 'Invalid engine profile did not safely bypass.'}
    }elseif($ui -ne 0 -or $scene -eq 0 -or !$executed){throw "Engine route failed: $case"}
    $result=[pscustomobject]@{Backend=$case;UIDifferentPixels=$ui;SceneDifferentPixels=$scene;EngineExecuted=$executed;SceneWidth=640;OutputWidth=800}
    $results+=$result;$result | ConvertTo-Json -Compress | Write-Output
    [IO.File]::WriteAllText((Join-Path $root 'results.json'),($results | ConvertTo-Json -Depth 4))
}
