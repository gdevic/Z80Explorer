set RELEASEDIR=Z80Explorer
set VCINSTALLDIR="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC"

set path=C:\Qt\5.14.2\msvc2015_64\bin;%VCINSTALLDIR%;%path%

call vcvarsall.bat

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

mkdir %RELEASEDIR%
mkdir %RELEASEDIR%\platforms
mkdir %RELEASEDIR%\styles
mkdir %RELEASEDIR%\resource

xcopy /Y "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\vccorlib140.dll  %RELEASEDIR%
xcopy /Y "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\vcruntime140.dll %RELEASEDIR%
xcopy /Y "%VCINSTALLDIR%"\redist\x64\Microsoft.VC140.CRT\msvcp140.dll     %RELEASEDIR%
rem copy vcredist_x64.exe %RELEASEDIR%

xcopy /Y Z80Explorer.exe  %RELEASEDIR%
xcopy /Y highDPI.bat      %RELEASEDIR%
xcopy /Y cleanup.reg      %RELEASEDIR%
xcopy /Y Qt5Core.dll      %RELEASEDIR%
xcopy /Y Qt5Gui.dll       %RELEASEDIR%
xcopy /Y Qt5Script.dll    %RELEASEDIR%
xcopy /Y Qt5Widgets.dll   %RELEASEDIR%
xcopy /Y platforms\qwindows.dll %RELEASEDIR%\platforms
xcopy /Y styles\qwindowsvistastyle.dll %RELEASEDIR%\styles

rem Assumes Z80Explorer_Z80 resource files are at the same level as this project
xcopy /Y /E ..\Z80Explorer_Z80\*  %RELEASEDIR%\resource

:end
