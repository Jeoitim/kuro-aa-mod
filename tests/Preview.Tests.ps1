param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$VendorPreview,[switch]$RecreateViews,[int[]]$BackendIds=@(0,1,2))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use fresh preview test directory.'}
$package=(Resolve-Path $PackageDirectory).Path;$fixture=Join-Path (Resolve-Path $NativeDirectory).Path 'fixture.exe'
function Run([string]$directory){
    $mode=if($RecreateViews){'preview-recreate'}else{'preview'}
    $p=Start-Process -FilePath (Join-Path $directory 'fixture.exe') -ArgumentList '420',$mode -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput (Join-Path $directory 'stdout.log') -PassThru
    $handle=$p.Handle;if(!$p.WaitForExit(60000)){throw 'Preview fixture timed out.'};if($p.ExitCode){throw 'Preview fixture failed.'}
}
$baseline=Join-Path $root 'baseline';New-Item -ItemType Directory -Path $baseline -Force | Out-Null
Copy-Item -LiteralPath $fixture -Destination $baseline;Run $baseline
$raw=[IO.File]::ReadAllBytes((Join-Path $baseline 'smoke.bmp'));$stdout=Get-Content (Join-Path $baseline 'stdout.log') -Raw
$v=@{};foreach($key in @('HUD pixel shader hash','Engine UI RVA','Engine UI hash','Engine timestamp','Engine image size')){$v[$key]=[regex]::Match($stdout,([regex]::Escape($key)+'=([^\r\n]+)')).Groups[1].Value}
$results=@()
$cases=@($BackendIds | ForEach-Object{"engine-$_";"shader-$_"})
foreach($case in $cases){
    $rule=$case.Split('-')[0]
    $dir=Join-Path $root $case;New-Item -ItemType Directory -Path $dir | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $dir $_.Name) -Recurse}
    Copy-Item -LiteralPath $fixture -Destination $dir
    $enabled=1;$backend=$case.Split('-')[1];$vendorView=1
    [IO.File]::WriteAllText((Join-Path $dir 'KuroAA\AeonSR.ini'),"[AeonSR]`nEnabled=$enabled`nUpscaler=$backend`nUpscaleMode=0`nSpatialJitter=0`nKeepInterface=0`nNeuralRender=0`nSharpness=0`n")
    [IO.File]::WriteAllText((Join-Path $dir 'KuroAA\KuroUI.ini'),"[KuroUI]`nAARule=$rule`nPreviewVendorAA=$vendorView`nAllowOffscreenTarget=1`nEarlyUIShaderHash=$($v['HUD pixel shader hash'])`nEarlyUIVertexShaderHash=0`nEngineUIFunctionRVA=$($v['Engine UI RVA'])`nEngineUIFunctionHash=$($v['Engine UI hash'])`nEngineImageTimestamp=$($v['Engine timestamp'])`nEngineImageSize=$($v['Engine image size'])`n")
    Run $dir;$image=[IO.File]::ReadAllBytes((Join-Path $dir 'smoke.bmp'))
    $model=0;$ui=0;$alpha=0
    for($y=40;$y -lt 250;$y++){for($x=40;$x -lt 600;$x++){$o=54+($y*640+$x)*4;if($raw[$o] -ne $image[$o] -or $raw[$o+1] -ne $image[$o+1] -or $raw[$o+2] -ne $image[$o+2]){$model++};if($raw[$o+3] -ne $image[$o+3]){$alpha++}}}
    for($y=280;$y -lt 345;$y++){for($x=20;$x -lt 620;$x++){$o=54+($y*640+$x)*4;if($raw[$o] -ne $image[$o] -or $raw[$o+1] -ne $image[$o+1] -or $raw[$o+2] -ne $image[$o+2]){$ui++}}}
    $log=Get-Content (Join-Path $dir 'KuroAA\KuroUI.log') -Raw
    $aeon=Get-Content (Join-Path $dir 'KuroAA\AeonSR.log') -Raw
    $reuses=([regex]::Matches($aeon,'reusing warm preview context')).Count
    $result=[pscustomobject]@{Rule=$rule;Backend=$backend;VendorPreview=$true;WarmReuses=$reuses;ModelDifferentPixels=$model;UIDifferentPixels=$ui;AlphaDifferentPixels=$alpha;PreviewExecuted=($log -match 'preview AA=[1-9]')}
    $results+=$result;$result|ConvertTo-Json -Compress|Write-Output
    [IO.File]::WriteAllText((Join-Path $root 'results.json'),($results|ConvertTo-Json))
    if(!$result.PreviewExecuted -or $model -eq 0 -or $ui -ne 0 -or $alpha -ne 0){throw 'Preview AA isolation or state restoration failed.'}
    if($RecreateViews -and $reuses -ne 3){throw 'Preview context was not reused for all three texture replacements.'}
}
