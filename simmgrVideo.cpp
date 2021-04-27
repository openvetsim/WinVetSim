/*
 * simmgrVideo.cpp
 *
 * Video Recording Support.
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSimDevelopers/sim-mgr).
 *
 * Copyright (c) 2019 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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
#include "sendKeys.h"
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")

using namespace std;

// CSendKeys Class
CSendKeys skey;

struct obsData obsd;



DWORD processId;
/**
 * recordStartStop
 * @record - Start if 1, Stop if 0
 *
 *
 */


BOOL CALLBACK EnumWindowsProcMy(HWND hwnd, LPARAM lParam)
{
	DWORD lpdwProcessId;
	GetWindowThreadProcessId(hwnd, &lpdwProcessId);
	if (lpdwProcessId == lParam)
	{
		obsd.obsWnd = hwnd;

		printf("Found OBS Process\n");
		return FALSE;
	}
	return TRUE;
}

void
getObsHandle(int first, const char *appName )
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	BOOL sts;
	wchar_t app[512];
	
	swprintf(app, 512, L"%S", appName );

	if (obsd.obsWnd == NULL)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

		if (Process32First(snapshot, &entry) == TRUE)
		{
			do
			{
				//wprintf(L"%s\n", entry.szExeFile);
				if (wcscmp(entry.szExeFile, app) == 0)
				{
					HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
					wprintf(L"Found App %S\n", app);
					obsd.obsPid = entry.th32ProcessID; 
					sts = EnumWindows(EnumWindowsProcMy, obsd.obsPid);
					// Do stuff..

					CloseHandle(hProcess);
				}
			} while (Process32Next(snapshot, &entry) == TRUE);
		}
		CloseHandle(snapshot);
		if (first && obsd.obsWnd == NULL)
		{
			// Not found. Start OBS and wait 2 seconds
			sts = skey.SendKeys(L"{LWIN}obs{ENTER}", 0);
			Sleep(2000);
			getObsHandle(0, "obs64.exe");
		}
	}
}
wchar_t startKeys[] = L"{F4}";
wchar_t stopKeys[] = L"{F5}";
void
recordStartStop(int record)
{
	BOOL sts;
	wchar_t keys[32];

	getObsHandle(1, "obs64.exe");

	if (obsd.obsWnd == NULL)
	{
		printf("No OBS Found\n");
		return;
	}
	skey.AppActivate(obsd.obsWnd);
	if (record)
	{
		// Send Start

		sts = skey.SendKeys(startKeys, 0);
	}
	else
	{
		sprintf_s(obsd.newFilename, sizeof(obsd.newFilename), "%s", simmgr_shm->logfile.vfilename);

		// Send Stop
		sts = skey.SendKeys(stopKeys, 0);
		obsd.obsWnd = NULL;
	}
}

off_t
getFileSize(char* fname)
{
	struct stat sbuf;
	int sts;
	off_t size = -1;

	sts = stat(fname, &sbuf);
	if (sts == 0)
	{
		size = sbuf.st_size;
	}
	return (size);
}

