; =====================================================================
; PrismCraft Inno Setup Script
; Free Inno Setup Compiler download: https://jrsoftware.org/isdl.php
; To compile: Right-click this file -> "Compile", or run:
;   iscc.exe installer\PrismCraft_Setup.iss
; =====================================================================

#define MyAppName "PrismCraft"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "PrismCraft Team"
#define MyAppExeName "PrismCraft.exe"
#define MyAppSourceDir "..\dist\PrismCraft"

[Setup]
; Basic Application Info
AppId={{C38A89A4-126F-4B1C-8A51-24B79C41890B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; Output Configuration
OutputDir=..\dist
OutputBaseFilename=PrismCraft_Setup_v{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

; Visual Details
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Copy all game files, assets, and bundled CRT DLLs from dist\PrismCraft
Source: "{#MyAppSourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Filename: "{app}\{#MyAppExeName}"; Flags: nowait postinstall skipifsilent
