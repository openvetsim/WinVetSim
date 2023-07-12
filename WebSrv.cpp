/*
 * WebSrv.cpp
 *
 * Manage WebServer for WinVetSim
 * 
 * We use PHP for a simple web service. The port can be 80, but can also be configured differently to allow
 * operarion with a stanard web server on the same host.
 *
 * This file is part of the sim-mgr distribution
 * TODO: Add link to GitHub for WinVetSim
 *
 * Copyright (c) 2021 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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

#include "vetsim.h"

int checkURL(string url, string port, string page);

int isServerRunning(void)
{
	string host;
	string page;
	string port;
	int sts;

	host = PHP_SERVER_ADDR;
	port  = std::to_string(PHP_SERVER_PORT );
	page = "sim-ii/hostCheck.php";
	sts = checkURL(host, port, page);
	cout << "URL: " << host << ":" << port << "/" << page << " sts: " << sts << endl;

	return (sts);
}
void
stopPHPServer(void)
{
	system("taskkill /FI \"WINDOWTITLE eq WinVetSim PHP\"");
}

char phpPath[FILENAME_MAX];

int
findPhpPath(void)
{
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	// Local copy is preferred
	if ((hFind = FindFirstFile(L"./PHP8.0/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "./PHP8.0/");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 64-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files/PHP/v8.0/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files/PHP/v8.0");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 32-bit
	if ((hFind = FindFirstFile(L"C:/Program Files (x86)/PHP/v8.0/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files (x86)/PHP/8.0");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 64-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files/PHP/v7.4/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files/PHP/v7.4");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 32-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files (x86)/PHP/v7.4/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files (x86)/PHP/v7.4");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 64-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files/PHP/v7.3/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files/PHP/v7.3");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 32-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files (x86)/PHP/v7.3/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files (x86)/PHP/v7.3");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 64-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files/PHP/v7.2/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files/PHP/v7.2");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	// Shared 32-Bit
	if ((hFind = FindFirstFile(L"C:/Program Files (x86)/PHP/v7.2/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files (x86)/PHP/v7.2");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	return (0);
}
int startPHPServer(void )
{
	char commandLine[2048];
	int rval = -1;
	int checkCount;

	if (isServerRunning() == 0)
	{
		if (findPhpPath() == 0)
		{
			printf("Cannot find PHP\n");
			return ( rval );
		}
		printf("Starting PHP Server %s\n", phpPath );

		
		// start [<title>] //d <path>] [program [<parameter>...]]
		sprintf_s(commandLine, 2048, 
				"start \"WinVetSim PHP\" /d  \"%s\" /min \"%s/php.exe\" -S %s:%d sim-ii/router.php",
			localConfig.html_path, phpPath, PHP_SERVER_ADDR, PHP_SERVER_PORT );
		system(commandLine);
		for ( checkCount = 100 ; checkCount > 0; checkCount-- )
		{
			Sleep(10);
			if (isServerRunning() != 0)
			{
				rval = 0;
				break;
			}
		}
	}
	else
	{
		rval = 0;
	}
	return (rval);
}
int
checkURL(string host, string port, string page)
{
	WSADATA wsaData;
	SOCKET Socket;
	int lineCount = 0;
	int rowCount = 0;
	locale local;
	char buffer[10000];
	int i = 0;
	int nDataLength = 0;
	string website_HTML;
	DWORD dwRetval;
	int found = 0;
	int sts;

	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;

	//HTTP GET
	string get_http = "GET /" + page +" HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup failed.\n";
		return 0;
	}

	// host = gethostbyname(url.c_str());
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	cout << host << ":" << port << endl;
	dwRetval = getaddrinfo(host.c_str(), 
							port.c_str(), 
							&hints, 
							&result);
	if (dwRetval != 0) {
		printf("getaddrinfo failed with error: %d\n", dwRetval);
		WSACleanup();
		return 0;
	}
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		switch (ptr->ai_family)
		{
		case AF_INET:
			found = 1;
			break;
		}
		if (found) break;
	}
	
	if (found)
	{
		Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (Socket != -1)
		{
			sts = connect(Socket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if ( sts == 0 )
			{
				//cout << "Connected" << endl;
				// send GET / HTTP
				sts = send(Socket, get_http.c_str(),(int) strlen(get_http.c_str()), 0);
				//cout << "send returns " << sts << endl;
				// recieve html
				
				while ((nDataLength = recv(Socket, buffer, 10000, 0)) > 0) {
					//cout << "recv returns " << nDataLength << endl;
					
					while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {
						website_HTML += buffer[i];
						i += 1;
					}
				}
				//cout << "recv returns " << i << endl;
				// Display HTML source 
				cout << "HTML: " << website_HTML << endl;
			}
			else
			{
				//cout << "Connect failed" << endl;
				i = 0;
			}
		}
		else
		{
			//cout << "socket fails" << endl;
			i = 0;
		}

		closesocket(Socket);
	}
	freeaddrinfo(result);
	WSACleanup();

	// Display HTML source 
	//cout << website_HTML;

	// pause
	//cout << "\n\nPress ANY key to close.\n\n";
	//cin.ignore(); cin.get();

	return ((int)website_HTML.length());
}
