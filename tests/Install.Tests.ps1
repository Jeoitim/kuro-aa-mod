param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh test directory.'}
New-Item -ItemType Directory -Path $root | Out-Null
New-Item -ItemType File -Path (Join-Path $root 'ed9.exe') | Out-Null
$gameHash=(Get-FileHash -LiteralPath (Join-Path $root 'ed9.exe')).Hash
$install=Join-Path $PSScriptRoot '..\tools\Install.ps1'
$uninstall=Join-Path $PSScriptRoot '..\tools\Uninstall.ps1'
& $install -GameDirectory $root -PackageDirectory $PackageDirectory
$record=Join-Path $root '.kuro-aa-install.json'
$receipt=Get-Content -LiteralPath $record -Raw | ConvertFrom-Json
$rejected=$false
try{& $install -GameDirectory $root -PackageDirectory $PackageDirectory}catch{$rejected=$true}
if(!$rejected){throw 'Repeated installation was not rejected.'}
$exe=Join-Path $root 'KuroAA\KuroAA.Settings.exe'
function Run-Settings([string[]]$arguments){
    $p=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
    if($p.ExitCode){throw 'Settings command failed.'}
}
Run-Settings @('--self-test')
$ruleFile=Join-Path $root 'KuroAA\KuroUI.ini'
if((Get-Content -LiteralPath $ruleFile -Raw) -notmatch '(?m)^AARule=engine'){throw 'Default engine rule missing.'}
if((Get-Content -LiteralPath $ruleFile -Raw) -notmatch '(?m)^PreviewVendorAA=0\r?$'){throw 'Experimental character AA must default to off.'}
foreach($rule in @('engine','shader','full')){
    Run-Settings @('--rule',$rule)
    $ini=Get-Content -LiteralPath $ruleFile -Raw
    if($ini -notmatch "(?m)^AARule=$rule\r?$"){throw 'Rule save failed.'}
    if($rule -eq 'engine' -and ($ini -notmatch '(?m)^EngineUIFunctionRVA=37f480' -or $ini -notmatch '(?m)^EarlyUIShaderHash=0')){throw 'Engine rule isolation failed.'}
    if($rule -eq 'shader' -and ($ini -notmatch '(?m)^EarlyUIShaderHash=23f7ff8def7a9871' -or $ini -notmatch '(?m)^EngineUIFunctionRVA=0')){throw 'Shader rule isolation failed.'}
    if($rule -eq 'full' -and ($ini -notmatch '(?m)^EnableEarlyAA=0' -or $ini -notmatch '(?m)^SkipUnmatchedFrames=0')){throw 'Full-frame rule mapping failed.'}
    Run-Settings @('--backend','fsr')
    if((Get-Content -LiteralPath $ruleFile -Raw) -notmatch "(?m)^AARule=$rule\r?$"){throw 'Backend switch reset the rule.'}
}
Run-Settings @('--rule','engine')
Run-Settings @('--disable')
if(!(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll.kuro-disabled'))){throw 'Disable failed.'}
Run-Settings @('--enable')
foreach($backend in @('dlss','fsr','xess','off','auto')){
    Run-Settings @('--backend',$backend)
    $ini=Get-Content -LiteralPath (Join-Path $root 'KuroAA\AeonSR.ini') -Raw
    $expected=@{dlss='0';fsr='1';xess='2';off='3';auto='4294967295'}[$backend]
    if($ini -notmatch "(?m)^Upscaler=$expected\r?$" -or $ini -notmatch '(?m)^SpatialJitter=0' -or $ini -notmatch '(?m)^Sharpness=0' -or $ini -notmatch '(?m)^NeuralRender=0'){throw 'Backend mapping or defaults failed.'}
    $enabled=if($backend -eq 'off'){0}else{1}
    if($ini -notmatch "(?m)^Enabled=$enabled\r?$"){throw 'Enable state failed.'}
}
$backup=[IO.File]::ReadAllText($record)
$receipt.Files[0].Path='..\must-not-remove.txt'
[IO.File]::WriteAllText($record,($receipt | ConvertTo-Json -Depth 5))
$rejected=$false
try{& $uninstall -GameDirectory $root}catch{$rejected=$true}
[IO.File]::WriteAllText($record,$backup)
if(!$rejected){throw 'Receipt path traversal was not rejected.'}
& $uninstall -GameDirectory $root
if((Get-FileHash -LiteralPath (Join-Path $root 'ed9.exe')).Hash -ne $gameHash){throw 'Game file changed.'}
if(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll')){throw 'Proxy remains after uninstall.'}
if(Test-Path -LiteralPath (Join-Path $root 'KuroAA')){throw 'Empty owned directory was not removed.'}
& $install -GameDirectory $root -PackageDirectory $PackageDirectory
& $uninstall -GameDirectory $root
Write-Output 'PASS: compact layout, config mapping, defaults, self-test, conflict rejection, enable/disable, safe uninstall.'
