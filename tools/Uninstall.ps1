param([Parameter(Mandatory=$true)][string]$GameDirectory)
$ErrorActionPreference = 'Stop'
$gameRoot = [IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
if (!(Test-Path -LiteralPath (Join-Path $gameRoot 'ed9.exe'))) { throw 'The selected directory does not contain ed9.exe.' }
foreach ($gameProcess in @(Get-Process ed9 -ErrorAction SilentlyContinue)) {
    if ([string]::Equals($gameProcess.Path,(Join-Path $gameRoot 'ed9.exe'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Close the game before uninstalling.' }
}
$receiptPath = Join-Path $gameRoot '.kuro-tfaa-install.json'
if (!(Test-Path -LiteralPath $receiptPath)) { throw 'No installation receipt. Refusing to remove unowned files.' }
$receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if ($receipt.Package -notin @('Kuro AA 0.2.0','Kuro AA 0.3.0-scene') -or $receipt.Version -ne 1) { throw 'Unknown installation receipt.' }
$paths = @()
foreach ($entry in $receipt.Files) {
    $path = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.Path))
    if (!$path.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe receipt path.' }
    if ($entry.Path -eq 'dxgi.dll' -and !(Test-Path -LiteralPath $path) -and (Test-Path -LiteralPath ($path + '.kuro-disabled'))) { $path += '.kuro-disabled' }
    if (Test-Path -LiteralPath $path) {
        if (!$entry.Mutable -and (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Hash) { throw "Changed file preserved; resolve before uninstall: $($entry.Path)" }
        $paths += $path
    }
}
# No recursive deletion: remove only verified receipt entries, never game files or logs.
foreach ($path in $paths) { Remove-Item -LiteralPath $path }
Remove-Item -LiteralPath $receiptPath
Write-Output "Removed $($paths.Count) owned files. Game files and runtime logs preserved."
