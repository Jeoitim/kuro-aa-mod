param([string]$Destination = (Join-Path $PSScriptRoot '..\external'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $root) { throw 'Dependency destination exists; refusing to mix versions.' }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$aeonZip=Join-Path $root 'AeonSR-v1.0.1.zip'
Invoke-WebRequest -Uri 'https://github.com/BarbatosAWLS/AeonSR/releases/download/v1.0.1/AeonSR-v1.0.1.zip' -OutFile $aeonZip
if((Get-FileHash -LiteralPath $aeonZip).Hash -ne '0292D2F6BB74F03A0F0A3A7FC8EDB0F4BEDF9545EB329DDB76BE198A639A7DA6') { throw 'AeonSR archive checksum mismatch.' }
Expand-Archive -LiteralPath $aeonZip -DestinationPath (Join-Path $root 'AeonSR-release')
$setup=Join-Path $root 'ReShade_Setup_6.8.0_Addon.exe'
Invoke-WebRequest -Uri 'https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe' -OutFile $setup
# Read only the official installer's attached ZIP; do not run its installer.
Add-Type -AssemblyName System.IO.Compression
$bytes=[IO.File]::ReadAllBytes($setup)
$offset=-1
for($i=0;$i -lt $bytes.Length-4;$i+=512) {
    if($bytes[$i] -eq 0x50 -and $bytes[$i+1] -eq 0x4b -and $bytes[$i+2] -eq 3 -and $bytes[$i+3] -eq 4) { $offset=$i; break }
}
if($offset -lt 0) { throw 'Official installer archive not found.' }
$memory=[IO.MemoryStream]::new($bytes,$offset,$bytes.Length-$offset,$false)
try {
    $zip=[IO.Compression.ZipArchive]::new($memory,[IO.Compression.ZipArchiveMode]::Read)
    try {
        $entry=$zip.GetEntry('ReShade64.dll')
        if(!$entry) { throw '64-bit runtime missing.' }
        $runtimeRoot=Join-Path $root 'runtime-addon'
        New-Item -ItemType Directory -Path $runtimeRoot | Out-Null
        $archiveStream=$entry.Open(); $runtimeStream=[IO.File]::Create((Join-Path $runtimeRoot 'ReShade64.dll'))
        try { $archiveStream.CopyTo($runtimeStream) } finally { $archiveStream.Dispose(); $runtimeStream.Dispose() }
    } finally { $zip.Dispose() }
} finally { $memory.Dispose() }
if((Get-FileHash -LiteralPath (Join-Path $root 'runtime-addon\ReShade64.dll')).Hash -ne '0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7') { throw 'ReShade runtime checksum mismatch.' }
$licenseRoot=Join-Path $root 'reshade'
New-Item -ItemType Directory -Path $licenseRoot | Out-Null
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/crosire/reshade/v6.8.0/LICENSE.md' -OutFile (Join-Path $licenseRoot 'LICENSE.md')
git clone --depth 1 --branch v6.8.0 https://github.com/crosire/reshade.git (Join-Path $root 'reshade-sdk')
if($LASTEXITCODE -ne 0){throw 'Native SDK fetch failed.'}
Write-Output 'Fetched and checksum-verified fixed runtime dependencies; no installer executed.'
