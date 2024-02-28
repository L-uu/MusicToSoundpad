#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <Windows.h>
#include <Psapi.h>

bool IsProcessRunning(const char* processName) {
    DWORD processIds[1024], bytesReturned;
    if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned))
        return false;

    // Calculate how many process identifiers were returned.
    DWORD numProcesses = bytesReturned / sizeof(DWORD);

    for (DWORD i = 0; i < numProcesses; i++)
    {
        DWORD pid = processIds[i];
        if (pid != 0)
        {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess != NULL)
            {
                char szProcessName[MAX_PATH] = { 0 };
                DWORD bytesCopied = GetModuleBaseNameA(hProcess, NULL, szProcessName, sizeof(szProcessName));
                CloseHandle(hProcess);
                if (bytesCopied > 0 && strcmp(szProcessName, processName) == 0)
                    return true;
            }
        }
    }
    return false;
}

int main()
{
    const char* processName = "Spotify.exe";
    if (!IsProcessRunning(processName))
    {
        printf("%s is not running.\n", processName);
        return 1;
    }
    printf("%s is running.\n", processName);
    return 0;
}
