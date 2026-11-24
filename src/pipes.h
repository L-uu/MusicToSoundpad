#pragma once

void init(void);
void uninit(void);
char* sendRequest(const char* request);
int isSuccessPrintResponse(const char* response);
char* getSoundList(void);
int addSound(const char* url);
int playSound(int index);
int getIndex(char* inputCopy, const char* searchTerm);
int checkForEntry(char* input, const char* addPath, const char* searchPath);
