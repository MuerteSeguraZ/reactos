/*
 * PROJECT:     ReactOS TAPI Service
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     TSP loading, enumeration and event routing
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(tapisrv);

static const CHAR s_szProvidersKey[] =
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Providers";

VOID CALLBACK
TapiLineEventProc(
    _In_ HTAPILINE htLine,
    _In_ HTAPICALL htCall,
    _In_ DWORD dwMsg,
    _In_ DWORD_PTR dwParam1,
    _In_ DWORD_PTR dwParam2,
    _In_ DWORD_PTR dwParam3)
{
    PTAPI_LINE pLine = (PTAPI_LINE)(ULONG_PTR)htLine;

    TRACE("LineEvent: line=%p msg=%lu p1=%Iu p2=%Iu p3=%Iu\n",
          htLine, dwMsg, dwParam1, dwParam2, dwParam3);

    if (!pLine || !pLine->pApp)
    {
        return;
    }

    TapiPostEvent(pLine->pApp,
                  dwMsg,
                  (DWORD)dwParam1,
                  (DWORD)dwParam2,
                  (DWORD)dwParam3);
}

VOID CALLBACK
TapiAsyncCompletion(
    _In_ DRV_REQUESTID dwRequestID,
    _In_ LONG lResult)
{
    DWORD i;

    TRACE("AsyncCompletion: reqID=%lu result=%ld\n", dwRequestID, lResult);

    /* Find the line this request ID is from and notify its app */
    EnterCriticalSection(&g_Tapi.Lock);
    for (i = 0; i < ARRAYSIZE(g_Tapi.pCalls); i++)
    {
        PTAPI_CALL pCall = g_Tapi.pCalls[i];
        if (pCall && pCall->pLine && pCall->pLine->pApp)
        {
            TapiPostEvent(pCall->pLine->pApp,
                          LINE_REPLY,
                          dwRequestID,
                          (DWORD)lResult,
                          0);
            break;
        }
    }
    LeaveCriticalSection(&g_Tapi.Lock);
}

static PROC
GetTSPIProc(
    _In_ HMODULE hModule,
    _In_ LPCSTR pszName,
    _In_ BOOL bRequired)
{
    PROC pfn = GetProcAddress(hModule, pszName);
    if (!pfn && bRequired)
        WARN("TSP missing required export: %s\n", pszName);
    return pfn;
}

