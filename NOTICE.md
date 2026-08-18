# Third-party notices

## Qt

MarionettaPlus is built against **Qt 6.6.1** (`core`, `gui`, `widgets`), used
under the **GNU Lesser General Public License v3** (LGPLv3). The full license
text is included at [LICENSES/LGPLv3.txt](LICENSES/LGPLv3.txt).

- Qt is linked **dynamically** — the Qt shared libraries (`Qt6Core.dll`,
  `Qt6Gui.dll`, `Qt6Widgets.dll`, etc.) ship alongside the executable and are
  not modified or statically linked in.
- Qt's own source code is available from The Qt Company at
  <https://download.qt.io/official_releases/qt/> (this build: the 6.6.1
  release archive).
- Qt has not been patched; no source obligation beyond linking to the above
  applies.
- Qt copyright notices are not stripped from the shipped binaries or
  installer. "Qt ▸ Help ▸ About Qt" in the application shows Qt's own
  version and license information at runtime.
- "Qt" and the Qt logo are trademarks of The Qt Company Ltd. This project is
  an independent application built with Qt and is not endorsed by or
  affiliated with The Qt Company.

## MinGW runtime (Windows builds)

Windows builds link the MinGW-w64 runtime (`libgcc`, `libstdc++`,
`libwinpthread`), distributed under GPLv3 with the GCC Runtime Library
Exception, which permits linking into non-GPL applications without imposing
a source-distribution requirement on this project.

## FFmpeg

FFmpeg is **not bundled**. Video export invokes an FFmpeg executable located
on the user's own system (or one the user separately downloads and points
the app to, per the export dialog) as a subprocess; no FFmpeg code or binary
is distributed with this project.
