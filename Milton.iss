; -- Milton.iss --


[Setup]
AppName=Milton
AppVersion=1.1.1
DefaultDirName={pf}\Milton
DefaultGroupName=Milton
UninstallDisplayIcon={app}\Milton.exe
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=MiltonSetup

[Files]
Source: "Milton.exe"; DestDir: "{app}"
Source: "Carlito.ttf"; DestDir: "{app}"
Source: "Carlito.LICENSE"; DestDir: "{app}"

[Icons]
Name: "{group}\Milton"; Filename: "{app}\Milton.exe"
