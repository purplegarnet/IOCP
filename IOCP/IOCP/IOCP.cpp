// IOCP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <winsock2.h>  
#include <mswsock.h>  
#include <windows.h>  
#include <iostream>

#pragma comment(lib,"ws2_32.lib")

int g_ThreadCount;
HANDLE g_hIOCP = INVALID_HANDLE_VALUE;
SOCKET g_ServerSocket = INVALID_SOCKET;
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
	IO_OPERATION opCode;
	SOCKET activeSocket;
};

DWORD WINAPI WorkerThread(LPVOID WorkThreadContext) {
	LPWSAOVERLAPPED lpOverlapped = NULL;
	IO_DATA *lpIOContext = NULL;
	WSABUF buffSend;
	DWORD dwRecvNumBytes = 0;
	DWORD dwSendNumBytes = 0;
	DWORD dwFlags = 0;
	DWORD dwIoSize = 0;
	BOOL bSuccess = FALSE;
	int nRet = 0;
	while (1) {
		void * lpCompletionKey = NULL;
		bSuccess = GetQueuedCompletionStatus(g_hIOCP, &dwIoSize,
			(LPDWORD)&lpCompletionKey,
			(LPOVERLAPPED *)&lpOverlapped,
			INFINITE);
		if (!bSuccess)
		{
			cout << "GetQueuedCompletionStatus() failed:" << GetLastError() << endl;
			break;
		}
		lpIOContext = (IO_DATA *)lpOverlapped;
		if (dwIoSize == 0) //socket closed?  
		{
			cout << "Client disconnect" << endl;
			closesocket(lpIOContext->activeSocket);
			delete lpIOContext;
			continue;
		}
		if (lpIOContext->opCode == IO_READ) // a read operation complete  
		{
			lpIOContext->nTotalBytes = lpIOContext->wsabuf.len;
			lpIOContext->nSendBytes = 0;
			lpIOContext->opCode = IO_WRITE;
			dwFlags = 0;

            cout<<lpIOContext->wsabuf.buf<<endl;
			nRet = WSASend(
				lpIOContext->activeSocket,
				&lpIOContext->wsabuf, 1, &dwSendNumBytes,
				dwFlags,
				&(lpIOContext->Overlapped), NULL);
			if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
				cout << "WASSend Failed::Reason Code::" << WSAGetLastError() << endl;
				closesocket(lpIOContext->activeSocket);
				delete lpIOContext;
				continue;
			}
		}
		else if (lpIOContext->opCode == IO_WRITE) //a write operation complete  
		{
			lpIOContext->nSendBytes += dwIoSize;
			dwFlags = 0;
			if (lpIOContext->nSendBytes < lpIOContext->nTotalBytes) {
				lpIOContext->opCode = IO_WRITE;
				// A Write operation has not completed yet, so post another  
				// Write operation to post remaining data.  
				buffSend.buf = lpIOContext->Buffer + lpIOContext->nSendBytes;
				buffSend.len = lpIOContext->nTotalBytes - lpIOContext->nSendBytes;
				nRet = WSASend(
					lpIOContext->activeSocket,
					&buffSend, 1, &dwSendNumBytes,
					dwFlags,
					&(lpIOContext->Overlapped), NULL);
				if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
					cout << "WASSend Failed::Reason Code::" << WSAGetLastError() << endl;
					closesocket(lpIOContext->activeSocket);
					delete lpIOContext;
					continue;
				}
			}
			else {
				// Write operation completed, so post Read operation.  
				lpIOContext->opCode = IO_READ;
				dwRecvNumBytes = 0;
				dwFlags = 0;
				lpIOContext->wsabuf.buf = lpIOContext->Buffer,
					ZeroMemory(lpIOContext->wsabuf.buf, MAX_BUFF_SIZE);
				lpIOContext->Overlapped.Internal = 0;
				lpIOContext->Overlapped.InternalHigh = 0;
				lpIOContext->Overlapped.Offset = 0;
				lpIOContext->Overlapped.OffsetHigh = 0;
				lpIOContext->Overlapped.hEvent = NULL;
				lpIOContext->wsabuf.len = MAX_BUFF_SIZE;
				nRet = WSARecv(
					lpIOContext->activeSocket,
					&lpIOContext->wsabuf, 1, &dwRecvNumBytes,
					&dwFlags,
					&lpIOContext->Overlapped, NULL);
				if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
					cout << "WASRecv Failed::Reason Code::" << WSAGetLastError() << endl;
					closesocket(lpIOContext->activeSocket);
					delete lpIOContext;
					continue;
				}
			}
		}
	}
	return 0;
}
void _tmain(int argc, _TCHAR* argv[])
{


	{ // Init winsock2  
		WSADATA wsaData;
		ZeroMemory(&wsaData, sizeof(WSADATA));
		int retVal = -1;
		if ((retVal = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
			cout << "WSAStartup Failed::Reason Code::" << retVal << endl;
			return;
		}
	}
	{  //Create socket  
		g_ServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (g_ServerSocket == INVALID_SOCKET) {
			cout << "Server Socket Creation Failed::Reason Code::" << WSAGetLastError() << endl;
			return;
		}
	}
	{   //bind  
		sockaddr_in service;
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(INADDR_ANY);
		service.sin_port = htons(6000);
		int retVal = bind(g_ServerSocket, (SOCKADDR *)&service, sizeof(service));
		if (retVal == SOCKET_ERROR) {
			cout << "Server Soket Bind Failed::Reason Code::" << WSAGetLastError() << endl;
			return;
		}
	}
	{   //listen  
		int retVal = listen(g_ServerSocket, 8);
		if (retVal == SOCKET_ERROR) {
			cout << "Server Socket Listen Failed::Reason Code::" << WSAGetLastError() << endl;
			return;
		}
	}
	{   // Create IOCP  
		SYSTEM_INFO sysInfo;
		ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
		GetSystemInfo(&sysInfo);
		g_ThreadCount = sysInfo.dwNumberOfProcessors * 1;
		g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_ThreadCount);
		if (g_hIOCP == NULL) {
			cout << "CreateIoCompletionPort() Failed::Reason::" << GetLastError() << endl;
			return;
		}
		if (CreateIoCompletionPort((HANDLE)g_ServerSocket, g_hIOCP, 0, 0) == NULL){
			cout << "Binding Server Socket to IO Completion Port Failed::Reason Code::" << GetLastError() << endl;
			return;
		}
	}
	{  //Create worker threads  
		for (DWORD dwThread = 0; dwThread < g_ThreadCount; dwThread++)
		{
			HANDLE  hThread;
			DWORD   dwThreadId;
			hThread = CreateThread(NULL, 0, WorkerThread, 0, 0, &dwThreadId);
			CloseHandle(hThread);
		}
	}
	{ //accept new connection  
		while (1)
		{
			SOCKET ls = accept(g_ServerSocket, NULL, NULL);
			if (ls == SOCKET_ERROR)  break;
			cout << "Client connected." << endl;
			{ //diable buffer to improve performance  
				int nZero = 0;
				setsockopt(ls, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero));
			}
			if (CreateIoCompletionPort((HANDLE)ls, g_hIOCP, 0, 0) == NULL){
				cout << "Binding Client Socket to IO Completion Port Failed::Reason Code::" << GetLastError() << endl;
				closesocket(ls);
			}
			else { //post a recv request  
				IO_DATA * data = new IO_DATA;
				ZeroMemory(&data->Overlapped, sizeof(data->Overlapped));//用0来填充一块内存区域  para1:指向一块准备用0来填充的内存区域的开始地址。   
				ZeroMemory(data->Buffer, sizeof(data->Buffer));                               //   para2:准备用0来填充内存区域的大小，按字节来计算。  
				data->opCode = IO_READ;
				data->nTotalBytes = 0;
				data->nSendBytes = 0;
				data->wsabuf.buf = data->Buffer;
				data->wsabuf.len = sizeof(data->Buffer);
				data->activeSocket = ls;
				DWORD dwRecvNumBytes = 0, dwFlags = 0;
				int nRet = WSARecv(ls, &data->wsabuf, 1, &dwRecvNumBytes,
					&dwFlags,
					&data->Overlapped, NULL); //WSARecv 在重叠模型中，接收数据就, 参数也比recv要多 WSA_IO_PENDING ： 最常见的返回值，这是说明我们的WSARecv操作成功了，但是I/O操作还没有完成，所以我们就需要绑定一个事件来通知我们操作何时完成.  
				if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())){
					cout << "WASRecv Failed::Reason Code::" << WSAGetLastError() << endl;
					closesocket(ls);
					delete data;
				}
			}
		}
	}
	closesocket(g_ServerSocket);
	WSACleanup();
	return;
}

