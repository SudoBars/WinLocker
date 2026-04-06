#!/bin/bash

set -e

COMPILER="x86_64-w64-mingw32-g++"
SOURCE="MAMP.cpp"
OUTPUT="MAMP.exe"

echo "[*] Проверка компилятора..."

if ! command -v "$COMPILER" &>/dev/null; then
    echo "[!] Компилятор не найден. Установка mingw-w64..."

    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y mingw-w64
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm mingw-w64-gcc
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y mingw64-gcc-c++
    elif command -v zypper &>/dev/null; then
        sudo zypper install -y cross-x86_64-w64-mingw32-gcc-c++
    else
        echo "[X] Неизвестный пакетный менеджер. Установи mingw-w64 вручную."
        exit 1
    fi
else
    echo "[+] Компилятор найден: $(which $COMPILER)"
fi

# Проверка наличия исходника
if [ ! -f "$SOURCE" ]; then
    echo "[X] Файл $SOURCE не найден в $(pwd)"
    exit 1
fi

echo "[*] Компиляция $SOURCE -> $OUTPUT ..."

$COMPILER "$SOURCE" \
    -lshell32 -mwindows -O2 \
    -static-libgcc -static-libstdc++ \
    -fexec-charset=CP1251 \
    -finput-charset=UTF-8 \
    -o "$OUTPUT"

echo "[+] Готово: $(pwd)/$OUTPUT"
echo "[+] Размер: $(du -sh $OUTPUT | cut -f1)"