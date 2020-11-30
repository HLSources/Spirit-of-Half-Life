echo off
set CODEDIR="C:\Program Files (x86)\Steam\SteamApps\XXXXX\half-life\Spirit18\_dev\SpiritSource18a1_VS2010_compatible"
set RELEASEDIR="C:\Program Files (x86)\Steam\SteamApps\XXXXX\half-life\Spirit18"

echo on

copy %CODEDIR%\dlls\debughl\spirit.dll %RELEASEDIR%\dlls
copy %CODEDIR%\cl_dll\Debug\client.dll %RELEASEDIR%\cl_dlls
pause
..\..\..\hl -dev -console -num_edicts 4096 -game Spirit18