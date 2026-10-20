// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "network.h"

#if defined(__LINUX__) || defined(__PS3__)
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <fcntl.h>
#include <cstring>


#if defined(__PS3__)
#include <net/net.h>
#endif


#include <engine/debug/debug.h>
#include <engine/engine_settings.h>
#include <engine/debug/stack_debug_object.h>
#include <engine/debug/performance.h>

static void CloseSocketId(int socketId)
{
#if defined(_WIN32) || defined(_WIN64)
	closesocket(socketId);
#else
	close(socketId);
#endif
}

std::vector<std::shared_ptr<Socket>> NetworkManager::s_sockets;

void NetworkManager::Init()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);
#if defined(__PS3__)
	netInitialize();
#elif defined(_WIN32) || defined(_WIN64)
	WSADATA WSAData;
	int startupResult = WSAStartup(MAKEWORD(2, 0), &WSAData);
	if (startupResult != 0)
	{
		Debug::PrintError("[NetworkManager::CreateSocket] Could not start win socket");
	}
#endif

#if !defined(EDITOR)
	if (EngineSettings::values.useOnlineDebugger)
	{
		Debug::ConnectToOnlineConsole();
	}
#endif
}

void NetworkManager::Stop()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);
}

std::shared_ptr<Socket> NetworkManager::GetClientSocket()
{
#if defined(_WIN32) || defined(_WIN64)
	SOCKADDR_IN sin;
	SOCKADDR_IN csin;
	int newSocketId = 1;
	int newClientSocketId = 1;
	newSocketId = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(6004);
	bind(newSocketId, reinterpret_cast<SOCKADDR*>(& sin), sizeof(sin));
	listen(newSocketId, 0);
	while (true)
	{
		int sinsize = sizeof(csin);
		if ((newClientSocketId = static_cast<int>(accept(newSocketId, reinterpret_cast<SOCKADDR*>(&csin), &sinsize))) != INVALID_SOCKET)
		{
			Debug::Print("New client connected");
			break;
			/*send(csock, "Hello world!\r\n", 14, 0);
			closesocket(csock);*/
		}
	}
	unsigned long nonblocking_long = false ? 0 : 1;
	ioctlsocket(newClientSocketId, FIONBIO, &nonblocking_long);

	std::shared_ptr<Socket> myNewSocket = std::make_shared<Socket>(newClientSocketId);
	s_sockets.push_back(myNewSocket);
	return myNewSocket;
#else
	return nullptr;
#endif
}

void NetworkManager::Update()
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	SCOPED_PROFILER("NetworkManager::Update", scopeBenchmark);

	const size_t socketCount = s_sockets.size();
	for (size_t i = 0; i < socketCount; i++)
	{
		s_sockets[i]->Update();
	}
}

void Socket::Update()
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	if (m_socketId < 0)
		return;

	char recvBuff[1024];
	int recvd_len;

	// Read a maximum of 1022 char in one loop
	while ((recvd_len = recv(m_socketId, recvBuff, 1023, 0)) > 0) // if recv returns 0, the socket has been closed. (Sometimes yes, sometimes not, lol)
	{
		recvBuff[recvd_len] = 0;
		m_incommingData += recvBuff;
	}
}

void Socket::Close()
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	if (m_socketId < 0)
		return;

	CloseSocketId(m_socketId);
	// Forget the id, the system can give it to a new socket (closing it twice could close another socket)
	m_socketId = -1;
}

Socket::~Socket()
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	Close();
}

void Socket::SendData(const std::string& text)
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	if (m_socketId < 0 || text.empty())
		return;

	const int infoLentgh = (int)text.size();
	send(m_socketId, text.c_str(), infoLentgh, 0); // Send data to server
}

void Socket::SendData(const char* data, int size)
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	if (m_socketId < 0 || data == nullptr || size <= 0)
		return;

	send(m_socketId, data, size, 0); // Send data to server
}

std::shared_ptr<Socket> NetworkManager::CreateSocket(const std::string& address, int port)
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	int newSocketId = -1;
#if !defined(__LINUX__)
	if (address.empty() || port <= 0)
	{
		Debug::PrintError("[NetworkManager::CreateSocket] Invalid address or port");
		return nullptr;
	}

	struct sockaddr_in serv_addr;

	// memset(recvBuff, '0', sizeof(recvBuff));
//#if defined(_WIN32) || defined(_WIN64)
//	WSADATA WSAData;
//	int startupResult = WSAStartup(MAKEWORD(2, 0), &WSAData);
//	if (startupResult != 0)
//	{
//		Debug::PrintError("[NetworkManager::CreateSocket] Could not start win socket");
//		return nullptr;
//	}
//#endif
	if ((newSocketId = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0))) < 0)
	{
		Debug::PrintError("[NetworkManager::CreateSocket] Could not create socket");
		return nullptr;
	}
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) <= 0)
	{
		Debug::PrintError("[NetworkManager::CreateSocket] inet_pton error occured");
		CloseSocketId(newSocketId);
		return nullptr;
	}
	if (connect(newSocketId, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0)
	{
		Debug::PrintError("[NetworkManager::CreateSocket] Connect Failed");
		CloseSocketId(newSocketId);
		return nullptr;
	}
#if defined(_WIN32) || defined(_WIN64)
	unsigned long nonblocking_long = false ? 0 : 1;
	ioctlsocket(newSocketId, FIONBIO, &nonblocking_long);
#elif defined(__PS3__)
	int i = 1;
	if (setsockopt(newSocketId, SOL_SOCKET, SO_NBIO, (char*)&i, sizeof(i)) < 0)
	{
		Debug::PrintError("Failed to change socket flags");
		CloseSocketId(newSocketId);
		return nullptr;
	}
#else
	int i = 1;
	if (setsockopt(newSocketId, SOL_SOCKET, SO_NONBLOCK, (char*)&i, sizeof(i)) < 0)
	{
		Debug::PrintError("Failed to change socket flags");
		CloseSocketId(newSocketId);
		return nullptr;
	}
#endif
#endif
	std::shared_ptr<Socket> myNewSocket = std::make_shared<Socket>(newSocketId);
	s_sockets.push_back(myNewSocket);
	return myNewSocket;
}