/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     COM port open/close and AT command engine
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(unimodem);

/*
 * UnimodemOpenComm
 *
 * @param pLine Pointer to the modem line structure containing the port name and handle.
 *
 * @return ERROR_SUCCESS or a Win32 error code.
 */
LONG UnimodemOpenComm(PUNIMODEM_LINE pLine)
{
    CHAR szPath[32];
    DCB dcb;
    COMMTIMEOUTS ct;

    if (pLine->hComm != INVALID_HANDLE_VALUE)
        return ERROR_SUCCESS; /* already open */

    StringCbPrintfA(szPath, sizeof(szPath), "\\\\.\\%s", pLine->szPort);

    pLine->hComm = CreateFileA(szPath,
                                GENERIC_READ | GENERIC_WRITE,
                                0, NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
    if (pLine->hComm == INVALID_HANDLE_VALUE)
    {
        DWORD dwErr = GetLastError();
        WARN("Cannot open %s: error %lu\n", szPath, dwErr);
        return (LONG)dwErr;
    }

    /* Configure port */
    ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = TRUE;
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(pLine->hComm, &dcb))
    {
        DWORD dwErr = GetLastError();
        WARN("SetCommState failed on %s: error %lu\n", szPath, dwErr);
        CloseHandle(pLine->hComm);
        pLine->hComm = INVALID_HANDLE_VALUE;
        return (LONG)dwErr;
    }

    ZeroMemory(&ct, sizeof(ct));
    ct.ReadIntervalTimeout = 100;
    ct.ReadTotalTimeoutMultiplier = 0;
    ct.ReadTotalTimeoutConstant = 5000;
    ct.WriteTotalTimeoutMultiplier = 0;
    ct.WriteTotalTimeoutConstant = 5000;
    SetCommTimeouts(pLine->hComm, &ct);

    PurgeComm(pLine->hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    TRACE("Opened %s\n", szPath);
    return ERROR_SUCCESS;
}

/*
 * UnimodemCloseComm
 *
 * @param pLine Pointer to the modem line structure whose COM port handle is to be closed.
 */
VOID UnimodemCloseComm(PUNIMODEM_LINE pLine)
{
    if (pLine->hComm == INVALID_HANDLE_VALUE)
        return;

    EscapeCommFunction(pLine->hComm, CLRDTR);
    PurgeComm(pLine->hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    CloseHandle(pLine->hComm);
    pLine->hComm = INVALID_HANDLE_VALUE;
    TRACE("Closed %s\n", pLine->szPort);
}

/*
 * UnimodemSendATCommand
 *
 * @param hComm Handle to the open COM port.
 * @param pszCmd AT command string to send (CR is appended automatically).
 * @param pszResponse Buffer that receives the last response line, or NULL.
 * @param cbResponse Size in bytes of pszResponse.
 * @param dwTimeoutMs Maximum time in milliseconds to wait for a response.
 *
 * @returns TRUE if response contains "OK", FALSE otherwise.
 */
BOOL UnimodemSendATCommand(HANDLE hComm, LPCSTR pszCmd,
                            LPSTR pszResponse, DWORD cbResponse,
                            DWORD dwTimeoutMs)
{
    CHAR szBuf[256];
    DWORD cbWritten, cbRead;
    DWORD dwPos = 0;
    DWORD dwDeadline = GetTickCount() + dwTimeoutMs;
    CHAR ch;

    StringCbPrintfA(szBuf, sizeof(szBuf), "%s\r", pszCmd);

    TRACE("AT>> %s\n", pszCmd);

    if (!WriteFile(hComm, szBuf, (DWORD)strlen(szBuf), &cbWritten, NULL) ||
        cbWritten == 0)
    {
        WARN("WriteFile failed\n");
        return FALSE;
    }

    if (pszResponse && cbResponse > 0)
    {
        pszResponse[0] = '\0';
    }

    ZeroMemory(szBuf, sizeof(szBuf));

    while (GetTickCount() < dwDeadline)
    {
        if (!ReadFile(hComm, &ch, 1, &cbRead, NULL) || cbRead == 0)
        {
            continue;
        }

        if (ch == '\r' || ch == '\n')
        {
            if (dwPos > 0)
            {
                szBuf[dwPos] = '\0';
                TRACE("AT<< %s\n", szBuf);

                if (pszResponse && cbResponse > 0)
                {
                    StringCbCopyA(pszResponse, cbResponse, szBuf);
                }

                /* Accept OK anywhere in response line */
                if (strstr(szBuf, "OK") != NULL)
                    return TRUE;

                /* Are there any terminal errors? */
                if (strstr(szBuf, "ERROR") != NULL ||
                    strstr(szBuf, "NO CARRIER") != NULL)
                    return FALSE;

                dwPos = 0;
                ZeroMemory(szBuf, sizeof(szBuf));
            }
        }
        else if (dwPos < sizeof(szBuf) - 1)
        {
            szBuf[dwPos++] = ch;
        }
    }

    WARN("AT command '%s' timed out\n", pszCmd);
    return FALSE;
}

/*
 * UnimodemWaitForConnect
 *
 * @param hComm Handle to the open COM port.
 * @param dwTimeoutMs Maximum time in milliseconds to wait for a response.
 *
 * @returns TRUE on CONNECT, FALSE on NO CARRIER / ERROR / timeout.
 */
BOOL UnimodemWaitForConnect(HANDLE hComm, DWORD dwTimeoutMs)
{
    CHAR szBuf[128];
    DWORD dwPos = 0;
    DWORD dwDeadline = GetTickCount() + dwTimeoutMs;
    CHAR ch;
    DWORD cbRead;

    ZeroMemory(szBuf, sizeof(szBuf));

    while (GetTickCount() < dwDeadline)
    {
        if (!ReadFile(hComm, &ch, 1, &cbRead, NULL) || cbRead == 0)
        {
            continue;
        }

        if (ch == '\r' || ch == '\n')
        {
            if (dwPos > 0)
            {
                szBuf[dwPos] = '\0';
                TRACE("Modem: %s\n", szBuf);

                if (strstr(szBuf, "CONNECT") != NULL)
                {
                    return TRUE;
                }

                if (strstr(szBuf, "NO CARRIER") != NULL ||
                    strstr(szBuf, "NO DIAL") != NULL    ||
                    strstr(szBuf, "BUSY")      != NULL  ||
                    strstr(szBuf, "ERROR")     != NULL)
                {
                    return FALSE;
                }

                dwPos = 0;
                ZeroMemory(szBuf, sizeof(szBuf));
            }
        }
        else if (dwPos < sizeof(szBuf) - 1)
        {
            szBuf[dwPos++] = ch;
        }
    }

    WARN("WaitForConnect timed out\n");
    return FALSE;
}
