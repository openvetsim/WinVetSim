/*
 * main.cpp
 *
 * SimMgr applicatiopn
 *
 * This file is part of the sim-mgr distribution.
 *
 * Copyright (c) 2019-2021 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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


#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_MAXVER
#ifdef NDEBUG
// Windows Header Files
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <strsafe.h>
#include <afxwin.h>


// C RunTime Header Files
#include <malloc.h>
#include <memory.h>
#include <shellapi.h>

#include "vetsimTasks.h"
#include "version.h"

#ifdef _UNICODE
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif // _UNICODE

int vetsim(void);

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance            
static TCHAR szWindowClass[] = _T("DesktopApp");	// the main window class name
static TCHAR szTitle[] = _T("WinVetSim");			// The title bar text

// Forward declarations of functions included in this code module:
//ATOM                MyRegisterClass(HINSTANCE hInstance);
//BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void ErrorExit(LPCTSTR lpszFunction);


//using namespace System::Windows::Forms;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	WNDCLASSEX wcex;
	memset((void*)&wcex, 0, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	//wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = L"WinVetSim Menu"; // NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		LPCTSTR s = _T("RegisterClassEx");
		ErrorExit(s);

		return 1;
	}


	// The parameters to CreateWindow explained:
	// szWindowClass: the name of the application
	// szTitle: the text that appears in the title bar
	// WS_OVERLAPPEDWINDOW: the type of window to create
	// CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
	// 500, 100: initial size (width, length)
	// NULL: the parent of this window
	// NULL: this application does not have a menu bar
	// hInstance: the first parameter from WinMain
	// NULL: not used in this application
	HWND hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		500, 500,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}
	// The parameters to ShowWindow explained:
	// hWnd: the value returned from CreateWindow
	// nCmdShow: the fourth parameter from WinMain
	ShowWindow(hWnd,
		nCmdShow);
	UpdateWindow(hWnd);

	//CMenu menu;
	//ASSERT(menu.LoadMenu(IDR_MENU1));
	//CMenu* pPopup = menu.GetSubMenu(0);
	//ASSERT(pPopup != NULL);
	//pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, 0, 0, AfxGetMainWnd());

	start_task("VetSim", vetsim );

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR greeting[] = _T("Open VetSim Simulator System");
	TCHAR leaving[] = _T("Closing WinVetSim Server");
	TCHAR version[128] = { 0, };

	swprintf_s(version, L"Version %d.%d\n", SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN);

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		// Print greeting in the top left corner.
		TextOut(hdc,
			5, 
			5,
			greeting, 
			(int)_tcslen(greeting));
		TextOut(hdc,
			5, 
			20,
			version, 
			(int)_tcslen(greeting));
		// End application-specific layout section.

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		hdc = BeginPaint(hWnd, &ps);
		TextOut(hdc,
			5, 5,
			leaving, (int)_tcslen(leaving));
		EndPaint(hWnd, &ps);
		stopPHPServer();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

void ErrorExit(LPCTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
	/*
	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argc > 1)
	{
		for (i = 1; i < argc; i++)
		{
			if (wcsncmp(argv[i], L"-v", 2) == 0 || wcsncmp(argv[i], L"-V", 2) == 0 ||
				wcsncmp(argv[i], L"\\v", 2) == 0 || wcsncmp(argv[i], L"\\V", 2) == 0 ||
				wcsncmp(argv[i], L"/v", 2) == 0 || wcsncmp(argv[i], L"/V", 2) == 0 ||
				wcsncmp(argv[i], L"--version", 9) == 0 || wcsncmp(argv[i], L"--Version", 9) == 0)
			{
				ptr = argv[0];
				size_t c = 0;
				while (c < wcslen(argv[0]))
				{
					if (argv[0][c] == '\\')
					{
						ptr = &argv[0][c + 1];
					}
					c++;
				}
				printf("%S: Version %d.%d\n", ptr, SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN);
				exit(0);
			}
			else
			{
				printf("Unrecognized argumment: \"%S\"\n", argv[i]);
				exit(-1);
			}
		}
	}
	vetsim();
	
	return 0;
	*/
}
#else
#include "vetsim.h"
int main(int argc, char *argv[] )
{
	int i;
	char *ptr;

	if (argc > 1)
	{
		for (i = 1; i < argc; i++)
		{
			if (strncmp(argv[i], "-v", 2) == 0 || strncmp(argv[i], "-V", 2) == 0 ||
				strncmp(argv[i], "\\v", 2) == 0 || strncmp(argv[i], "\\V", 2) == 0 ||
				strncmp(argv[i], "/v", 2) == 0 || strncmp(argv[i], "/V", 2) == 0 ||
				strncmp(argv[i], "--version", 9) == 0 || strncmp(argv[i], "--Version", 9) == 0)
			{
				ptr = argv[0];
				size_t c = 0;
				while (c < strlen(argv[0]))
				{
					if (argv[0][c] == '\\')
					{
						ptr = &argv[0][c + 1];
					}
					c++;
				}
				printf("%s: Version %d.%d\n", ptr, SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN);
				exit(0);
			}
			else
			{
				printf("Unrecognized argumment: \"%s\"\n", argv[i]);
				exit(-1);
			}
		}
	}
	vetsim();
	
	return 0;
}

#endif