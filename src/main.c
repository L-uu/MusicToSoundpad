#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>

DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (wcscmp(processEntry.szExeFile, processName) == 0) {
                    pid = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId = *(DWORD*)lParam;
    DWORD currentProcessId;
    GetWindowThreadProcessId(hwnd, &currentProcessId);
    if (currentProcessId == processId) {
        wchar_t windowTitle[256];
        if (GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0])) > 0) {
            if (wcscmp(windowTitle, L"Spotify Premium") != 0) {
                wprintf(L"MainWindowTitle: %s\n", windowTitle);

                // If the title changes, send a pause command to the process
                HWND hwndSpotify = FindWindowW(NULL, windowTitle);
                if (hwndSpotify != NULL) {
                    SendMessageW(hwndSpotify, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_MEDIA_PAUSE));
                }
            }
            return FALSE; // Stop enumerating windows
        }
    }
    return TRUE; // Continue enumerating windows
}

void GetMainWindowTitle(DWORD processId) {
    EnumWindows(EnumWindowsProc, (LPARAM)&processId);
}

int main() {
    const wchar_t* processName = L"Spotify.exe";
    DWORD pid = GetProcessIdByName(processName);
    if (pid != 0) {
        wprintf(L"Process ID of %s: %lu\n", processName, pid);
    } else {
        wprintf(L"Process %s is not running.\n", processName);
        return 1; // Exit if process is not found
    }

    // Monitor MainWindowTitle for changes
    while (1) {
        if (GetProcessIdByName(processName) == 0) {
            wprintf(L"Process %s has been closed.\n", processName);
            break;
        }
        GetMainWindowTitle(pid);
        Sleep(1000); // Sleep for 1 second before checking again
    }

    return 0;
}