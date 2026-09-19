@echo off
REM Wrapper untuk create-shortcut.ps1 — biar bisa di-double-click.
REM
REM Pemakaian:
REM   create-shortcut.bat                     (auto-detect frans.exe)
REM   create-shortcut.bat "C:\path\frans.exe" (path eksplisit)

setlocal

set "SCRIPT_DIR=%~dp0"
set "PS1=%SCRIPT_DIR%create-shortcut.ps1"

if not exist "%PS1%" (
    echo [frans] create-shortcut.ps1 tidak ditemukan di: %SCRIPT_DIR%
    exit /b 1
)

if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -TargetPath "%~1"
)

set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%