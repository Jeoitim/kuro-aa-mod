param([Parameter(Mandatory=$true)][string]$ReShadeSource,[string]$Compiler='g++')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source=(Resolve-Path -LiteralPath $ReShadeSource).Path
$build=Join-Path $root 'build\validation'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$names=@('effect_codegen_dxbc.cpp','effect_expression.cpp','effect_lexer.cpp','effect_parser_exp.cpp','effect_parser_stmt.cpp','effect_preprocessor.cpp','effect_symbol_table.cpp')
$files=@($names | ForEach-Object{Join-Path $source ('source\'+$_)})
$validator=Join-Path $build 'validate_fx.exe'
& $Compiler '-std=c++17' '-O1' '-include' 'share.h' ('-I'+(Join-Path $source 'source')) (Join-Path $root 'tests\validate_fx.cpp') @files '-o' $validator '-ld3dcompiler'
if($LASTEXITCODE){throw 'Validator compilation failed.'}
foreach($resolution in @(@(1920,1080),@(2560,1440))){
    $result=& $validator (Join-Path $root 'packaging\Shaders\Runtime.fx') $resolution[0] $resolution[1] 2>&1
    if($LASTEXITCODE -or $result -match 'warning|error'){$result;throw 'Runtime anchor shader compilation failed.'}
}
Write-Output 'Runtime callback anchor: 1080p and 1440p compiled without diagnostics.'
