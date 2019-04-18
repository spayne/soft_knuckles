@echo off
IF EXIST "D:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg.exe" set STEAMVR_BIN_PATH="D:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\"
IF EXIST "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg.exe" set STEAMVR_BIN_PATH="C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\"

set PWD=%cd%

copy %PWD%\x64\Debug\soft_knuckles.dll %PWD%\soft_knuckles\bin\win64\driver_soft_knuckles.dll
%STEAMVR_BIN_PATH%\vrpathreg.exe adddriver %PWD%\soft_knuckles
%STEAMVR_BIN_PATH%\vrpathreg.exe 

