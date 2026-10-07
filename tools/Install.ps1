param([Parameter(Mandatory=$true)][string]$GameDirectory, [string]$PackageDirectory = '')
$ErrorActionPreference = 'Stop'
if (!$PackageDirectory) {
    $releaseFiles = Join-Path $PSScriptRoot '..\GameFiles'
    $PackageDirectory = if (Test-Path -LiteralPath $releaseFiles) { $releaseFiles } else { Join-Path $PSScriptRoot '..\dist\GameFiles' }
}
$gameRoot = [IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path.TrimEnd('\')
if (!(Test-Path -LiteralPath (Join-Path $gameRoot 'ed9.exe'))) { throw 'The selected directory does not contain ed9.exe.' }
foreach ($gameProcess in @(Get-Process ed9 -ErrorAction SilentlyContinue)) {
    if ([string]::Equals($gameProcess.Path,(Join-Path $gameRoot 'ed9.exe'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Close the game before installation.' }
}
$receiptPath = Join-Path $gameRoot '.kuro-aa-install.json'
if(@(Get-ChildItem -LiteralPath $gameRoot -Filter '.kuro-*-install.json' -File).Count){throw 'Uninstall the existing version before installing.'}
if(Test-Path -LiteralPath (Join-Path $gameRoot 'KuroAA')){throw 'Existing KuroAA directory will not be overwritten.'}
if (Test-Path -LiteralPath $receiptPath) { throw 'This Mod is already installed. Uninstall it before reinstalling.' }
foreach ($proxy in @('d3d11.dll','dxgi.dll.kuro-disabled')) {
    if (Test-Path -LiteralPath (Join-Path $gameRoot $proxy)) { throw "Existing injection file: $proxy" }
}
$files = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse -Force)
$plan = @()
foreach ($file in $files) {
    $relative = $file.FullName.Substring($packageRoot.Length + 1)
    $target = [IO.Path]::GetFullPath((Join-Path $gameRoot $relative))
    if (!$target.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package path.' }
    if (Test-Path -LiteralPath $target) { throw "Existing file will not be overwritten: $relative" }
    $plan += [pscustomobject]@{ Path=$relative; Hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash; Mutable=($file.Extension -eq '.ini'); Source=$file.FullName }
}
# A receipt is written before mutation and updated after every successful copy.
# If copying fails, Uninstall.ps1 can remove the already-copied owned files.
$receipt = [ordered]@{ Version=1; Package='Kuro AA 0.4.0'; Files=@() }
[IO.File]::WriteAllText($receiptPath,($receipt | ConvertTo-Json -Depth 5))
foreach ($entry in $plan) {
    $target = Join-Path $gameRoot $entry.Path
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
    Copy-Item -LiteralPath $entry.Source -Destination $target
    $receipt.Files += [pscustomobject]@{ Path=$entry.Path; Hash=$entry.Hash; Mutable=$entry.Mutable }
    [IO.File]::WriteAllText($receiptPath,($receipt | ConvertTo-Json -Depth 5))
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.Hash) { throw "Verification failed: $($entry.Path)" }
}
Write-Output "Installed and hash-verified $($plan.Count) Mod files."
