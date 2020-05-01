set path=%path%;C:\Qt\5.14.1\msvc2015_64\bin

windeployqt.exe Z80Explorer.exe
if errorlevel 1 goto end

mkdir RELEASE
mkdir RELEASE\platforms
mkdir RELEASE\styles
mkdir RELEASE\resource

copy Z80Explorer.exe RELEASE
copy Qt5Core.dll     RELEASE
copy Qt5Gui.dll      RELEASE
copy Qt5Script.dll   RELEASE
copy Qt5Widgets.dll  RELEASE
copy platforms\qwindows.dll RELEASE\platforms
copy styles\qwindowsvistastyle.dll RELEASE\styles

:end
pause
