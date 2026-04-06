#!/bin/bash

set -e

COMPANY="YourCompany"
PASSWORD="mamp_corp_2026"
EXE="MAMP.exe"
SIGNED_EXE="MAMP_signed.exe"

echo "=== MAMP Installer ==="

for cmd in openssl osslsigncode; do
    if ! command -v $cmd &>/dev/null; then
        echo "Устанавливаем $cmd..."
        sudo apt install -y $cmd
    fi
done

if [ ! -f "$EXE" ]; then
    echo "Ошибка: $EXE не найден в текущей папке"
    exit 1
fi

if [ ! -f "MAMP_cert.pfx" ]; then
    echo "Создаём сертификат..."
    openssl genrsa -out MAMP_key.pem 4096
    openssl req -new -x509 -key MAMP_key.pem -out MAMP_cert.pem \
        -days 1825 \
        -subj "/C=UA/O=$COMPANY/CN=MAMP Corp"
    openssl pkcs12 -export \
        -out MAMP_cert.pfx \
        -inkey MAMP_key.pem \
        -in MAMP_cert.pem \
        -passout pass:$PASSWORD
    openssl x509 -in MAMP_cert.pem -out MAMP_cert.cer -outform DER
    echo "Сертификат создан"
else
    echo "Сертификат уже существует, пропускаем..."
fi

echo "Подписываем $EXE..."
osslsigncode sign \
    -pkcs12 MAMP_cert.pfx \
    -pass $PASSWORD \
    -t http://timestamp.digicert.com \
    -in $EXE \
    -out $SIGNED_EXE

echo ""
echo "=== Готово ==="
echo "Подписанный файл: $SIGNED_EXE"
echo "Сертификат для GPO: MAMP_cert.cer"
echo ""
echo "Следующий шаг — добавить MAMP_cert.cer в GPO на Windows:"
echo "  Group Policy → Computer Configuration →"
echo "  Windows Settings → Security Settings →"
echo "  Public Key Policies → Trusted Publishers → Import"