_Must_inspect_result_
DWORD
TapiLoadProviders(VOID)
{
    HKEY hKey = NULL;
    LONG lRet;
    DWORD dwNumProviders = 0;
    DWORD cbData;
    DWORD dwType;
    DWORD i;
    DWORD dwDeviceBase = 0;
    PTAPI_PROVIDER pProviders = NULL;

    OutputDebugStringA("TapiSrv: TapiLoadProviders entered\n");

    lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, s_szProvidersKey,
                          0, KEY_READ, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        OutputDebugStringA("TapiSrv: Providers key not found!\n");
        return ERROR_SUCCESS;
    }

    OutputDebugStringA("TapiSrv: Providers key opened\n");

    cbData = sizeof(dwNumProviders);
    lRet = RegQueryValueExA(hKey, "NumProviders", NULL, &dwType,
                             (LPBYTE)&dwNumProviders, &cbData);
    if (lRet != ERROR_SUCCESS || dwType != REG_DWORD || dwNumProviders == 0)
    {
        OutputDebugStringA("TapiSrv: NumProviders missing or zero!\n");
        RegCloseKey(hKey);
        return ERROR_SUCCESS;
    }

    OutputDebugStringA("TapiSrv: NumProviders found, allocating\n");

    pProviders = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                           dwNumProviders * sizeof(TAPI_PROVIDER));
    if (!pProviders)
    {
        OutputDebugStringA("TapiSrv: HeapAlloc failed!\n");
        RegCloseKey(hKey);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    OutputDebugStringA("TapiSrv: Starting provider loop\n");

    for (i = 0; i < dwNumProviders; i++)
    {
        CHAR szValName[32];
        CHAR szFilename[MAX_PATH];
        WCHAR szBaseName[MAX_PATH];
        WCHAR szFilenameW[MAX_PATH];
        WCHAR szSystem32[MAX_PATH];
        DWORD dwProviderID = 0;
        PTAPI_PROVIDER pProv = &pProviders[i];

        StringCchPrintfA(szValName, ARRAYSIZE(szValName), "ProviderID%lu", i);
        cbData = sizeof(dwProviderID);
        RegQueryValueExA(hKey, szValName, NULL, NULL,
                          (LPBYTE)&dwProviderID, &cbData);

        StringCchPrintfA(szValName, ARRAYSIZE(szValName), "ProviderFilename%lu", i);
        cbData = sizeof(szFilename);
        if (RegQueryValueExA(hKey, szValName, NULL, NULL,
                              (LPBYTE)szFilename, &cbData) != ERROR_SUCCESS)
        {
            OutputDebugStringA("TapiSrv: ProviderFilename missing!\n");
            continue;
        }

        OutputDebugStringA("TapiSrv: Got provider filename\n");

        GetSystemDirectoryW(szSystem32, ARRAYSIZE(szSystem32));
        MultiByteToWideChar(CP_ACP, 0, szFilename, -1,
                             szBaseName, ARRAYSIZE(szBaseName));
        StringCchPrintfW(szFilenameW, ARRAYSIZE(szFilenameW),
                          L"%s\\%s", szSystem32, szBaseName);

        OutputDebugStringA("TapiSrv: Calling LoadLibraryW\n");

        pProv->hModule = LoadLibraryW(szFilenameW);
        if (!pProv->hModule)
        {
            OutputDebugStringA("TapiSrv: LoadLibraryW failed!\n");
            continue;
        }

        OutputDebugStringA("TapiSrv: LoadLibraryW succeeded\n");

        pProv->dwPermanentProviderID = dwProviderID ? dwProviderID : (i + 1);

        pProv->pfnProviderEnumDevices =
            GetTSPIProc(pProv->hModule, "TSPI_providerEnumDevices", TRUE);
        pProv->pfnProviderInit =
            GetTSPIProc(pProv->hModule, "TSPI_providerInit", TRUE);
        pProv->pfnProviderShutdown =
            GetTSPIProc(pProv->hModule, "TSPI_providerShutdown", TRUE);
        pProv->pfnLineNegotiateTSPIVersion =
            GetTSPIProc(pProv->hModule, "TSPI_lineNegotiateTSPIVersion", TRUE);
        pProv->pfnLineOpen =
            GetTSPIProc(pProv->hModule, "TSPI_lineOpen", TRUE);
        pProv->pfnLineClose =
            GetTSPIProc(pProv->hModule, "TSPI_lineClose", TRUE);
        pProv->pfnLineGetDevCaps =
            GetTSPIProc(pProv->hModule, "TSPI_lineGetDevCaps", TRUE);
        pProv->pfnLineGetAddressCaps =
            GetTSPIProc(pProv->hModule, "TSPI_lineGetAddressCaps", TRUE);
        pProv->pfnLineMakeCall =
            GetTSPIProc(pProv->hModule, "TSPI_lineMakeCall", TRUE);
        pProv->pfnLineDrop =
            GetTSPIProc(pProv->hModule, "TSPI_lineDrop", TRUE);
        pProv->pfnLineAnswer =
            GetTSPIProc(pProv->hModule, "TSPI_lineAnswer", FALSE);
        pProv->pfnLineCloseCall =
            GetTSPIProc(pProv->hModule, "TSPI_lineCloseCall", FALSE);
        pProv->pfnLineGetCallStatus =
            GetTSPIProc(pProv->hModule, "TSPI_lineGetCallStatus", FALSE);

        OutputDebugStringA("TapiSrv: All exports resolved\n");

        if (!pProv->pfnProviderEnumDevices || !pProv->pfnProviderInit ||
            !pProv->pfnLineOpen || !pProv->pfnLineMakeCall)
        {
            OutputDebugStringA("TapiSrv: Missing critical exports, skipping\n");
            FreeLibrary(pProv->hModule);
            pProv->hModule = NULL;
            continue;
        }

        OutputDebugStringA("TapiSrv: Calling TSPI_providerEnumDevices\n");

        {
            typedef LONG (TSPIAPI *PFN_ENUM)(DWORD, LPDWORD, LPDWORD,
                                              HPROVIDER, LINEEVENT, PHONEEVENT);
            DWORD dwNumLines = 0, dwNumPhones = 0;
            LONG lRes;

            lRes = ((PFN_ENUM)pProv->pfnProviderEnumDevices)(
                pProv->dwPermanentProviderID,
                &dwNumLines, &dwNumPhones,
                (HPROVIDER)(ULONG_PTR)pProv,
                TapiLineEventProc,
                NULL);

            if (lRes != ERROR_SUCCESS)
            {
                OutputDebugStringA("TapiSrv: TSPI_providerEnumDevices failed!\n");
                FreeLibrary(pProv->hModule);
                pProv->hModule = NULL;
                continue;
            }

            OutputDebugStringA("TapiSrv: TSPI_providerEnumDevices succeeded\n");

            pProv->dwLineDeviceIDBase = dwDeviceBase;
            pProv->dwNumLines = dwNumLines;
            dwDeviceBase += dwNumLines;
        }

        OutputDebugStringA("TapiSrv: Calling TSPI_providerInit\n");

        {
            typedef LONG (TSPIAPI *PFN_INIT)(DWORD, DWORD, DWORD, DWORD,
                                              DWORD_PTR, DWORD_PTR,
                                              ASYNC_COMPLETION, LPDWORD);
            DWORD dwOptions = 0;
            LONG lRes;

            lRes = ((PFN_INIT)pProv->pfnProviderInit)(
                0x00020000,
                pProv->dwPermanentProviderID,
                pProv->dwLineDeviceIDBase,
                0,
                pProv->dwNumLines,
                0,
                TapiAsyncCompletion,
                &dwOptions);

            if (lRes != ERROR_SUCCESS)
            {
                OutputDebugStringA("TapiSrv: TSPI_providerInit failed!\n");
                FreeLibrary(pProv->hModule);
                pProv->hModule = NULL;
                continue;
            }

            OutputDebugStringA("TapiSrv: TSPI_providerInit succeeded\n");
        }
    }

    OutputDebugStringA("TapiSrv: Provider loop done\n");

    RegCloseKey(hKey);

    EnterCriticalSection(&g_Tapi.Lock);
    g_Tapi.pProviders = pProviders;
    g_Tapi.dwNumProviders = dwNumProviders;
    g_Tapi.dwTotalLines = dwDeviceBase;
    LeaveCriticalSection(&g_Tapi.Lock);

    OutputDebugStringA("TapiSrv: TapiLoadProviders complete\n");

    return ERROR_SUCCESS;
}

