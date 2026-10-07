param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root){throw 'Use a fresh cache test directory.'}
New-Item -ItemType Directory -Path $root | Out-Null
$exe=(Resolve-Path -LiteralPath $Executable).Path
$old=$env:KURO_AA_SHADER_CACHE;$env:KURO_AA_SHADER_CACHE=Join-Path $root 'cache'
try{
    $results=@()
    foreach($case in @('cold','warm','corrupt')){
        if($case -eq 'corrupt'){
            $file=Get-ChildItem -LiteralPath $env:KURO_AA_SHADER_CACHE -Filter '*.cso' | Select-Object -First 1
            $bytes=[IO.File]::ReadAllBytes($file.FullName);$bytes[100]=$bytes[100] -bxor 1;[IO.File]::WriteAllBytes($file.FullName,$bytes)
        }
        $stdout=Join-Path $root ($case+'.json');$p=Start-Process -FilePath $exe -WindowStyle Hidden -RedirectStandardOutput $stdout -PassThru -Wait
        if($p.ExitCode){throw 'Cache fixture failed.'};$result=Get-Content -LiteralPath $stdout -Raw|ConvertFrom-Json
        if($result.memory -ne 1 -or ($case -eq 'warm' -and ($result.disk -ne 1 -or $result.compiles -ne 0)) -or ($case -ne 'warm' -and $result.compiles -ne 1)){throw 'Cache miss/hit or corruption recovery failed.'}
        $results+=[pscustomobject]@{Case=$case;MemoryHits=$result.memory;DiskHits=$result.disk;Compiles=$result.compiles}
    }
    $results|ConvertTo-Json
}finally{$env:KURO_AA_SHADER_CACHE=$old}
