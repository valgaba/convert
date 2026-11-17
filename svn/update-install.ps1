param([Parameter(Mandatory=$true)][string]$PlanPath)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$planFile = [IO.Path]::GetFullPath($PlanPath)
$workRoot = [IO.Path]::GetDirectoryName($planFile)
$resultPath = Join-Path $workRoot 'result.json'
$parentExited = $false
$applicationRoot = $null
$installed = @()
$backups = @()
$backupRoot = $null

function Write-Result([string]$status, [string]$errorText) {
    $value = @{status=$status; error=$errorText; backup=$backupRoot} | ConvertTo-Json
    $temporaryResult = Inside $workRoot ('result-'+[guid]::NewGuid().ToString('N')+'.tmp')
    [IO.File]::WriteAllText($temporaryResult, $value, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryResult -Destination $resultPath -Force
}

function Inside([string]$root, [string]$relative) {
    if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains(':')) { throw "Ruta absoluta no permitida: $relative" }
    $full = [IO.Path]::GetFullPath((Join-Path $root $relative))
    if (-not $full.StartsWith($root.TrimEnd('\')+'\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Ruta fuera del destino: $relative"
    }
    return $full
}

function Reject-Links([string]$root, [string]$path) {
    $candidate = $path
    while ($candidate -and $candidate.Length -ge $root.Length) {
        if (Test-Path -LiteralPath $candidate) {
            if ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "No se actualizan enlaces de archivos o carpetas: $candidate"
            }
        }
        if ($candidate -eq $root) { break }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}

function Assert-X64([string]$path) {
    $stream = [IO.File]::OpenRead($path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 256 -or $reader.ReadUInt16() -ne 0x5A4D) { throw "Ejecutable invalido: $path" }
        $stream.Position = 0x3C
        $offset = $reader.ReadUInt32()
        if ($offset -gt $stream.Length-6) { throw "Cabecera PE invalida: $path" }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw "El paquete no contiene un ejecutable Windows x64: $path"
        }
    } finally { $reader.Dispose(); $stream.Dispose() }
}

