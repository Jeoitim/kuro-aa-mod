param([Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$TraceTargets)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$native=(Resolve-Path -LiteralPath $NativeDirectory).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh test directory.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)){
    $vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    & "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
New-Item -ItemType Directory -Path (Join-Path $output 'KuroAA') -Force | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /DNOMINMAX (Join-Path $root 'tests\vrs_probe.cpp') /Fe:vrs_probe.exe /link d3d11.lib dxgi.lib d3dcompiler.lib user32.lib
    if($LASTEXITCODE){throw 'VRS fixture compilation failed.'}
    & .\vrs_probe.exe
    if($LASTEXITCODE){throw 'VRS hardware fixture failed or hardware is unsupported.'}
    Copy-Item -LiteralPath (Join-Path $root 'vendor\reshade\dxgi.dll') -Destination $output
    Copy-Item -LiteralPath (Join-Path $root 'packaging\ReShade.ini') -Destination $output
    Copy-Item -LiteralPath (Join-Path $native 'KuroUI.addon64') -Destination (Join-Path $output 'KuroAA')
    [IO.File]::WriteAllText((Join-Path $output 'KuroAA\KuroUI.ini'),"[KuroUI]`nSceneVRS=0`nEnableEarlyAA=0`nSkipUnmatchedFrames=0`n")
    if($TraceTargets){[IO.File]::AppendAllText((Join-Path $output 'KuroAA\KuroUI.ini'),"TraceRenderTargets=1`n")}
    $p=Start-Process -FilePath (Join-Path $output 'vrs_probe.exe') -ArgumentList injected -WorkingDirectory $output -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output 'stdout.log') -RedirectStandardError (Join-Path $output 'stderr.log') -PassThru
    $handle=$p.Handle
    if(!$p.WaitForExit(60000)){throw 'VRS injection fixture timed out.'}
    Get-Content -LiteralPath (Join-Path $output 'stdout.log')
    Get-Content -LiteralPath (Join-Path $output 'stderr.log')
    if($p.ExitCode){throw 'VRS injection fixture failed.'}
    if((Get-Content -LiteralPath (Join-Path $output 'KuroAA\KuroUI.log') -Raw) -notmatch 'experimental scene VRS active'){throw 'Native VRS integration did not activate.'}
    if($TraceTargets){
        $resources=@(Import-Csv -LiteralPath (Join-Path $output 'KuroAA\KuroUI-targets.csv'))
        $passes=@(Import-Csv -LiteralPath (Join-Path $output 'KuroAA\KuroUI-target-passes.csv'))
        if(!$resources.Count -or !$passes.Count){throw 'Render target tracing did not capture resources and passes.'}
        Write-Output "Render target trace: $($resources.Count) resources, $($passes.Count) passes."
    }
} finally { Pop-Location }
