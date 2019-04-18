@echo off
IF EXIST "D:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg.exe" set STEAMVR_BIN_PATH="D:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\"
IF EXIST "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg.exe" set STEAMVR_BIN_PATH="C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\"

set PWD=%cd%

%STEAMVR_BIN_PATH%\vrpathreg.exe removedriver %PWD%\soft_knuckles
%STEAMVR_BIN_PATH%\vrpathreg.exe
