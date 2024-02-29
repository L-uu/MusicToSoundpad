#pragma once

void init();
void uninit();
char* sendRequest(const char* request);
int isSuccessPrintResponse(const char* response);
char* getSoundList();
int addSound(const char* url);
int playSound(const int index);
int getIndex(const char* inputCopy, const char* searchTerm);
int checkForEntry(const char* input, const char* searchTerm);
