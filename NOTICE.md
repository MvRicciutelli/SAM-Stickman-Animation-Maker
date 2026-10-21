# Third-party notices

SAM itself is distributed under the PolyForm Strict License 1.0.0, with
additional permissions that allow any use, commercial included —
see [LICENSE](LICENSE). It is a proprietary (source-available, freeware)
application, which the LGPLv3 expressly permits for a work that merely links
against LGPL libraries. The notices below cover the third-party components it
is built with, and the third-party brushes and patterns installed with it.
The application shows this file under **Help ▸ Third-party notices**.

## Bundled brushes and patterns

The installer copies these collections into `Documents\SAM\Brushes` and
`Documents\SAM\myPatterns`. They are **third-party works, not part of SAM, and
not covered by SAM's PolyForm Strict license**: each stays under its own license,
below, and anyone may use, copy, modify and share it under those terms, inside
or outside SAM. A credits file with the same information sits next to them
(`Brushes\Bundled brushes - credits and licenses.txt`, and a `License.txt` in
each pattern folder).

| Collection | Author | License | Source |
|---|---|---|---|
| Kenney Pattern Pack 1.1 (84 tiles, unmodified) | Kenney | CC0 1.0 | <https://kenney.nl/assets/pattern-pack> |
| Seamless Pattern Pack (unmodified, sorted into subfolders) | n4 | CC0 1.0 | <https://opengameart.org/content/seamless-pattern-pack> |
| Popple Cartography Brushes 2.1 (.abr, unmodified) | K. M. Alexander | CC0 1.0 | <https://kmalexander.com> |
| David Revoy 2023-01 (converted from Krita) | David Revoy | CC0 1.0 | <https://www.davidrevoy.com/article953/krita-brushes-2023-01-bundle> |
| Nylnook v1.1 (converted from Krita) | Camille Bissuel (Nylnook) | CC0 1.0 | <https://github.com/nylnook/nylnook-krita-brushes> |
| Nylnook v2 Generic Pen (converted from Krita) | Camille Bissuel (Nylnook) | CC0 1.0 | <https://github.com/nylnook/nylnook-krita-brushes> |

- The three Krita collections were converted from their `.bundle` files by SAM's
  own brush importer. Krita's pressure curves, textures, blending modes and
  non-pixel engines were left out, and tips larger than 512 px were downscaled.
- Nylnook v1.1's "Nylnook Airbrush" preset is **deliberately not included**: its
  tip (`cazu-spray.gbr`) is from Pablo Cazorla's brush set, for which no
  license could be verified.
- Several tips in the David Revoy and Nylnook v1.1 collections are identical to
  tips in Krita's default resources. Krita's bundles label those CC0, but
  Krita's data README says the `.gbr`/`.gih`/`.vbr` brushes shipped with Krita
  come from David Revoy's "Chaos and Evolutions" set and Ramon Miranda's "GIMP
  Paint Studio" under **CC BY 3.0** (attributed to David Revoy and the Blender
  Foundation). To satisfy either reading, they are credited that way here and in
  the credits file: by David Revoy and Ramon Miranda, used under
  [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/), converted to SAM's
  format. No other terms are imposed on them.

## Qt

SAM is built against **Qt 6.6.1** (`core`, `gui`, `widgets`, `network`, plus the
`svg` image and icon plugins `windeployqt` adds), used
under the **GNU Lesser General Public License v3** (LGPLv3). The full license
text is included at [LICENSES/LGPLv3.txt](LICENSES/LGPLv3.txt), together with
the [GPLv3](LICENSES/GPLv3.txt) it incorporates by reference — LGPLv3 §4(b)
requires both to accompany the combined work.

- Qt is linked **dynamically** — the Qt shared libraries (`Qt6Core.dll`,
  `Qt6Gui.dll`, `Qt6Widgets.dll`, etc.) ship alongside the executable and are
  not modified or statically linked in.
- Qt's own source code is available from The Qt Company at
  <https://download.qt.io/official_releases/qt/> (this build: the 6.6.1
  release archive).
- Qt has not been patched; no source obligation beyond linking to the above
  applies.
- SAM reads Krita bundles through `QZipReader`, a Qt-internal class
  (`core-private`). It is still dynamic linking against the unmodified
  library, so nothing above changes; it only means a replacement Qt must be
  interface-compatible with 6.6.1 (a modified or rebuilt 6.6.1, which is what
  LGPLv3 §4(d)(1) asks to keep working) rather than any later release.
- The Qt binaries contain third-party code of their own, which keeps its own
  licenses rather than Qt's — see the next section.
- Qt copyright notices are not stripped from the shipped binaries or
  installer. The application's **Help ▸ About** dialog names Qt, its copyright
  holder and where these license texts are found (LGPLv3 §4(a) and §4(c),
  the latter applying because that dialog displays copyright notices during
  execution), and **Help ▸ About Qt** shows Qt's own version and license
  information at runtime.
- Users are expressly permitted to modify, rebuild and replace the Qt
  libraries, to relink SAM against their own build of them, and to
  reverse engineer SAM as far as needed to debug such changes —
  see "Additional permissions" in [LICENSE](LICENSE). LGPLv3 §4 requires that
  the application's own terms not restrict this, so that grant is a
  compliance requirement rather than a courtesy.
- "Qt" and the Qt logo are trademarks of The Qt Company Ltd. This project is
  an independent application built with Qt and is not endorsed by or
  affiliated with The Qt Company.

