param([Parameter(Mandatory=$true)][string]$GameDirectory,[Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path.TrimEnd('\')
$mod=Join-Path $gameRoot 'KuroAA'
if(!(Test-Path -LiteralPath (Join-Path $gameRoot 'ed9.exe')) -or !(Test-Path -LiteralPath $mod)){throw 'An existing Kuro AA installation is required.'}
foreach($process in @(Get-Process ed9,'KuroAA.Settings' -ErrorAction SilentlyContinue)){
    if($process.Path -and $process.Path.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Close the game and settings manager before upgrading.'}
}
$proxy=Join-Path $gameRoot 'dxgi.dll'
if(!(Test-Path -LiteralPath $proxy) -or (Get-FileHash -LiteralPath $proxy).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package 'dxgi.dll')).Hash){throw 'The injection file is not the expected Kuro AA runtime. Resolve the conflict first.'}
$backup=[IO.Path]::GetFullPath((Join-Path $gameRoot ('.kuro-aa-backups/0.4.1-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))))
if(!$backup.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe backup path.'}
New-Item -ItemType Directory -Path $backup | Out-Null
$files=@(Get-ChildItem -LiteralPath $package -Recurse -File)
$plan=@();foreach($file in $files){
    $relative=$file.FullName.Substring($package.Length+1);$target=[IO.Path]::GetFullPath((Join-Path $gameRoot $relative))
    if(!$target.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe package path.'}
    $exists=Test-Path -LiteralPath $target
    $mutable=$file.Extension -eq '.ini'
    $preserve=$exists -and ($mutable -or $relative.StartsWith('KuroAA\runtime\',[StringComparison]::OrdinalIgnoreCase))
    if($exists){$saved=Join-Path $backup $relative;New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($saved)) -Force | Out-Null;Copy-Item -LiteralPath $target -Destination $saved}
    $plan+=[pscustomobject]@{Relative=$relative;Source=$file.FullName;Target=$target;Existed=$exists;Preserve=$preserve;Mutable=$mutable}
}
$receipt=Join-Path $gameRoot '.kuro-aa-install.json'
if(Test-Path -LiteralPath $receipt){Copy-Item -LiteralPath $receipt -Destination (Join-Path $backup '.kuro-aa-install.json')}
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class KuroNativeProfile {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
    public static bool RemoveKey(string section,string key,string path) { return WritePrivateProfileString(section,key,null,path); }
}
'@
function Set-Key([string]$path,[string]$section,[string]$key,[AllowNull()][string]$value){
    if(![KuroNativeProfile]::WritePrivateProfileString($section,$key,$value,$path)){throw "Cannot update $key"}
}
try {
    foreach($entry in $plan){if(!$entry.Preserve){
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($entry.Target)) -Force | Out-Null
        Copy-Item -LiteralPath $entry.Source -Destination $entry.Target -Force
        if((Get-FileHash -LiteralPath $entry.Source).Hash -ne (Get-FileHash -LiteralPath $entry.Target).Hash){throw "Checksum mismatch: $($entry.Relative)"}
    }}
    Set-Key (Join-Path $mod 'AeonSR.ini') 'AeonSR' 'UpscaleMode' '0'
    $ui=Join-Path $mod 'KuroUI.ini'
    foreach($key in @('SceneVendorDirect','SceneScaleProfileFrames','SceneScaleContinuous','SceneScaleCompareOnce','SceneRenderScale','SceneVRS','TraceRenderTargets','TraceRenderLayouts','TraceRenderValues')){
        if(![KuroNativeProfile]::RemoveKey('KuroUI',$key,$ui)){throw "Cannot remove legacy key $key"}
    }
    $records=@($plan | ForEach-Object {[pscustomobject]@{Path=$_.Relative;Hash=(Get-FileHash -LiteralPath $_.Target).Hash;Mutable=$_.Mutable}})
    $data=[ordered]@{Version=1;Package='Kuro AA 0.4.1';Files=$records}
    [IO.File]::WriteAllText($receipt,($data | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
} catch {
    foreach($entry in $plan){if($entry.Existed){Copy-Item -LiteralPath (Join-Path $backup $entry.Relative) -Destination $entry.Target -Force}else{if(Test-Path -LiteralPath $entry.Target){Remove-Item -LiteralPath $entry.Target -Force}}}
    $savedReceipt=Join-Path $backup '.kuro-aa-install.json'
    if(Test-Path -LiteralPath $savedReceipt){Copy-Item -LiteralPath $savedReceipt -Destination $receipt -Force}else{if(Test-Path -LiteralPath $receipt){Remove-Item -LiteralPath $receipt -Force}}
    throw
}
# Archive local experiment diagnostics; retain user captures and shader caches.
$diagnostics=@(Get-ChildItem -LiteralPath $mod -File | Where-Object {$_.Name -match '^KuroUI-(scale-|targets?|render-layouts|scene-values)'})
if($diagnostics.Count){$archive=Join-Path $backup 'experiment-diagnostics';New-Item -ItemType Directory -Path $archive | Out-Null;foreach($file in $diagnostics){Move-Item -LiteralPath $file.FullName -Destination $archive}}
[pscustomobject]@{Version='0.4.1';NativeMode=0;Backup=$backup;PreservedRuntime=$true} | ConvertTo-Json
