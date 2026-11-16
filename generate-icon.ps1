# Generates the C icon from vector geometry, using the Radit reference palette.
# No third-party package is required. Run only to regenerate the checked-in assets.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assetDir = Join-Path $PSScriptRoot 'svn'
$image = New-Object System.Drawing.Bitmap(512, 512)
$graphics = [System.Drawing.Graphics]::FromImage($image)
$letter = New-Object System.Drawing.Drawing2D.GraphicsPath
$brush = $null
try {
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform((512.0 / 190.0), (512.0 / 190.0))
    $letter.AddLine(165,36,143,69)
    $letter.AddBezier(143,69,133,54,123,48,105,48)
    $letter.AddBezier(105,48,75,48,60,69,53,99)
    $letter.AddBezier(53,99,47,127,59,145,84,145)
    $letter.AddBezier(84,145,104,145,119,138,135,123)
    $letter.AddLine(135,123,149,151)
    $letter.AddBezier(149,151,129,172,105,183,74,183)
    $letter.AddBezier(74,183,24,183,4,147,18,94)
    $letter.AddBezier(18,94,31,43,62,13,111,13)
    $letter.AddBezier(111,13,136,13,153,21,165,36)
    $letter.CloseFigure()
    $graphics.SetClip($letter)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        [System.Drawing.PointF]::new(0,0), [System.Drawing.PointF]::new(85.5,190),
        [System.Drawing.ColorTranslator]::FromHtml('#B9D8DF'),
        [System.Drawing.ColorTranslator]::FromHtml('#587B87'))
    $blend = New-Object System.Drawing.Drawing2D.ColorBlend(3)
    $blend.Colors = [System.Drawing.Color[]]@(
        [System.Drawing.ColorTranslator]::FromHtml('#B9D8DF'),
        [System.Drawing.ColorTranslator]::FromHtml('#80A4AE'),
        [System.Drawing.ColorTranslator]::FromHtml('#587B87'))
    $blend.Positions = [single[]]@(0,0.45,1)
    $brush.InterpolationColors = $blend
    $graphics.FillRectangle($brush,0,0,190,190)
    $shine = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(36,212,237,241))
    $shade = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(41,61,97,110))
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(122,224,243,246),3)
    $edge = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(77,185,216,223),2)
    try {
        $graphics.FillPolygon($shine,[System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new(35,0), [System.Drawing.PointF]::new(190,0),
            [System.Drawing.PointF]::new(190,110)))
        $graphics.FillPolygon($shade,[System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new(0,83), [System.Drawing.PointF]::new(190,168),
            [System.Drawing.PointF]::new(190,190), [System.Drawing.PointF]::new(0,190)))
        $graphics.DrawLine($pen,17,-11,181,62)
        $graphics.DrawLine($pen,14,1,181,76)
        $graphics.DrawLine($pen,11,13,181,90)
        $graphics.DrawLine($pen,8,25,181,104)
        $graphics.DrawLine($edge,0,83,190,168)
    } finally { $shine.Dispose(); $shade.Dispose(); $pen.Dispose(); $edge.Dispose() }
    $image.Save((Join-Path $assetDir 'convert.png'),[System.Drawing.Imaging.ImageFormat]::Png)
    # ICO directory with embedded PNG frames, including sizes for Windows Explorer.
    $sizes = @(16,24,32,48,64,128,256)
    $frames = @()
    foreach ($size in $sizes) {
        $frame = New-Object System.Drawing.Bitmap($size,$size)
        $painter = [System.Drawing.Graphics]::FromImage($frame)
        $stream = New-Object System.IO.MemoryStream
        try {
            $painter.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $painter.DrawImage($image,0,0,$size,$size)
            $frame.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
            $frames += ,($stream.ToArray())
        } finally { $stream.Dispose(); $painter.Dispose(); $frame.Dispose() }
    }
    $iconStream = [System.IO.File]::Create((Join-Path $assetDir 'convert.ico'))
    $writer = New-Object System.IO.BinaryWriter($iconStream)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i=0; $i -lt $sizes.Count; $i++) {
            $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
            $offset += $frames[$i].Length
        }
        foreach ($bytes in $frames) { $writer.Write([byte[]]$bytes) }
    } finally { $writer.Dispose(); $iconStream.Dispose() }
} finally {
    if ($brush) { $brush.Dispose() }
    $letter.Dispose(); $graphics.Dispose(); $image.Dispose()
}
Write-Host 'Generated svn/convert.png and svn/convert.ico'
