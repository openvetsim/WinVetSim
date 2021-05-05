
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <fileapi.h>
#include <iostream>
#include "XMLRead.h"

void DisplayError(LPTSTR lpszFunction);

int
XMLRead::open(const char* path)
{
    LARGE_INTEGER filelen;
    size_t len;
    DWORD ol;
    BOOL sts;
    HANDLE hFile; 
    char* cptr;
    char* cptr1;
    char* cptr2;
    int length;

    depth = -1;
    fileLength = 0;
    name[0] = 0;
    value[0] = 0;
    idx = 0;
    TCHAR* tchar = new TCHAR[strlen(path) + 4];
    size_t i;
    for (i = 0; i <= strlen(path); i++)
    {
        tchar[i] = path[i];
    }

    printf("XMLRead open %s\n", path);

    hFile = CreateFile((LPCWSTR)tchar,               // file to open
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        NULL,                  // default security
        OPEN_EXISTING,         // existing file only
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DisplayError((LPTSTR)(TEXT("CreateFile")));
        _tprintf(TEXT("Terminal failure: unable to open file \"%s\" for read.\n"), tchar);
        return (-1);
    }
	sts = GetFileSizeEx(hFile, &filelen);
    if ( !sts )
    {
        DisplayError((LPTSTR)(TEXT("GetFileSizeEx")));
        _tprintf(TEXT("Terminal failure: unable to get file length\"%s\" for read.\n"), tchar);
        return (-1);
    }
    
    len = (size_t)filelen.LowPart;
    XMLRead::xml = (char *)calloc(len+32, 1);
    XMLRead::idx = 0;
    _tprintf(TEXT("ReadFile Request %d bytes.\n"), (int)len);

    sts = ReadFile(hFile, (LPVOID)&XMLRead::xml[0], (DWORD)len, &ol, NULL);
    if ( !sts || ol != len)
    {
        DisplayError((LPTSTR)(TEXT("ReadFile")));
        _tprintf(TEXT("Terminal failure: ReadFile for \"%s\" returned %d bytes with %d epected.\n"), tchar, ol, (int)len);
        printf(" %.40s...\n", XMLRead::xml);
        return (-1);
    }
    printf("Read is good\n");
    
    CloseHandle(hFile);

    cptr = XMLRead::xml;
    // printf("%s\n", cptr);
    // Strip out prolog, if present  ( replace with spaces )
    cptr1 = strstr(cptr, "<?xml");
    if (!cptr1)
    {
        cptr1 = strstr(cptr, "<?XML");
    }
    if (cptr1)
    {
        cptr2 = strstr(cptr1, "?>");
        if (cptr2)
        {
            cptr2 += 2;
            length = (int)(cptr2 - cptr1);
            memset(cptr1, ' ', length);
        }
    }

    // Strip out all comments ( replace with spaces )
    cptr1 = cptr;
    while (1)
    {
        cptr1 = strstr(cptr1, "<!--");
        if (cptr1)
        {
            cptr2 = strstr(cptr1, "-->");
            if (cptr2)
            {
                cptr2 += 3;
                length = (int)(cptr2 - cptr1);
                memset(cptr1, ' ', length);
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
       
    XMLRead::fileLength = ol;

    printf("XMLRead::open complete\n");
    return ( 0 );
}

int
XMLRead::getEntry(void)
{
    int i;
    size_t maxLen;
    char* cptr = XMLRead::xml + XMLRead::idx;
    char* cptr2;
    XMLRead::type = XML_TYPE_FILE_END;  // Default return if nothing is found

    //printf("Get Entry: ");
    // If not inElement, then skip everything up to the next 
    if (XMLRead::state == xmlParseState::initial || XMLRead::state == xmlParseState::closedElement)
    {    
        for (i = 0; cptr[i]; i++)
        {
            if (strncmp(&cptr[i], "</", 2) == 0 )
            {
                // Next is an end of element
                cptr2 = &cptr[i+2];
                for (i = 0; cptr2[i]; i++)
                {
                    if (cptr2[i] == '>' || isspace(cptr2[i]))
                    {
                        break;
                    }
                }
                strncpy_s(XMLRead::name, cptr2, size_t(i)-1);
                XMLRead::name[i] = 0;
                XMLRead::value[0] = 0;
                XMLRead::type = XML_TYPE_END_ELEMENT;
                if (XMLRead::depth > 0)
                {
                    XMLRead::depth--;
                }
                XMLRead::idx = (int)(&cptr2[i] - XMLRead::xml + 1);
                XMLRead::state = xmlParseState::closedElement;
                break;
            }
            else if (cptr[i] == '<')
            {
                cptr = &cptr[i+1];
                for (i = 0; cptr[i]; i++)
                {
                    if (cptr[i] == '>' || isspace(cptr[i]))
                    {
                        if (i > 0)
                        {
                            maxLen = (size_t)(i);
                            strncpy_s(XMLRead::name, cptr, maxLen);
                            XMLRead::name[i] = 0;
                        }
                        else
                        {
                            XMLRead::name[0] = 0;
                        }
                        XMLRead::value[0] = 0;
                        XMLRead::type = XML_TYPE_ELEMENT;
                        XMLRead::idx = (int)(&cptr[i] - XMLRead::xml + 1);
                        XMLRead::depth++;
                        XMLRead::state = xmlParseState::foundElement;
                        break;
                    }
                }
                break;
            }
        }
    }
    else if (XMLRead::state == xmlParseState::foundElement)
    {
        // Skip white space up to next text
        for (i = 0; cptr[i]; i++)
        {
            if (!isspace(cptr[i]))
            {
                cptr = &cptr[i];
                break;
            }
        }
        if (strncmp(cptr, "</", 2) == 0)
        {
            // Next is an end of element
            cptr2 = &cptr[2];
            for (i = 0; cptr2[i]; i++)
            {
                if (cptr2[i] == '>' || isspace(cptr2[i]) )
                {
                    break;
                }
            }
            strncpy_s(XMLRead::name, cptr2, size_t(i));
            XMLRead::name[i] = 0;
            XMLRead::value[0] = 0;
            XMLRead::type = XML_TYPE_END_ELEMENT;
            if (XMLRead::depth > 0)
            {
                XMLRead::depth--;
            }
            XMLRead::idx = (int)(&cptr2[i] - XMLRead::xml + 1);
            XMLRead::state = xmlParseState::closedElement;
        }
        else if (cptr[0] == '<')
        {
            // Next is an start of element
            cptr++;
            for (i = 0; cptr[i]; i++)
            {
                if (cptr[i] == '>' || isspace(cptr[i]))
                {
                    strncpy_s(XMLRead::name, cptr, size_t(i));
                    XMLRead::name[i] = 0;
                    XMLRead::value[0] = 0;
                    XMLRead::type = XML_TYPE_ELEMENT;
                    XMLRead::depth++;
                    XMLRead::idx = (int)(&cptr[i] - XMLRead::xml + 1);
                    XMLRead::state = xmlParseState::foundElement;
                    break;
                }
            }
        }
        else
        {
            // Not a tag open or close, so return any content up to the next tag
            for ( i = 0 ; cptr[i]; i++)
            {
                if (cptr[i] == '<')
                {
                    XMLRead::value[i] = 0;
                    XMLRead::state = xmlParseState::returnedText;
                    XMLRead::type = XML_TYPE_TEXT;
                    XMLRead::idx = (int)(&cptr[i] - XMLRead::xml);
                    break;
                }
                else
                {
                    XMLRead::value[i] = cptr[i];
                }
            }
        }
    }
    else if (XMLRead::state == xmlParseState::returnedText)
    { 
        // Skip white space up to next text
        for (i = 0; cptr[i]; i++)
        {
            if (!isspace(cptr[i]))
            {
                cptr = &cptr[i];
                break;
            }
        }
        if (strncmp(cptr, "</", 2) == 0)
        {
            // Next is an end of element
            cptr2 = &cptr[2];
            for (i = 0; cptr2[i]; i++)
            {
                if (cptr2[i] == '>')
                {
                    cptr2 = &cptr2[i];
                    break;
                }
            }
            strncpy_s(XMLRead::name, &cptr[2], size_t(i));
            XMLRead::name[i + 1] = 0;
            XMLRead::value[0] = 0;
            XMLRead::type = XML_TYPE_END_ELEMENT;
            if (XMLRead::depth > 0)
            {
                XMLRead::depth--;
            }
            XMLRead::idx = XMLRead::idx + i;
            XMLRead::state = xmlParseState::closedElement;
        }
        else if (cptr[0] == '<')
        {
            // Next is an start of element
            cptr++;
            for (i = 0; cptr[i]; i++)
            {
                if (cptr[i] == '>' || isspace(cptr[i]))
                {
                    strncpy_s(XMLRead::name, cptr, size_t(i));

                    XMLRead::name[i] = 0;
                    XMLRead::value[0] = 0;
                    XMLRead::type = XML_TYPE_ELEMENT;
                    XMLRead::depth++;
                    XMLRead::idx = (int)(&cptr[i] - XMLRead::xml + 1);
                    XMLRead::state = xmlParseState::foundElement;
                    break;
                }
            }
        }
        else
        {
            XMLRead::name[0] = 0;
            XMLRead::value[0] = 0;
            XMLRead::type = XML_TYPE_NONE;
            XMLRead::idx = (int)(&cptr[i] - XMLRead::xml + 1);
            XMLRead::state = xmlParseState::initial;
        }
    }
    if (XMLRead::type == XML_TYPE_FILE_END)
        return (1);
    else
        return (0);
}
void DisplayError(LPTSTR lpszFunction)
// Routine Description:
// Retrieve and output the system error message for the last-error code
{
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();
    SIZE_T length;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL);
    length = lstrlen((LPCTSTR)lpMsgBuf);
    length += lstrlen((LPCTSTR)lpszFunction);
    length += 40;  // account for format string
    length *= sizeof(TCHAR);
    lpDisplayBuf =
        (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            length );
    if (lpDisplayBuf)
    {
        if (FAILED(StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("%s failed with error code %d as follows:\n%s"),
            lpszFunction,
            dw,
            (LPTSTR)lpMsgBuf)))
        {
            printf("FATAL ERROR: Unable to output error code.\n");
        }

        _tprintf(TEXT("ERROR: %s\n"), (LPCTSTR)lpDisplayBuf);

        
        LocalFree(lpDisplayBuf);
    }
    LocalFree(lpMsgBuf);
}