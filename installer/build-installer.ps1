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
# The notices the shipped components' licenses require to travel with the binaries (NOTICE.md).
# A missing one is a license violation, not a cosmetic gap: stop instead of shipping without it.
$notices = 'LICENSE', 'NOTICE.md', 'LICENSES\LGPLv3.txt', 'LICENSES\GPLv3.txt',
           'LICENSES\Qt-third-party-notices.txt', 'LICENSES\winpthreads-COPYING.txt',
           'LICENSES\MinGW-w64-runtime-COPYING.txt', 'LICENSES\GCC-RUNTIME-LIBRARY-EXCEPTION.txt',
           'installer\content\Brushes\Bundled brushes - credits and licenses.txt',
           'installer\content\myPatterns\Kenney Pattern Pack\License.txt',
           'installer\content\myPatterns\n4 Seamless Pattern Pack\License.txt'
foreach ($f in $notices) {
    if (-not (Test-Path "$root\$f")) { throw "Missing license notice: $f (see NOTICE.md)" }
}
# Qt-third-party-notices.txt lists what is inside THIS Qt build, and kritabundle.cpp uses Qt-private
# API: another Qt version needs both re-checked (regenerate the notices from its *-attribution-*.html)
if ($QtDir -notmatch '[\\/]6\.6\.1[\\/]') {
    throw "Qt $QtDir is not 6.6.1: regenerate LICENSES\Qt-third-party-notices.txt for it and update NOTICE.md first."
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
# LGPLv3 4(b), and the notice clauses of the code inside Qt and the MinGW runtime: the license
# texts must accompany the program (NOTICE.md)
Copy-Item "$root\LICENSE", "$root\NOTICE.md" $app
Copy-Item "$root\LICENSES" $app -Recurse

if ($SkipInstaller) {
    Write-Host "== Portable folder ready: $app"
    return
}

Write-Host "== Compiling installer"
# samSetupIcon.ico = the app icon with a "setup" badge (box + arrow), so the setup file is
# told apart from the installed program at a glance
Exec $Iscc /Q "/DAppVersion=$version" "/DAppDir=$app" "/DContentDir=$content" `
     "/DIconFile=$root\resources\samSetupIcon.ico" "/O$out" "$PSScriptRoot\SAM.iss"
# the asset to attach to the GitHub release: README's Download button links to
# releases/latest/download/SAM-Setup.exe, which only works if the name never changes
Copy-Item "$out\SAM-$version-setup.exe" "$out\SAM-Setup.exe"
Write-Host "== Done: $out\SAM-$version-setup.exe (release asset: $out\SAM-Setup.exe)"
