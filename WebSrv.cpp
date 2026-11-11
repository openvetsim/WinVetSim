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
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iphlpapi.h>
#include <vector>

#pragma comment(lib, "Iphlpapi.lib")

using namespace std;

int checkURL(string url, string port, string page);

int isServerRunning(void)
{
	string host;
	string page;
	string port;
	int sts;
	char mbuf[300];

	sprintf_s(mbuf, 300, "isServerRunning() %s:%d", PHP_SERVER_ADDR, PHP_SERVER_PORT);
	log_message("", mbuf);

	host = PHP_SERVER_ADDR;
	port  = std::to_string(PHP_SERVER_PORT );
	page = "sim-ii/hostCheck.php";
	sts = checkURL(host, port, page);
	//cout << "URL: " << host << ":" << port << "/" << page << " sts: " << sts << endl;

	return (sts);
}

PROCESS_INFORMATION PHP_pi = { 0, 0 };

struct PHPWindowData {
	DWORD pid;
	HWND  hwnd;
};

BOOL CALLBACK FindWindowCallback(HWND hwnd, LPARAM lParam)
{
	auto* data = reinterpret_cast<PHPWindowData*>(lParam);
	DWORD windowPid = 0;
	GetWindowThreadProcessId(hwnd, &windowPid);
	if (windowPid == data->pid && IsWindowVisible(hwnd))
	{
		data->hwnd = hwnd;
		return FALSE; // stop enumeration
	}
	return TRUE; // keep looking
}

HWND FindMainWindowForPid(DWORD pid)
{
	PHPWindowData data = { pid, NULL };
	EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&data));
	return data.hwnd; // NULL if not found
}

void
KillProcess(PROCESS_INFORMATION& pi, DWORD gracePeriodMs = 3000)
{
	// 1. Ask nicely. Only if a window was found: PostMessage(NULL, ...) would post
	// WM_CLOSE to the calling thread's own message queue.
	HWND hwnd = FindMainWindowForPid(pi.dwProcessId);
	if (hwnd)
	{
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}

	// 2. Wait for graceful exit
	if (WaitForSingleObject(pi.hProcess, gracePeriodMs) == WAIT_TIMEOUT)
	{
		// 3. Force it if still alive
		TerminateProcess(pi.hProcess, 1);
		WaitForSingleObject(pi.hProcess, INFINITE);
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	pi.hProcess = pi.hThread = NULL;
}


void
stopPHPServer(void)
{
	if (PHP_pi.dwProcessId != 0)
	{
		KillProcess(PHP_pi, 3000);
	}
}

/*
 * killOrphanedPHPServer
 *
 * Find any php.exe process listening on PHP_SERVER_PORT and terminate it.
 *
 * A PHP server left running by a previous WinVetSim session (one that crashed, was
 * killed from the debugger, or stopped in startPHPServer's failure loop) keeps the
 * port. A newly launched PHP then cannot bind to it, and if the orphan has stopped
 * answering, every server check fails and startup hangs. Only processes whose image
 * is php.exe are terminated; anything else on the port is logged and left alone.
 *
 * Returns the number of processes terminated.
 */
int
killOrphanedPHPServer(void)
{
	char mbuf[600];
	int killed = 0;
	ULONG size = 0;
	DWORD sts;
	std::vector<DWORD> pids;

	// Get the table of listening IPv4 TCP sockets with their owning process IDs
	sts = GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
	if (sts != ERROR_INSUFFICIENT_BUFFER)
	{
		sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: GetExtendedTcpTable size query failed (%lu)", sts);
		log_message("", mbuf);
		return 0;
	}
	std::vector<char> tableBuf(size);
	PMIB_TCPTABLE_OWNER_PID table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(tableBuf.data());
	sts = GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
	if (sts != NO_ERROR)
	{
		sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: GetExtendedTcpTable failed (%lu)", sts);
		log_message("", mbuf);
		return 0;
	}

	for (DWORD r = 0; r < table->dwNumEntries; r++)
	{
		if (ntohs((u_short)table->table[r].dwLocalPort) == PHP_SERVER_PORT)
		{
			DWORD pid = table->table[r].dwOwningPid;
			if (pid != 0 && pid != GetCurrentProcessId() &&
				std::find(pids.begin(), pids.end(), pid) == pids.end())
			{
				pids.push_back(pid);
			}
		}
	}

	for (DWORD pid : pids)
	{
		HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
		if (!hProc)
		{
			sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: cannot open PID %lu on port %d (error %lu)",
				pid, PHP_SERVER_PORT, GetLastError());
			log_message("", mbuf);
			continue;
		}

		char imagePath[MAX_PATH] = { 0 };
		DWORD pathLen = MAX_PATH;
		if (!QueryFullProcessImageNameA(hProc, 0, imagePath, &pathLen))
		{
			sprintf_s(imagePath, sizeof(imagePath), "%s", "(unknown)");
		}
		const char* baseName = strrchr(imagePath, '\\');
		baseName = baseName ? baseName + 1 : imagePath;

		if (_stricmp(baseName, "php.exe") != 0)
		{
			sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: port %d is held by %s (PID %lu), not PHP; leaving it running",
				PHP_SERVER_PORT, imagePath, pid);
			log_message("", mbuf);
			CloseHandle(hProc);
			continue;
		}

		sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: terminating orphaned PHP server %s (PID %lu) on port %d",
			imagePath, pid, PHP_SERVER_PORT);
		log_message("", mbuf);
		if (TerminateProcess(hProc, 1))
		{
			WaitForSingleObject(hProc, 3000);
			killed++;
		}
		else
		{
			sprintf_s(mbuf, sizeof(mbuf), "killOrphanedPHPServer: TerminateProcess failed for PID %lu (error %lu)",
				pid, GetLastError());
			log_message("", mbuf);
		}
		CloseHandle(hProc);
	}
	return killed;
}

