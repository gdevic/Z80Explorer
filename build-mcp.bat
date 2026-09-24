@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\6.11.2\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%
echo === env done ===
cmake --version
echo === configuring ===
cmake -B build-mcp -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\msvc2022_64 -DCMAKE_BUILD_TYPE=Release .
echo === building ===
cmake --build build-mcp 2>&1
