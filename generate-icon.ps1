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
    # Upright, heavy, rounded geometry matching the supplied letter reference.
    $letter.AddLine(164,46,164,65)
    $letter.AddBezier(164,65,164,70,162,72,157,72)
    $letter.AddLine(157,72,127,72)
    $letter.AddBezier(127,72,122,72,120,70,120,65)
    $letter.AddLine(120,65,120,61)
    $letter.AddBezier(120,61,120,56,118,54,113,54)
    $letter.AddLine(113,54,77,54)
    $letter.AddBezier(77,54,72,54,70,56,70,61)
    $letter.AddLine(70,61,70,129)
    $letter.AddBezier(70,129,70,134,72,136,77,136)
    $letter.AddLine(77,136,113,136)
    $letter.AddBezier(113,136,118,136,120,134,120,129)
    $letter.AddLine(120,129,120,125)
    $letter.AddBezier(120,125,120,120,122,118,127,118)
    $letter.AddLine(127,118,157,118)
    $letter.AddBezier(157,118,162,118,164,120,164,125)
    $letter.AddLine(164,125,164,144)
    $letter.AddBezier(164,144,164,166,154,176,132,176)
    $letter.AddLine(132,176,58,176)
    $letter.AddBezier(58,176,36,176,26,166,26,144)
    $letter.AddLine(26,144,26,46)
    $letter.AddBezier(26,46,26,24,36,14,58,14)
    $letter.AddLine(58,14,132,14)
    $letter.AddBezier(132,14,154,14,164,24,164,46)
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

    # Radit-style splash artwork at 2x resolution. Qt adds the live version.
    $splash = New-Object System.Drawing.Bitmap(1000,572)
    $painter = [System.Drawing.Graphics]::FromImage($splash)
    $background = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        [System.Drawing.PointF]::new(0,0), [System.Drawing.PointF]::new(500,286),
        [System.Drawing.ColorTranslator]::FromHtml('#181e2c'),
        [System.Drawing.ColorTranslator]::FromHtml('#0e1725'))
    $accent = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml('#38b0d1'))
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml('#f4f4f7'))
    $muted = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml('#80A4AE'))
    $word = New-Object System.Drawing.Drawing2D.GraphicsPath
    $family = New-Object System.Drawing.FontFamily('Arial Rounded MT Bold')
    $caption = New-Object System.Drawing.Font('Segoe UI',9,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
    $matrix = New-Object System.Drawing.Drawing2D.Matrix
    try {
        $painter.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $painter.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $painter.ScaleTransform(2,2)
        $painter.FillRectangle($background,0,0,500,286)
        $painter.FillRectangle($accent,0,126,44,9)
        foreach ($x in @(48,65)) {
            $painter.FillPolygon($accent,[System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new($x,135),[System.Drawing.PointF]::new(($x+9),126),
                [System.Drawing.PointF]::new(($x+20),126),[System.Drawing.PointF]::new(($x+11),135)))
        }
        $painter.DrawImage($image,47,58,190,190)
        $word.AddString('convert',$family,0,68,[System.Drawing.PointF]::new(0,0),[System.Drawing.StringFormat]::GenericTypographic)
        $bounds = $word.GetBounds()
        $scale = [Math]::Min((231.0/$bounds.Width),(54.0/$bounds.Height))
        $matrix.Scale($scale,$scale)
        $word.Transform($matrix)
        $matrix.Reset()
        $bounds = $word.GetBounds()
        $matrix.Translate((244-$bounds.X),(119-$bounds.Y))
        $word.Transform($matrix)
        $painter.FillPath($white,$word)
        $painter.DrawString('AUDIO  /  VIDEO  /  DESCARGAS',$caption,$muted,246,181)
        $splash.Save((Join-Path $assetDir 'splashscreen.png'),[System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $matrix.Dispose(); $caption.Dispose(); $family.Dispose(); $word.Dispose()
        $muted.Dispose(); $white.Dispose(); $accent.Dispose(); $background.Dispose()
        $painter.Dispose(); $splash.Dispose()
    }
} finally {
    if ($brush) { $brush.Dispose() }
    $letter.Dispose(); $graphics.Dispose(); $image.Dispose()
}
Write-Host 'Generated svn/convert.png, svn/convert.ico and svn/splashscreen.png'
