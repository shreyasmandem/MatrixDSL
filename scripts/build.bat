@echo off
REM ===========================================================================
REM MatrixDSL - Build script (Windows)
REM
REM Usage:
REM   scripts\build.bat                full build (requires LLVM)
REM   scripts\build.bat --sim-only     MDT simulator only, no LLVM needed
REM   scripts\build.bat --release      optimized build
REM   scripts\build.bat --clean        remove the build directory first
REM ===========================================================================

setlocal enabledelayedexpansion

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build"
set "BUILD_TYPE=Debug"
set "CMAKE_EXTRA="

:parse
if "%~1"=="" goto configure
if /I "%~1"=="--sim-only"      set "CMAKE_EXTRA=!CMAKE_EXTRA! -DMATRIXDSL_BUILD_SIM_ONLY=ON"
if /I "%~1"=="--frontend-only" set "CMAKE_EXTRA=!CMAKE_EXTRA! -DMATRIXDSL_BUILD_FRONTEND_ONLY=ON"
if /I "%~1"=="--benchmarks"    set "CMAKE_EXTRA=!CMAKE_EXTRA! -DMATRIXDSL_BUILD_BENCHMARKS=ON"
if /I "%~1"=="--release"       set "BUILD_TYPE=Release"
if /I "%~1"=="--clean"         if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
shift
goto parse

:configure
echo MatrixDSL build
echo   root : %ROOT%
echo   type : %BUILD_TYPE%

if defined LLVM_DIR (
  echo   llvm : %LLVM_DIR%
  set "CMAKE_EXTRA=!CMAKE_EXTRA! -DLLVM_DIR=%LLVM_DIR%"
)

cmake -S "%ROOT%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% !CMAKE_EXTRA!
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE%
if errorlevel 1 exit /b 1

echo.
echo Build complete. Binaries in %BUILD_DIR%\bin
endlocal
