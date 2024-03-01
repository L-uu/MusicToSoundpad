#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include <Urlmon.h>
#include "md5.h"
#include "pipes.h"

#pragma comment(lib, "urlmon.lib")

void CheckAndCreateDirectory(const wchar_t* directoryName) {
    // Check if the directory exists
    DWORD fileAttributes = GetFileAttributesW(directoryName);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        // Directory doesn't exist, create it
        if (!CreateDirectoryW(directoryName, NULL)) {
            wprintf(L"Failed to create directory: %s\n", directoryName);
        }
    } else if (!(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        wprintf(L"A file with the name %s already exists. Cannot create directory.\n", directoryName);
    }
}

BOOL DownloadFile(const wchar_t* url, const wchar_t* filePath) {
    HRESULT hr = URLDownloadToFileW(NULL, url, filePath, 0, NULL);
    if (hr == S_OK) {
        wprintf(L"Download successful.\n");
        return TRUE;
    } else {
        wprintf(L"Failed to download file. Error code: %08lx\n", hr);
        return FALSE;
    }
}

BOOL CheckAndDownloadYTDLP(const wchar_t* utilsFolderPath) {
    const wchar_t* ytDlpFileName = L"yt-dlp.exe";
    const wchar_t* ytDlpUrl = L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
    wchar_t ytDlpFilePath[MAX_PATH];
    swprintf_s(ytDlpFilePath, MAX_PATH, L"%s\\%s", utilsFolderPath, ytDlpFileName);

    // Check if yt-dlp.exe exists in the utils folder
    DWORD fileAttributes = GetFileAttributesW(ytDlpFilePath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        // File doesn't exist, download it
        wprintf(L"Downloading yt-dlp.exe...\n");
        return DownloadFile(ytDlpUrl, ytDlpFilePath);
    } else {
        wprintf(L"yt-dlp.exe already exists in the utils folder. Checking for updates...\n");
        if (UpdateYTDLP() == 2)
        {
            wprintf(L"Updating yt-dlp.exe...\n");
            return DownloadFile(ytDlpUrl, ytDlpFilePath);
        }
        return TRUE;
    }
}

DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD pid = 0; // Will store the process ID once found
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // Create a snapshot of the currently running processes
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry; // We need to store information about each process
        processEntry.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (wcscmp(processEntry.szExeFile, processName) == 0) { // Simple string comparison, check if the process name is what we're looking for
                    pid = processEntry.th32ProcessID; // Store the ID of the process
                    break; // End the loop, we got what we want
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot); // Close our snapshot, we no longer need it
    }
    return pid; // Return the process ID, this will remain as 0 if not found
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId = *(DWORD*)lParam;
    DWORD currentProcessId;
    GetWindowThreadProcessId(hwnd, &currentProcessId);
    if (currentProcessId == processId) {
        wchar_t windowTitle[256];
        if (GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0])) > 0) {
            // If a song isn't playing, do nothing
            if (wcscmp(windowTitle, L"Spotify") != 0
                && wcscmp(windowTitle, L"Spotify Premium") != 0
                && wcscmp(windowTitle, L"Spotify Free") != 0
                && wcscmp(windowTitle, L"GDI+ Window (Spotify.exe)") != 0) {
                wprintf(L"Now Playing: %s\n", windowTitle);

                // If the title changes, send a pause command to the process
                HWND hwndSpotify = FindWindowW(NULL, windowTitle);
                if (hwndSpotify != NULL) {
                    SendMessageW(hwndSpotify, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_MEDIA_PAUSE));
                    
                    // Execute yt-dlp.exe
                    DWORD lastError;
                    STARTUPINFOW si = { sizeof(si) };
                    PROCESS_INFORMATION pi;
                    wchar_t commandLine[MAX_PATH];
                    swprintf_s(commandLine, MAX_PATH, L"\"utils\\yt-dlp.exe\" -f ba -x --audio-format mp3 -o \"%%USERPROFILE%%\\Music\\%s.%%(ext)s\" ytsearch:\"%s\"", windowTitle, windowTitle);
                    if (CreateProcessW(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        // Wait for the process to finish before continuing
                        WaitForSingleObject(pi.hProcess, INFINITE);

                        // FIXME: We need to convert windowTitle from Multibyte to Byte

                        // Determine the size of the required buffer
                        size_t bufferSize = 0;
                        if (wcstombs_s(&bufferSize, NULL, 0, windowTitle, 0) != 0) {
                            perror("wcstombs_s");
                            return 1;
                        }

                        // Allocate memory for the narrow character buffer
                        char* narrowWindowTitle = (char*)malloc(bufferSize + 8); // +1 for null terminator
                        if (narrowWindowTitle == NULL) {
                            perror("malloc");
                            return 1;
                        }

                        // Perform the conversion
                        if (wcstombs_s(NULL, narrowWindowTitle, bufferSize + 1, windowTitle, bufferSize) != 0) {
                            perror("wcstombs");
                            free(narrowWindowTitle);
                            return 1;
                        }

                        // We now have to construct a path to the .mp3 file for SoundPad
                        char userProfile[512];
                        char path[512];

                        // Get the value of the USERPROFILE environment variable
                        char* userProfileEnv = getenv("USERPROFILE");
                        if (userProfileEnv == NULL) {
                            fprintf(stderr, "USERPROFILE environment variable not found\n");
                            return 1;
                        }

                        // Construct the path using USERPROFILE
                        snprintf(path, sizeof(path), "%s\\Music\\%s.mp3", userProfileEnv, narrowWindowTitle);

                        // Add to SoundPad library and start playing
                        checkForEntry(getSoundList(), path);

                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    else {
                        lastError = GetLastError();
                        LPWSTR errorMessage = NULL;
                        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorMessage, 0, NULL);
                        if (errorMessage != NULL) {
                            wprintf(L"Failed to execute yt-dlp.exe. Error code: %lu, Error message: %s\n", lastError, errorMessage);
                            LocalFree(errorMessage);
                        }
                        else {
                            wprintf(L"Failed to execute yt-dlp.exe. Error code: %lu\n", lastError);
                        }
                    }
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

    // We don't want to spam the console
    BOOL isMessagePrinted1 = FALSE;
    BOOL isMessagePrinted2 = FALSE;

    // Check and create the "utils" directory
    const wchar_t* utilsDirectoryName = L"utils";
    CheckAndCreateDirectory(utilsDirectoryName);

    // Check and download yt-dlp.exe
    const wchar_t* utilsFolderPath = L"utils";
    if (!CheckAndDownloadYTDLP(utilsFolderPath)) {
        return 1; // Exit if failed to download yt-dlp.exe
    }

    while (1) {
        pid = GetProcessIdByName(processName);
        if (pid != 0) {
            if (!isMessagePrinted1) {
                wprintf(L"Process ID of %s: %lu\n", processName, pid);
                isMessagePrinted1 = TRUE;
                isMessagePrinted2 = FALSE;
            }
            GetMainWindowTitle(pid);
        } else {
            if (!isMessagePrinted2) {
                wprintf(L"Process %s is not running.\n", processName);
                isMessagePrinted1 = FALSE;
                isMessagePrinted2 = TRUE;
            }
        }
        Sleep(1000); // Sleep for 1 second before checking again
    }

    return 0;
}
