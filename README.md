# WINLOCKER

---

## 🔧 Компиляция

### Linux

```sh
chmod +x build.sh
./build.sh
```

### Windows

```bat
build.bat
```

> Требуется [mingw-w64](https://www.mingw-w64.org/) в PATH.

---

## ✍️ Подпись exe

### Linux

```sh
chmod +x auto_exe_sign.sh
./auto_exe_sign.sh
```

### Windows

```bat
sign.bat
```

> Требуются `openssl` и `osslsigncode` в PATH.
>
> - openssl: https://slproweb.com/products/Win32OpenSSL.html
> - osslsigncode: https://github.com/mtrojnar/osslsigncode

После подписи получишь:

- `MAMP_signed.exe` — финальный файл для деплоя
- `MAMP_cert.cer` — сертификат для GPO

---

## 🖥️ Установка на машину

1. Скопируй `MAMP_signed.exe` на целевую машину в любую угодно вам папку.
2. Запусти **от имени администратора** — программа сама пропишется в автозапуск

---

## 🔓 Разблокировка

Введи пароль цифровыми кнопками на экране и нажми **"Подтвердить"**.

Пароль по умолчанию:

```
314159265
```

---

## 📁 Структура проекта

MAMP.cpp — исходный код
MAMP.rc — ресурсы (иконка, манифест)
app.manifest — UAC манифест (requireAdministrator)
icon1.ico — иконка
build.sh — сборка (Linux)
build.bat — сборка (Windows)
auto_exe_sign.sh — подпись (Linux)
sign.bat — подпись (Windows)
