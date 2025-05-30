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

using namespace std;

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
initSHM(void )
{
	extern struct simmgr_shm shmSpace;
	simmgr_shm = &shmSpace;

	return (0);
}
/*
 * Function: getTimeStr
 *
 * Get the current timestamp for logging
 *
 * Parameters: pointer to buffer to receive the timestamp
 *
 * Returns: pointer to buffer to receive the timestamp
 */
char *
getTimeStr(char* timeStr)
{
	time_t timer;
	struct tm tm_info;

	timer = time(NULL);
	localtime_s(&tm_info, &timer);
	strftime(timeStr, 26, "%Y-%m-%d %H:%M:%S", &tm_info);
	return(timeStr);
}

/*
 * Function: log_message_init
 *
 * Create sema for limiting access to the common log file.
 * Write a startup message into the file.
 *
 * Parameters: none
 *
 * Returns: none
 */

#include <filesystem>
namespace fs = std::filesystem;

char log_dir[512] = { 0, };
char default_log_file[512] = { 0, };


HANDLE log_sema;
void
log_message_init(void)
{
	sprintf_s(log_dir, 512, "%s/simlogs", localConfig.html_path );
	printf("log_dir is %s\n", log_dir);
	sprintf_s(default_log_file, 512, "%s/simlogs/vetsim.log", localConfig.html_path);

	DWORD ftyp = GetFileAttributesA(log_dir);
	if (ftyp == INVALID_FILE_ATTRIBUTES)
	{
		// simlogs file does not exist. Create it.
		CreateDirectoryA(log_dir, NULL);
	}
	log_sema = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex

	log_message("", "Log Started");

	// Restore current directory

	//fs::current_path(pwd);
}
#ifdef NDEBUG
#include <windows.h>
#include <string>

extern HWND hEdit;

void append_text_to_edit(const wchar_t* newText)
{
	if (!hEdit) return;

	// Get current length
	int len = GetWindowTextLengthW(hEdit);

	// Allocate buffer for current + new text
	std::wstring buffer;
	buffer.resize(len);

	if (len > 0)
		GetWindowTextW(hEdit, &buffer[0], len + 1);

	// Append new text (with newline)
	buffer.append(newText);
	buffer.append(L"\r\n");

	// Set updated text
	SetWindowTextW(hEdit, buffer.c_str());

	// Scroll to bottom
	SendMessageW(hEdit, EM_SETSEL, buffer.length(), buffer.length());
	SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
}
#endif
/*
 * Function: log_message
 *
 * Log a message to the common log file or to a named file. The file is opened
 * for Append and closed on completion of the write. (The file write will be disabled after
 * development is completed).
 *
 * Parameters: filename - filename to open for writing, NULL for default log file
 *             message - Pointer to message string, NULL terminated
 *
 * Returns: none
 */


