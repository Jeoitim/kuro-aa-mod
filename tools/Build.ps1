param([string]$DependencyDirectory,[string]$NativeUIDirectory,[string]$OutputDirectory,[string]$Compiler = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe")
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$DependencyDirectory) { throw 'Supply a directory containing the verified AeonSR-release and runtime-addon folders.' }
$dependencyRoot = (Resolve-Path -LiteralPath $DependencyDirectory).Path
$aeon = Join-Path $dependencyRoot 'AeonSR-release'
$reshade = Join-Path $dependencyRoot 'runtime-addon\ReShade64.dll'
if ((Get-FileHash -LiteralPath $reshade).Hash -ne '0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7') { throw 'Unexpected ReShade add-on runtime.' }
if ((Get-FileHash -LiteralPath (Join-Path $aeon 'AeonSR.addon64')).Hash -ne 'B246F0558ACE78AAFB23F668003FC9C32CEFEFB367549ADEEB4AB065FFA33166') { throw 'Unexpected AeonSR add-on.' }
if(!$NativeUIDirectory){$NativeUIDirectory=Join-Path $projectRoot 'build\native-ui'}
$nativeModule=Join-Path $NativeUIDirectory 'KuroUI.addon64'
if(!(Test-Path -LiteralPath $nativeModule)){throw 'Build KuroUI.addon64 with MSVC first, or supply verified Windows workflow artifacts with -NativeUIDirectory.'}
$outputRoot = if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot 'dist\GameFiles'}
if (Test-Path -LiteralPath $outputRoot) { throw 'Output exists; use a fresh build directory or archive the prior build.' }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
Copy-Item -LiteralPath $reshade -Destination (Join-Path $outputRoot 'dxgi.dll')
Copy-Item -LiteralPath $nativeModule -Destination (Join-Path $outputRoot 'KuroUI.addon64')
foreach ($file in @('AeonSR.addon64','AeonSRPrebuild.exe','runtime','Licenses')) {
    Copy-Item -LiteralPath (Join-Path $aeon $file) -Destination (Join-Path $outputRoot $file) -Recurse
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'KuroTFAA') -Destination (Join-Path $outputRoot 'KuroTFAA') -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\ReShade.ini') -Destination (Join-Path $outputRoot 'ReShade.ini')
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\AeonSR.ini') -Destination (Join-Path $outputRoot 'AeonSR.ini')
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\profiles\KuroCLE.ini') -Destination (Join-Path $outputRoot 'KuroUI.ini')
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination (Join-Path $outputRoot 'Licenses\Kuro_AA_LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $dependencyRoot 'reshade\LICENSE.md') -Destination (Join-Path $outputRoot 'Licenses\ReShade_LICENSE.md')
& $Compiler /nologo /target:winexe /platform:x64 ("/out:" + (Join-Path $outputRoot 'KuroMod.Manager.exe')) /r:System.Windows.Forms.dll /r:System.Drawing.dll (Join-Path $projectRoot 'src\Manager.cs')
if ($LASTEXITCODE -ne 0) { throw 'Manager compilation failed.' }
$textures = Join-Path $outputRoot 'KuroTFAA\Textures'
New-Item -ItemType Directory -Path $textures -Force | Out-Null
Add-Type -AssemblyName System.Drawing
$mask = [Drawing.Bitmap]::new(8,8)
try { $graphics = [Drawing.Graphics]::FromImage($mask); try { $graphics.Clear([Drawing.Color]::Black) } finally { $graphics.Dispose() }; $mask.Save((Join-Path $textures 'KuroUIMask.png'),[Drawing.Imaging.ImageFormat]::Png) } finally { $mask.Dispose() }
$manifest = @(Get-ChildItem -LiteralPath $outputRoot -Recurse -File | ForEach-Object { [pscustomobject]@{ Path=$_.FullName.Substring($outputRoot.Length+1).Replace('\','/'); SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant() } })
[IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($outputRoot)) 'SHA256.json'),($manifest | ConvertTo-Json -Depth 4))
Write-Output "Built portable Mod with $($manifest.Count) files."
