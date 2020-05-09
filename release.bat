set RELEASEDIR=Z80Explorer
set VCINSTALLDIR="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC"

set path=C:\Qt\5.14.1\msvc2015_64\bin;%VCINSTALLDIR%;%path%

call vcvarsall.bat

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

mkdir %RELEASEDIR%
mkdir %RELEASEDIR%\platforms
mkdir %RELEASEDIR%\styles
mkdir %RELEASEDIR%\resource

copy "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\vccorlib140.dll  %RELEASEDIR%
copy "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\vcruntime140.dll %RELEASEDIR%
copy "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\msvcp140.dll     %RELEASEDIR%
rem copy vcredist_x64.exe %RELEASEDIR%

copy Z80Explorer.exe  %RELEASEDIR%
copy highDPI.bat      %RELEASEDIR%
copy Qt5Core.dll      %RELEASEDIR%
copy Qt5Gui.dll       %RELEASEDIR%
copy Qt5Script.dll    %RELEASEDIR%
copy Qt5Widgets.dll   %RELEASEDIR%
copy platforms\qwindows.dll %RELEASEDIR%\platforms
copy styles\qwindowsvistastyle.dll %RELEASEDIR%\styles
copy resource\*       %RELEASEDIR%\resource

:end
