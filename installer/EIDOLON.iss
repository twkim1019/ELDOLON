; EIDOLON - MIDI Universe Visualizer - Windows installer (Inno Setup 6)
; Build:  iscc installer\EIDOLON.iss /DPluginDir=<path to EIDOLON.vst3 folder>
; Output: dist\EIDOLON-1.0.0-Setup.exe

#define AppName "EIDOLON"
#define AppVersion "1.0.0"
#define AppPublisher "EIDOLON Lab"
#ifndef PluginDir
  #define PluginDir "..\build-win\Eidolon_artefacts\Release\VST3\EIDOLON.vst3"
#endif

[Setup]
AppId={{4E1D0A7C-3B6F-4C2E-9A58-6D0F1E2B7C93}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
; uninstaller + documentation live here; the plugin itself goes to the shared VST3 folder
DefaultDirName={autopf}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=EIDOLON-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName} {#AppVersion} (VST3)
CloseApplications=yes

[Messages]
FinishedLabel=EIDOLON has been installed to:%n%nC:\Program Files\Common Files\VST3\EIDOLON.vst3%n%nIn FL Studio: Options > Manage plugins > Find installed plugins, then add EIDOLON from the Generators (instrument) list to the Channel Rack and play the Piano Roll or your controller.

[InstallDelete]
; remove an older copy first so no stale files remain inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\EIDOLON.vst3"

[Files]
Source: "{#PluginDir}\*"; DestDir: "{commoncf64}\VST3\EIDOLON.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Docs\FL_STUDIO_ROUTING.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Docs\USER_GUIDE.md"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\EIDOLON.vst3"
