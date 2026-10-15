@echo off
rem ===========================================================================
rem  build.bat - build with Visual Studio 2022 (MSVC 14.44, C++17)
rem ===========================================================================
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvarsall.bat not found
  exit /b 1
)

cd /d "%~dp0"

rem /std:c++17  /W4 high warning level  /O2 optimize  /EHsc standard exceptions
rem /utf-8 tells MSVC the source is UTF-8, so no BOM is needed any more
cl /nologo /std:c++17 /utf-8 /W4 /O2 /EHsc /Fe:sci_calc.exe scientific_calc.cpp
if errorlevel 1 (
  echo [FAILED] compile error
  exit /b 1
)

del /q scientific_calc.obj >nul 2>&1
echo [OK] sci_calc.exe built
endlocal
