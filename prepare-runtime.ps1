[CmdletBinding()]
param(
    [string]$RuntimeDirectory = (Join-Path $PSScriptRoot 'out\bin')
)
$ErrorActionPreference = 'Stop'

# A low integrity label on an EXE also lowers the process launched by Qt Creator.
# Restore the standard user level on this application's executables only.
# This does not grant administrator rights or change personal-folder permissions.
$runtimePath = [IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\')
$applicationPath = Join-Path $runtimePath 'convert.exe'
if (-not (Test-Path -LiteralPath $applicationPath -PathType Leaf)) {
    throw "No existe $applicationPath. Compila Convert primero."
}
$executables = @(Get-Item -LiteralPath $applicationPath)
# Set the directory's inherited label as well: the linker replaces the EXE
# on each build, and the replacement otherwise inherits the low folder label.
$runtimeFolders = @((Get-Item -LiteralPath $runtimePath))
foreach ($name in @('bin', 'ffmpeg', 'tools')) {
    $folder = Join-Path $runtimePath $name
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) { continue }
    if ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
    $runtimeFolders += Get-Item -LiteralPath $folder
    $executables += @(Get-ChildItem -LiteralPath $folder -Filter '*.exe' -File)
}
foreach ($folder in $runtimeFolders) {
    if ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
    $result = & icacls.exe $folder.FullName /setintegritylevel '(OI)(CI)M' 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "No se pudo restaurar la integridad media de $($folder.FullName). $($result -join ' ')"
    }
}
foreach ($executable in $executables) {
    if ($executable.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
    $path = [IO.Path]::GetFullPath($executable.FullName)
    if (-not $path.StartsWith($runtimePath + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "El ejecutable queda fuera de la carpeta de ejecucion: $path"
    }
    $result = & icacls.exe $path /setintegritylevel M 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "No se pudo restaurar la integridad media de $path. $($result -join ' ')"
    }
}
Write-Host 'Convert y sus herramientas: nivel de integridad normal (medio).'
