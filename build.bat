@echo off
rem Build Lida (ThemeSwitcher) - requires Visual Studio 2022 Build Tools or VS with C++ workload.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
rc /nologo /fo resource.res resource.rc || exit /b 1
cl /nologo /std:c++17 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /utf-8 src\main.cpp resource.res ^
   user32.lib gdi32.lib shell32.lib advapi32.lib comdlg32.lib ole32.lib ^
   /link /SUBSYSTEM:WINDOWS /OUT:Lida.exe || exit /b 1
echo Built Lida.exe
