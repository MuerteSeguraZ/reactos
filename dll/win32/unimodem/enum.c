/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Modem / COM port enumeration from registry
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(unimodem);

static const CHAR szModemClassKey[] =
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4D36E96D-E325-11CE-BFC1-08002BE10318}";

static const CHAR szSerialCommKey[] =
    "HARDWARE\\DEVICEMAP\\SERIALCOMM";

static BOOL
ReadModemSubKey(HKEY hSubKey, PUNIMODEM_LINE pLine)
{
    CHAR szPort[32];
    CHAR szInit[128];
    DWORD cbData;

    cbData = sizeof(szPort);
    if (RegQueryValueExA(hSubKey, "AttachedTo", NULL, NULL,
                         (LPBYTE)szPort, &cbData) != ERROR_SUCCESS || cbData == 0)
    {
        return FALSE;
    }

    StringCbCopyA(pLine->szPort, sizeof(pLine->szPort), szPort);

    cbData = sizeof(szInit);
    if (RegQueryValueExA(hSubKey, "UserInit", NULL, NULL,
                         (LPBYTE)szInit, &cbData) == ERROR_SUCCESS && cbData > 0)
    {
        StringCbCopyA(pLine->szInitStr, sizeof(pLine->szInitStr), szInit);
    }
    else
    {
        StringCbCopyA(pLine->szInitStr, sizeof(pLine->szInitStr), "ATZ");
    }

    pLine->hComm = INVALID_HANDLE_VALUE;
    pLine->dwSpeakerVolume = 1;
    pLine->bPulseDial = FALSE;
    InitializeCriticalSection(&pLine->Lock);
    return TRUE;
}

static PUNIMODEM_LINE
GrowArray(PUNIMODEM_LINE pLines, DWORD *pdwCapacity)
{
    DWORD dwNewCap = *pdwCapacity * 2;
    PUNIMODEM_LINE pNew = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      pLines, dwNewCap * sizeof(UNIMODEM_LINE));
    if (pNew)
    {
        *pdwCapacity = dwNewCap;
    }

    return pNew;
}

LONG UnimodemEnumeratePorts(VOID)
{
    HKEY hClassKey = NULL;
    HKEY hSubKey = NULL;
    LONG lRet;
    DWORD dwIndex;
    CHAR szSubName[32];
    DWORD cbSubName;
    PUNIMODEM_LINE pLines = NULL;
    DWORD dwCount = 0;
    DWORD dwCap = 4;

    /* Free any previous run */
    if (g_Provider.pLines)
    {
        DWORD i;

        for (i = 0; i < g_Provider.dwNumLines; i++)
        {
            DeleteCriticalSection(&g_Provider.pLines[i].Lock);
        }

        HeapFree(GetProcessHeap(), 0, g_Provider.pLines);
        g_Provider.pLines = NULL;
        g_Provider.dwNumLines = 0;
    }

    pLines = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                       dwCap * sizeof(UNIMODEM_LINE));
    if (!pLines)
    {
        return 0;
    }

    lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, szModemClassKey,
                         0, KEY_READ, &hClassKey);
    if (lRet == ERROR_SUCCESS)
    {
        for (dwIndex = 0; ; dwIndex++)
        {
            cbSubName = sizeof(szSubName);
            lRet = RegEnumKeyExA(hClassKey, dwIndex, szSubName, &cbSubName,
                                 NULL, NULL, NULL, NULL);

            if (lRet == ERROR_NO_MORE_ITEMS)
            {
                break;
            }

            if (lRet != ERROR_SUCCESS)
            {
                continue;
            }

            /* Skip non-numeric subkeys */
            if (szSubName[0] < '0' || szSubName[0] > '9')
            {
                continue;
            }

            if (RegOpenKeyExA(hClassKey, szSubName, 0,
                              KEY_READ, &hSubKey) != ERROR_SUCCESS)
            {
                continue;
            }

            if (dwCount >= dwCap)
            {
                PUNIMODEM_LINE pNew = GrowArray(pLines, &dwCap);
                if (!pNew)
                {
                  RegCloseKey(hSubKey);
                  break;
                }

                pLines = pNew;
            }

            if (ReadModemSubKey(hSubKey, &pLines[dwCount]))
            {
                TRACE("Found modem on %s (init='%s')\n",
                      pLines[dwCount].szPort, pLines[dwCount].szInitStr);
                dwCount++;
            }
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hClassKey);
    }

    if (dwCount == 0)
    {
        WARN("No INF modems. Falling back to SERIALCOMM scan\n");

        lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, szSerialCommKey,
                             0, KEY_READ, &hClassKey);
        if (lRet == ERROR_SUCCESS)
        {
            for (dwIndex = 0; ; dwIndex++)
            {
                CHAR szValName[32], szPortName[32];
                DWORD cbValName = sizeof(szValName);
                DWORD cbPortName = sizeof(szPortName);
                DWORD dwType;

                lRet = RegEnumValueA(hClassKey, dwIndex,
                                     szValName, &cbValName, NULL, &dwType,
                                     (LPBYTE)szPortName, &cbPortName);

                if (lRet == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }

                if (lRet != ERROR_SUCCESS || dwType != REG_SZ)
                {
                    continue;
                }

                if (dwCount >= dwCap)
                {
                    PUNIMODEM_LINE pNew = GrowArray(pLines, &dwCap);
                    if (!pNew)
                    {
                        break;
                    }

                    pLines = pNew;
                }

                ZeroMemory(&pLines[dwCount], sizeof(UNIMODEM_LINE));
                StringCbCopyA(pLines[dwCount].szPort,
                              sizeof(pLines[dwCount].szPort), szPortName);
                StringCbCopyA(pLines[dwCount].szInitStr,
                              sizeof(pLines[dwCount].szInitStr), "ATZ");
                pLines[dwCount].hComm = INVALID_HANDLE_VALUE;
                InitializeCriticalSection(&pLines[dwCount].Lock);
                TRACE("Fallback serial: %s\n", szPortName);
                dwCount++;
            }
            RegCloseKey(hClassKey);
        }
    }

    if (dwCount == 0)
    {
        HeapFree(GetProcessHeap(), 0, pLines);
        WARN("No modem devices found\n");
        return 0;
    }

    g_Provider.pLines = pLines;
    g_Provider.dwNumLines = dwCount;

    {
        DWORD i;
        for (i = 0; i < dwCount; i++)
        {
            g_Provider.pLines[i].dwDeviceID = g_Provider.dwLineDeviceIDBase + i;
        }
    }

    return (LONG)dwCount;
}