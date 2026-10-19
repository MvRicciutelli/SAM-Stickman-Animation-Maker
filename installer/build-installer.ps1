<#
.SYNOPSIS
  Builds SAM - Stickman Animation Maker in Release, stages it with the Qt/MinGW runtime and compiles
  the Inno Setup installer: installer\out\SAM-<version>-setup.exe

.DESCRIPTION
  Bundled content = whatever sits in installer\content\ (Brushes, myPatterns,
  Libraries, myStickFigures). The installer seeds it into Documents\SAM\,
  the folder Widget::CreatePaths() reads, and never overwrites a file the user
  already has there. FFmpeg is NOT bundled (see NOTICE.md).

  Version comes from VERSION in SAM.pro.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1
  powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1 -SkipInstaller
#>
param(
    [string]$QtDir    = 'C:\Qt\6.6.1\mingw_64',
    [string]$MinGWDir = 'C:\Qt\Tools\mingw1120_64',
    [string]$Iscc     = '',
    # stop after staging: installer\out\app is then a runnable portable folder
    [switch]$SkipInstaller
)
$ErrorActionPreference = 'Stop'

$root    = Split-Path $PSScriptRoot -Parent
$build   = Join-Path (Split-Path $root -Parent) 'build-SAM-installer'
$out     = Join-Path $PSScriptRoot 'out'
$app     = Join-Path $out 'app'
$content = Join-Path $PSScriptRoot 'content'

function Exec($exe) {
    & $exe @args
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

# fallisci subito, non dopo una build di minuti
if (-not $SkipInstaller) {
    if (-not $Iscc) {
        $Iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
                  "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                  "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
                Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $Iscc -or -not (Test-Path $Iscc)) {
        throw "Inno Setup 6 not found. Install it (winget install JRSoftware.InnoSetup) or pass -Iscc <path to ISCC.exe>."
    }
}
foreach ($tool in "$QtDir\bin\qmake.exe", "$QtDir\bin\windeployqt.exe", "$MinGWDir\bin\mingw32-make.exe") {
    if (-not (Test-Path $tool)) { throw "Not found: $tool (check -QtDir / -MinGWDir)" }
}

$version = (Select-String -Path "$root\SAM.pro" -Pattern '^\s*VERSION\s*=\s*(\S+)').Matches[0].Groups[1].Value
Write-Host "== SAM - Stickman Animation Maker $version"

# windeployqt trova il runtime MinGW tramite il g++ sul PATH
$env:PATH = "$QtDir\bin;$MinGWDir\bin;$env:PATH"

Write-Host "== Building (Release) in $build"
New-Item -ItemType Directory -Force $build | Out-Null
Push-Location $build
try {
    Exec qmake "$root\SAM.pro" -spec win32-g++
    Exec mingw32-make -f Makefile.Release "-j$env:NUMBER_OF_PROCESSORS"
} finally {
    Pop-Location
}

Write-Host "== Staging $app"
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $app | Out-Null
Copy-Item "$build\release\SAM.exe" $app
# widgets-only, raster painting: the software GL rasterizer (20 MB), the D3D compiler and the
# network-information plugins are dead weight. The TLS plugins are NOT: the startup update check
# (updatecheck.cpp) talks HTTPS to GitHub, and without a TLS backend it fails silently forever.
Exec windeployqt --release --no-translations --no-opengl-sw --no-system-d3d-compiler `
     --skip-plugin-types 'networkinformation' "$app\SAM.exe"
# LGPLv3 4(b): the license texts must accompany the program (NOTICE.md)
Copy-Item "$root\LICENSE", "$root\NOTICE.md" $app
Copy-Item "$root\LICENSES" $app -Recurse

if ($SkipInstaller) {
    Write-Host "== Portable folder ready: $app"
    return
}

Write-Host "== Compiling installer"
Exec $Iscc /Q "/DAppVersion=$version" "/DAppDir=$app" "/DContentDir=$content" `
     "/DIconFile=$root\samIcon.ico" "/O$out" "$PSScriptRoot\SAM.iss"
Write-Host "== Done: $out\SAM-$version-setup.exe"
