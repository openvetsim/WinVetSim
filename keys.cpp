/*
 * key.cpp
 *
 * Windows Registry Keys access.
 *
 * This file is part of the WinVetSim distribution (https://github.com/OpenVetSimDevelopers/sim-mgr).
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

// QueryKey - Enumerates the subkeys of key and its associated values.
// hKey - Key whose subkeys and values are to be enumerated.

#include "vetsim.h"
#include <windows.h>
#include <stdio.h>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

WCHAR achValue[MAX_VALUE_NAME];

void QueryKey(HKEY hKey)
{
	WCHAR achKey[MAX_KEY_LENGTH]; // buffer for subkey name
	DWORD cbName; // size of name string
	WCHAR achClass[MAX_PATH] = TEXT(""); // buffer for class name
	DWORD cchClassName = MAX_PATH; // size of class string
	DWORD cSubKeys = 0; // number of subkeys
	DWORD cbMaxSubKey; // longest subkey size
	DWORD cchMaxClass; // longest class string
	DWORD cValues; // number of values for key
	DWORD cchMaxValue; // longest value name
	DWORD cbMaxValueData; // longest value data
	DWORD cbSecurityDescriptor; // size of security descriptor
	FILETIME ftLastWriteTime; // last write time
	DWORD i, retCode;
	DWORD cchValue = MAX_VALUE_NAME;
	// Get the class name and the value count.
	retCode = RegQueryInfoKey(
		hKey, // key handle
		achClass, // buffer for class name
		&cchClassName, // size of class string
		NULL, // reserved
		&cSubKeys, // number of subkeys
		&cbMaxSubKey, // longest subkey size
		&cchMaxClass, // longest class string
		&cValues, // number of values for this key
		&cchMaxValue, // longest value name
		&cbMaxValueData, // longest value data
		&cbSecurityDescriptor, // security descriptor
		&ftLastWriteTime); // last write time
	wprintf(L"RegQueryInfoKey() returns %u\n", retCode);
	// Enumerate the subkeys, until RegEnumKeyEx() fails
	if (cSubKeys)
	{
		wprintf(L"\nNumber of subkeys: %d\n", cSubKeys);
		for (i = 0; i < cSubKeys; i++)
		{
			cbName = MAX_KEY_LENGTH;
			retCode = RegEnumKeyEx(hKey,
				i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime);
			if (retCode == ERROR_SUCCESS)
			{
				wprintf(L"(%d) %s\n", i + 1, achKey);
			}
		}
	}
	else
	{
		wprintf(L"No subkeys to be enumerated!\n");
	}
	// Enumerate the key values
	if (cValues)
	{
		wprintf(L"\nNumber of values: %d\n", cValues);
		for (i = 0, retCode = ERROR_SUCCESS; i < cValues; i++)
		{
			cchValue = MAX_VALUE_NAME;
			achValue[0] = '\0';
			retCode = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL,
				NULL, NULL);
			if (retCode == ERROR_SUCCESS)
			{
				wprintf(L"(%d) %s\n", i + 1, achValue);
			}
		}
	}
	else
	{
		wprintf(L"No values to be enumerated!\n");
	}
}
BOOL writeStringInRegistry(HKEY hKeyParent, LPCWSTR subkey, LPCSTR valueName, char* strData, int len)
{
	DWORD Ret;
	HKEY hKey;
	LPBYTE bptr = (LPBYTE)strData;

	//Check if the registry exists
	Ret = RegOpenKeyEx(
		hKeyParent,
		subkey,
		0,
		KEY_WRITE,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		if (ERROR_SUCCESS !=
			RegSetValueExA(
				hKey,
				valueName,
				0,
				REG_SZ,
				bptr,
				len ) )
		{
			RegCloseKey(hKey);
			return FALSE;
		}
		RegCloseKey(hKey);
		return TRUE;
	}
	return FALSE;
}

int WriteInRegistry(HKEY hKeyParent, LPCWSTR subkey, LPCWSTR valueName, DWORD data )
{
	DWORD Ret; //use to check status
	HKEY hKey; //key
	//Open the key
	Ret = RegOpenKeyEx(
		hKeyParent,
		subkey,
		0,
		KEY_WRITE,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		//Set the value in key
		if (ERROR_SUCCESS !=
			RegSetValueEx(
				hKey,
				valueName,
				0,
				REG_DWORD,
				reinterpret_cast<BYTE*>(&data),
				sizeof(data)))
		{
			RegCloseKey(hKey);
			return FALSE;
		}
		//close the key
		RegCloseKey(hKey);
		return TRUE;
	}
	return FALSE;
}
DWORD CreateRegistryKey(HKEY hKeyParent, LPCWSTR subkey)
{
	DWORD dwDisposition; //It verify new key is created or open existing key
	HKEY  hKey;
	DWORD Ret;
	Ret =
		RegCreateKeyEx(
			hKeyParent,
			subkey,
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&hKey,
			&dwDisposition);
	if (Ret != ERROR_SUCCESS)
	{
		printf("Error opening or creating key.\n");
	}
	else
	{
		RegCloseKey(hKey);
	}
	return Ret;
}
//Read data from registry
DWORD readDwordValueRegistry(HKEY hKeyParent, LPCWSTR subkey, LPCWSTR valueName, DWORD* readData)
{
	HKEY hKey;
	DWORD Ret;
	//Check if the registry exists
	Ret = RegOpenKeyExW(
		hKeyParent,
		subkey,
		0,
		KEY_READ,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		DWORD data;
		DWORD len = sizeof(DWORD);//size of data
		Ret = RegQueryValueExW(
			hKey,
			valueName,
			NULL,
			NULL,
			(LPBYTE)(&data),
			&len
		);
		if (Ret == ERROR_SUCCESS)
		{
			(*readData) = data;
		}
		RegCloseKey(hKey);
	}

	return Ret;
}

DWORD readStringFromRegistry(HKEY hKeyParent, LPCWSTR subkey, LPCSTR valueName, char* readData, DWORD len)
{
	HKEY hKey;
	DWORD readDataLen = len;

	//Check if the registry exists
	DWORD Ret = RegOpenKeyEx(
		hKeyParent,
		subkey,
		0,
		KEY_READ,
		&hKey
	);
	if (Ret == ERROR_SUCCESS)
	{
		Ret = RegQueryValueExA(
			hKey,
			valueName,
			NULL,
			NULL,
			(BYTE*)readData,
			&readDataLen
		);
		
		RegCloseKey(hKey);
	}

	return (Ret);
}
void
readSubKeys(void)
{
	DWORD data;
	DWORD Ret;
	char stringBuf[FILENAME_SIZE];
	int len;

	Ret = readDwordValueRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"PulsePortNum", &data);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		data = localConfig.port_pulse;
		Ret = WriteInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"PulsePortNum", data);
	}
	else if (Ret == ERROR_SUCCESS)
	{
		localConfig.port_pulse = data;
	}

	Ret = readDwordValueRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"StatusPortNum", &data);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		data = localConfig.port_status;
		Ret = WriteInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"StatusPortNum", data);
	}
	else if (Ret == ERROR_SUCCESS)
	{
		localConfig.port_status = data;
	}

	Ret = readDwordValueRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"ServerPortNum", &data);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		data = localConfig.php_server_port;
		Ret = WriteInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", L"ServerPortNum", data);
	}
	else if (Ret == ERROR_SUCCESS)
	{
		localConfig.php_server_port = data;
	}
	else
	{
		printf("Ret of %d is not decoded\n", Ret);
	}
	Ret = readStringFromRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "ServerAddress", stringBuf, STR_SIZE);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		len = (int)strlen(localConfig.php_server_addr);
		Ret = writeStringInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "ServerAddress", localConfig.php_server_addr, len );
	}
	else
	{
		sprintf_s(localConfig.php_server_addr, "%s", stringBuf);
	}
	Ret = readStringFromRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "LogName", stringBuf, STR_SIZE);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		len = (int)strlen(localConfig.log_name);
		Ret = writeStringInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "LogName", localConfig.log_name, len);
	}
	else
	{
		sprintf_s(localConfig.log_name, "%s", stringBuf);
	}

	Ret = readStringFromRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "HTML_Path", stringBuf, STR_SIZE);
	if (Ret == ERROR_FILE_NOT_FOUND)
	{
		len = (int)strlen(localConfig.html_path);
		Ret = writeStringInRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", "HTML_Path", localConfig.html_path, len);
	}
	else
	{
		sprintf_s(localConfig.html_path, "%s", stringBuf);
	}
}
int getKeys()
{
	HKEY theKey;
	LPCTSTR strKeyName = L"SOFTWARE\\WinVetSim";
	int rval = 0;
	long sts = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\WinVetSim", 0, KEY_READ, &theKey);
	if ( ERROR_SUCCESS == sts )
	{
		rval = 1;
	}
	else if (ERROR_NO_MATCH == sts || ERROR_FILE_NOT_FOUND == sts)
	{
		cout << "Creating registry key " << "SOFTWARE\\WinVetSim" << endl;
		PHKEY hKey = &theKey;

		long j = RegCreateKeyEx(HKEY_CURRENT_USER,		//HKEY
								L"SOFTWARE\\WinVetSim",				// lpSubKey
								0L,						// Reserved
								NULL,					// lpClass
								REG_OPTION_NON_VOLATILE, // dwOptions
								KEY_ALL_ACCESS,			//samDesired
								NULL,					// lpSecurityAttributes
								hKey,				// phkResult
								NULL);				// lpdwDisposition	

		if (ERROR_SUCCESS != j)
			cout << "Error: Could not create registry key " << "SOFTWARE\\WinVetSim" << endl << "\tERROR: " << j << GetLastErrorAsString() << endl;
		else
		{
			cout << "Success: Key created" << endl;
			rval = 1;
		}
	}
	else
	{
		
		rval = -1;
	}
	if (rval >= 0)
	{
		// Main key is present. So set SubKeys
		RegCloseKey(theKey);
		readSubKeys();

	}
	return (rval);
}

