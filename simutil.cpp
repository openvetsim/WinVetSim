/*
 * simutil.c
 *
 * Common functions for the various SimMgr processes
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
/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <termios.h>
*/
#define SIMUTIL	1

extern char msg_buf[];

struct simmgr_shm* simmgr_shm;	// Data structure of the shared memory

/*
 * FUNCTION: initSHM
 *
 * Open the shared memory space
 *   In Linux, a shared memory file is used. 
 *   In Windows, the shared memory is inherited from the parent process
 *
 * Parameters: none
 *
 * Returns: 0 for success
 */

int
initSHM(int create, char* sesid)
{
	extern struct simmgr_shm shmSpace;
	simmgr_shm = &shmSpace;

	return (0);
}


/*
 * Function: log_message
 *
 * Log a message to syslog. The message is also logged to a named file. The file is opened
 * for Append and closed on completion of the write. (The file write will be disabled after
 * development is completed).
 *
 *
 * Parameters: filename - filename to open for writing, NULL for default log file
 *             message - Pointer to message string, NULL terminated
 *
 * Returns: none
 */
void log_message(const char* filename, const char* message)
{
	FILE* logfile;
	errno_t err;
	if (strlen(filename) > 0 )
	{
		err = fopen_s(&logfile, filename, "a");
	}
	else
	{
		err = fopen_s(&logfile, LOG_NAME, "a");
	}
	if (logfile)
	{
		fprintf(logfile, "%s\n", message);
		fclose(logfile);
	}

	// printf("%s\n", message);
	// OutputDebugStringA(message);
}

void log_messaget(const char* filename, TCHAR* message)
{
	FILE* logfile;
	errno_t err;
	if (strlen(filename) > 0)
	{
		err = fopen_s(&logfile, filename, "a");
	}
	else
	{
		err = fopen_s(&logfile, LOG_NAME, "a");
	}
	if (logfile)
	{
#define MAX_LENGTH 500
		char szString[MAX_LENGTH];
		size_t nNumCharConverted;
		wcstombs_s(&nNumCharConverted, szString, MAX_LENGTH,
			message, MAX_LENGTH);

		fprintf(logfile, "%s\n", szString);
		fclose(logfile);
	}
}

// Issue a shell command and read the first line returned from it.

char*
do_command_read(const char* cmd_str, char* buffer, int max_len)
{
	FILE* fp;
	char* cp;
	char* token1 = NULL;

	fp = _popen(cmd_str, "r");
	if (fp == NULL)
	{
		cp = NULL;
	}
	else
	{
		cp = fgets(buffer, max_len, fp);
		if (cp != NULL)
		{
			{
				FILE* fp;
				char* cp;

				fp = _popen(cmd_str, "r");
				if (fp == NULL)
				{
					cp = NULL;
				}
				else
				{
					cp = fgets(buffer, max_len, fp);
					if (cp != NULL)
					{
						(void)strtok_s(buffer, "\n", &token1); // Strip the newline
					}
					_pclose(fp);
				}
				return (cp);
			}(buffer, "\n"); // Strip the newline
		}
		_pclose(fp);
	}
	return (cp);
}
void
get_date(char* buffer, int maxLen)
{
	struct tm newtime;
	__time64_t long_time;
	char timebuf[64];
	errno_t err;
	char* ptr;

	// Get time as 64-bit integer.
	_time64(&long_time);
	// Convert to local time.
	err = _localtime64_s(&newtime, &long_time);
	if (err)
	{
		printf("Invalid argument to _localtime64_s.");
		exit(1);
	}
	// Convert to an ASCII representation.
	err = asctime_s(timebuf, 64, &newtime);
	if (err)
	{
		printf("Invalid argument to asctime_s.");
		exit(1);
	}
	ptr = timebuf;
	while (*ptr)
	{
		if (*ptr == '\n')
		{
			*ptr = 0;
		}
		ptr++;
	}
	sprintf_s(buffer, maxLen, "%s", timebuf );
}

char eth0_ip[512] = { 0, };
char wifi_ip[512] = { 0, };
char ipAddr[512] = { 0, };

