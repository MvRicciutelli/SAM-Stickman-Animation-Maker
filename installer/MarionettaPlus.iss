; Compiled by build-installer.ps1, which passes AppVersion, AppDir, ContentDir and IconFile.
#ifndef AppVersion
  #error Run installer\build-installer.ps1 instead of compiling this script directly.
#endif

[Setup]
AppId={{5BA9927F-8417-42FA-A571-BE9AB8CF819F}
AppName=MarionettaPlus
AppVersion={#AppVersion}
AppPublisher=Matteo Vittorio Ricciutelli
AppPublisherURL=https://sites.google.com/view/marionetta/home
AppCopyright=Copyright (C) 2026 Matteo Vittorio Ricciutelli
; Per-user install ({autopf} = %LOCALAPPDATA%\Programs), deliberately: the app extracts
; image2Avi.exe from its resources NEXT TO ITSELF at run time (MyMainWindow::ExportAvi()),
; which fails under Program Files.
; It also makes {userdocs} the documents folder of the user actually installing.
PrivilegesRequired=lowest
DefaultDirName={autopf}\MarionettaPlus
DisableProgramGroupPage=yes
; Qt 6 needs Windows 10 and a 64-bit OS
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#AppDir}\LICENSE
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\MarionettaPlus.exe
OutputBaseFilename=MarionettaPlus-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Bundled brushes/patterns/libraries/stick figures go where Widget::CreatePaths() looks.
; They become the user's documents: never overwrite one they already have (they may have
; edited it, e.g. "My brushes.mbrush"), never remove them on uninstall.
Source: "{#ContentDir}\*"; DestDir: "{userdocs}\MarionettaPlus"; Excludes: ".gitkeep"; \
    Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\MarionettaPlus"; Filename: "{app}\MarionettaPlus.exe"
Name: "{autodesktop}\MarionettaPlus"; Filename: "{app}\MarionettaPlus.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MarionettaPlus.exe"; Description: "{cm:LaunchProgram,MarionettaPlus}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; extracted by the app at run time, so the uninstaller doesn't know about it
Type: files; Name: "{app}\image2Avi.exe"
