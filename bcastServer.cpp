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
#include <ws2tcpip.h>
#include <iostream>
#include <cstdio>
#include <conio.h>
#include <functional>
#include "vetsimDefs.h"

extern int closeFlag;   // defined in simstatus.cpp; set to 1 on shutdown

int bcastReply(void)
{
	WSADATA wsaData;
	auto sts = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (sts == 0)
	{
		SOCKET sock;
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		int broadcast = 1;
		//     This option is needed on the socket in order to be able to receive broadcast messages
		//   If not set the receiver will not receive broadcast messages in the local network.
		if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) < 0)
		{
			std::cout << "Error in setting Broadcast option";
			sts = -1;
		}
		else
		{
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
				sts = -1;
			}
			else
			{
				// Wake up every second so the loop can check closeFlag even
				// when no broadcast arrives.
				DWORD recvTimeout = 1000;
				if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
					(const char*)&recvTimeout, sizeof(recvTimeout)) < 0)
				{
					std::cout << "Warning: SO_RCVTIMEO failed; shutdown may be delayed\n";
				}

				do
				{
					//std::cout << "\nWaiting for message...\n";
					memset(recvbuff, 0, sizeof(recvbuff));
					auto sts = recvfrom(sock, recvbuff, recvbufflen, 0, (sockaddr*)&Sender_addr, &len);
					if (sts != SOCKET_ERROR)
					{
						std::cout << "Received Message is: " << recvbuff;
						if (strcmp("WVS_LOOK", recvbuff) == 0)
						{
							inet_ntop(AF_INET, &(Sender_addr.sin_addr), recvbuff, INET_ADDRSTRLEN);
							std::cout << " From " << recvbuff;
							sprintf_s(recvbuff, "WVS_FOUND");
							ret = sendto(sock, recvbuff, (int)strlen(recvbuff), 0, (struct sockaddr*)&Sender_addr, len);
							sts = 0;
						}
					}
				} while (!closeFlag);
			}
		}
		closesocket(sock);
	}
	else
	{

		printf("WSAStartup returns %d\n", sts);
	}
	WSACleanup();
	return (sts);
}