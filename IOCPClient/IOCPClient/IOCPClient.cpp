// IOCPClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <winsock2.h>  
#include <mswsock.h>  
#include <windows.h>  
#include <iostream>

#pragma comment(lib,"ws2_32.lib")
using namespace std;
#define MAX_BUFF_SIZE 8192  
enum IO_OPERATION{ IO_READ, IO_WRITE };
struct IO_DATA
{
	WSAOVERLAPPED Overlapped;
	char Buffer[MAX_BUFF_SIZE];
	WSABUF wsabuf;
	int nTotalBytes;
	int nSendBytes;
    DWORD Flag;
	IO_OPERATION opCode;
	SOCKET activeSocket;
};


static DWORD WINAPI ClientWorkerThread(LPVOID lpParameter) 
{ 
    HANDLE hCompletionPort = (HANDLE)lpParameter; 
    DWORD NumBytesRecv = 0; 
    ULONG CompletionKey; 
    IO_DATA * PerIoData; 


    while (GetQueuedCompletionStatus(hCompletionPort, &NumBytesRecv, &CompletionKey, (LPOVERLAPPED*)&PerIoData, INFINITE))
    {
        if (!PerIoData)
            continue;

        if (NumBytesRecv == 0) 
        {
            std::cout << "Server disconnected!\r\n\r\n";  
        }
        else
        {
            // use PerIoData->Buffer as needed...
            std::cout << std::string(PerIoData->Buffer, NumBytesRecv).c_str();

            PerIoData->wsabuf.len = sizeof(PerIoData->Buffer); 
            PerIoData->opCode = IO_READ; 

            if (WSARecv(PerIoData->activeSocket, &(PerIoData->wsabuf), 1, &NumBytesRecv, &(PerIoData->Flag), &(PerIoData->Overlapped), NULL) == 0)
                continue;

            if (WSAGetLastError() == WSA_IO_PENDING)
                continue;
        }

        closesocket(PerIoData->activeSocket);
        delete PerIoData;
    } 

    return 0; 
} 
int _tmain(int argc, _TCHAR* argv[])
{
    WSADATA WsaDat; 
    if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
        return 0; 

    HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); 
    if (!hCompletionPort)
        return 0;

    SYSTEM_INFO systemInfo; 
    GetSystemInfo(&systemInfo); 

    for (DWORD i = 0; i < systemInfo.dwNumberOfProcessors; ++i) 
    { 
        HANDLE hThread = CreateThread(NULL, 0, ClientWorkerThread, hCompletionPort, 0, NULL); 
        CloseHandle(hThread); 
    } 

    SOCKET Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED); 
    if (Socket == INVALID_SOCKET)
        return 0;

    SOCKADDR_IN SockAddr; 
    SockAddr.sin_family = AF_INET; 
    SockAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    SockAddr.sin_port = htons(6000); 

    CreateIoCompletionPort((HANDLE)Socket, hCompletionPort, 0, 0); 

    if (WSAConnect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr), NULL, NULL, NULL, NULL) == SOCKET_ERROR)
        return 0;

    IO_DATA *pPerIoData = new IO_DATA;
    ZeroMemory(pPerIoData, sizeof(IO_DATA)); 

    pPerIoData->activeSocket = Socket; 
    pPerIoData->Overlapped.hEvent = WSACreateEvent();
    strcpy(pPerIoData->Buffer, "Welcome to IOCP");
    pPerIoData->wsabuf.buf = pPerIoData->Buffer; 
    pPerIoData->wsabuf.len = sizeof(pPerIoData->Buffer); 

    DWORD dwNumRecv;
    DWORD dwNumSend;
    if(WSASend(Socket, &(pPerIoData->wsabuf), 1, &dwNumSend, pPerIoData->Flag, &(pPerIoData->Overlapped),NULL)== SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            delete pPerIoData;
            return 0;
        }
    }

    while (TRUE) 
        Sleep(1000); 

    shutdown(Socket, SD_BOTH); 
    closesocket(Socket); 

    WSACleanup(); 
    return 0; 

	return 0;
}

