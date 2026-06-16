/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     DLL entry point
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(unimodem);

HINSTANCE g_hInstance = NULL;
UNIMODEM_PROVIDER g_Provider = {0};

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            g_hInstance = hInstDLL;
            DisableThreadLibraryCalls(hInstDLL);
            TRACE("unimodem.tsp loaded\n");
            break;

        case DLL_PROCESS_DETACH:
            if (g_Provider.pLines)
            {
                DWORD i;
                for (i = 0; i < g_Provider.dwNumLines; i++)
                {
                    PUNIMODEM_LINE pLine = &g_Provider.pLines[i];
                    if (pLine->hComm != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(pLine->hComm);
                    }
                    DeleteCriticalSection(&pLine->Lock);
                }
                HeapFree(GetProcessHeap(), 0, g_Provider.pLines);
                g_Provider.pLines = NULL;
                g_Provider.dwNumLines = 0;
            }
            break;
    }
    return TRUE;
}

PUNIMODEM_LINE UnimodemGetLine(DWORD dwDeviceID)
{
    DWORD i;
    if (!g_Provider.pLines)
    {
        return NULL;
    }

    for (i = 0; i < g_Provider.dwNumLines; i++)
    {
        if (g_Provider.pLines[i].dwDeviceID == dwDeviceID)
        {
            return &g_Provider.pLines[i];
        }
    }
    
    return NULL;
}

PUNIMODEM_LINE UnimodemGetLineByHandle(HDRVLINE hdLine)
{
    return (PUNIMODEM_LINE)(ULONG_PTR)hdLine;
}

PUNIMODEM_CALL UnimodemGetCallByHandle(HDRVCALL hdCall)
{
    return (PUNIMODEM_CALL)(ULONG_PTR)hdCall;
}