; Compiled by build-installer.ps1, which passes AppVersion, AppDir, ContentDir and IconFile.
#ifndef AppVersion
  #error Run installer\build-installer.ps1 instead of compiling this script directly.
#endif

[Setup]
AppId={{5BA9927F-8417-42FA-A571-BE9AB8CF819F}
AppName=SAM - Stickman Animation Maker
AppVersion={#AppVersion}
AppPublisher=Matteo Vittorio Ricciutelli
AppCopyright=Copyright (C) 2026 Matteo Vittorio Ricciutelli
; Per-user install ({autopf} = %LOCALAPPDATA%\Programs), deliberately: the app extracts
; image2Avi.exe from its resources NEXT TO ITSELF at run time (MyMainWindow::ExportAvi()),
; which fails under Program Files.
; It also makes {userdocs} the documents folder of the user actually installing.
PrivilegesRequired=lowest
DefaultDirName={autopf}\SAM
DisableProgramGroupPage=yes
; Qt 6 needs Windows 10 and a 64-bit OS
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#AppDir}\LICENSE
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\SAM.exe
OutputBaseFilename=SAM-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; AppDir includes LICENSE, NOTICE.md and LICENSES\: the license texts and notices the bundled
; libraries require to accompany the binaries (see NOTICE.md) - they must stay in this copy.
Source: "{#AppDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Bundled brushes/patterns/libraries/stick figures go where Widget::CreatePaths() looks.
; They become the user's documents: never overwrite one they already have (they may have
; edited it, e.g. "My brushes.mbrush"), never remove them on uninstall.
Source: "{#ContentDir}\*"; DestDir: "{userdocs}\SAM"; Excludes: ".gitkeep,*.txt"; \
    Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall skipifsourcedoesntexist
; ...except their credits and license files, which are always brought up to date: they must
; describe the packs as shipped, and nobody edits them.
Source: "{#ContentDir}\*.txt"; DestDir: "{userdocs}\SAM"; \
    Flags: recursesubdirs createallsubdirs ignoreversion uninsneveruninstall skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\SAM - Stickman Animation Maker"; Filename: "{app}\SAM.exe"
Name: "{autodesktop}\SAM - Stickman Animation Maker"; Filename: "{app}\SAM.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\SAM.exe"; Description: "{cm:LaunchProgram,SAM - Stickman Animation Maker}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; extracted by the app at run time, so the uninstaller doesn't know about it
Type: files; Name: "{app}\image2Avi.exe"
