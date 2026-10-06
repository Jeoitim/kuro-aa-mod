param([Parameter(Mandatory=$true)][string]$PackageDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root) { throw 'Test directory already exists.' }
New-Item -ItemType Directory -Path $root -Force | Out-Null
New-Item -ItemType File -Path (Join-Path $root 'ed9.exe') | Out-Null
$gameHash=(Get-FileHash -LiteralPath (Join-Path $root 'ed9.exe')).Hash
$install=Join-Path $PSScriptRoot '..\tools\Install.ps1'
$uninstall=Join-Path $PSScriptRoot '..\tools\Uninstall.ps1'
& $install -GameDirectory $root -PackageDirectory $PackageDirectory
$receipt=Get-Content -LiteralPath (Join-Path $root '.kuro-tfaa-install.json') -Raw | ConvertFrom-Json
if(@($receipt.Files).Count -lt 10) { throw 'Incomplete installation receipt.' }
$rejected=$false
try { & $install -GameDirectory $root -PackageDirectory $PackageDirectory } catch { $rejected=$true }
if(!$rejected) { throw 'Reinstallation conflict was not rejected.' }
$manager=Start-Process -FilePath (Join-Path $root 'KuroMod.Manager.exe') -ArgumentList '--disable' -WindowStyle Hidden -PassThru
$handle=$manager.Handle; $manager.WaitForExit()
if($manager.ExitCode -ne 0 -or !(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll.kuro-disabled'))) { throw 'Disable test failed.' }
$manager=Start-Process -FilePath (Join-Path $root 'KuroMod.Manager.exe') -ArgumentList '--enable' -WindowStyle Hidden -PassThru
$handle=$manager.Handle; $manager.WaitForExit()
if($manager.ExitCode -ne 0 -or !(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll'))) { throw 'Enable test failed.' }
foreach($backend in @('dlss','fsr','xess','tfaa','off')) {
    $manager=Start-Process -FilePath (Join-Path $root 'KuroMod.Manager.exe') -ArgumentList '--backend',$backend -WindowStyle Hidden -PassThru
    $handle=$manager.Handle; $manager.WaitForExit()
    if($manager.ExitCode -ne 0) { throw "Backend switch failed: $backend" }
    $aeon=Get-Content -LiteralPath (Join-Path $root 'AeonSR.ini') -Raw
    $reshade=Get-Content -LiteralPath (Join-Path $root 'ReShade.ini') -Raw
    if($backend -eq 'tfaa' -and ($aeon -notmatch '(?m)^Enabled=0' -or $reshade -notmatch 'Stable.ini')) { throw 'TFAA exclusivity failed.' }
    if($backend -ne 'tfaa' -and $reshade -notmatch 'Native.ini') { throw 'Native preset exclusivity failed.' }
    if($aeon -notmatch '(?m)^SpatialJitter=0') { throw 'Jitter-free default was not preserved.' }
}
# Scene profile remains separate from backend choices and disables unknown-frame AA.
if(Test-Path -LiteralPath (Join-Path $root 'KuroUI.ini')) {
    $sceneConfig=Get-Content -LiteralPath (Join-Path $root 'KuroUI.ini') -Raw
    if($sceneConfig -notmatch '(?m)^SkipUnmatchedFrames=1'){throw 'Unknown-frame bypass is not configured.'}
}
# Verify a crafted receipt cannot reach outside the intended game root.
$receiptPath=Join-Path $root '.kuro-tfaa-install.json'
$originalReceipt=[IO.File]::ReadAllText($receiptPath)
$receipt.Files[0].Path='..\must-not-remove.txt'
[IO.File]::WriteAllText($receiptPath,($receipt | ConvertTo-Json -Depth 5))
$rejected=$false
try { & $uninstall -GameDirectory $root } catch { $rejected=$true }
[IO.File]::WriteAllText($receiptPath,$originalReceipt)
if(!$rejected -or !(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll'))) { throw 'Receipt path safety test failed.' }
& $uninstall -GameDirectory $root
if((Get-FileHash -LiteralPath (Join-Path $root 'ed9.exe')).Hash -ne $gameHash) { throw 'Game file changed.' }
if(Test-Path -LiteralPath (Join-Path $root 'dxgi.dll')) { throw 'Injection file left behind.' }
Write-Output 'PASS: conflict detection, backend exclusivity, jitter default, enable/disable, path safety, uninstall, game file preservation.'
