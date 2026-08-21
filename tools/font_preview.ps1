#!/usr/bin/env pwsh
# font_preview.ps1 - 字体 QA：测量字形墨迹 bbox 并输出局部 8x 放大裁剪图
param(
    [Parameter(Mandatory = $true)][string]$Image,
    [int]$X0 = 0, [int]$Y0 = 0, [int]$X1 = 427, [int]$Y1 = 141,
    [string]$Crop = '',
    [int]$Threshold = 60
)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Bitmap]::FromFile($Image)
$minX = 999; $maxX = -1; $minY = 999; $maxY = -1; $n = 0
for ($y = $Y0; $y -le $Y1; $y++) {
    for ($x = $X0; $x -le $X1; $x++) {
        $c = $img.GetPixel($x, $y)
        if ($c.R -gt $Threshold) {
            $n++
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
Write-Output "bbox x=$minX..$maxX y=$minY..$maxY ink=$n"
if ($Crop) {
    $w = ($X1 - $X0 + 1) * 8
    $h = ($Y1 - $Y0 + 1) * 8
    $dst = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $srcRect = New-Object System.Drawing.Rectangle($X0, $Y0, ($X1 - $X0 + 1), ($Y1 - $Y0 + 1))
    $dstRect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
    $g.DrawImage($img, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $dst.Save($Crop, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
    Write-Output "crop saved: $Crop"
}
$img.Dispose()
