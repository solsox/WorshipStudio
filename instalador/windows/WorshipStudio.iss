; WorshipStudio installer for Windows (Inno Setup 6).
; Build: iscc /DAppVersion=0.7.0 /DSourceDir=<installed folder> /DOutputDir=<dist> WorshipStudio.iss
; The language picked here is also the language WorshipStudio opens in.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\app\build_x64\WorshipStudio"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{6E7C3B1A-2F4D-4B8E-9C1A-57D2E0A9B3F4}
AppName=WorshipStudio
AppVersion={#AppVersion}
AppVerName=WorshipStudio {#AppVersion}
AppPublisher=WorshipStudio
AppPublisherURL=https://solsox.github.io/WorshipStudio/
AppSupportURL=mailto:contact.worshipstudio@gmail.com
DefaultDirName={autopf}\WorshipStudio
DefaultGroupName=WorshipStudio
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=WorshipStudio-Windows-x64-Instalador
SetupIconFile=..\..\app\frontend\cmake\windows\obs-studio.ico
UninstallDisplayIcon={app}\bin\64bit\WorshipStudio.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=yes
UsePreviousLanguage=no
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
#if FileExists(AddBackslash(CompilerPath) + "Languages\Korean.isl")
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
#endif

[CustomMessages]
spanish.LangCode=es-ES
english.LangCode=en-US
portuguese.LangCode=pt-BR
french.LangCode=fr-FR
german.LangCode=de-DE
italian.LangCode=it-IT
#if FileExists(AddBackslash(CompilerPath) + "Languages\Korean.isl")
korean.LangCode=ko-KR
#endif
spanish.Desktop=Crear un acceso directo en el escritorio
english.Desktop=Create a desktop shortcut
portuguese.Desktop=Criar um atalho na área de trabalho
french.Desktop=Créer un raccourci sur le bureau
german.Desktop=Verknüpfung auf dem Desktop erstellen
italian.Desktop=Crea un collegamento sul desktop
#if FileExists(AddBackslash(CompilerPath) + "Languages\Korean.isl")
korean.Desktop=바탕 화면에 바로 가기 만들기
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:Desktop}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\WorshipStudio"; Filename: "{app}\bin\64bit\WorshipStudio.exe"; WorkingDir: "{app}\bin\64bit"
Name: "{autodesktop}\WorshipStudio"; Filename: "{app}\bin\64bit\WorshipStudio.exe"; WorkingDir: "{app}\bin\64bit"; Tasks: desktopicon

[INI]
; WorshipStudio reads its interface language from here on the first start.
; Only on a first install: an existing configuration keeps its language.
Filename: "{userappdata}\WorshipStudio\user.ini"; Section: "General"; Key: "Language"; String: "{cm:LangCode}"; Check: IsFirstInstall

[Run]
Filename: "{app}\bin\64bit\WorshipStudio.exe"; WorkingDir: "{app}\bin\64bit"; Description: "{cm:LaunchProgram,WorshipStudio}"; Flags: nowait postinstall skipifsilent

[Code]
var
  FirstInstall: Boolean;

function InitializeSetup(): Boolean;
begin
  FirstInstall := not FileExists(ExpandConstant('{userappdata}\WorshipStudio\user.ini'));
  Result := True;
end;

function IsFirstInstall(): Boolean;
begin
  Result := FirstInstall;
end;
