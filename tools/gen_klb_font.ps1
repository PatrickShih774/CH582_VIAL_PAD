#!/usr/bin/env pwsh
<#
 * gen_klb_font.ps1 - regenerate HAL/bm_font.c (Klein-Blue KLB UI font, v0.7)
 *
 * Fonts (vial-pad-klb-ui.md font stack: Saira display + Azeret Mono tabular + Chinese Light):
 *   Small sizes use Regular 400 for LCD legibility (Light 300 was too faint on this panel).
 *   16px base ASCII : Azeret Mono Light 300 (small values / expression)
 *   12px label ASCII: Azeret Mono Regular 400 (labels / chips / head, +30% coverage boost)
 *   16px Chinese    : Noto Sans CJK SC Regular (39 chars; open equivalent of MS YaHei UI Light)
 *   32px display    : Saira Light 300         (clock, digits + ':')
 *   40px display    : Saira Light 300         (calc result, digits + ',.-')
 *
 * Glyph format: 4-bit anti-aliased coverage (2 px/byte, high nibble first),
 * fixed grid per set (16/12/32/40 rows), packed stride = (grid+1)/2 bytes/row.
 * Rendering: GDI+ AntiAlias at 4x, 4x4 box average -> coverage 0..15.
#>
param(
    [string]$AsciiFont = (Join-Path $env:TEMP 'fonts\AzeretMono-Light.ttf'),
    [string]$DisplayFont = (Join-Path $env:TEMP 'fonts\Saira-Thin.ttf'),
    [string]$CjkFont = (Join-Path $env:TEMP 'fonts\NotoSansCJKsc-Light.otf'),
    [string]$OutFile = (Join-Path $PSScriptRoot '..\HAL\bm_font.c')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-GlyphBitmap {
    param($FontPath, [int]$CodePoint, [int]$PixelSize)
    $pfc = New-Object System.Drawing.Text.PrivateFontCollection
    $pfc.AddFontFile($FontPath)
    $font = New-Object System.Drawing.Font($pfc.Families[0], $PixelSize,
        [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $bmp = New-Object System.Drawing.Bitmap($PixelSize, [int]($PixelSize * 1.3))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::White)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.DrawString([string][char]$CodePoint, $font, [System.Drawing.Brushes]::Black, 0.0, 0.0)
    $g.Dispose(); $font.Dispose(); $pfc.Dispose()
    return $bmp
}

# Render one glyph into a Grid x Grid 4-bit coverage cell.
function ConvertTo-Coverage {
    param($Bitmap, [int]$Grid, [int]$Scale, [int]$Boost = 10)
    $stride = ($Grid + 1) -shr 1
    $data = New-Object 'System.Byte[]' ($Grid * $stride)
    $minX = 999; $maxX = -1; $ink = 0
    for ($gy = 0; $gy -lt $Grid; $gy++) {
        for ($gx = 0; $gx -lt $Grid; $gx++) {
            $sum = 0
            for ($sy = 0; $sy -lt $Scale; $sy++) {
                for ($sx = 0; $sx -lt $Scale; $sx++) {
                    $c = $Bitmap.GetPixel($gx * $Scale + $sx, $gy * $Scale + $sy)
                    $sum += (255 - $c.R)   # GDI+ AA encodes coverage as gray on white
                }
            }
            $cov = [int](($sum / ($Scale * $Scale)) * 15.0 / 255.0 + 0.5)
            if ($cov -gt 15) { $cov = 15 }
            if ($Boost -ne 10 -and $cov -gt 0) {
                $cov = [int]($cov * $Boost / 10.0 + 0.5)
                if ($cov -gt 15) { $cov = 15 }
            }
            if ($cov -gt 0) {
                $ink++
                if ($gx -lt $minX) { $minX = $gx }
                if ($gx -gt $maxX) { $maxX = $gx }
            }
            $bi = $gy * $stride + ($gx -shr 1)
            if (($gx -band 1) -eq 0) { $data[$bi] = ($data[$bi] -band 0x0F) -bor ($cov -shl 4) }
            else                     { $data[$bi] = ($data[$bi] -band 0xF0) -bor $cov }
        }
    }
    if ($ink -eq 0) { return @{ data = $data; w = 0; ox = 0; ink = 0; grid = $Grid; stride = $stride } }
    return @{ data = $data; w = ($maxX - $minX + 1); ox = $minX; ink = $ink; grid = $Grid; stride = $stride }
}

function Get-Glyph {
    param($FontPath, [int]$CodePoint, [int]$EmPx, [int]$Grid, [int]$Boost = 10, [int]$Scale = 4)
    $bmp = New-GlyphBitmap $FontPath $CodePoint ($EmPx * $Scale)
    $g = ConvertTo-Coverage $bmp $Grid $Scale $Boost
    $bmp.Dispose()
    return $g
}

# Fill version for display glyphs: GDI+ em metrics leave big empty rows
# (Saira Thin digits render ~18px of ink inside a 32px em).  Detect the ink
# bounding box at full resolution, then scale it to fill the whole grid
# (preserving aspect ratio, horizontally centered).
function ConvertTo-CoverageFill {
    param($Bitmap, [int]$Grid, [int]$Boost = 10, [double]$FillRatio = 1.0)
    $W = $Bitmap.Width; $H = $Bitmap.Height
    $minX = $W; $minY = $H; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $H; $y += 2) {
        for ($x = 0; $x -lt $W; $x += 2) {
            $c = $Bitmap.GetPixel($x, $y)
            if ((255 - $c.R) -gt 30) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    $stride = ($Grid + 1) -shr 1
    if ($maxX -lt 0) {
        return @{ data = (New-Object 'System.Byte[]' ($Grid * $stride)); w = 0; ox = 0; ink = 0; grid = $Grid; stride = $stride }
    }
    $pad = 2
    $minX = [Math]::Max(0, $minX - $pad); $minY = [Math]::Max(0, $minY - $pad)
    $maxX = [Math]::Min($W - 1, $maxX + $pad); $maxY = [Math]::Min($H - 1, $maxY + $pad)
    $bw = $maxX - $minX + 1; $bh = $maxY - $minY + 1

    $targetH = [Math]::Round($Grid * $FillRatio)
    if ($targetH -lt 1) { $targetH = 1 }
    $targetW = [Math]::Round($targetH * $bw / $bh)
    if ($targetW -gt $Grid) { $targetW = $Grid }
    if ($targetW -lt 1) { $targetW = 1 }
    $ox = [Math]::Floor(($Grid - $targetW) / 2)
    $oy = [Math]::Floor(($Grid - $targetH) / 2)
    $data = New-Object 'System.Byte[]' ($Grid * $stride)
    for ($gy = 0; $gy -lt $targetH; $gy++) {
        $srcY0 = $minY + [Math]::Floor($gy * $bh / $targetH)
        $srcY1 = $minY + [Math]::Ceiling(($gy + 1) * $bh / $targetH) - 1
        if ($srcY1 -lt $srcY0) { $srcY1 = $srcY0 }
        for ($gx = 0; $gx -lt $targetW; $gx++) {
            $srcX0 = $minX + [Math]::Floor($gx * $bw / $targetW)
            $srcX1 = $minX + [Math]::Ceiling(($gx + 1) * $bw / $targetW) - 1
            if ($srcX1 -lt $srcX0) { $srcX1 = $srcX0 }
            $sum = 0; $cnt = 0
            for ($sy = $srcY0; $sy -le $srcY1; $sy++) {
                for ($sx = $srcX0; $sx -le $srcX1; $sx++) {
                    $c = $Bitmap.GetPixel($sx, $sy)
                    $sum += (255 - $c.R); $cnt++
                }
            }
            $cov = [int](($sum / $cnt) * 15.0 / 255.0 + 0.5)
            if ($cov -gt 15) { $cov = 15 }
            if ($Boost -ne 10 -and $cov -gt 0) {
                $cov = [int]($cov * $Boost / 10.0 + 0.5)
                if ($cov -gt 15) { $cov = 15 }
            }
            $gc = $gx + $ox
            $gr = $gy + $oy
            $bi = $gr * $stride + ($gc -shr 1)
            if (($gc -band 1) -eq 0) { $data[$bi] = ($data[$bi] -band 0x0F) -bor ($cov -shl 4) }
            else                      { $data[$bi] = ($data[$bi] -band 0xF0) -bor $cov }
        }
    }
    return @{ data = $data; w = $targetW; ox = $ox; ink = 1; grid = $Grid; stride = $stride }
}

function Get-GlyphFill {
    param($FontPath, [int]$CodePoint, [int]$Grid, [int]$Boost = 10, [double]$FillRatio = 1.0)
    $bmp = New-GlyphBitmap $FontPath $CodePoint ($Grid * 4)
    $g = ConvertTo-CoverageFill $bmp $Grid $Boost $FillRatio
    $bmp.Dispose()
    return $g
}

function Format-Data {
    param($Data)
    $s = '    { '
    for ($i = 0; $i -lt $Data.Length; $i++) {
        $s += ('0x{0:X2}' -f $Data[$i])
        if ($i -lt $Data.Length - 1) { $s += ',' }
    }
    return $s + ' }'
}

function Format-WOx {
    param($Items, $Field)
    return (($Items | ForEach-Object { $_.$Field }) -join ',')
}


function Optimize-DisplayGlyph {
    param($Glyph, [int]$Grid)
    if ($Glyph.ink -eq 0) { return $Glyph }
    $stride = $Glyph.stride
    $data = $Glyph.data

    # Find first and last non-zero rows
    $firstRow = -1; $lastRow = -1
    for ($r = 0; $r -lt $Grid; $r++) {
        $hasInk = $false
        for ($c = 0; $c -lt $Grid; $c++) {
            $bi = $r * $stride + [Math]::Floor($c / 2)
            $nib = if (($c -band 1) -eq 0) { ($data[$bi] -shr 4) -band 0x0F } else { $data[$bi] -band 0x0F }
            if ($nib -gt 0) { $hasInk = $true; break }
        }
        if ($hasInk) {
            if ($firstRow -lt 0) { $firstRow = $r }
            $lastRow = $r
        }
    }
    $inkH = $lastRow - $firstRow + 1
    $newFirst = [int](($Grid - $inkH) / 2)
    if ($newFirst -eq $firstRow) { return $Glyph }  # already centered

    # Rebuild data with centered rows
    $newData = New-Object 'System.Byte[]' ($Grid * $stride)
    for ($r = 0; $r -lt $inkH; $r++) {
        [Array]::Copy($data, ($firstRow + $r) * $stride, $newData, ($newFirst + $r) * $stride, $stride)
    }
    return @{ data = $newData; w = $Glyph.w; ox = $Glyph.ox; ink = $Glyph.ink; grid = $Grid; stride = $stride }
}

# ---------------- render sets ----------------
$hzCodes = @(
    0x4EAE, 0x5EA6, 0x4F11, 0x7720, 0x4E3B, 0x9898, 0x91CD, 0x7F6E,
    0x8FDE, 0x63A5, 0x6267, 0x884C, 0x6C38, 0x4E0D, 0x6781, 0x7B80,
    0x6D45, 0x6DF1, 0x50CF, 0x7D20, 0x7EFF, 0x7425, 0x73C0, 0x9ED1,
    0x5BA2, 0x8BA4, 0x7259, 0x5468, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
    0x4E94, 0x516D, 0x65E5, 0x79D2, 0x5B8C, 0x6210, 0x00B7,
    0x6700, 0x8FD1, 0x514B, 0x83B1, 0x56E0, 0x660E, 0x9AD8, 0x5BF9, 0x6BD4, 0x5916, 0x89C2, 0x8017
    0x65F6, 0x95F4, 0x5F53, 0x524D, 0x6A21, 0x5F0F, 0x8BBE, 0x81EA,
    0x52A8, 0x590D, 0x8272, 0x5206, 0x8BA1, 0x7B97, 0x5668, 0x529F,
    0x80FD, 0x5C42, 0x6570, 0x5B57, 0x952E, 0x533A, 0x84DD
)

function Set-UniformDigits {
    param($Items, [int]$Base)
    $maxW = 0
    for ($d = 0; $d -lt 10; $d++) { if ($Items[$Base + $d].w -gt $maxW) { $maxW = $Items[$Base + $d].w } }
    for ($d = 0; $d -lt 10; $d++) { $Items[$Base + $d].w = $maxW }
    return $Items
}

# 16px base ASCII (Azeret Mono Light) - small values
$asc16 = @()
for ($i = 0; $i -lt 95; $i++) { $asc16 += Get-Glyph $AsciiFont (0x20 + $i) 16 16 }
$asc16 = Set-UniformDigits $asc16 16

# 8px micro ASCII (brand / chip / pageno)
$mic = @()
for ($i = 0; $i -lt 95; $i++) { $mic += Get-Glyph $AsciiFont (0x20 + $i) 8 8 12 6 }

# 9px label ASCII (t-label / h-label / t-sub)
$lbl = @()
for ($i = 0; $i -lt 95; $i++) { $lbl += Get-Glyph $AsciiFont (0x20 + $i) 9 9 13 8 }
$lbl = Set-UniformDigits $lbl 16

# 12px Chinese label (Noto Sans CJK SC Light, CJK needs >=12px at this panel)
$hz12 = @()
foreach ($cp in $hzCodes) {
    if ($cp -eq 0x00B7) { $g = Get-Glyph $AsciiFont $cp 12 12 } else { $g = Get-Glyph $CjkFont $cp 12 12 }
    if ($g.ink -eq 0) { $g = Get-Glyph $AsciiFont $cp 12 12 }
    # no scaling - keep native glyph proportions
    $hz12 += $g
}

# 16px Chinese (Noto Sans CJK SC Light; middle dot from JetBrains)
$hz = @()
foreach ($cp in $hzCodes) {
    if ($cp -eq 0x00B7) { $g = Get-Glyph $AsciiFont $cp 16 16 } else { $g = Get-Glyph $CjkFont $cp 16 16 }
    if ($g.ink -eq 0) { $g = Get-Glyph $AsciiFont $cp 16 16 }
    # no scaling - keep native glyph proportions
    $hz += $g
}


# 13px expr ASCII (calculator expression) - Azeret Mono Light
$expr = @()
for ($i = 0; $i -lt 95; $i++) { $expr += Get-Glyph $AsciiFont (0x20 + $i) 13 13 12 6 }
$expr = Set-UniformDigits $expr 16

# 22px mode value ASCII (A-Z 0-9 space - .) - Saira Thin
$modeChars = @('A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
               '0','1','2','3','4','5','6','7','8','9',' ','-','.','+','*','/','%','=')
$mode = @()
foreach ($ch in $modeChars) { $mode += Get-GlyphFill $DisplayFont ([int][char]$ch) 22 }
$mode = Set-UniformDigits $mode 26

# 32px clock display (HOME-02 big time) - Saira Thin
$disp32Chars = @('0','1','2','3','4','5','6','7','8','9',':' ,' ')
$disp32 = @()
foreach ($ch in $disp32Chars) { $disp32 += Get-GlyphFill $DisplayFont ([int][char]$ch) 32 }
$disp32 = Set-UniformDigits $disp32 0

# 40px result display: digits + ',' '.' '-' ' ' - Saira Thin
$res40Chars = @('0','1','2','3','4','5','6','7','8','9',',','.','-',' ')
$res40 = @()
foreach ($ch in $res40Chars) { $res40 += Get-GlyphFill $DisplayFont ([int][char]$ch) 40 }
$res40 = Set-UniformDigits $res40 0


# Center glyph vertically: trim equal top/bottom blank rows so ink is centered in grid.
# ---------------- emit C file ----------------
$sb = New-Object System.Text.StringBuilder
$sb.AppendLine('/**') | Out-Null
$sb.AppendLine(' * bm_font.c - KLB UI shared font (anti-aliased 4-bit coverage, v0.7)') | Out-Null
$sb.AppendLine(' *') | Out-Null
$sb.AppendLine(' * Generated by tools/gen_klb_font.ps1 (GDI+ AntiAlias 4x, 4x4 box average).') | Out-Null
$sb.AppendLine(' * Fonts (vial-pad-klb-ui.md section 3):') | Out-Null
$sb.AppendLine(' *   16px base ASCII : Azeret Mono Regular 400 (OFL) - small values / expression') | Out-Null
$sb.AppendLine(' *   12px label ASCII: Azeret Mono Regular 400 (OFL) - labels / chips / head') | Out-Null
$sb.AppendLine(' *   16px Chinese    : Noto Sans CJK SC Regular (OFL) - 39 chars') | Out-Null
$sb.AppendLine(' *   32px display    : Saira Thin 200 (OFL) - clock digits') | Out-Null
$sb.AppendLine(' *   40px display    : Saira Thin 200 (OFL) - calc result digits') | Out-Null
$sb.AppendLine(' * Glyph format: 4-bit coverage, 2px/byte (high nibble first),') | Out-Null
$sb.AppendLine(' * fixed stride per set = (grid+1)/2 bytes/row.') | Out-Null
$sb.AppendLine(' */') | Out-Null
$sb.AppendLine('#include "bm_font.h"') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 16px base ASCII ---
$sb.AppendLine('/* ---- 16px base ASCII (JetBrains Mono Light, digits tabular) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_asc16_data[95][128] = {') | Out-Null
for ($i = 0; $i -lt 95; $i++) {
    $ch = [char](0x20 + $i)
    $label = $ch
    if ($ch -eq ' ') { $label = ' ' }
    $sb.AppendLine((Format-Data $asc16[$i].data) + ",  /* $label */") | Out-Null
}
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_asc16_ox[95] = { ' + (Format-WOx $asc16 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_asc16_w[95] = { ' + (Format-WOx $asc16 'w') + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 8px micro ASCII ---
$sb.AppendLine('/* ---- 8px micro ASCII (Azeret Mono Light, brand/chip/pageno) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_mic8_data[95][32] = {') | Out-Null
for ($i = 0; $i -lt 95; $i++) {
    $ch = [char](0x20 + $i)
    $label = $ch
    if ($ch -eq ' ') { $label = ' ' }
    $sb.AppendLine((Format-Data $mic[$i].data) + ',  /* $label */') | Out-Null
}
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_mic8_ox[95] = { ' + (Format-WOx $mic 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_mic8_w[95] = { ' + (Format-WOx $mic 'w') + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 9px label ASCII ---
$sb.AppendLine('/* ---- 9px label ASCII (Azeret Mono Light, t-label/t-sub) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_lbl_data[95][45] = {') | Out-Null
for ($i = 0; $i -lt 95; $i++) {
    $ch = [char](0x20 + $i)
    $label = $ch
    if ($ch -eq ' ') { $label = ' ' }
    $sb.AppendLine((Format-Data $lbl[$i].data) + ',  /* $label */') | Out-Null
}
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_lbl_ox[95] = { ' + (Format-WOx $lbl 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_lbl_w[95] = { ' + (Format-WOx $lbl 'w') + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 16px Chinese ---
$sb.AppendLine('/* ---- 16px Chinese (Noto Sans CJK SC Light) ---- */') | Out-Null
$sb.AppendLine('static const uint16_t g_hz_codes[' + $hzCodes.Count + '] = {') | Out-Null
for ($i = 0; $i -lt $hzCodes.Count; $i++) { $sb.AppendLine(('    0x{0:X4},  /* {1} */' -f $hzCodes[$i], [char]$hzCodes[$i])) | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_hz_ox[' + $hzCodes.Count + '] = { ' + (Format-WOx $hz 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_hz_w[' + $hzCodes.Count + '] = { ' + (Format-WOx $hz 'w') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_hz_data[' + $hzCodes.Count + '][128] = {') | Out-Null
for ($i = 0; $i -lt $hzCodes.Count; $i++) { $sb.AppendLine((Format-Data $hz[$i].data) + ",  /* $([char]$hzCodes[$i]) */") | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('') | Out-Null


# --- 9px Chinese label ---
$sb.AppendLine('/* ---- 12px Chinese label (Noto Sans CJK SC Light) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_hz12_ox[' + $hzCodes.Count + '] = { ' + (Format-WOx $hz12 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_hz12_w[' + $hzCodes.Count + '] = { ' + (Format-WOx $hz12 'w') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_hz12_data[' + $hzCodes.Count + '][72] = {') | Out-Null
for ($i = 0; $i -lt $hzCodes.Count; $i++) { $sb.AppendLine((Format-Data $hz12[$i].data) + ",  /* $([char]$hzCodes[$i]) */") | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 13px expr ASCII ---
$sb.AppendLine('/* ---- 13px expr ASCII (Azeret Mono Light, calc expression) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_expr13_data[95][91] = {') | Out-Null
for ($i = 0; $i -lt 95; $i++) {
    $ch = [char](0x20 + $i)
    $label = $ch
    if ($ch -eq ' ') { $label = ' ' }
    $sb.AppendLine((Format-Data $expr[$i].data) + ',  /* $label */') | Out-Null
}
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_expr13_ox[95] = { ' + (Format-WOx $expr 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_expr13_w[95] = { ' + (Format-WOx $expr 'w') + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 22px mode ASCII ---
$sb.AppendLine('/* ---- 22px mode ASCII (Saira Thin, mode value) ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_mode22_codes[44] = { ' + (($modeChars | ForEach-Object { '0x{0:X2}' -f [int][char]$_ }) -join ",") + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_mode22_ox[44] = { ' + (Format-WOx $mode 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_mode22_w[44] = { ' + (Format-WOx $mode 'w') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_mode22_data[44][242] = {') | Out-Null
for ($i = 0; $i -lt $modeChars.Count; $i++) { $sb.AppendLine((Format-Data $mode[$i].data) + ",  /* $($modeChars[$i]) */") | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 32px clock display ---
$sb.AppendLine('/* ---- 32px clock display (Saira Thin): 0-9 : space ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_disp32_data[12][512] = {') | Out-Null
for ($i = 0; $i -lt $disp32Chars.Count; $i++) { $sb.AppendLine((Format-Data $disp32[$i].data) + ',  /* ' + $disp32Chars[$i] + ' */') | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_disp32_ox[12] = { ' + (Format-WOx $disp32 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_disp32_w[12] = { ' + (Format-WOx $disp32 'w') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_disp32_codes[12] = { ' + (($disp32Chars | ForEach-Object { '0x{0:X2}' -f [int][char]$_ }) -join ",") + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- 40px result display ---
$sb.AppendLine('/* ---- 40px result display (Source Sans 3 Light): 0-9 , . - space ---- */') | Out-Null
$sb.AppendLine('static const uint8_t g_res40_data[14][800] = {') | Out-Null
for ($i = 0; $i -lt $res40Chars.Count; $i++) { $sb.AppendLine((Format-Data $res40[$i].data) + ",  /* $($res40Chars[$i]) */") | Out-Null }
$sb.AppendLine('};') | Out-Null
$sb.AppendLine('static const uint8_t g_res40_ox[14] = { ' + (Format-WOx $res40 'ox') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_res40_w[14] = { ' + (Format-WOx $res40 'w') + ' };') | Out-Null
$sb.AppendLine('static const uint8_t g_res40_codes[14] = { ' + (($res40Chars | ForEach-Object { '0x{0:X2}' -f [int][char]$_ }) -join ',') + ' };') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('static const uint8_t g_missing_data[8] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };') | Out-Null
$sb.AppendLine('') | Out-Null

# --- lookup helpers ---
$sb.AppendLine('static glyph_t g_find(const uint8_t *data, const uint8_t *ox, const uint8_t *w,') | Out-Null
$sb.AppendLine('                  const uint8_t *codes, uint8_t n, uint16_t ch,') | Out-Null
$sb.AppendLine('                  uint8_t grid, uint8_t stride)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    uint8_t i;') | Out-Null
$sb.AppendLine('    for (i = 0; i < n; i++) {') | Out-Null
$sb.AppendLine('        if (codes[i] == (uint8_t)ch)') | Out-Null
$sb.AppendLine('            return (glyph_t){ data + (uint32_t)i * stride * grid, w[i] ? w[i] : 1, grid, ox[i], stride };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return (glyph_t){ g_missing_data, 1, 1, 0, 1 };') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('glyph_t bm_font_glyph(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    if (ch >= 0x20 && ch <= 0x7E) {') | Out-Null
$sb.AppendLine('        uint8_t i = (uint8_t)(ch - 0x20);') | Out-Null
$sb.AppendLine('        return (glyph_t){ g_asc16_data[i], g_asc16_w[i] ? g_asc16_w[i] : 1, 16, g_asc16_ox[i], 8 };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return (glyph_t){ g_missing_data, 1, 1, 0, 1 };') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('glyph_t bm_font_glyph_micro(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    if (ch >= 0x20 && ch <= 0x7E) {') | Out-Null
$sb.AppendLine('        uint8_t i = (uint8_t)(ch - 0x20);') | Out-Null
$sb.AppendLine('        return (glyph_t){ g_mic8_data[i], g_mic8_w[i] ? g_mic8_w[i] : 1, 8, g_mic8_ox[i], 4 };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return bm_font_glyph_label(ch);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_label(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    if (ch >= 0x20 && ch <= 0x7E) {') | Out-Null
$sb.AppendLine('        uint8_t i = (uint8_t)(ch - 0x20);') | Out-Null
$sb.AppendLine('        return (glyph_t){ g_lbl_data[i], g_lbl_w[i] ? g_lbl_w[i] : 1, 9, g_lbl_ox[i], 5 };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return bm_font_glyph(ch);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('static glyph_t find_hz(uint16_t cp, const uint8_t *data, const uint8_t *ox,') | Out-Null
$sb.AppendLine('                       const uint8_t *w, uint8_t grid, uint8_t stride)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    uint16_t i;') | Out-Null
$sb.AppendLine('    for (i = 0; i < (uint16_t)(sizeof(g_hz_codes) / sizeof(g_hz_codes[0])); i++) {') | Out-Null
$sb.AppendLine('        if (g_hz_codes[i] == cp)') | Out-Null
$sb.AppendLine('            return (glyph_t){ data + (uint32_t)i * stride * grid, w[i] ? w[i] : 1, grid, ox[i], stride };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return (glyph_t){ g_missing_data, 1, 1, 0, 1 };') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_utf8(const char **sp)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    const uint8_t *p = (const uint8_t *)*sp;') | Out-Null
$sb.AppendLine('    uint32_t cp;') | Out-Null
$sb.AppendLine('    if (p[0] < 0x80) { cp = p[0]; (*sp) += 1; return bm_font_glyph((uint16_t)cp); }') | Out-Null
$sb.AppendLine('    if ((p[0] & 0xE0) == 0xC0 && p[1]) {') | Out-Null
$sb.AppendLine('        cp = (uint32_t)(((p[0] & 0x1Fu) << 6) | (p[1] & 0x3Fu)); (*sp) += 2;') | Out-Null
$sb.AppendLine('    } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {') | Out-Null
$sb.AppendLine('        cp = (uint32_t)(((p[0] & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu));') | Out-Null
$sb.AppendLine('        (*sp) += 3;') | Out-Null
$sb.AppendLine('    } else { cp = (uint32_t)''?''; (*sp) += 1; }') | Out-Null
$sb.AppendLine('    return find_hz((uint16_t)cp, g_hz_data[0], g_hz_ox, g_hz_w, 16, 8);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_label_utf8(const char **sp)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    const uint8_t *p = (const uint8_t *)*sp;') | Out-Null
$sb.AppendLine('    uint32_t cp;') | Out-Null
$sb.AppendLine('    if (p[0] < 0x80) { cp = p[0]; (*sp) += 1; return bm_font_glyph_label((uint16_t)cp); }') | Out-Null
$sb.AppendLine('    if ((p[0] & 0xE0) == 0xC0 && p[1]) {') | Out-Null
$sb.AppendLine('        cp = (uint32_t)(((p[0] & 0x1Fu) << 6) | (p[1] & 0x3Fu)); (*sp) += 2;') | Out-Null
$sb.AppendLine('    } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {') | Out-Null
$sb.AppendLine('        cp = (uint32_t)(((p[0] & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu));') | Out-Null
$sb.AppendLine('        (*sp) += 3;') | Out-Null
$sb.AppendLine('    } else { cp = (uint32_t)''?''; (*sp) += 1; }') | Out-Null
$sb.AppendLine('    return find_hz((uint16_t)cp, g_hz12_data[0], g_hz12_ox, g_hz12_w, 12, 6);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('glyph_t bm_font_glyph_clock(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    return g_find(g_disp32_data[0], g_disp32_ox, g_disp32_w, g_disp32_codes, 12, ch, 32, 16);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_expr(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    if (ch >= 0x20 && ch <= 0x7E) {') | Out-Null
$sb.AppendLine('        uint8_t i = (uint8_t)(ch - 0x20);') | Out-Null
$sb.AppendLine('        return (glyph_t){ g_expr13_data[i], g_expr13_w[i] ? g_expr13_w[i] : 1, 13, g_expr13_ox[i], 7 };') | Out-Null
$sb.AppendLine('    }') | Out-Null
$sb.AppendLine('    return bm_font_glyph(ch);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_mode(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    return g_find(g_mode22_data[0], g_mode22_ox, g_mode22_w, g_mode22_codes, 44, ch, 22, 11);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('glyph_t bm_font_glyph_result(uint16_t ch)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    return g_find(g_res40_data[0], g_res40_ox, g_res40_w, g_res40_codes, 14, ch, 40, 20);') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$sb.AppendLine('static uint16_t bm_text_width_s(const char *s, glyph_t (*get)(uint16_t))') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    uint16_t w = 0;') | Out-Null
$sb.AppendLine('    while (*s) { glyph_t g = get((uint8_t)*s++); w = (uint16_t)(w + g.w + 1); }') | Out-Null
$sb.AppendLine('    return w ? (uint16_t)(w - 1) : 0;') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_micro(const char *s) { return bm_text_width_s(s, bm_font_glyph_micro); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_expr(const char *s) { return bm_text_width_s(s, bm_font_glyph_expr); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_mode(const char *s) { return bm_text_width_s(s, bm_font_glyph_mode); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_label(const char *s) { return bm_text_width_s(s, bm_font_glyph_label); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_label_utf8(const char *s)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    uint16_t w = 0;') | Out-Null
$sb.AppendLine('    while (*s) { glyph_t g = bm_font_glyph_label_utf8(&s); w = (uint16_t)(w + g.w + 1); }') | Out-Null
$sb.AppendLine('    return w ? (uint16_t)(w - 1) : 0;') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_clock(const char *s) { return bm_text_width_s(s, bm_font_glyph_clock); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width_result(const char *s) { return bm_text_width_s(s, bm_font_glyph_result); }') | Out-Null
$sb.AppendLine('uint16_t bm_text_width(const char *s)') | Out-Null
$sb.AppendLine('{') | Out-Null
$sb.AppendLine('    uint16_t w = 0;') | Out-Null
$sb.AppendLine('    while (*s) { glyph_t g = bm_font_glyph_utf8(&s); w = (uint16_t)(w + g.w + 1); }') | Out-Null
$sb.AppendLine('    return w ? (uint16_t)(w - 1) : 0;') | Out-Null
$sb.AppendLine('}') | Out-Null
$sb.AppendLine('') | Out-Null

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Resolve-Path $OutFile), $sb.ToString(), $utf8NoBom)
Write-Output "bm_font.c regenerated: $OutFile"





