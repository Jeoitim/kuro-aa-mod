param([string]$SourceDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$SourceDirectory){$SourceDirectory=Join-Path $root 'external\aeonsr'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\aeonsr-preview'}
if(!(Test-Path -LiteralPath $SourceDirectory)){
    & git clone --no-checkout https://github.com/BarbatosAWLS/AeonSR.git $SourceDirectory
    if($LASTEXITCODE){throw 'AeonSR source download failed.'}
    & git -C $SourceDirectory checkout --detach 8e8456848557d6e7282db3451473531a392209da
    if($LASTEXITCODE){throw 'Pinned AeonSR checkout failed.'}
}
$revision=& git -C $SourceDirectory rev-parse HEAD
if($revision -ne '8e8456848557d6e7282db3451473531a392209da'){throw 'Unexpected AeonSR source revision.'}
$patch=Join-Path $PSScriptRoot 'aeonsr-preview.patch'
& git -C $SourceDirectory apply --ignore-space-change --reverse --check $patch 2>$null
if($LASTEXITCODE){
    & git -C $SourceDirectory apply --ignore-space-change --check $patch
    if($LASTEXITCODE){throw 'AeonSR source conflicts with the preview patch.'}
    & git -C $SourceDirectory apply --ignore-space-change $patch
    if($LASTEXITCODE){throw 'AeonSR preview patch failed.'}
}
& git -C $SourceDirectory submodule update --init --depth 1 external/reshade external/DLSS external/Vulkan-Headers
if($LASTEXITCODE){throw 'AeonSR SDK setup failed.'}
& git -C (Join-Path $SourceDirectory 'external\reshade') submodule update --init --depth 1 deps/imgui deps/minhook deps/spirv
if($LASTEXITCODE){throw 'ReShade compiler dependency setup failed.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)){
    $locator="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs=& $locator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    & "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
$cmake='cmake'
$locator="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $locator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$bundled=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if(Test-Path -LiteralPath $bundled){$cmake=$bundled}
& $cmake -S $SourceDirectory -B $OutputDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS=/utf-8 /EHsc' ("-DAEONSR_BUNDLED_RUNTIME="+(Join-Path $root 'vendor\aeonsr\runtime'))
if($LASTEXITCODE){throw 'AeonSR configuration failed.'}
& $cmake --build $OutputDirectory --target AeonSR --parallel 4
if($LASTEXITCODE){throw 'AeonSR compilation failed.'}
