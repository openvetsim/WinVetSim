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


//#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_MAXVER
//#include "version.h"
int checkProcessRunning(void);

#ifdef NDEBUG
// Windows Header Files
//#include <stdlib.h>
//#include <string.h>
//#include <tchar.h>
#include <strsafe.h>
#include <afxwin.h>
//#include <tlhelp32.h>

// C RunTime Header Files
//#include <malloc.h>
//#include <memory.h>
#include <shellapi.h>


#include "vetsim.h"

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
static HINSTANCE ghInstance = NULL;

// Forward declarations of functions included in this code module:
//ATOM                MyRegisterClass(HINSTANCE hInstance);
//BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void ErrorExit(LPCTSTR lpszFunction);

HWND mainWindow;

//using namespace System::Windows::Forms;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	//UNREFERENCED_PARAMETER(hPrevInstance);
	int sts;
	MSG msg;
	BOOL bRet;
	WNDCLASS wc;

	sts = checkProcessRunning();
	if (sts == 0)
	{
		MessageBox(0, L"WinVetSim process not found", L"Error!", MB_ICONSTOP | MB_OK);
		exit(-1);
	}
	if (sts != 1)
	{
		MessageBox(0, L"An instance of WinVetSim is already running.", L"Error!", MB_ICONSTOP | MB_OK);
		exit(-1);
	}

	if (!hPrevInstance)
	{
		wc.style = 0;
		wc.lpfnWndProc = (WNDPROC)WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon((HINSTANCE)NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor((HINSTANCE)NULL, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); ; // GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName = L"MainMenu";
		wc.lpszClassName = L"MainWndClass";

		if (!RegisterClass(&wc))
			return FALSE;
	}
	initializeConfiguration();

	ghInstance = hInstance;

	WNDCLASSEX wcex;
	memset((void*)&wcex, 0, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = L"WinVetSim Menu"; // NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(0, L"Window Registration Failed!", L"Error!", MB_ICONSTOP | MB_OK);
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
	HWND hWnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		700, 500,
		NULL, NULL,
		hInstance,
		NULL	);

	if (!hWnd)
	{
		MessageBox(0, L"Window Creation Failed!", L"Error!", MB_ICONSTOP | MB_OK);
		return 1;
	}
	// The parameters to ShowWindow explained:
	//    hWnd: the value returned from CreateWindow
	//    nCmdShow: the fourth parameter from WinMain
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	mainWindow = hWnd;

	//CMenu menu;
	//ASSERT(menu.LoadMenu(IDR_MENU1));
	//CMenu* pPopup = menu.GetSubMenu(0);
	//ASSERT(pPopup != NULL);
	//pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, 0, 0, AfxGetMainWnd());

	(void)start_task("VetSim", vetsim );

	while ( (bRet = GetMessage(&msg, NULL, 0, 0)) != 0 )
	{
		if (bRet == -1)
		{
			// handle the error and possibly exit
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
HWND hButton, hCombo, hEdit, hList, hScroll, hStatic;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR greeting[] = _T("Open VetSim Simulator System");
	TCHAR leaving[] = _T("Closing WinVetSim Server");
	TCHAR buffer[128] = { 0, };
	HWND hwndCombo;
	int cTxtLen;
	PSTR pszMem;

	switch (message)
	{
	
	case WM_CREATE:
		/*hButton = CreateWindowEx(
			NULL,
			L"Button",
			L"Button Example",
			WS_BORDER | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			20, 20,
			100, 30,
			hWnd, NULL,
			ghInstance,
			NULL); */
		/*hCombo = CreateWindowEx(
			NULL,
			L"ComboBox",
			L"darkblue",
			WS_BORDER | WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
			20, 50,
			100, 100,
			hWnd, NULL,
			ghInstance,
			NULL);*/
		/*hEdit = CreateWindowEx(
			NULL,
			L"Edit",
			L"edit box example",
			WS_BORDER | WS_CHILD | WS_VISIBLE,
			20, 300,
			100, 300,
			hWnd, NULL,
			ghInstance,
			NULL);*/
		/*hList = CreateWindowEx(
			NULL,
			L"ListBox",
			L"db db db",
			WS_BORDER | WS_CHILD | WS_VISIBLE,
			100, 0,
			100, 200,
			hWnd, NULL,
			ghInstance,
			NULL); */
		/*hScroll = CreateWindowEx(
			NULL,
			L"ScrollBar",
			L"",
			WS_BORDER | WS_CHILD | WS_VISIBLE | SBS_VERT,
			10, 50,
			650, 400,
			hWnd, NULL,
			ghInstance,
			NULL); */
		/*hStatic = CreateWindowEx(
			NULL,
			L"Static",
			L"",
			WS_BORDER | WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
			300, 90,
			100, 30,
			hWnd, NULL,
			ghInstance,
			NULL);*/
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		// Print greeting in the top left corner.
		TextOut(hdc,
			10, 
			10,
			greeting, 
			(int)_tcslen(greeting));
		swprintf_s(buffer, L"SimMgr Build %d.%d.%d\n", SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN, SIMMGR_VERSION_BUILD);
		TextOut(hdc,
			10, 
			30,
			buffer, 
			(int)_tcslen(buffer));
		if (PHP_SERVER_PORT == 80)
		{
			swprintf_s(buffer, L"Control URL: http://%hs/sim-ii/ii.php\n", PHP_SERVER_ADDR);
		}
		else
		{
			swprintf_s(buffer, L"Control URL: http://%hs:%d/sim-ii/ii.php\n", PHP_SERVER_ADDR, PHP_SERVER_PORT);
		}
		TextOut(hdc,
			10,
			50,
			buffer,
			(int)_tcslen(buffer));
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

	case WM_CLOSE:
		DestroyWindow(hWnd);
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

	if ( lpMsgBuf != NULL ) LocalFree(lpMsgBuf);
	if ( lpDisplayBuf != NULL )	LocalFree(lpDisplayBuf);
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
				printf("%S: SimMgr %d.%d.%d\n", ptr, SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN, SIMMGR_VERSION_BUILD);
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
using namespace std;

int main(int argc, char *argv[] )
{
	int i;
	char *ptr;
	int sts;

	sts = checkProcessRunning();
	if (sts == 0)
	{
		MessageBox(0, L"WinVetSim process not found", L"Error!", MB_ICONSTOP | MB_OK);
		exit(-1);
	}
	if (sts != 1)
	{
		MessageBox(0, L"An instance of WinVetSim is already running.", L"Error!", MB_ICONSTOP | MB_OK);
		exit(-1);
	}
	initializeConfiguration();
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
				printf("%s: SimMgr %d.%d.%d\n", ptr, SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN, SIMMGR_VERSION_BUILD);
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

#include <comdef.h>
int checkProcessRunning(void)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	int count = 0;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			_bstr_t b(entry.szExeFile);
			const char* c = b;

			if (_stricmp(c, "WinVetSim.exe") == 0)
			{
				//HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
				// Do stuff..
				// CloseHandle(hProcess);
				count++;
			}
		}
	}
	CloseHandle(snapshot);
	return count;
}

void
initializeConfiguration(void)
{
	// Set configurable parameters to defaults
	localConfig.port_pulse = DEFAULT_PORT_PULSE;
	localConfig.port_status = DEFAULT_PORT_STATUS;
	localConfig.php_server_port = DEFAULT_PHP_SERVER_PORT;
	sprintf_s(localConfig.php_server_addr, "%s", DEFAULT_PHP_SERVER_ADDRESS);
	sprintf_s(localConfig.log_name, "%s", DEFAULT_LOG_NAME);
	sprintf_s(localConfig.html_path, "%s", DEFAULT_HTML_PATH);

	// Allow parameters to be overridedn from registry
	getKeys();  
}