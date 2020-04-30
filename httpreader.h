#pragma once
#include <WinInet.h>

#pragma comment(lib, "Wininet")

const char* readRawHttp(const char* szUrl, size_t* len) {
    std::string host, requestPart;
    std::string output = "";
    host += szUrl;
    if (host.substr(0, 7) == "http://") host.erase(0, 7);
    if (host.substr(0, 8) == "https://") host.erase(0, 8);

    int i = 0;
    for (; i < host.length(); i++) {
        if (host.at(i) == '/') {
            break;
        }
    }

    requestPart = host.substr(i + 1, host.length() - (i + 1));
    host = host.substr(0, i);

    HINTERNET hIntSession = InternetOpenA("MemLua", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    HINTERNET hHttpSession = InternetConnectA(hIntSession, host.c_str(), 80, 0, 0, INTERNET_SERVICE_HTTP, 0, NULL);
    HINTERNET hHttpRequest = HttpOpenRequestA(hHttpSession, "GET", requestPart.c_str(), 0, 0, 0, INTERNET_FLAG_RELOAD, 0);

    const char* szHeaders = "Content-Type: text/html\n";
    char szReq[1024];

    if (!HttpSendRequestA(hHttpRequest, szHeaders, lstrlenA(szHeaders), szReq, 1024)) {
        DWORD dwCode = GetLastError();
        printf("Error in readRawHttp [dwCode: %i]\n", dwCode);
        return nullptr;
    }

    char szBuffer[0x16000];
    szBuffer[0] = '\0';
    DWORD dwRead = 0;
    while (InternetReadFile(hHttpRequest, szBuffer, sizeof(szBuffer) - 1, &dwRead) && dwRead) {
        if (len != nullptr) {
            *len = dwRead;
        }
        szBuffer[dwRead] = 0;
        OutputDebugStringA(szBuffer);
        dwRead = 0;
    }

    InternetCloseHandle(hHttpRequest);
    InternetCloseHandle(hHttpSession);
    InternetCloseHandle(hIntSession);

    return szBuffer;
}
