[CmdletBinding()]
param(
    [switch]$Deploy,
    [string]$QtRoot = 'C:\Qt\6.7.0\mingw_64',
    [string]$MinGWRoot = 'C:\Qt\Tools\mingw1120_64'
)
$ErrorActionPreference = 'Stop'
$qtBin = Join-Path $QtRoot 'bin'
$compilerBin = Join-Path $MinGWRoot 'bin'
$compiler = Join-Path $compilerBin 'g++.exe'
$make = Join-Path $compilerBin 'mingw32-make.exe'
$deployTool = Join-Path $qtBin 'windeployqt.exe'
$qmake = Join-Path $qtBin 'qmake.exe'
$projectFile = Join-Path $PSScriptRoot 'convert.pro'
foreach ($required in @($qmake, $compiler, $make, $projectFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "No se encuentra: $required. Revisa las rutas del kit oficial."
    }
}
$buildDir = Join-Path $PSScriptRoot 'out\qmake-qt67-mingw-release'
$executable = Join-Path $PSScriptRoot 'out\bin\convert.exe'
$savedPath = $env:PATH
try {
    # qmake y MinGW del kit oficial. No se reutiliza la cache de CMake.
    $env:PATH = "$qtBin;$compilerBin;$savedPath"
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    Push-Location -LiteralPath $buildDir
    try {
        & $qmake $projectFile -spec win32-g++ 'CONFIG+=release' 'CONFIG-=debug'
        if ($LASTEXITCODE -ne 0) { throw 'Error al configurar qmake.' }
        $parallelJobs = [Math]::Max(1, [Environment]::ProcessorCount)
        & $make "-j$parallelJobs"
        if ($LASTEXITCODE -ne 0) { throw 'Error de compilación.' }
    } finally {
        Pop-Location
    }
    if ($Deploy) {
        if (-not (Test-Path -LiteralPath $deployTool -PathType Leaf)) { throw "Falta $deployTool" }
        & $deployTool --release --compiler-runtime --no-translations $executable
        if ($LASTEXITCODE -ne 0) { throw 'Error al copiar las dependencias Qt.' }
    }
    Write-Host "Ejecutable: $executable"
    Write-Host 'Configura FFmpeg, ffprobe, yt-dlp y spotDL en Preferencias.'
} finally {
    $env:PATH = $savedPath
}
