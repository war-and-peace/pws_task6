#include "Client.h"
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <conio.h>
#include <iostream>

Client::Client(std::wstring_view pipeName) : pipeName(pipeName) {
	this->hPipe = nullptr;
}

bool Client::connect() {
    while (1)
    {
        hPipe = CreateFile(
            this->pipeName.data(),   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file  

        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            std::cerr << "Could not open the pipe, error code: " << WSAGetLastError() << std::endl;
            return false;
        }

        if (!WaitNamedPipe(this->pipeName.data(), 20000))
        {
            std::cerr << "timeout finished. Did not connect within the time limit" << std::endl;
            return false;
        }
    }
    std::cerr << "Connected!" << std::endl;

    DWORD dwMode = PIPE_READMODE_MESSAGE;
    bool fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    // Send a message to the pipe server. 

    /*cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
    _tprintf(TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage);*/
    std::string message{ "start" };
    DWORD cbWritten = 0;

    fSuccess = WriteFile(
        hPipe,                  // pipe handle 
        message.data(),             // message 
        message.length() + 1,              // message length 
        &cbWritten,             // bytes written 
        NULL);                  // not overlapped 

    if (!fSuccess)
    {
        std::cout << "error while sending, error code: " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}

void Client::halt() {
    std::cin.get();
}