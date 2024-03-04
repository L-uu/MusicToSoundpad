#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")

#define BUFSIZE 1024
#define MD5_HASH_SIZE 16

bool _DownloadFile(const wchar_t* localFilePath, const wchar_t* url) {
    HINTERNET hInternet = InternetOpen(L"MD5Check", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        fwprintf(stderr, L"Error opening internet connection.\n");
        return false;
    }

    HINTERNET hURL = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hURL == NULL) {
        fwprintf(stderr, L"Error opening URL: %s\n", url);
        InternetCloseHandle(hInternet);
        return false;
    }

    FILE* outputFile;
    if (_wfopen_s(&outputFile, localFilePath, L"wb") != 0 || outputFile == NULL) {
        fwprintf(stderr, L"Error creating local file: %s\n", localFilePath);
        InternetCloseHandle(hURL);
        InternetCloseHandle(hInternet);
        return false;
    }

    BYTE buffer[BUFSIZE];
    DWORD bytesRead;
    while (InternetReadFile(hURL, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        fwrite(buffer, 1, bytesRead, outputFile);
    }

    fclose(outputFile);
    InternetCloseHandle(hURL);
    InternetCloseHandle(hInternet);

    return true;
}

bool CalculateMD5HashOfFile(const wchar_t* filename, BYTE* md5hash) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HANDLE hFile = NULL;
    BYTE rgbFile[BUFSIZE];
    DWORD cbRead = 0;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return false;
    }

    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    while (ReadFile(hFile, rgbFile, BUFSIZE, &cbRead, NULL) && cbRead > 0) {
        if (!CryptHashData(hHash, rgbFile, cbRead, 0)) {
            CloseHandle(hFile);
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }
    }

    DWORD cbHash = MD5_HASH_SIZE;
    if (!CryptGetHashParam(hHash, HP_HASHVAL, md5hash, &cbHash, 0)) {
        CloseHandle(hFile);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    CloseHandle(hFile);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return true;
}

int UpdateFile(const wchar_t* localFile, const wchar_t* remoteURL) {
    BYTE localMD5[MD5_HASH_SIZE];
    BYTE remoteMD5[MD5_HASH_SIZE];

    wchar_t tempFilePath[MAX_PATH];
    if (GetTempPath(sizeof(tempFilePath)/sizeof(wchar_t), tempFilePath) == 0) {
        fwprintf(stderr, L"Error getting temporary file path.\n");
        return 1;
    }

    wcscat_s(tempFilePath, sizeof(tempFilePath)/sizeof(wchar_t), L"tempfile.dat");

    if (!_DownloadFile(tempFilePath, remoteURL)) {
        fwprintf(stderr, L"Error downloading remote file.\n");
        return 1;
    }

    if (!CalculateMD5HashOfFile(localFile, localMD5)) {
        fwprintf(stderr, L"Error calculating MD5 hash of local file.\n");
        return 1;
    }

    if (!CalculateMD5HashOfFile(tempFilePath, remoteMD5)) {
        fwprintf(stderr, L"Error calculating MD5 hash of remote file.\n");
        return 1;
    }

    wprintf(L"Local MD5: ");
    for (int i = 0; i < MD5_HASH_SIZE; i++) {
        wprintf(L"%02x", localMD5[i]);
    }
    wprintf(L"\n");

    wprintf(L"Remote MD5: ");
    for (int i = 0; i < MD5_HASH_SIZE; i++) {
        wprintf(L"%02x", remoteMD5[i]);
    }
    wprintf(L"\n");

    if (memcmp(localMD5, remoteMD5, sizeof(localMD5)) == 0) {
        wprintf(L"MD5 checksums match.\n");
    } else {
        wprintf(L"MD5 checksums do not match.\n");
        if (localFile == L"MusicToSoundpad.exe") {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
            wprintf(L"UPDATE AVAILABLE... PLEASE DOWNLOAD LATEST VERSION FROM GITHUB...\n");
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            return 0;
        }
        return 2;
    }

    if (_wremove(tempFilePath) != 0) {
        fwprintf(stderr, L"Error deleting temporary file.\n");
    }

    return 0;
}
