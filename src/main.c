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

BOOL DownloadFile(const wchar_t* filePath, const wchar_t* url) {
    HRESULT hr = URLDownloadToFile(NULL, url, filePath, 0, NULL);
    if (hr == S_OK) {
        wprintf(L"Download successful.\n");
        return TRUE;
    } else {
        wprintf(L"Failed to download file. Error code: %08lx\n", hr);
        return FALSE;
    }
}

BOOL CheckUpdates(const wchar_t* filePath, const wchar_t* url) {

    // Check if file exists
    DWORD fileAttributes = GetFileAttributes(filePath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        // File doesn't exist, download it
        wprintf(L"Downloading %s...\n", filePath);
        return DownloadFile(filePath, url);
    } else {
        wprintf(L"%s already exists. Checking for updates...\n", filePath);
        if (UpdateFile(filePath, url) == 2)
        {
            wprintf(L"Updating %s...\n", filePath);
            return DownloadFile(filePath, url);
        }
        return TRUE;
    }
}

DWORD GetProcessIdByName(const wchar_t* processNames[2], const wchar_t** ptrToProcessFoundName) {
    DWORD pid = 0; // Will store the process ID once found
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // Create a snapshot of the currently running processes
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry; // We need to store information about each process
        processEntry.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (wcscmp(processEntry.szExeFile, processNames[0]) == 0) { // Simple string comparison, check if the process name is what we're looking for
                    pid = processEntry.th32ProcessID; // Store the ID of the process
                    *ptrToProcessFoundName = processNames[0]; // We dereference a pointer to a pointer to change the value that the original pointer points to
                    break; // End the loop, we got what we want
                }
                else if (wcscmp(processEntry.szExeFile, processNames[1]) == 0) { // We repeat the process above if the other process was not found
                    pid = processEntry.th32ProcessID;
                    *ptrToProcessFoundName = processNames[1];
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot); // Close our snapshot, we no longer need it
    }
    return pid; // Return the process ID, this will remain as 0 if not found
}

void SanitiseTitle(wchar_t *title) {
    wchar_t invalidCharacters[] = { '<', '>', ':', '"', '/', '\\', '|', '?', '*', '&'};
    for (int i = 0; i < wcslen(title); i++) {
        for (int j = 0; j < sizeof(invalidCharacters); j++) {
            if (title[i] == invalidCharacters[j]) {
                title[i] = '_';
                break;
            }
        }

		// Replace non-ASCII characters with underscore
		// FIXME: Unicode handling needed for full support
        if (title[i] > 127) {
            title[i] = '_';
        }
    }
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
                && wcscmp(windowTitle, L"GDI+ Window (Spotify.exe)") != 0
                && wcscmp(windowTitle, L"TIDAL") != 0) {
                wprintf(L"Now Playing: %s\n", windowTitle);

                // If the title changes, send a pause command to the process
                HWND hwndSpotify = FindWindowW(NULL, windowTitle);
                if (hwndSpotify != NULL) {
                    SendMessageW(hwndSpotify, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_MEDIA_PAUSE));
                    
                    // Sanitise the title, some characters aren't allowed in file names
                    SanitiseTitle(windowTitle);

                    // Execute yt-dlp.exe
                    DWORD lastError;
                    STARTUPINFOW si = { sizeof(si) };
                    PROCESS_INFORMATION pi;
                    wchar_t commandLine[MAX_PATH];
                    swprintf_s(commandLine, MAX_PATH, L"\"utils\\yt-dlp.exe\" -f ba[ext=m4a] -o \"%%USERPROFILE%%\\Music\\%s.%%(ext)s\" ytsearch:\"%s\"", windowTitle, windowTitle);
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
                        char* narrowWindowTitle = (char*)malloc(bufferSize + 1); // +1 for null terminator
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

                        // We now have to construct a path to the .m4a file for SoundPad
                        // Get the value of the USERPROFILE environment variable
                        char* userProfileEnv = getenv("USERPROFILE");
                        if (userProfileEnv == NULL) {
                            fprintf(stderr, "USERPROFILE environment variable not found\n");
                            return 1;
                        }

                        // Construct the path using USERPROFILE
                        char path[512];
                        snprintf(path, sizeof(path), "%s\\Music\\%s.m4a", userProfileEnv, narrowWindowTitle);

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
    // Check and create the "utils" directory
    const wchar_t* utilsDirectoryName = L"utils";
    CheckAndCreateDirectory(utilsDirectoryName);

    // Check for updates
    if (!CheckUpdates(L"MusicToSoundpad.exe", L"http://96.126.111.48/MusicToSoundpad/MusicToSoundpad.exe")) {
        return 1; // Something has gone badly wrong
    }
    if (!CheckUpdates(L"utils\\ffmpeg.exe", L"http://96.126.111.48/MusicToSoundpad/ffmpeg.exe")) {
        return 1; // Exit if failed to download ffmpeg.exe
    }
    if (!CheckUpdates(L"utils\\yt-dlp.exe", L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe")) {
        return 1; // Exit if failed to download yt-dlp.exe
    }

    const wchar_t* processNames[2] = { L"Spotify.exe", L"TIDAL.exe" };
    const wchar_t* processFoundName;
    DWORD pid;

    // We don't want to spam the console
    BOOL isMessagePrinted1 = FALSE;
    BOOL isMessagePrinted2 = FALSE;

    while (1) {
        pid = GetProcessIdByName(processNames, &processFoundName);
        if (pid != 0) {
            if (!isMessagePrinted1) {
                wprintf(L"Process ID of %s: %lu\n", processFoundName, pid);
                isMessagePrinted1 = TRUE;
                isMessagePrinted2 = FALSE;
            }
            GetMainWindowTitle(pid);
        } else {
            if (!isMessagePrinted2) {
                wprintf(L"No processes running.\n");
                isMessagePrinted1 = FALSE;
                isMessagePrinted2 = TRUE;
            }
        }
        Sleep(1000); // Sleep for 1 second before checking again
    }

    return 0;
}
