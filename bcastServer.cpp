/*
 * bcastServer.cpp
 *
 * SimMgr applicatiopn
 *
 * This file is part of the sim-mgr distribution.
 *
 * Copyright (c) 2024 ITown Design LLC, Ithaca, NY
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "winsock2.h"
#include <iostream>
#include <conio.h>
#include <functional>
#include <ws2tcpip.h>

#include <iostream>
#include <cstdio>
#include <conio.h>
#include "vetsimDefs.h"

int bcastReply(void)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET sock;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	char broadcast = '1';
	//     This option is needed on the socket in order to be able to receive broadcast messages
	//   If not set the receiver will not receive broadcast messages in the local network.
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
	{
		std::cout << "Error in setting Broadcast option";
		closesocket(sock);
		return 0;
	}

	struct sockaddr_in Recv_addr;
	struct sockaddr_in Sender_addr;
	int len = sizeof(struct sockaddr_in);
	char recvbuff[50];
	int recvbufflen = 50;
	Recv_addr.sin_family = AF_INET;
	Recv_addr.sin_port = htons(PORT_STATUS);
	Recv_addr.sin_addr.s_addr = INADDR_ANY;
	auto ret = bind(sock, (sockaddr*)&Recv_addr, sizeof(Recv_addr));

	if (ret < 0)
	{
		std::cout << "Error in BINDING" << WSAGetLastError();
		closesocket(sock);
		return 0;
	}

	do
	{
		std::cout << "\nWaiting for message...\n";
		memset(recvbuff, 0, sizeof(recvbuff));
		recvfrom(sock, recvbuff, recvbufflen, 0, (sockaddr*)&Sender_addr, &len);
		std::cout << "Received Message is: " << recvbuff;
		if (strcmp("WVS_LOOK", recvbuff) == 0)
		{
			inet_ntop(AF_INET, &(Sender_addr.sin_addr), recvbuff, INET_ADDRSTRLEN);
			std::cout << " From " << recvbuff;
			sprintf_s(recvbuff, "WVS_FOUND");
			ret = sendto(sock, recvbuff, strlen(recvbuff), 0, (struct sockaddr*)&Sender_addr, len);
		}
	} while (1);

	closesocket(sock);
	WSACleanup();
}