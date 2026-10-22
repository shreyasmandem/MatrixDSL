@echo off
title MatrixDSL Lab backend
rem Starts the MatrixDSL compiler backend and opens the Phase 3 page.
rem Keep this window open while presenting; close it to stop the backend.

set "ROOT=%~dp0."
set "SERVER=%ROOT%\build\bin\mtxserve.exe"

if not exist "%SERVER%" (
  echo Backend not built yet. Building it now - this takes a minute...
  cmake -S "%ROOT%" -B "%ROOT%\build" -DCMAKE_BUILD_TYPE=Release || goto :fail
  cmake --build "%ROOT%\build" -j || goto :fail
)

rem Stop a copy that is already running, so the port is free.
taskkill /im mtxserve.exe /f >nul 2>&1

start "" "%~dp0presentation\MatrixDSL-Phase3.html"
start "" "http://127.0.0.1:8765/"

"%SERVER%" --root "%ROOT%"
goto :eof

:fail
echo.
echo Build failed. Make sure CMake and a C++ compiler are installed.
pause
