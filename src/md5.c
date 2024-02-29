#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")

#define BUFSIZE 1024
#define MD5_HASH_SIZE 16

bool _DownloadFile(const char* url, const char* localFilePath) {
    HINTERNET hInternet = InternetOpenA("MD5Check", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        fprintf(stderr, "Error opening internet connection.\n");
        return false;
    }

    HINTERNET hURL = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hURL == NULL) {
        fprintf(stderr, "Error opening URL: %s\n", url);
        InternetCloseHandle(hInternet);
        return false;
    }

    FILE* outputFile;
    if (fopen_s(&outputFile, localFilePath, "wb") != 0 || outputFile == NULL) {
        fprintf(stderr, "Error creating local file: %s\n", localFilePath);
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

bool CalculateMD5HashOfFile(const char* filename, BYTE* md5hash) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HANDLE hFile = NULL;
    BYTE rgbFile[BUFSIZE];
    DWORD cbRead = 0;

    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return false;
    }

    hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
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

int UpdateYTDLP() {
    char* localFile = "utils\\yt-dlp.exe";
    char* remoteURL = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
    BYTE localMD5[MD5_HASH_SIZE];
    BYTE remoteMD5[MD5_HASH_SIZE];

    char tempFilePath[MAX_PATH];
    if (GetTempPathA(sizeof(tempFilePath), tempFilePath) == 0) {
        fprintf(stderr, "Error getting temporary file path.\n");
        return 1;
    }

    strcat_s(tempFilePath, sizeof(tempFilePath), "tempfile.dat");

    if (!_DownloadFile(remoteURL, tempFilePath)) {
        fprintf(stderr, "Error downloading remote file.\n");
        return 1;
    }

    if (!CalculateMD5HashOfFile(localFile, localMD5)) {
        fprintf(stderr, "Error calculating MD5 hash of local file.\n");
        return 1;
    }

    if (!CalculateMD5HashOfFile(tempFilePath, remoteMD5)) {
        fprintf(stderr, "Error calculating MD5 hash of remote file.\n");
        return 1;
    }

    printf("Local MD5: ");
    for (int i = 0; i < MD5_HASH_SIZE; i++) {
        printf("%02x", localMD5[i]);
    }
    printf("\n");

    printf("Remote MD5: ");
    for (int i = 0; i < MD5_HASH_SIZE; i++) {
        printf("%02x", remoteMD5[i]);
    }
    printf("\n");

    if (memcmp(localMD5, remoteMD5, sizeof(localMD5)) == 0) {
        printf("MD5 checksums match.\n");
    } else {
        printf("MD5 checksums do not match.\n");
        return 2;
    }

    if (remove(tempFilePath) != 0) {
        fprintf(stderr, "Error deleting temporary file.\n");
    }

    return 0;
}
