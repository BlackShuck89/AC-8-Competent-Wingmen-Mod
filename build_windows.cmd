@echo off
setlocal
where cmake >nul 2>nul
if errorlevel 1 (
  echo Install CMake and Visual Studio 2022 Desktop development with C++ first.
  exit /b 1
)
cmake -S "%~dp0" -B "%~dp0build" -A x64
if errorlevel 1 exit /b 1
cmake --build "%~dp0build" --config Release
if errorlevel 1 exit /b 1
ctest --test-dir "%~dp0build" -C Release --output-on-failure
if errorlevel 1 exit /b 1
echo Native library: build\Release\AC8RoleWingmen.dll
