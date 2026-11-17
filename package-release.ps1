[CmdletBinding()]
param(
    [string]$RuntimeDirectory = (Join-Path $PSScriptRoot 'out\bin'),
    [string]$LicenseDirectory = (Join-Path $RuntimeDirectory 'licenses')
)
$ErrorActionPreference = 'Stop'
$version = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'app-version.txt') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'app-version.txt debe contener mayor.menor.parche' }
$runtime = [IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\')
foreach ($required in @('convert.exe','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','platforms\qwindows.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $runtime $required) -PathType Leaf)) {
        throw "Falta $required. Ejecuta primero .\build.ps1 -Deploy."
    }
}
if (-not (Test-Path -LiteralPath $LicenseDirectory -PathType Container)) {
    throw 'Falta la carpeta de licencias de Qt, sus dependencias y las herramientas. Indicala con -LicenseDirectory.'
}
$output = Join-Path $PSScriptRoot ('out\releases\'+$version+'-'+[guid]::NewGuid().ToString('N'))
$name = 'convert-'+$version+'-windows-x64'
$stage = Join-Path $output $name
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $runtime 'convert.exe') -Destination $stage
Get-ChildItem -LiteralPath $runtime -File -Filter '*.dll' | Copy-Item -Destination $stage
foreach ($folder in @('generic','iconengines','imageformats','networkinformation','platforms','styles','tls','translations')) {
    $source = Join-Path $runtime $folder
    if (Test-Path -LiteralPath $source -PathType Container) {
        if ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Carpeta enlazada: $source" }
        Copy-Item -LiteralPath $source -Destination $stage -Recurse
    }
}
$tools = Join-Path $stage 'tools'
New-Item -ItemType Directory -Path $tools | Out-Null
foreach ($tool in @('ffmpeg.exe','ffprobe.exe','ffplay.exe','yt-dlp.exe','spotdl.exe','deno.exe')) {
    $source = Join-Path $runtime ('tools\'+$tool)
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Falta tools\$tool" }
    if ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Ejecutable enlazado: $source" }
    Copy-Item -LiteralPath $source -Destination $tools
}
Get-ChildItem -LiteralPath (Join-Path $runtime 'tools') -File | Where-Object { $_.Name -match '(?i)license|copying' } | Copy-Item -Destination $tools
if (Test-Path -LiteralPath (Join-Path $runtime 'tools\licenses') -PathType Container) {
    Copy-Item -LiteralPath (Join-Path $runtime 'tools\licenses') -Destination $tools -Recurse
}
Copy-Item -LiteralPath $LicenseDirectory -Destination (Join-Path $stage 'licenses') -Recurse
$readme = @"
Convert $version - Windows x64

Extrae toda la carpeta y abre convert.exe. Conserva las DLL, plugins, tools y licenses.
Ayuda > Actualizaciones permite comprobar e instalar versiones nuevas de Convert y las herramientas.
Las actualizaciones de Convert conservan tools, las preferencias y las carpetas de salida.
Fuentes: https://github.com/valgaba/convert
Compilacion: Qt oficial 6.7 / MinGW 11.2.0.
"@
[IO.File]::WriteAllText((Join-Path $stage 'LEEME.txt'),$readme,[Text.UTF8Encoding]::new($false))
$manifest = Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_.FullName.Substring($stage.Length+1).Replace('\','/')
}
[IO.File]::WriteAllLines((Join-Path $stage 'FILES-SHA256SUMS.txt'),[string[]]$manifest,[Text.Encoding]::ASCII)
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $output ($name+'.zip')
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'),($hash+'  '+$name+".zip`n"),[Text.Encoding]::ASCII)
Write-Host "Paquete preparado: $zip"
Write-Host "Publica el ZIP y SHA256SUMS.txt como assets de la Release v$version. No se ha subido ningun archivo."
