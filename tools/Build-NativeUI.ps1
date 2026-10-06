param([Parameter(Mandatory=$true)][string]$ReShadeSDK,[string]$OutputDirectory=(Join-Path $PSScriptRoot '..\build\native-ui'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sdk=(Resolve-Path -LiteralPath $ReShadeSDK).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $locator="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if(!(Test-Path -LiteralPath $locator)) { throw 'MSVC Build Tools and Windows SDK are required. GCC cannot safely call the ReShade C++ ABI.' }
    $vsPath=& $locator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    & "$vsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /LD /DNOMINMAX /DWIN32_LEAN_AND_MEAN ("/I"+(Join-Path $sdk 'include')) (Join-Path $root 'src\KuroUI.cpp') /link ("/OUT:"+(Join-Path $output 'KuroUI.addon64')) d3d11.lib user32.lib
    if($LASTEXITCODE -ne 0) { throw 'Native UI compilation failed.' }
} finally { Pop-Location }
