# Generates the Crosshair logo and the Windows app icon (no external tools or assets).
#   Build/Windows/Application.ico           - icon compiled into the packaged game exe (16-256 px)
#   docs/branding/crosshair-icon-1024.png   - full-size icon
#   docs/branding/crosshair-wordmark.png    - icon plus the CROSSHAIR name, for README/menus
# Design: a dark rounded tile, an orange scope ring drawn as a 300-degree arc that ends in an arrowhead
# (the 360 spin of a trickshot), white crosshair arms with a centre gap, and a red aim dot.
# Run from the project folder:  powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\create_logo.ps1
param([string]$Root = (Split-Path -Parent $PSScriptRoot))
Add-Type -AssemblyName System.Drawing

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure(); return $p
}

function Draw-Icon([System.Drawing.Graphics]$g, [float]$ox, [float]$oy, [float]$s) {
    $g.SmoothingMode = 'AntiAlias'; $g.PixelOffsetMode = 'HighQuality'
    # Small sizes get relatively heavier strokes so the mark stays readable in the taskbar.
    $weight = if ($s -le 24) { 1.45 } elseif ($s -le 48) { 1.2 } else { 1.0 }
    $c = $s / 2
    # Tile
    $tile = New-RoundedPath ($ox + $s * .02) ($oy + $s * .02) ($s * .96) ($s * .96) ($s * .2)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush((New-Object System.Drawing.PointF($ox, $oy)), (New-Object System.Drawing.PointF(($ox + $s), ($oy + $s))), [System.Drawing.Color]::FromArgb(255, 22, 36, 48), [System.Drawing.Color]::FromArgb(255, 6, 10, 14))
    $g.FillPath($bg, $tile)
    $edge = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 54, 72, 86), [Math]::Max(1, $s * .018))
    $g.DrawPath($edge, $tile)
    # Spin ring: 300 degrees, starting top-right and running clockwise, with an arrowhead.
    $r = $s * .31; $ringW = $s * .075 * $weight
    $ring = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 255, 122, 26), $ringW)
    $ring.StartCap = 'Round'; $ring.EndCap = 'Flat'
    $start = -60.0; $sweep = 300.0
    $g.DrawArc($ring, $ox + $c - $r, $oy + $c - $r, 2 * $r, 2 * $r, $start, $sweep)
    $endA = ($start + $sweep) * [Math]::PI / 180
    $tip = New-Object System.Drawing.PointF(($ox + $c + $r * [Math]::Cos($endA)), ($oy + $c + $r * [Math]::Sin($endA)))
    $tan = New-Object System.Drawing.PointF((-[Math]::Sin($endA)), ([Math]::Cos($endA)))      # clockwise direction
    $nrm = New-Object System.Drawing.PointF(([Math]::Cos($endA)), ([Math]::Sin($endA)))
    $len = $s * .13 * $weight; $half = $s * .085 * $weight
    $head = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(($tip.X + $tan.X * $len), ($tip.Y + $tan.Y * $len))),
        (New-Object System.Drawing.PointF(($tip.X + $nrm.X * $half), ($tip.Y + $nrm.Y * $half))),
        (New-Object System.Drawing.PointF(($tip.X - $nrm.X * $half), ($tip.Y - $nrm.Y * $half))))
    $g.FillPolygon((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 160, 60))), $head)
    # Crosshair arms with a centre gap, reaching just past the ring.
    $arm = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 236, 242, 246), ($s * .05 * $weight))
    $arm.StartCap = 'Round'; $arm.EndCap = 'Round'
    $gap = $s * .1; $reach = $s * .4
    $g.DrawLine($arm, $ox + $c, $oy + $c - $reach, $ox + $c, $oy + $c - $gap)
    $g.DrawLine($arm, $ox + $c, $oy + $c + $gap, $ox + $c, $oy + $c + $reach)
    $g.DrawLine($arm, $ox + $c - $reach, $oy + $c, $ox + $c - $gap, $oy + $c)
    $g.DrawLine($arm, $ox + $c + $gap, $oy + $c, $ox + $c + $reach, $oy + $c)
    # Aim dot
    $dot = $s * .045 * $weight
    $g.FillEllipse((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 59, 48))), $ox + $c - $dot, $oy + $c - $dot, 2 * $dot, 2 * $dot)
}