function Unpack-Zip([string]$zip, [string]$destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Directory]::CreateDirectory($destination) | Out-Null
    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
    [long]$total = 0
    try {
        foreach ($entry in $archive.Entries) {
            $relative = $entry.FullName.Replace('/', '\')
            if (-not $relative) { continue }
            if ($relative.Split('\') -contains '..') { throw 'El ZIP contiene una ruta no permitida' }
            if ((($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw 'El ZIP contiene enlaces' }
            $target = Inside $destination $relative
            $total += $entry.Length
            if ($total -gt 4GB -or $entry.Length -gt 1GB) { throw 'El ZIP supera el tamano permitido' }
            if ($relative.EndsWith('\')) { [IO.Directory]::CreateDirectory($target) | Out-Null; continue }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            $inputStream = $entry.Open()
            $outputStream = [IO.File]::Open($target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
            try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
            if ((Get-Item -LiteralPath $target).Length -ne $entry.Length) { throw 'El ZIP esta incompleto' }
        }
    } finally { $archive.Dispose() }
}

function Relaunch {
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    $env:QML2_IMPORT_PATH = $null
    # This is the interactive application the user explicitly chose to restart.
    Start-Process -FilePath (Join-Path $applicationRoot 'convert.exe') -WorkingDirectory $applicationRoot -WindowStyle Normal
}

try {
    $plan = Get-Content -LiteralPath $planFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($plan.schema -ne 1 -or $plan.component -notin @('convert','ffmpeg','yt-dlp','spotdl','deno')) {
        throw 'Plan de actualizacion no valido'
    }
    if ($plan.sha256 -notmatch '^[a-f0-9]{64}$' -or $plan.version -notmatch '^\d+(\.\d+){1,3}$') {
        throw 'Version o SHA-256 no validos'
    }
    $applicationRoot = [IO.Path]::GetFullPath($plan.application).TrimEnd('\')
    if ($applicationRoot -eq [IO.Path]::GetPathRoot($applicationRoot).TrimEnd('\') -or
        -not (Test-Path -LiteralPath (Join-Path $applicationRoot 'convert.exe') -PathType Leaf)) {
        throw 'No se ha identificado la carpeta de Convert'
    }
    Reject-Links $applicationRoot $applicationRoot
    Reject-Links $workRoot $workRoot
    $package = [IO.Path]::GetFullPath($plan.package)
    if (-not $package.StartsWith($workRoot+'\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'El paquete queda fuera del area de actualizacion'
    }
    if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant() -ne $plan.sha256) {
        throw 'El paquete no coincide con su SHA-256'
    }
    $stage = Inside $workRoot 'stage'
    $files = @()
    if ($plan.component -in @('yt-dlp','spotdl')) {
        Assert-X64 $package
        $files += @{source=$package; relative=('tools\'+$plan.component+'.exe')}
    } else {
        Unpack-Zip $package $stage
        if ($plan.component -eq 'convert') {
            $executables = @(Get-ChildItem -LiteralPath $stage -Filter 'convert.exe' -Recurse -File)
            if ($executables.Count -ne 1) { throw 'El ZIP no identifica una unica instalacion de Convert' }
            $payload = $executables[0].DirectoryName
            Assert-X64 $executables[0].FullName
            foreach ($required in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','platforms\qwindows.dll')) {
                if (-not (Test-Path -LiteralPath (Inside $payload $required) -PathType Leaf)) { throw "Falta $required en el paquete" }
            }
            foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse) {
                $relative = $file.FullName.Substring($payload.Length+1)
                $first = $relative.Split('\')[0]
                $rootFile = -not $relative.Contains('\')
                $allowed = ($rootFile -and ($file.Name -eq 'convert.exe' -or $file.Extension -eq '.dll' -or
                    $file.Name -in @('LEEME.txt','FILES-SHA256SUMS.txt'))) -or
                    $first -in @('generic','iconengines','imageformats','networkinformation','platforms','styles','tls','translations','licenses')
                # tools, bin, data, temp, salida and user documents are never replaced.
                if ($allowed) { $files += @{source=$file.FullName; relative=$relative} }
            }
        } elseif ($plan.component -eq 'ffmpeg') {
            foreach ($name in @('ffmpeg.exe','ffprobe.exe','ffplay.exe')) {
                $matches = @(Get-ChildItem -LiteralPath $stage -Filter $name -Recurse -File)
                if ($matches.Count -ne 1) { throw "No se identifica $name en el paquete" }
                Assert-X64 $matches[0].FullName
                $files += @{source=$matches[0].FullName; relative=('tools\'+$name)}
            }
            foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object { $_.Name -in @('LICENSE','LICENSE.txt','LICENSE.md','README.txt') }) {
                $files += @{source=$file.FullName; relative=('tools\licenses\ffmpeg-'+$file.Name)}
            }
        } else {
            $matches = @(Get-ChildItem -LiteralPath $stage -Filter 'deno.exe' -Recurse -File)
            if ($matches.Count -ne 1) { throw 'No se identifica deno.exe en el paquete' }
            Assert-X64 $matches[0].FullName
            $files += @{source=$matches[0].FullName; relative='tools\deno.exe'}
        }
    }
    if ($files.Count -eq 0) { throw 'El paquete no contiene archivos instalables' }
    $seen = @{}
    foreach ($file in $files) {
        $target = Inside $applicationRoot $file.relative
        Reject-Links $applicationRoot $target
        if ($seen.ContainsKey($target)) { throw "Destino repetido en el paquete: $target" }
        $seen[$target] = $true
    }
    $probe = Inside $applicationRoot ('.update-write-'+[guid]::NewGuid().ToString('N'))
    $probeStream = [IO.File]::Open($probe, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None, 1, [IO.FileOptions]::DeleteOnClose)
    $probeStream.Dispose()
    if ($plan.component -eq 'convert') {
        if (Test-Path -LiteralPath (Inside $workRoot 'cancel')) { throw 'Actualizacion cancelada' }
        [IO.File]::WriteAllText((Inside $workRoot 'ready'), 'ready')
        $deadline = [DateTime]::UtcNow.AddMinutes(2)
        while (Get-Process -Id ([int]$plan.parentPid) -ErrorAction SilentlyContinue) {
            if (Test-Path -LiteralPath (Inside $workRoot 'cancel')) { throw 'Actualizacion cancelada' }
            if ([DateTime]::UtcNow -gt $deadline) { throw 'Convert no se ha cerrado; no se han cambiado archivos' }
            Start-Sleep -Milliseconds 250
        }
        $parentExited = $true
    }
    $executableTargets = @($files | Where-Object { [IO.Path]::GetExtension($_.relative) -eq '.exe' } | ForEach-Object { Inside $applicationRoot $_.relative })
    $processNames = @($executableTargets | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) } | Select-Object -Unique)
    foreach ($name in $processNames) {
        foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            $runningPath = $null
            try { $runningPath = $process.Path } catch { }
            if ($runningPath -and ($executableTargets -contains $runningPath)) {
                throw "El programa sigue en uso: $runningPath. Cierra las otras instancias e intentalo de nuevo."
            }
        }
    }
    $backupRoot = Inside $applicationRoot ('.update-backup\'+[guid]::NewGuid().ToString('N'))
    Reject-Links $applicationRoot $backupRoot
    [IO.Directory]::CreateDirectory($backupRoot) | Out-Null
    foreach ($file in $files) {
        $target = Inside $applicationRoot $file.relative
        Reject-Links $applicationRoot $target
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
        if (Test-Path -LiteralPath $target) {
            $backup = Inside $backupRoot $file.relative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            Move-Item -LiteralPath $target -Destination $backup
            $backups += @{target=$target; backup=$backup}
        }
        $installed += $target
        Copy-Item -LiteralPath $file.source -Destination $target
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $file.source -Algorithm SHA256).Hash) { throw "No coincide la copia de $target" }
        if ([IO.Path]::GetExtension($target) -eq '.exe') {
            $label = & icacls.exe $target /setintegritylevel M 2>&1
            if ($LASTEXITCODE -ne 0) { throw "No se pudo establecer el nivel normal: $target. $label" }
        }
    }
    $tools = Inside $applicationRoot 'tools'
    if (Test-Path -LiteralPath $tools -PathType Container) {
        $label = & icacls.exe $tools /setintegritylevel '(OI)(CI)M' 2>&1
        if ($LASTEXITCODE -ne 0) { throw "No se pudo preparar tools: $label" }
    }
    $receipt = @{component=$plan.component; version=$plan.version; sha256=$plan.sha256; installed=[DateTime]::UtcNow.ToString('o')}
    $receiptName = '.update-'+$plan.component+'.json'
    $receiptPath = Inside $applicationRoot $receiptName
    Reject-Links $applicationRoot $receiptPath
    if (Test-Path -LiteralPath $receiptPath) {
        $receiptBackup = Inside $backupRoot $receiptName
        Move-Item -LiteralPath $receiptPath -Destination $receiptBackup
        $backups += @{target=$receiptPath; backup=$receiptBackup}
    }
    $installed += $receiptPath
    [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    Write-Result 'ok' ''
    Write-Output ('Actualizacion instalada. Copia anterior: '+$backupRoot)
    if ($parentExited) { Relaunch }
    exit 0
} catch {
    $message = $_.Exception.Message
    $rollbackErrors = @()
    # Roll back only the exact files recorded in this installation transaction.
    for ($i=$installed.Count-1; $i -ge 0; $i--) {
        try {
            $target = $installed[$i]
            Reject-Links $applicationRoot $target
            if (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force }
        } catch { $rollbackErrors += $_.Exception.Message }
    }
    for ($i=$backups.Count-1; $i -ge 0; $i--) {
        try {
            $entry = $backups[$i]
            Reject-Links $applicationRoot $entry.target
            Move-Item -LiteralPath $entry.backup -Destination $entry.target
        } catch { $rollbackErrors += $_.Exception.Message }
    }
    if ($rollbackErrors.Count) { $message += ' Errores al recuperar archivos: '+($rollbackErrors -join '; ')+'. Copias: '+$backupRoot }
    Write-Result 'error' $message
    Write-Output $message
    if ($parentExited -and $rollbackErrors.Count -eq 0) {
        try { Relaunch } catch { Write-Output $_.Exception.Message }
    }
    exit 1
}
