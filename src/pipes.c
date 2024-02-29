#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "pipes.h"

#define PIPE_NAME "\\\\.\\pipe\\sp_remote_control"
#define BUFFER_SIZE 4096
#define RESPONSE_SIZE 4096

HANDLE pipeHandle = NULL;

void init() {
    if (pipeHandle == NULL || pipeHandle == INVALID_HANDLE_VALUE) {
        pipeHandle = CreateFileA(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (pipeHandle == INVALID_HANDLE_VALUE) {
            printf("Error opening pipe\n");
            exit(EXIT_FAILURE);
        }
    }
}

void uninit() {
    if (pipeHandle != NULL && pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle);
        pipeHandle = NULL;
    }
}

char* sendRequest(const char* request) {
    init();

    DWORD bytesWritten;
    if (!WriteFile(pipeHandle, request, strlen(request), &bytesWritten, NULL)) {
        printf("Error writing to pipe\n");
        exit(EXIT_FAILURE);
    }

    char response[RESPONSE_SIZE];
    DWORD bytesRead;
    if (!ReadFile(pipeHandle, response, RESPONSE_SIZE - 1, &bytesRead, NULL)) {
        printf("Error reading from pipe\n");
        exit(EXIT_FAILURE);
    }
    response[bytesRead] = '\0'; // Null-terminate the string

    return _strdup(response);
}

int isSuccessPrintResponse(const char* response) {
    if (strcmp(response, "R-200") != 0) {
        printf("Error: %s\n", response);
        return 0;
    }
    return 1;
}

char* getSoundList() {
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "GetSoundlist()");

    // Call sendRequest function and store response
    char* response = sendRequest(request);
    if (response == NULL) {
        fprintf(stderr, "Failed to get sound list\n");
        return NULL; // Return NULL if sendRequest fails
    }

    return response;
}

int addSound(const char* url) {
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "DoAddSound(\"%s\")", url);
    char* response = sendRequest(request);
    int success = isSuccessPrintResponse(response);
    free(response);
    return success;
}

int playSound(const int index) {
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "DoPlaySound(%d)", index);
    char* response = sendRequest(request);
    int success = isSuccessPrintResponse(response);
    free(response);
    return success;
}

int getIndex(const char* inputCopy, const char* searchTerm) {
    int songIndex = 0;
    char* context = NULL;
    char* line = strtok_s(inputCopy, "\n", &context);
    while (line != NULL) {
        if (strstr(line, searchTerm) != NULL) {
            printf("%s\n", line);
            if (sscanf_s(line, "%*[^0-9]%d", &songIndex) == 1) {
                printf("Extracted index value: %d\n", songIndex);
            }
            else {
                printf("Failed to extract index value.\n");
                return 0;
            }
            break; // Exit loop after finding the line
        }
        line = strtok_s(NULL, "\n", &context);
    }
    return songIndex;
}

int checkForEntry(const char* input, const char* searchTerm) {
    int songIndex = getIndex(input, searchTerm);

    if (!songIndex) { // Item is not present in the Soundpad library. We need to add it.
        if (!addSound(searchTerm)) {
            printf("Failed to add sound: %s", searchTerm);
            return 1;
        }
        printf("Sound added to SoundPad library.\n");
        Sleep(1000);
        songIndex = getIndex(getSoundList(), searchTerm);
    }
    if (!playSound(songIndex)) {
        printf("Failed to play sound\n");
    }
    uninit(); // Clean up resources
    return 0;
}