function New-IconPng([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.Clear([System.Drawing.Color]::Transparent)
    Draw-Icon $g 0 0 $size; $g.Dispose()
    $ms = New-Object System.IO.MemoryStream; $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    return , $ms.ToArray()
}

$branding = Join-Path $Root 'docs\branding'; $icoDir = Join-Path $Root 'Build\Windows'
New-Item -ItemType Directory -Force $branding, $icoDir | Out-Null

# Classic 32-bit DIB icon frame (BGRA, bottom-up, plus an all-clear AND mask) for the small sizes, which
# every Windows icon loader reads; only the 256 px frame is PNG-compressed.
function New-IconDib([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.Clear([System.Drawing.Color]::Transparent)
    Draw-Icon $g 0 0 $size; $g.Dispose()
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $data = $bmp.LockBits($rect, 'ReadOnly', [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $pixels = New-Object byte[] ($data.Stride * $size)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
    $bmp.UnlockBits($data); $bmp.Dispose()
    $maskRow = [int]([Math]::Ceiling($size / 32.0) * 4)
    $ms = New-Object System.IO.MemoryStream; $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([UInt32]40); $bw.Write([Int32]$size); $bw.Write([Int32]($size * 2)); $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]0); $bw.Write([UInt32]($size * $size * 4 + $maskRow * $size)); $bw.Write([Int32]0); $bw.Write([Int32]0); $bw.Write([UInt32]0); $bw.Write([UInt32]0)
    for ($y = $size - 1; $y -ge 0; $y--) { $bw.Write($pixels, $y * $data.Stride, $size * 4) }
    $bw.Write((New-Object byte[] ($maskRow * $size)))
    $bw.Flush(); return , $ms.ToArray()
}

# Multi-size .ico: DIB frames up to 128 px, PNG at 256 px.
$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$images = foreach ($s in $sizes) { if ($s -ge 256) { , (New-IconPng $s) } else { , (New-IconDib $s) } }
$out = New-Object System.IO.MemoryStream; $w = New-Object System.IO.BinaryWriter($out)
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $dim = if ($s -ge 256) { 0 } else { $s }
    $w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32); $w.Write([UInt32]$images[$i].Length); $w.Write([UInt32]$offset)
    $offset += $images[$i].Length
}
foreach ($img in $images) { $w.Write($img) }
$w.Flush(); [System.IO.File]::WriteAllBytes((Join-Path $icoDir 'Application.ico'), $out.ToArray())

[System.IO.File]::WriteAllBytes((Join-Path $branding 'crosshair-icon-1024.png'), (New-IconPng 1024))

# Wordmark: icon plus the name.
$wm = New-Object System.Drawing.Bitmap(1600, 400, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($wm); $g.Clear([System.Drawing.Color]::Transparent)
Draw-Icon $g 20 20 360
$g.TextRenderingHint = 'AntiAliasGridFit'
$font = New-Object System.Drawing.Font('Segoe UI', 150, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString('CROSSHAIR', $font, (New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 236, 242, 246))), 410, 70)
$tag = New-Object System.Drawing.Font('Segoe UI', 44, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString('TRICKSHOT PRACTICE', $tag, (New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 122, 26))), 422, 268)
$g.Dispose(); $wm.Save((Join-Path $branding 'crosshair-wordmark.png'), [System.Drawing.Imaging.ImageFormat]::Png); $wm.Dispose()
Write-Output ("CROSSHAIR_LOGO_OK icon_sizes=" + ($sizes -join ','))