## Third-party code inside the Qt libraries

The Qt Company builds code by other authors into the Qt libraries and plugins
shipped with SAM (`Qt6Core`, `Qt6Gui`, `Qt6Network`, `Qt6Svg`, the image-format
plugins). That code is **not** covered by Qt's LGPL; it stays under its authors'
licenses, several of which require their notices to accompany every binary copy.
The complete notices and license texts are in
[LICENSES/Qt-third-party-notices.txt](LICENSES/Qt-third-party-notices.txt), reproduced
from Qt 6.6.1's own documentation ("Licenses Used in Qt"), and are shown in full
under **Help ▸ Third-party notices**.

Acknowledgements these licenses ask for in the documentation:

- Portions of this software are copyright © 1996-2023 The FreeType Project
  (<https://freetype.org>). All rights reserved. This software is based in part
  on the work of the FreeType Team.
- This software is based in part on the work of the Independent JPEG Group.
- Unicode data: Copyright © 1991-2022 Unicode, Inc. All rights reserved.
  Distributed under the Terms of Use in <https://www.unicode.org/copyright.html>.

| License | Components |
|---|---|
| FreeType License (FTL) | FreeType 2.13.2, its anti-aliasing rasterizer |
| IJG, BSD-3-Clause, Zlib | libjpeg-turbo 3.0.1 (JPEG image plugin) |
| libpng License | libpng 1.6.40 |
| Zlib | zlib 1.3, FreeType's zlib module |
| BSD-3-Clause / BSD-2-Clause | PCRE2 10.42 and its JIT compiler, double-conversion 3.3.0, libpsl, SHA-384/512 (RFC 6234), SHA-3 endian helper, Easing Equations (Robert Penner), Adobe Glyph List, IAccessible2 IDL |
| MIT | HarfBuzz 8.3.0, MD4C 0.4.8, TinyCBOR 0.6.0, Pixman 0.17.12, D3D12 and Vulkan Memory Allocators, D3D12 mipmap generator, OpenGL headers, WebGradients, FreeType BDF driver |
| Unicode-DFS-2016 | Unicode Character Database and CLDR locale data |
| MPL-2.0 | The Public Suffix List data (Qt Network) |
| HPND | XSVG (Qt SVG) |
| ICC License | sRGB colour profile |
| CC0 / public domain | BLAKE2, MD4, MD5, SHA-1, SHA-3 (Keccak), SipHash |

The Public Suffix List is the one component under a file-level copyleft
(MPL-2.0 §3.2): its source form is available from
<https://github.com/publicsuffix/list>, and the exact copy compiled into
`Qt6Network.dll` is `qtbase/src/3rdparty/libpsl/psl_data.cpp` in the Qt 6.6.1
source archive linked above. It is unmodified, and nothing in SAM's terms limits
anyone's rights in it.

## MinGW runtime (Windows builds)

Windows builds ship the GCC runtime (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`),
distributed under GPLv3 with the GCC Runtime Library Exception
([LICENSES/GCC-RUNTIME-LIBRARY-EXCEPTION.txt](LICENSES/GCC-RUNTIME-LIBRARY-EXCEPTION.txt)),
which permits linking into non-GPL applications without imposing a
source-distribution requirement on this project.

`libwinpthread-1.dll` is **not** under that exception: it is MIT-licensed with
parts under a BSD license, both of which require their notice to accompany the
binary — see [LICENSES/winpthreads-COPYING.txt](LICENSES/winpthreads-COPYING.txt).
The MinGW-w64 startup code linked into `SAM.exe` is covered by
[LICENSES/MinGW-w64-runtime-COPYING.txt](LICENSES/MinGW-w64-runtime-COPYING.txt).

## Installer

The Windows installer is built with Inno Setup, Copyright (C) 1997-2026 Jordan
Russell, Portions Copyright (C) 2000-2026 Martijn Laan (<https://jrsoftware.org>).
Its copyright notices in the setup program (e.g. its About box) are left in place,
as its license requires.

## FFmpeg

FFmpeg is **not bundled**, and no FFmpeg code or binary is distributed with this
project. It is the optional, advanced half of video export: the user points the app
at their own installation through the export dialog's "Locate FFmpeg" button, and
SAM then runs that executable as a **subprocess** — it links no FFmpeg
code and shares no address space with it, so FFmpeg's licence attaches to the user's
own executable alone and never to this project.

MP4 export does not involve FFmpeg at all (see "Windows Media Foundation" below), so
a SAM install with no FFmpeg anywhere on the system still exports video.
The wmv, avi and gif formats are FFmpeg's, and are disabled until one is located.

Should a build ever be shipped alongside the application, it would have to be an
LGPL one (an `ffmpeg-*-lgpl` package: no `libx264`, `libx265` or other GPL-only
component) and the release would then also owe FFmpeg's licence text, that build's
exact version and `configure` line, and access to its corresponding sources from
the same place as the installer. None of that applies as things stand.

## Windows Media Foundation

MP4 (H.264) export uses the encoder built into Windows, through Media Foundation
(`mfplat`, `mfreadwrite` — see `mfvideo.cpp`). It is an operating-system component:
nothing is bundled, and the AVC patent licence covering it is the one that came
with the user's copy of Windows. The import libraries linked against it
(`libmfplat.a`, `libmfreadwrite.a`, `libmfuuid.a`) are part of the MinGW-w64
runtime covered above.
