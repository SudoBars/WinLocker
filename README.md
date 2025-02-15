# MAMP (Windows Locker)

MAMP - это программа-блокировщик Windows, которая:
- Блокирует экран пользователя.
- Отключает `Task Manager` и предотвращает его открытие.
- Автоматически перезапускается, если её закрывают.
- Запускается при старте Windows через реестр, автозагрузку и `Task Scheduler`.
- Блокирует клавиатуру, но позволяет пользоваться мышью.
- Показывает баннер перед вводом пароля.

## 📌 Функционал
✅ Полностью скрывает рабочий стол, создавая полноэкранное окно.  
✅ Блокирует `Task Manager`, `Process Hacker`, `procexp64.exe` и любые попытки их запустить.  
✅ Самоперезапускается через 3 секунды после закрытия.  
✅ Работает как системный процесс с привилегиями `Administrator`.  
✅ Добавляет себя в **автозапуск Windows** через:
  - **Реестр** (`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`)
  - **Папку `Startup`**
  - **Task Scheduler** (с `RL HIGHEST` для обхода антивирусов)  
✅ Позволяет ввести **пароль для разблокировки**.  

---

## 🔧 **Настройка и установка**
### **1️⃣ Компиляция**
Для компиляции программы в `MinGW` используйте:
```sh
g++ MAMP.cpp -o MAMP.exe -mwindows -static
```
Если выдаёт ошибку `Permission denied`:
```sh
taskkill /F /IM MAMP.exe
del /F /Q MAMP.exe
g++ MAMP.cpp -o MAMP.exe -mwindows -static
```
Или **запустите терминал от имени администратора**.

### **2️⃣ Запуск**
После компиляции запустите `MAMP.exe`.  
Программа **автоматически добавится в автозапуск Windows**.

### **3️⃣ Разблокировка**
Введите пароль в окне ввода и нажмите `"Не загоняй себя"`.  
По умолчанию **пароль**:
```
314159265
```
---

## 🔄 **Автозапуск**
MAMP добавляет себя в автозапуск сразу **тремя способами**:

1️⃣ **Реестр** (`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`)  
2️⃣ **Ярлык в папке "Автозагрузка"** (`C:\Users\Имя_Пользователя\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup`)  
3️⃣ **Task Scheduler**  
   ```sh
   schtasks /Create /SC ONLOGON /TN "MAMP AutoStart" /TR "C:\путь_к_MAMP.exe" /RL HIGHEST /F
   ```
   Это позволяет программе запускаться **даже если её удалят из реестра или папки Startup**.

---

## 🛑 **Как удалить?**
1️⃣ Открыть **Windows PowerShell от имени администратора**  
2️⃣ Ввести команду:
```sh
taskkill /F /IM MAMP.exe
schtasks /Delete /TN "MAMP AutoStart" /F
REG DELETE "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run" /V "MAMP" /F
del "C:\Users\%USERNAME%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\MAMP.lnk"
del /F /Q MAMP.exe
```
3️⃣ Перезагрузить компьютер.

---

## ⚠ **Примечания**
- **MAMP НЕ является вирусом**, но антивирусы могут его блокировать.
- Работает **только в Windows 7/8/10/11**.
- Требует **администраторские права** для работы.
- После установки **удаление возможно только вручную** или **через разблокировку** (см. раздел 🛑 Удаление).

Программа предназначена **исключительно в образовательных целях**.