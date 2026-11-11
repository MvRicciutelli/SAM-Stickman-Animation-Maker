## Release installer

`installer/build-installer.ps1` builds Release in a sibling `build-SAM-installer`,
stages `installer/out/app` (exe + `windeployqt` + LICENSE/NOTICE/LICENSES; `-SkipInstaller`
stops there, a portable folder) and compiles `installer/SAM.iss` with Inno Setup 6.
Version = `VERSION` in the `.pro`, the one source of truth. qmake writes it into `version.h`
in the build directory (`QMAKE_SUBSTITUTES`, template `src/version.h.in`) as `APP_VERSION`,
which the window title, the About box and the update check read. Keep it a generated header,
never a `-D` define: make does not track compiler flags, so after a bump a define left
`updatecheck.o` on the old version and the app offered an update to the version it already
was. The template is parsed as qmake syntax: quotes stay escaped, and no apostrophes.

**Update check** (`updatecheck.h/.cpp`, called from `MyMainWindow`'s constructor): asks
GitHub's `releases/latest` in the background, and if its tag is newer (`IsNewerRelease()`:
numeric, optional `v`, trailing zeros ignored) offers to open the release page in the browser.
No download, no self-install, on purpose. Silent on every failure. **The releases must be
public**: the app has no GitHub login, and a token in the exe is what got the previous update
check removed (`version.exe` shipped MySQL credentials). While the repo is private GitHub answers
404 and the check never prompts. So a release is: bump `VERSION`, build the installer, publish a
GitHub release tagged `v<VERSION>` with the installer attached **as `SAM-Setup.exe`** (the
fixed-name copy `build-installer.ps1` writes next to the versioned one): README's Download button
links to `releases/latest/download/SAM-Setup.exe`, so any other asset name breaks it. The install is **per-user on purpose** (`PrivilegesRequired=lowest`): the
app extracts `image2Avi.exe` next to its own exe at run time, which fails under Program Files.
Bundled content is whatever sits in `installer/content/`, seeded into
`Documents\SAM\` with `onlyifdoesntexist` — a user's existing `defaultLib.samlib`
is never replaced, so new stock figures for existing users belong in a separately named
`.samlib`. FFmpeg is never bundled.

**The license is accepted once, on the installer's license page** (`LicenseFile=` in
`SAM.iss`). The app has no first-run prompt any more, and Help ▸ Terms and Conditions shows
the compiled-in LICENSE read-only (the "SAM license" tab of the notices dialog). Use is
allowed for any purpose, commercial included: that is additional permission 1 in LICENSE,
layered on PolyForm Strict, whose own text stays verbatim. Keep the README, NOTICE.md and
the About box summaries in step with it.

**Bundled brushes and patterns are third-party CC0 packs, and they stay outside SAM's license.**
The packs are Kenney Pattern Pack, n4's Seamless Pattern Pack, Popple (`.abr`), David Revoy
2023-01, Nylnook v1.1 and v2 Generic Pen. The last three are `.mbrush` files that the app's own
`ImportKritaBundle()` produced from the original bundles. LICENSE's preamble, NOTICE.md and the
credits files next to the packs all say that SAM's license does not cover them. Two licensing
facts drive the choices:
- Some Revoy/Nylnook tips are byte-identical to Krita's default tips, which Krita's data README
  puts under CC BY 3.0. They are credited that way, and CC BY forbids adding restrictions, which
  is exactly why the packs must not fall under PolyForm.
- Nylnook's "Airbrush" was deleted from the bundled v1.1 because its `cazu-spray.gbr` is Pablo
  Cazorla's, with no verifiable license.

Add a pack only with its license verified, and with a row in NOTICE.md plus a license file
beside it.

**Qt's LGPL does not cover the code Qt bundles.** zlib, libpng, FreeType, HarfBuzz, PCRE2,
libjpeg-turbo and the rest keep their authors' licenses, and most of those require a notice
with every binary copy. That is why `LICENSES/Qt-third-party-notices.txt` exists: it was
generated from `C:\Qt\Docs\Qt-6.6.1\qt*\*-attribution-*.html`. NOTICE.md's "Third-party code
inside the Qt libraries" section carries the sentences FreeType's FTL and the IJG license want
in the documentation, a license → component table, and the MPL-2.0 source pointer for the Public
Suffix List inside `Qt6Network`. `libwinpthread` is MIT/BSD, not under the GCC exception, so its
COPYING ships too.

These notices ship in three places:
- **The app:** NOTICE.md, LICENSE and every `LICENSES/*.txt` are compiled in (`myres.qrc`).
  Help ▸ Third-party notices shows them, one tab per text, and a NOTICE.md link to a file
  switches to its tab.
- **The install folder:** `build-installer.ps1` copies LICENSE, NOTICE.md and `LICENSES\` next
  to the exe. It **refuses to build** if any required notice file is missing, or if `-QtDir` is
  not 6.6.1, since the notices file and kritabundle's private-API use are both tied to that
  build. On a Qt upgrade, regenerate the notices and update the table.
- **The documents folder:** `SAM.iss` installs content `*.txt` (credits and license files) with
  plain `ignoreversion` rather than `onlyifdoesntexist`, so they always describe the packs as
  shipped.

