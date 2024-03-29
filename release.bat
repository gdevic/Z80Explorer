@REM Relase script specific to Visual Studio 2019 and Qt 5.15.2
@REM Z80Explorer.exe must be precompiled and stored in the root project folder
if not exist "Z80Explorer.exe" Goto end

@REM call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

set path=C:\Qt\5.15.2\msvc2019_64\bin;%VCINSTALLDIR%;%path%

mkdir release
cd release
xcopy /Y ..\Z80Explorer.exe .
xcopy /Y ..\highDPI.bat .
xcopy /Y ..\cleanup.reg .
xcopy /Y ..\Z80Explorer.pdf .

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vccorlib140.dll    .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140.dll   .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140_1.dll .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140.dll       .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_1.dll     .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_2.dll     .
REM xcopy /Y "%VCToolsRedistDir%"\vcredist_x64.exe %RELEASEDIR%

mkdir resource
xcopy /Y /S ..\resource resource

rmdir /S /Q bearer iconengines imageformats translations

cd ..

@echo.
@echo DONE
:end
