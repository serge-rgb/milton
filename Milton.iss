; -- Milton.iss --


[Setup]
AppName=Milton
AppVersion=1.9.0
DefaultDirName={pf}\Milton
DefaultGroupName=Milton
;UninstallDisplayIcon={app}\Milton.exe
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=MiltonSetup_1.9.0_x64
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
ChangesAssociations=yes

[Files]
Source: "Milton.exe"; DestDir: "{app}"
Source: "Carlito.ttf"; DestDir: "{app}"
Source: "Carlito.LICENSE"; DestDir: "{app}"
Source: "LICENSE.txt"; DestDir: "{app}"
Source: "milton_icon.ico"; DestDir: "{app}"

[Icons]
Name: "{group}\Milton"; Filename: "{app}\Milton.exe"

[Registry]
Root: HKCR; Subkey: ".mlt";                             ValueData: "Milton";          Flags: uninsdeletevalue; ValueType: string;  ValueName: ""
Root: HKCR; Subkey: "Milton";                     ValueData: "Program Milton";  Flags: uninsdeletekey;   ValueType: string;  ValueName: ""
Root: HKCR; Subkey: "Milton\DefaultIcon";             ValueData: "{app}\Milton.exe,0";               ValueType: string;  ValueName: ""
Root: HKCR; Subkey: "Milton\shell\open\command";  ValueData: """{app}\Milton.exe"" ""%1""";  ValueType: string;  ValueName: ""
