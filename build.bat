@echo off
chcp 65001 >nul
setlocal

set COMPILER=x86_64-w64-mingw32-g++
set SOURCE=MAMP.cpp
set OUTPUT=MAMP.exe

echo [*] Checking compiler...
where %COMPILER% >nul 2>&1
if errorlevel 1 (
    echo [!] Compiler not found. Install mingw-w64 and add it to PATH.
    echo     Download: https://www.mingw-w64.org/
    pause
    exit /b 1
)
echo [+] Compiler found.

if not exist "%SOURCE%" (
    echo [X] %SOURCE% not found in %CD%
    pause
    exit /b 1
)

echo [*] Compiling %SOURCE% -> %OUTPUT% ...

%COMPILER% "%SOURCE%" MAMP_res.o ^
    -ladvapi32 -luser32 -lshell32 -mwindows -O2 ^
    -static-libgcc -static-libstdc++ ^
    -o "%OUTPUT%"

if errorlevel 1 (
    echo [X] Compilation failed.
    pause
    exit /b 1
)

echo [+] Done: %CD%\%OUTPUT%
pause
