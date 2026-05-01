@echo off
@REM Builds and packages a Windows release into .\release\ using the same CMake rules as the GitHub Actions workflow.
@REM Run from "Developer command prompt for VS 2022".
@REM Usage:   release.bat <Qt-path>
@REM Example: release.bat C:\Qt\6.10.3\msvc2022_64

if "%~1"=="" (
    echo Usage: release.bat ^<Qt-path^>
    echo Example: release.bat C:\Qt\6.10.3\msvc2022_64
    exit /b 1
)
if not exist "%~1\bin\windeployqt.exe" (
    echo ERROR: windeployqt.exe not found in %~1\bin
    exit /b 1
)

set "PATH=%~1\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%~1"

cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build --config Release --parallel || exit /b 1
cmake --install build --config Release --prefix "%CD%\release" || exit /b 1

echo.
echo DONE - packaged build is in .\release\