void log_message(const char* filename, const char* message)
{
	FILE* logfile;
	//LPCSTR lpMessage;
	errno_t err;
	char timeBuf[32];
	size_t convertedChars = 0;
	size_t origionalSize = strlen(message) + 1;
	size_t maxSize = 512;
	int sts;
	
	sts = WaitForSingleObject(log_sema, 1000 );
	if (sts == WAIT_OBJECT_0)
	{
		if (strlen(filename) > 0)
		{
			err = fopen_s(&logfile, filename, "a");
		}
		else
		{
			err = fopen_s(&logfile, default_log_file, "a");
		}

		if (err)
		{
			wchar_t tbuf[1024];
			char pstr[1024];
			string errstr = GetLastErrorAsString();
			sprintf_s(pstr, sizeof(pstr), "fopen_s %s returns %d: %s\n", default_log_file, err, errstr.c_str());
			mbstowcs_s(&convertedChars,
				tbuf,
				1024,
				(const char *)pstr,
				1024 );
			MessageBoxW(NULL, tbuf, tbuf, MB_OK);
		}
		if (logfile)
		{
			(void)getTimeStr(timeBuf);
			fprintf(logfile, "%s: %s\n", timeBuf, message);
			fclose(logfile);
		}
		
		wchar_t wcstring[512+4];
		//lpMessage = message;
		err = mbstowcs_s(&convertedChars, wcstring, origionalSize, message, maxSize);
		  
        errno_t err = wcscat_s(wcstring, 516, L"\n");  
        if (err != 0) {  
           // Handle the error appropriately, e.g., log or display an error message  
           wprintf(L"Error concatenating strings: %d\n", err);  
        }
		//printf("%s\n", message);
		//OutputDebugStringA(lpMessage);
		//MessageBox(0, wcstring, L"", MB_ICONSTOP | MB_OK);
		//SetWindowText(hEdit, wcstring);
#ifdef NDEBUG

		append_text_to_edit(wcstring);

		HFONT hFont, hOldFont;
		extern HWND mainWindow;
		HDC hdc;
		PAINTSTRUCT ps;

		hdc = BeginPaint(mainWindow, &ps);

		// Retrieve a handle to the variable stock font.  
		hFont = (HFONT)GetStockObject(ANSI_VAR_FONT);

		// Select the variable stock font into the specified device context. 
		if (hOldFont = (HFONT)SelectObject(hdc, hFont))
		{
			// Display the text string.
			mbstowcs_s(&convertedChars, wcstring, origionalSize, message, maxSize);
			TextOutW(hdc, 5, 40, wcstring, (int)convertedChars);

			// Restore the original font.        
			SelectObject(hdc, hOldFont);
		}
		EndPaint(mainWindow, &ps);
#endif
		ReleaseMutex(log_sema);
	}
}

//void log_messaget(const char* filename, TCHAR* message)
//{

//#define MAX_LENGTH 500
//	char szString[MAX_LENGTH];
//	size_t nNumCharConverted;
//	wcstombs_s(&nNumCharConverted, szString, MAX_LENGTH,
//		message, MAX_LENGTH);

//	log_message(filename, szString);
//}

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
get_date(char* buffer, int maxLen )
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
	eventNext = simmgr_shm->eventListNextWrite;

	sprintf_s(simmgr_shm->eventList[eventNext].eventName, STR_SIZE, "%s", str);

	snprintf(msg_buf, 2048, "Event %d: %s", eventNext, str);
	log_message("", msg_buf);

	eventNext += 1;
	if (eventNext >= EVENT_LIST_SIZE)
	{
		eventNext = 0;
	}
	simmgr_shm->eventListNextWrite = eventNext;
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

	// Truncate overly large comments
	if (strlen(str) >= COMMENT_SIZE)
	{
		str[COMMENT_SIZE - 1] = 0;
	}
	// Event: add to Comment list at end and increment commentListNext
	sprintf_s(simmgr_shm->commentList[commentNext].comment, COMMENT_SIZE, "%s", str);

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
			lpszFunction, dw, lpp_msg );
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
#ifdef DEBUG
	printf("Close Window to Exit\n");
	while (1)
	{
		Sleep(10);
	}
#else
	exit(signum);
#endif
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
/*
 * getDcode
 *
 * Get the current date code for the simulation
 * The date code is a 10 digit number in the format YYYYMMDDHH
 * YYYY = year since 1900
 * MM = Month (1-12)
 * DD = Day of Month (1-31)
 * HH = hour of day (0-23)
 *
 * Returns: dcode - the date code
 */
__int64
getDcode(void)
{
	struct tm newtime;
	__time64_t long_time;
    
    uint64_t dcode; // Replace __uint64 with uint64_t
	errno_t sts;

	_time64(&long_time);
	sts = _localtime64_s(&newtime, &long_time);
	if (sts)
	{
		dcode = 0;
	}
	else
	{
		uint64_t year = (newtime.tm_year + 1900);
		uint64_t month = newtime.tm_mon + 1;
		uint64_t day = newtime.tm_mday;
		uint64_t hour = newtime.tm_hour;

		//printf("%Iu %Iu %Iu %Iu", year, month, day, hour);

		dcode =  year * 1000000;
		dcode += month  * 10000;
		dcode += day      * 100;
		dcode += hour;
	}
	return dcode;
}