VOID
TapiUnloadProviders(VOID)
{
    DWORD i;

    EnterCriticalSection(&g_Tapi.Lock);

    if (g_Tapi.pProviders)
    {
        for (i = 0; i < g_Tapi.dwNumProviders; i++)
        {
            PTAPI_PROVIDER pProv = &g_Tapi.pProviders[i];
            if (pProv->hModule)
            {
                if (pProv->pfnProviderShutdown)
                {
                    typedef LONG (TSPIAPI *PFN_SHUTDOWN)(DWORD, DWORD);
                    ((PFN_SHUTDOWN)pProv->pfnProviderShutdown)(
                        0x00020000, pProv->dwPermanentProviderID);
                }
                FreeLibrary(pProv->hModule);
                pProv->hModule = NULL;
            }
        }
        HeapFree(GetProcessHeap(), 0, g_Tapi.pProviders);
        g_Tapi.pProviders = NULL;
        g_Tapi.dwNumProviders = 0;
        g_Tapi.dwTotalLines = 0;
    }

    LeaveCriticalSection(&g_Tapi.Lock);
}

_Must_inspect_result_
PTAPI_PROVIDER
TapiGetProviderForDevice(
    _In_ DWORD dwDeviceID)
{
    DWORD i;

    for (i = 0; i < g_Tapi.dwNumProviders; i++)
    {
        PTAPI_PROVIDER pProv = &g_Tapi.pProviders[i];
        if (pProv->hModule &&
            dwDeviceID >= pProv->dwLineDeviceIDBase &&
            dwDeviceID <  pProv->dwLineDeviceIDBase + pProv->dwNumLines)
        {
            return pProv;
        }
    }
    return NULL;
}