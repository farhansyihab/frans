# Buat shortcut Windows (.lnk) untuk frans.exe.
#
# Pemakaian:
#   powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1 -TargetPath "C:\path\ke\frans.exe"
#   powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1 -Location StartMenu
#
# Default: shortcut di Desktop DAN Start Menu user.

param(
    [string]$TargetPath = "",
    [string]$Name = "Frans",
    [ValidateSet("Desktop", "StartMenu", "Both")]
    [string]$Location = "Both"
)

$ErrorActionPreference = "Stop"

# ---- Cari frans.exe kalau tidak di-set eksplisit. ----
if (-not $TargetPath) {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\bin\Release\frans.exe"),
        (Join-Path $PSScriptRoot "..\build\bin\frans.exe"),
        (Join-Path $PSScriptRoot "..\build\win-msvc\bin\Release\frans.exe"),
        (Join-Path $PSScriptRoot "..\build\win-mingw\bin\frans.exe"),
        (Join-Path $PSScriptRoot "..\build\win-msvc\bin\frans.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) {
            $TargetPath = (Resolve-Path -LiteralPath $c).Path
            break
        }
    }
}

if (-not $TargetPath) {
    Write-Error "frans.exe tidak ditemukan. Build dulu, atau pakai -TargetPath."
    exit 1
}

if (-not (Test-Path -LiteralPath $TargetPath)) {
    Write-Error "TargetPath tidak ada: $TargetPath"
    exit 1
}

$TargetPath = (Resolve-Path -LiteralPath $TargetPath).Path
$WorkDir    = Split-Path -Parent $TargetPath

Write-Host "[frans] target  : $TargetPath"
Write-Host "[frans] workdir : $WorkDir"
Write-Host "[frans] lokasi  : $Location"

# ---- WScript.Shell COM untuk bikin .lnk. ----
$WshShell = New-Object -ComObject WScript.Shell

function New-FransShortcut {
    param([string]$LinkPath)

    $dir = Split-Path -Parent $LinkPath
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    $sc = $WshShell.CreateShortcut($LinkPath)
    $sc.TargetPath       = $TargetPath
    $sc.WorkingDirectory = $WorkDir
    $sc.Description      = "Frans — AI Code Importer & Build Runner"
    $sc.IconLocation     = "$TargetPath,0"
    $sc.WindowStyle      = 1
    $sc.Save()

    Write-Host "[frans] shortcut: $LinkPath"
}

$created = 0

if ($Location -eq "Desktop" -or $Location -eq "Both") {
    $desktop = [Environment]::GetFolderPath("Desktop")
    if ($desktop) {
        New-FransShortcut (Join-Path $desktop "$Name.lnk")
        $created++
    } else {
        Write-Warning "Desktop folder tidak ditemukan, skip."
    }
}

if ($Location -eq "StartMenu" -or $Location -eq "Both") {
    $startMenu = [Environment]::GetFolderPath("Programs")
    if ($startMenu) {
        # Taruh di subfolder "Frans" biar rapi di Start Menu.
        $dir = Join-Path $startMenu $Name
        New-FransShortcut (Join-Path $dir "$Name.lnk")
        $created++
    } else {
        Write-Warning "Start Menu folder tidak ditemukan, skip."
    }
}

if ($created -eq 0) {
    Write-Error "Tidak ada shortcut yang dibuat."
    exit 1
}

Write-Host ""
Write-Host "[frans] selesai. $created shortcut dibuat."
Write-Host "        Kalau binary dipindah, jalankan ulang script ini."