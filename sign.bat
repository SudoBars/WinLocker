@echo off
chcp 65001 >nul
setlocal

set COMPANY=YourCompany
set PASSWORD=mamp_corp_2026
set EXE=MAMP.exe
set SIGNED_EXE=MAMP_signed.exe
set CERT_PFX=MAMP_cert.pfx
set CERT_PEM=MAMP_cert.pem
set CERT_KEY=MAMP_key.pem
set CERT_CER=MAMP_cert.cer

echo === MAMP Signer ===

where openssl >nul 2>&1
if errorlevel 1 (
    echo [!] openssl not found. Install from https://slproweb.com/products/Win32OpenSSL.html
    pause
    exit /b 1
)

where osslsigncode >nul 2>&1
if errorlevel 1 (
    echo [!] osslsigncode not found. Install from https://github.com/mtrojnar/osslsigncode
    pause
    exit /b 1
)

if not exist "%EXE%" (
    echo [X] %EXE% not found in %CD%
    pause
    exit /b 1
)

if not exist "%CERT_PFX%" (
    echo [*] Creating certificate...
    openssl genrsa -out %CERT_KEY% 4096
    openssl req -new -x509 -key %CERT_KEY% -out %CERT_PEM% ^
        -days 1825 ^
        -subj "/C=UA/O=%COMPANY%/CN=MAMP Corp"
    openssl pkcs12 -export ^
        -out %CERT_PFX% ^
        -inkey %CERT_KEY% ^
        -in %CERT_PEM% ^
        -passout pass:%PASSWORD%
    openssl x509 -in %CERT_PEM% -out %CERT_CER% -outform DER
    echo [+] Certificate created.
) else (
    echo [+] Certificate already exists, skipping...
)

echo [*] Signing %EXE%...
osslsigncode sign ^
    -pkcs12 %CERT_PFX% ^
    -pass %PASSWORD% ^
    -t http://timestamp.digicert.com ^
    -in %EXE% ^
    -out %SIGNED_EXE%

if errorlevel 1 (
    echo [X] Signing failed.
    pause
    exit /b 1
)

echo.
echo === Done ===
echo Signed file: %SIGNED_EXE%
echo Certificate for GPO: %CERT_CER%
echo.
echo Next step - add %CERT_CER% to GPO on Windows:
echo   Group Policy -^> Computer Configuration -^>
echo   Windows Settings -^> Security Settings -^>
echo   Public Key Policies -^> Trusted Publishers -^> Import
pause