char* get_IP(const char* iface)
{
#ifdef TODO
	int fd;
	struct ifreq ifr;
	int sts;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	sts = ioctl(fd, SIOCGIFADDR, &ifr);
	if (sts == 0)
	{
		sprintf(ipAddr, "%s %s", iface, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
	}
	else
	{
		sprintf(ipAddr, "%s", "");
	}
	close(fd);
#endif
	return ipAddr;
}


char*
getETH0_IP()
{
	sprintf_s(eth0_ip, sizeof(eth0_ip), "" );

	return eth0_ip;
}

char*
getWIFI_IP()
{
	char* addr;

	addr = get_IP("wlp58s0");
	sprintf_s(wifi_ip, "%s", addr);
	return wifi_ip;
}
/**
 * cleanString
 *
 * remove leading and trailing spaces. Reduce internal spaces to single. Remove tabs, newlines, CRs.
*/
#define WHERE_START		0
#define WHERE_TEXT		1
#define WHERE_SPACE		2

void
cleanString(char* strIn)
{
	char* in;
	char* out;
	int where = WHERE_START;

	in = (char*)strIn;
	out = (char*)strIn;
	while (*in)
	{
		if (isspace(*in))
		{
			switch (where)
			{
			case WHERE_START:
			case WHERE_SPACE:
				break;

			case WHERE_TEXT:
				*out = ' ';
				out++;
				where = WHERE_SPACE;
				break;
			}
		}
		else
		{
			where = WHERE_TEXT;
			*out = *in;
			out++;
		}
		in++;
	}
	*out = 0;
}

/*
 * takeInstructorLock
 * @
 *
 * Call to take the Instructor area lock
 */

int
takeInstructorLock()
{
	int trycount = 0;
	int sts;

	while (trycount < 5)
	{
		sts = WaitForSingleObject(simmgr_shm->instructor.sema, INFINITE);
		switch (sts)
		{
		case WAIT_OBJECT_0:
			return (0);
		case WAIT_ABANDONED:
			return (-1);
		}
	}
	return (-1);
}

/*
 * releaseInstructorLock
 * @
 *
 * Call to release the Instructor area lock
 */

void
releaseInstructorLock()
{
	ReleaseMutex(simmgr_shm->instructor.sema );
}

/*
 * addEvent
 * @str - pointer to event to add
 *
 * Must be called with instructor lock held
 */
void
addEvent(char* str)
{
	int eventNext;

	// Event: add to event list at end and increment eventListNext
	eventNext = simmgr_shm->eventListNext;

	sprintf_s(simmgr_shm->eventList[eventNext].eventName, STR_SIZE, "%s", str);

	snprintf(msg_buf, 2048, "Event %d: %s", eventNext, str);
	log_message("", msg_buf);

	eventNext += 1;
	if (eventNext >= EVENT_LIST_SIZE)
	{
		eventNext = 0;
	}
	simmgr_shm->eventListNext = eventNext;
	if (strcmp(str, "aed") == 0)
	{
		simmgr_shm->instructor.defibrillation.shock = 1;
	}
}

/*
 * addComment
 * @str - pointer to comment to add
 *
 * Must be called with instructor lock held
 */
void
addComment(char* str)
{
	int commentNext;

	commentNext = simmgr_shm->commentListNext;

	// Event: add to Comment list at end and increment commentListNext
	sprintf_s(simmgr_shm->commentList[commentNext].comment, STR_SIZE, "%s", str);

	commentNext += 1;
	if (commentNext >= COMMENT_LIST_SIZE)
	{
		commentNext = 0;
	}
	simmgr_shm->commentListNext = commentNext;
}

/*
 * lockAndComment
 * @str - pointer to comment to add
 *
 * Take Instructor Lock and and comment
 */
void
lockAndComment(char* str)
{
	int sts;

	sts = takeInstructorLock();
	if (!sts)
	{
		addComment(str);
		releaseInstructorLock();
	}
}

void
forceInstructorLock(void)
{
	while (takeInstructorLock())
	{
		// Force Release until we can take the lock
		releaseInstructorLock();
	}
	releaseInstructorLock();
}

void showLastError(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpp_msg;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpp_msg,
		0, NULL);
	printf("%s\n", (char *)&lpp_msg);
	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpp_msg) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	if (lpDisplayBuf)
	{
		StringCchPrintf((LPTSTR)lpDisplayBuf,
			LocalSize(lpDisplayBuf) / sizeof(TCHAR),
			TEXT("%s failed with error %d: %s"),
			lpszFunction, dw, lpp_msg);
		MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
		LocalFree(lpDisplayBuf);
	}
	LocalFree(lpp_msg);
	//ExitProcess(dw);
}

std::string GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

STARTUPINFO php_si;
PROCESS_INFORMATION php_pi;

void signalHandler(int signum)
{
	cout << "Interrupt signal (" << signum << ") received.\n";

	// cleanup and close up stuff here
	// terminate program
	CloseHandle(php_pi.hProcess);
	CloseHandle(php_pi.hThread);

	exit(signum);

}

LARGE_INTEGER
getFILETIMEoffset()
{
	SYSTEMTIME s;
	FILETIME f;
	LARGE_INTEGER t;

	s.wYear = 1970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return (t);
}

int
clock_gettime(int X, struct timeval* tv)
{
	LARGE_INTEGER           t;
	FILETIME            f;
	LONGLONG                  microseconds;
	static LARGE_INTEGER    offset;
	LONGLONG           frequencyToMicroseconds = 10;
	static int              initialized = 0;
	static BOOL             usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter(&offset);
			frequencyToMicroseconds = performanceFrequency.QuadPart / 1000000;
		}
		else {
			offset = getFILETIMEoffset();
			frequencyToMicroseconds = 10;
		}
	}
	if (usePerformanceCounter) QueryPerformanceCounter(&t);
	else {
		GetSystemTimeAsFileTime(&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	microseconds = t.QuadPart / frequencyToMicroseconds;
	t.QuadPart = microseconds;
	tv->tv_sec = (long)(t.QuadPart / 1000000);
	tv->tv_usec = (long)(t.QuadPart % 1000000);
	return (0);
}