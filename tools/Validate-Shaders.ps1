param([Parameter(Mandatory=$true)][string]$ReShadeSource,[string]$Compiler='g++')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceRoot=(Resolve-Path -LiteralPath $ReShadeSource).Path
$buildRoot=Join-Path $projectRoot 'build\validation'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$sourceNames=@('effect_codegen_dxbc.cpp','effect_expression.cpp','effect_lexer.cpp','effect_parser_exp.cpp','effect_parser_stmt.cpp','effect_preprocessor.cpp','effect_symbol_table.cpp')
$sourceFiles=@($sourceNames | ForEach-Object { Join-Path $sourceRoot ('source\'+$_) })
$validator=Join-Path $buildRoot 'validate_fx.exe'
& $Compiler '-std=c++17' '-O1' '-include' 'share.h' ('-I'+(Join-Path $sourceRoot 'source')) (Join-Path $projectRoot 'tests\validate_fx.cpp') @sourceFiles '-o' $validator '-ld3dcompiler'
if($LASTEXITCODE -ne 0) { throw 'FX validator compilation failed.' }
foreach($phase in 1..5) {
    $diagnostics=& $validator (Join-Path $projectRoot 'KuroTFAA\Shaders\KuroTFAA.fx') 1920 1080 $phase 2>&1
    if($LASTEXITCODE -ne 0 -or ($diagnostics -match 'warning|error')) { $diagnostics; throw "Shader phase $phase failed." }
    Write-Output "Phase ${phase}: all entries compiled without diagnostics."
}
& $validator (Join-Path $projectRoot 'KuroTFAA\Shaders\KuroTFAA.fx') 2560 1440 5
if($LASTEXITCODE -ne 0) { throw '1440p shader compilation failed.' }
