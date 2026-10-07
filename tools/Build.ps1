param([string]$NativeUIDirectory,[string]$OutputDirectory,[string]$AeonPreviewDirectory,[string]$Compiler="$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe")
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vendor=Join-Path $root 'vendor'
$manifest=Get-Content -LiteralPath (Join-Path $vendor 'SHA256.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest){
    $file=[IO.Path]::GetFullPath((Join-Path $vendor $entry.Path))
    if(!$file.StartsWith($vendor+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe vendor manifest path.'}
    if((Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant() -ne $entry.SHA256){throw "Vendor checksum mismatch: $($entry.Path)"}
}
if(!$NativeUIDirectory){$NativeUIDirectory=Join-Path $root 'build\native-ui'}
$native=Join-Path $NativeUIDirectory 'KuroUI.addon64'
if(!(Test-Path -LiteralPath $native)){throw 'Build the native component with MSVC first.'}
$output=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $root 'dist\GameFiles'}
if(Test-Path -LiteralPath $output){throw 'Output exists; use a fresh build directory.'}
$mod=Join-Path $output 'KuroAA'
New-Item -ItemType Directory -Path $mod -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $vendor 'reshade\dxgi.dll') -Destination $output
Copy-Item -LiteralPath (Join-Path $root 'packaging\ReShade.ini') -Destination $output
foreach($name in @('AeonSR.addon64','AeonSRPrebuild.exe','runtime','Licenses')){
    Copy-Item -LiteralPath (Join-Path $vendor ('aeonsr\'+$name)) -Destination $mod -Recurse
}
if($AeonPreviewDirectory){
    Copy-Item -LiteralPath (Join-Path $AeonPreviewDirectory 'AeonSR.addon64') -Destination $mod -Force
    Copy-Item -LiteralPath (Join-Path $AeonPreviewDirectory 'AeonSRPrebuild.exe') -Destination $mod -Force
}
Copy-Item -LiteralPath $native -Destination $mod
foreach($name in @('AeonSR.ini','Native.ini','Shaders')){Copy-Item -LiteralPath (Join-Path $root ('packaging\'+$name)) -Destination $mod -Recurse}
Copy-Item -LiteralPath (Join-Path $root 'packaging\profiles\KuroCLE.ini') -Destination (Join-Path $mod 'KuroUI.ini')
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $mod 'Licenses\Kuro_AA_LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $vendor 'reshade\LICENSE.md') -Destination (Join-Path $mod 'Licenses\ReShade_LICENSE.md')
& $Compiler /nologo /codepage:65001 /target:winexe /platform:x64 ("/out:"+(Join-Path $mod 'KuroAA.Settings.exe')) /r:System.Windows.Forms.dll /r:System.Drawing.dll (Join-Path $root 'src\Settings.cs')
if($LASTEXITCODE){throw 'Settings compilation failed.'}
$files=@(Get-ChildItem -LiteralPath $output -Recurse -File | ForEach-Object{[pscustomobject]@{Path=$_.FullName.Substring($output.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
[IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($output)) 'SHA256.json'),($files | ConvertTo-Json -Depth 4))
Write-Output "Built Kuro AA 0.3.1: $($files.Count) files."
