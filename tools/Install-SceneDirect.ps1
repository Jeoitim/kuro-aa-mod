param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [ValidateRange(0,600)][int]$ProfileFrames=120
)
$ErrorActionPreference='Stop'
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$packageRoot=(Resolve-Path -LiteralPath $PackageDirectory).Path
$mod=Join-Path $gameRoot 'KuroAA'
if(!(Test-Path -LiteralPath (Join-Path $gameRoot 'ed9.exe'))){throw 'Game executable missing.'}
foreach($process in @(Get-Process ed9 -ErrorAction SilentlyContinue)){
    if([string]::Equals($process.Path,(Join-Path $gameRoot 'ed9.exe'),[StringComparison]::OrdinalIgnoreCase)){throw 'Close the game before replacing components.'}
}
$components=@('KuroUI.addon64','AeonSR.addon64','KuroAA.Settings.exe')
$settings=@('KuroUI.ini','AeonSR.ini')
foreach($name in $components){
    if(!(Test-Path -LiteralPath (Join-Path $packageRoot "KuroAA/$name")) || !(Test-Path -LiteralPath (Join-Path $mod $name))){throw "Missing component: $name"}
}
foreach($name in $settings){if(!(Test-Path -LiteralPath (Join-Path $mod $name))){throw "Missing existing settings: $name"}}
$backup=Join-Path $gameRoot ('.kuro-aa-backups/scene-direct-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$backup=[IO.Path]::GetFullPath($backup)
if(!$backup.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe backup path.'}
New-Item -ItemType Directory -Path $backup | Out-Null
foreach($name in ($components+$settings)){Copy-Item -LiteralPath (Join-Path $mod $name) -Destination (Join-Path $backup $name)}
$receipt=Join-Path $gameRoot '.kuro-aa-install.json'
if(Test-Path -LiteralPath $receipt){Copy-Item -LiteralPath $receipt -Destination (Join-Path $backup '.kuro-aa-install.json')}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KuroSceneProfile {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
}
'@
function Set-Key([string]$file,[string]$section,[string]$key,[string]$value){
    if(![KuroSceneProfile]::WritePrivateProfileString($section,$key,$value,$file)){throw "Could not write $key"}
}
try {
    foreach($name in $components){
        $source=Join-Path $packageRoot "KuroAA/$name";$target=Join-Path $mod $name
        Copy-Item -LiteralPath $source -Destination $target -Force
        if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash){throw "Installed checksum mismatch: $name"}
    }
    $ui=Join-Path $mod 'KuroUI.ini';$aeon=Join-Path $mod 'AeonSR.ini'
    Set-Key $ui 'KuroUI' 'SceneVendorDirect' '1'
    Set-Key $ui 'KuroUI' 'SceneScaleProfileFrames' $ProfileFrames.ToString()
    # Keep the old spatial test disabled. Existing unrelated preferences are preserved.
    Set-Key $ui 'KuroUI' 'SceneScaleContinuous' '0'
    Set-Key $ui 'KuroUI' 'SceneScaleCompareOnce' '0'
    Set-Key $ui 'KuroUI' 'SceneRenderScale' '100'
    Set-Key $aeon 'AeonSR' 'Upscaler' '0'
    Set-Key $aeon 'AeonSR' 'UpscaleMode' '3'
    if(Test-Path -LiteralPath $receipt){
        $data=Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
        foreach($file in $data.Files){
            $relative=$file.Path.Replace('\','/')
            foreach($name in $components){if($relative -eq "KuroAA/$name"){$file.Hash=(Get-FileHash -LiteralPath (Join-Path $mod $name)).Hash}}
        }
        [IO.File]::WriteAllText($receipt,($data | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    }
    [pscustomobject]@{Installed=$true;Mode='DLSS Balanced';SceneVendorDirect=1;ProfileFrames=$ProfileFrames;Backup=$backup} | ConvertTo-Json
} catch {
    foreach($name in ($components+$settings)){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $mod $name) -Force}
    if(Test-Path -LiteralPath (Join-Path $backup '.kuro-aa-install.json')){Copy-Item -LiteralPath (Join-Path $backup '.kuro-aa-install.json') -Destination $receipt -Force}
    throw
}