#include <minwinbase.h>
#include <winnt.h>
int
getLatestFile(char* rname, char* dir)
{
	struct dirent* dp;
	ULARGE_INTEGER lint;
	unsigned long long fileDate = 0;
	unsigned long long latestDate = 0;

	wchar_t fname[512] = L"C:/inetpub/wwwroot/simlogs/video/*.mkv";
	wchar_t latestFname[MAX_PATH] = { 0, };

	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;
	if ((hFind = FindFirstFile(fname, &FindFileData)) != INVALID_HANDLE_VALUE) {
		do {
			//printf("%s\n", FindFileData.cFileName);
			lint.LowPart = FindFileData.ftLastWriteTime.dwLowDateTime;
			lint.HighPart = FindFileData.ftLastWriteTime.dwHighDateTime;
			fileDate = lint.QuadPart;
			if (fileDate > latestDate)
			{
				latestDate = fileDate;
				swprintf_s(latestFname, MAX_PATH, L"%s/%S", dir, &FindFileData.cFileName);
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}

	if (wcslen(latestFname) != 0)
	{
		sprintf_s(rname, sizeof(rname), "%s", latestFname);
		return (0);
	}
	else
	{
		return (-1);
	}
}

int
renameVideoFile(char* filename)
{
	int sts;
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	char newFile[1024];
	char oldFile[1024];
	errno_t err;
	snprintf(oldFile, 1024, "%s", filename);

	err = _splitpath_s(filename, drive, _MAX_DRIVE, dir, _MAX_DRIVE, fname, _MAX_FNAME, ext, _MAX_EXT);

	snprintf(newFile, 1024, "%s/%s/%s", drive, dir, obsd.newFilename);
	printf("Rename %s to %s\n", oldFile, newFile);
	sts = rename(oldFile, newFile);
	return (sts);
}

#define MAX_FILE_WAIT_LOOPS 10
char smvbuf[512];


void
closeVideoCapture()
{
	int fsizeA = 0;
	int fsizeB = 0;
	char filename[512];
	char path[MAX_PATH];
	int loops = 0;
	int sts;

	// Wait for file to complete and then rename it.
	// 1 - Find the latest file in /var/www/html/simlogs/video
	sprintf_s(path, MAX_PATH, "%s", "/var/www/html/simlogs/video");
	sts = getLatestFile(filename, path);
	if (sts == 0)
	{
		// Found file
		printf("Last Video File is: %s\n", filename);

		// 2 - Wait until Stop has completed. Detected by no change in size for 2 seconds.
		fsizeA = getFileSize(filename);
		do {
			Sleep(2000);
			fsizeB = getFileSize(filename);
			if (fsizeA == fsizeB)
			{
				break;
			}
			fsizeA = fsizeB;
		} while (loops++ < MAX_FILE_WAIT_LOOPS);

		// 3 - Rename the file
		if (loops < MAX_FILE_WAIT_LOOPS)
		{
			sts = renameVideoFile(filename);
			if (sts != 0)
			{
				strerror_s(smvbuf, 512, errno);
				printf("renameVideoFile failed: %s", smvbuf);
			}
		}
		else
		{
			printf("File Size did not stop %d, %d", fsizeA, fsizeB);
		}
	}
}

int
getVideoFileCount(void)
{
	int file_count = 0;
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;
	wchar_t name[] = L"C:/inetpub/wwwroot/simlogs/video/*.mkv";

	if ((hFind = FindFirstFile(name, &FindFileData)) != INVALID_HANDLE_VALUE) {
		do {
			printf("%S\n", FindFileData.cFileName);
			file_count++;
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
	}

	return (file_count);
}

wchar_t testKeyStr1[] = L"This is a test\n";
wchar_t testKeyStr2[] = L"A";
wchar_t testKeyStr3[] = L"B";

//**********************************************************************
//
// Sends Win + D to toggle to the desktop
//
//**********************************************************************
void ShowDesktop()
{
	wprintf(L"Sending 'Win-D'\r\n");
	INPUT inputs[4];
	ZeroMemory(inputs, sizeof(inputs));

	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_LWIN;

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = 'D';

	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = 'D';
	inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = VK_LWIN;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

	UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
	if (uSent != ARRAYSIZE(inputs))
	{
		wprintf(L"SendInput failed: 0x%x\n", HRESULT_FROM_WIN32(GetLastError()));
	}
}

void
testKeys(void)
{
	BOOL sts;
	char pgm[] = "obs64.exe";
	HWND wnd;
	getObsHandle(0, pgm);
	if (obsd.obsWnd)
	{
		INPUT inputs[2];
		ZeroMemory(inputs, sizeof(inputs));

		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wVk = 'A';

		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.wVk = 'A';
		inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;


		wnd = obsd.obsWnd;
		printf("Got handle for %s\n", pgm );
		//skey.AppActivate(obsd.obsWnd);
		//::SendMessage(wnd, WM_SYSCOMMAND, SC_HOTKEY, (LPARAM)wnd);
		::SendMessage(wnd, WM_SYSCOMMAND, SC_RESTORE, (LPARAM)wnd);

		::ShowWindow(wnd, SW_SHOW);
		::SetForegroundWindow(wnd);
		::SetFocus(wnd);

		UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
		if (uSent != ARRAYSIZE(inputs))
		{
			wprintf(L"SendInput 'A' failed: 0x%x\n", HRESULT_FROM_WIN32(GetLastError()));
			return;
		}
		Sleep(10000);
		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wVk = 'B';

		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.wVk = 'B';
		inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

		uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
		if (uSent != ARRAYSIZE(inputs))
		{
			wprintf(L"SendInput 'B' failed: 0x%x\n", HRESULT_FROM_WIN32(GetLastError()));
			return;
		}
		//sts = skey.SendKeys(testKeyStr2, 0);
		//Sleep(10000);
		//skey.AppActivate(obsd.obsWnd);
		//sts = skey.SendKeys(testKeyStr3, 0);
		//Sleep(2000);
	}
	else
	{
		printf("Failed handle for %s\n", pgm);
	}
}