char phpPath[FILENAME_MAX];

int
findPhpPath(void)
{
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	sprintf_s(phpPath, "findPhpPath() %s", "Local Path");
	log_message("", phpPath);

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
	sprintf_s(phpPath, "findPhpPath() %s", "C:/Program Files/WinVetSim/PHP8.0/");
	log_message("", phpPath);
	if ((hFind = FindFirstFile(L"C:\\Program Files/WinVetSim/PHP8.0/*.exe", &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		do {
			//printf("Found %S\n", FindFileData.cFileName);
			if (wcscmp(FindFileData.cFileName, L"php.exe") == 0)
			{
				sprintf_s(phpPath, "%s", "C:/Program Files/WinVetSim/PHP8.0");
				FindClose(hFind);
				return (1);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}
	else
	{
		printf("No files found in %s\n", "C:/Program Files/WinVetSim/PHP/V8.0");
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
	char mbuf[300];
	
	sprintf_s(mbuf, 300, "%s", "startPHPServer()");
	log_message("", mbuf);

	// Clear out any PHP server left over from a previous session, so this session
	// launches (and on exit, stops) its own. Only one WinVetSim can run at a time
	// (checkProcessRunning), so any PHP on our port is an orphan.
	if (killOrphanedPHPServer() > 0)
	{
		Sleep(200);		// Give Windows a moment to release the port
	}

	for (checkCount = 4; checkCount > 0; checkCount--)
	{
		Sleep(200);
		if (isServerRunning() != 0)
		{
			rval = 0;
			break;
		}
	}
	if (rval != 0)
	{
		sprintf_s(mbuf, 300, "%s", "calling findPhpPath()");
		log_message("", mbuf);
		if (findPhpPath() == 0)
		{
			sprintf_s(mbuf, 300, "%s", "findPhpPath() Fails");
			log_message("", mbuf);
			printf("Cannot find PHP\n");
			return ( rval );
		}
		printf("Starting PHP Server %s\n", phpPath );

		// start [<title>] //d <path>] [program [<parameter>...]]
		sprintf_s(commandLine, 2048, 
			//"cmd /c start \"WinVetSim PHP\" /d  \"%s\" /min \"%s/php.exe\" -S %s:%d sim-ii/router.php",
			//localConfig.html_path, phpPath, PHP_SERVER_ADDR, PHP_SERVER_PORT );
			"\"%s/php.exe\" -S %s:%d sim-ii/router.php",
			phpPath, PHP_SERVER_ADDR, PHP_SERVER_PORT );
		STARTUPINFOA si;
		
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&PHP_pi, sizeof(PHP_pi));
		BOOL created;
		DWORD fAtr;
		if ( ( strlen(localConfig.html_path) != 0 ) && (fAtr = GetFileAttributesA(localConfig.html_path) ) & FILE_ATTRIBUTE_DIRECTORY )
		{
			// Good path. At least its a directory
			sprintf_s(mbuf, 300, "starting PHP: %s in directory %s", commandLine, localConfig.html_path);
			created = CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, localConfig.html_path, &si, &PHP_pi);
		}
		else
		{
			// Unknown HTML path. Start in current directory
			sprintf_s(mbuf, 300, "starting PHP: %s in current directory", commandLine );
			created = CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &PHP_pi);
		}
		log_message("", mbuf);
		if (!created)
		{
			DWORD error = GetLastError();

			// Log the command line that was attempted
			log_message("", commandLine);

			sprintf_s(mbuf, 300, "CreateProcess failed with error %lu: %s", error, std::system_category().message(error).c_str());
			printf("%s\n", mbuf);
			log_message("", mbuf);
		}
		else
		{
			// We don't need the handles; close them.
			//CloseHandle(PHP_pi.hProcess);
			//CloseHandle(PHP_pi.hThread);
		}
		sprintf_s(mbuf, 300, "CreateProcess PHP %s", "Started OK");
		log_message("", mbuf);
		//
		for ( checkCount = 10 ; checkCount > 0; checkCount-- )
		{
			Sleep(100);
			if (isServerRunning() != 0)
			{
				rval = 0;
				break;
			}
		}
	}
	else
	{
		sprintf_s(mbuf, 90, "%s", "isServerRunning() says PHP is Already Running");
		log_message("", mbuf); 
		rval = 0;
	}
	if (rval != 0)
	{
		sprintf_s(mbuf, 300, "%s", "startPHPServer() Failed");
		log_message("", mbuf);
		while (1)
		{
			Sleep(1000);
		}
	}
	return (rval);
}
// Returns 1 if the URL is reachable and returns a response, 0 otherwise
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
	DWORD timeout = 1000;
	int found = 0;
	int sts;
	extern int debug;
	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;

	//HTTP GET
	string get_http = "GET /" + page +" HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
	if (debug)
	{
		sprintf_s(buffer, 10000, "checkURL() %s:%s/%s", host.c_str(), port.c_str(), page.c_str());
		log_message("", buffer);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "WSAStartup failed.\n";
		return 0;
	}

	// host = gethostbyname(url.c_str());
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	std::cout << host << ":" << port << endl;
	if (debug)
	{
		sprintf_s(buffer, 10000, "Resolving %s:%s", host.c_str(), port.c_str());
		log_message("", buffer);
	}
	dwRetval = getaddrinfo(host.c_str(), 
							port.c_str(), 
							&hints, 
							&result);
	if (dwRetval != 0) {
		if (debug)
		{
			sprintf_s(buffer, 10000, "getaddrinfo failed with error: %d\n", dwRetval);
			log_message("", buffer);
		}
		//	WSACleanup();
		//	return 0;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		switch (ptr->ai_family)
		{
		case AF_INET:
			found = 1;
			if (debug)
			{
				sprintf_s(buffer, 10000, "getaddrinfo Found: \n");
				log_message("", buffer);
			}
			break;
		}
		if (found) break;
	}
	
	if (found)
	{
		Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (Socket != -1)
		{
			u_long mode = 1;
			sts = ioctlsocket(Socket, FIONBIO, &mode);
			if ( sts != 0)
			{
				sprintf_s(buffer, 10000, "ioctlsocket() Error %d:", sts);
				log_message("", buffer);
			}
			int result = setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

			if (result == SOCKET_ERROR)
			{
				sts = WSAGetLastError();
				sprintf_s(buffer, 10000, "setsockop() Error %d:", sts );
				log_message("", buffer);
				//return 0;
			}
			result = setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
			if (result == SOCKET_ERROR)
			{
				sts = WSAGetLastError();
				sprintf_s(buffer, 10000, "setsockop() Error %d:", sts);
				log_message("", buffer);
				//return 0;
			}
			if (debug)
			{
				// Convert socket address to printable IP string
				char addrstr[INET6_ADDRSTRLEN] = {0};
				if (ptr->ai_family == AF_INET)
				{
					struct sockaddr_in *sa = (struct sockaddr_in*)ptr->ai_addr;
					inet_ntop(AF_INET, &sa->sin_addr, addrstr, sizeof(addrstr));
				}
				else if (ptr->ai_family == AF_INET6)
				{
					struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)ptr->ai_addr;
					inet_ntop(AF_INET6, &sa6->sin6_addr, addrstr, sizeof(addrstr));
				}
				sprintf_s(buffer, 10000, "Calling connect: %s:%s", addrstr, port.c_str());
				log_message("", buffer);				
			}

			sts = connect(Socket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (sts == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					// An actual instant failure occurred
					sprintf_s(buffer, 10000, "connect() Error %d", err);
					log_message("", buffer);
					sts = -1;
				}
				else
				{
					// Non-blocking connect in progress: wait for it to complete or time out
					fd_set writeSet;
					fd_set errSet;
					FD_ZERO(&writeSet);
					FD_ZERO(&errSet);
					FD_SET(Socket, &writeSet);
					FD_SET(Socket, &errSet);

					timeval tv;
					tv.tv_sec = 3;
					tv.tv_usec = 0;

					int selectRes = select(0, NULL, &writeSet, &errSet, &tv);
					if (selectRes <= 0 || FD_ISSET(Socket, &errSet))
					{
						// selectRes == 0 means timeout, < 0 means a select API error,
						// errSet means the connection was refused.
						sprintf_s(buffer, 10000, "connect() did not complete (select %d)", selectRes);
						log_message("", buffer);
						sts = -1;
					}
					else
					{
						sts = 0;
					}
				}
			}
			if ( sts == 0 )
			{
				if (debug)
				{
					sprintf_s(buffer, 10000, "Connected:");
					log_message("", buffer);
				}
				// send GET / HTTP
				sts = send(Socket, get_http.c_str(),(int) strlen(get_http.c_str()), 0);
				if (debug)
				{
					sprintf_s(buffer, 10000, "send:%d", sts);
					log_message("", buffer);
				}
				//cout << "send returns " << sts << endl;
				// Receive the response. The socket is non-blocking (SO_RCVTIMEO has no effect),
				// so wait for data with select() rather than calling recv() once and hoping the
				// server has already replied. Read until the server closes the connection
				// ("Connection: close"), the buffer is full, or 3 seconds pass with no data.
				i = 0;
				memset(buffer, 0, 10000);
				nDataLength = 0;
				while (nDataLength < 9999)
				{
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(Socket, &readSet);
					timeval rtv;
					rtv.tv_sec = 3;
					rtv.tv_usec = 0;
					if (select(0, &readSet, NULL, NULL, &rtv) <= 0)
					{
						break;	// Timeout or error
					}
					int got = recv(Socket, buffer + nDataLength, 9999 - nDataLength, 0);
					if (got <= 0)
					{
						break;	// 0 = server closed the connection; < 0 = error
					}
					nDataLength += got;
				}
				if (nDataLength > 0)
				{
					while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {
						website_HTML += buffer[i];
						i += 1;
					}
				}
				//cout << "recv returns " << i << endl;
				// Display HTML source 
				if (debug)
				{
					log_message("", buffer);
				}
			}
			else
			{
				if (debug)
				{
					sprintf_s(buffer, 10000, "Connect Failed:");
					log_message("", buffer);
				}
				i = 0;
			}
		}
		else
		{
			if (debug)
			{
				sprintf_s(buffer, 10000, "Socket Failed:");
				log_message("", buffer);
			}
			i = 0;
		}
		closesocket(Socket);
	}
	freeaddrinfo(result);
	WSACleanup();
	if (i > 0)
	{
		if (debug)
		{
			sprintf_s(buffer, 10000, "checkURL() Success:");
			log_message("", buffer);
		}
		return (int)website_HTML.length();
	}
	else
	{
		if (debug)
		{
			sprintf_s(buffer, 10000, "checkURL() Failed:");
			log_message("", buffer);
		}
		return 0;
	}
	return ((int)website_HTML.length());
}
