# Pack Selt_Gui for machines without Qt/OpenCV/MinGW installed.
# Usage (PowerShell):
#   .\scripts\package_windows.ps1
# Optional:
#   .\scripts\package_windows.ps1 -BuildDir "D:\QT_CppPrograms\Selt_Gui\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug"

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$DistDir = "",
    [string]$QtBin = "D:\QT\6.11.1\mingw_64\bin",
    [string]$OpenCvBin = "D:\OpenCV\opencv\opencv-mingw-build\install\x64\mingw\bin",
    [string]$MingwBin = "D:\QT\Tools\mingw1310_64\bin"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

if (-not $BuildDir) {
    $candidate = Join-Path $Root "build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug"
    if (Test-Path (Join-Path $candidate "bin\Selt_Gui.exe")) {
        $BuildDir = $candidate
    } else {
        throw "BuildDir not found. Pass -BuildDir to the folder that contains bin\Selt_Gui.exe"
    }
}

if (-not $DistDir) {
    $DistDir = Join-Path $Root "dist\Selt_Gui"
}

$ExeSrc = Join-Path $BuildDir "bin\Selt_Gui.exe"
if (-not (Test-Path $ExeSrc)) {
    throw "Missing executable: $ExeSrc (build Selt_Gui first in Qt Creator)"
}

$Windeploy = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path $Windeploy)) {
    throw "windeployqt not found: $Windeploy"
}

Write-Host "==> Generate app icon ICO"
python (Join-Path $Root "scripts\generate_app_icon_ico.py")

Write-Host "==> Prepare dist: $DistDir"
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $DistDir | Out-Null
Copy-Item $ExeSrc (Join-Path $DistDir "Selt_Gui.exe")

Write-Host "==> windeployqt (Qt + MinGW runtime)"
& $Windeploy `
    --release `
    --compiler-runtime `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    (Join-Path $DistDir "Selt_Gui.exe")
if ($LASTEXITCODE -ne 0) {
    # Debug builds may need --debug; retry without --release.
    Write-Host "windeployqt --release failed, retrying without --release for Debug builds..."
    & $Windeploy `
        --compiler-runtime `
        --no-translations `
        --no-system-d3d-compiler `
        --no-opengl-sw `
        (Join-Path $DistDir "Selt_Gui.exe")
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit $LASTEXITCODE"
    }
}

Write-Host "==> Copy OpenCV DLLs"
$opencvNeeded = @(
    "libopencv_core4120.dll",
    "libopencv_imgproc4120.dll",
    "libopencv_imgcodecs4120.dll",
    "libopencv_videoio4120.dll",
    "libopencv_features2d4120.dll",
    "libopencv_objdetect4120.dll",
    "libopencv_dnn4120.dll",
    "libopencv_calib3d4120.dll",
    "libopencv_flann4120.dll",
    "opencv_videoio_ffmpeg4120_64.dll"
)
foreach ($dll in $opencvNeeded) {
    $src = Join-Path $OpenCvBin $dll
    if (Test-Path $src) {
        Copy-Item $src $DistDir -Force
        Write-Host "  + $dll"
    } else {
        Write-Warning "Missing OpenCV DLL (skip): $src"
    }
}

# Extra MinGW runtime safety net (in case windeployqt skipped them)
$mingwRuntime = @(
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)
if (Test-Path $MingwBin) {
    foreach ($dll in $mingwRuntime) {
        $src = Join-Path $MingwBin $dll
        $dst = Join-Path $DistDir $dll
        if ((Test-Path $src) -and -not (Test-Path $dst)) {
            Copy-Item $src $DistDir -Force
            Write-Host "  + $dll (mingw)"
        }
    }
}

# Optional plugins folder placeholder
$plugins = Join-Path $DistDir "plugins\nodes"
New-Item -ItemType Directory -Path $plugins -Force | Out-Null

$readme = @"
Selt_Gui portable package
=========================

1. Double-click Selt_Gui.exe to run.
2. This folder already includes Qt and OpenCV runtime DLLs.
3. Do not move Selt_Gui.exe out of this folder (DLLs must stay beside it).
4. Optional node plugins go into plugins\nodes\

Built from: $BuildDir
"@
Set-Content -Path (Join-Path $DistDir "README.txt") -Value $readme -Encoding UTF8

Write-Host ""
Write-Host "DONE. Portable folder:"
Write-Host "  $DistDir"
Write-Host "Copy the whole folder to a PC without Qt/OpenCV and run Selt_Gui.exe"
