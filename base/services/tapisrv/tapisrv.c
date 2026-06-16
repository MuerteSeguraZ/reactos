/*
 * PROJECT:     ReactOS TAPI Service
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Service entry point and SCM control handler
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(tapisrv);

HINSTANCE g_hInstance = NULL;
PSVCHOST_GLOBAL_DATA g_pSvcHostGlobals = NULL;
TAPI_GLOBALS g_Tapi = { 0 };

static WCHAR s_ServiceName[] = L"TapiSrv";
static SERVICE_STATUS_HANDLE s_hStatusHandle = NULL;
static SERVICE_STATUS s_ServiceStatus = { 0 };

static
VOID
UpdateServiceStatus(
    _In_ DWORD dwState)
{
    s_ServiceStatus.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    s_ServiceStatus.dwCurrentState = dwState;
    s_ServiceStatus.dwWin32ExitCode = 0;
    s_ServiceStatus.dwServiceSpecificExitCode = 0;
    s_ServiceStatus.dwCheckPoint = 0;

    if (dwState == SERVICE_RUNNING || dwState == SERVICE_PAUSED)
    {
        s_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                              SERVICE_ACCEPT_SHUTDOWN;
    }                                  
    else
    {
        s_ServiceStatus.dwControlsAccepted = 0;
    }

    s_ServiceStatus.dwWaitHint =
        (dwState == SERVICE_START_PENDING || dwState == SERVICE_STOP_PENDING)
        ? 10000 : 0;

    SetServiceStatus(s_hStatusHandle, &s_ServiceStatus);
}

static
DWORD
WINAPI
ServiceControlHandlerEx(
    _In_ DWORD dwControl,
    _In_ DWORD dwEventType,
    _In_ LPVOID lpEventData,
    _In_ LPVOID lpContext)
{
    UNREFERENCED_PARAMETER(dwEventType);
    UNREFERENCED_PARAMETER(lpEventData);
    UNREFERENCED_PARAMETER(lpContext);

    switch (dwControl)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            TRACE("Got Stop/Shutdown\n");
            UpdateServiceStatus(SERVICE_STOP_PENDING);
            StopRpcServer();
            TapiUnloadProviders();
            DeleteCriticalSection(&g_Tapi.Lock);
            UpdateServiceStatus(SERVICE_STOPPED);
            return ERROR_SUCCESS;

        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(s_hStatusHandle, &s_ServiceStatus);
            return ERROR_SUCCESS;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

VOID
WINAPI
SvcEntry_TapiSrv(
    _In_ INT ArgCount,
    _In_ PWSTR *ArgVector)
{
    DWORD dwError;

    UNREFERENCED_PARAMETER(ArgCount);
    UNREFERENCED_PARAMETER(ArgVector);

    OutputDebugStringA("TapiSrv: SvcEntry_TapiSrv called\n");
    TRACE("SvcEntry_TapiSrv()\n");

    s_hStatusHandle = RegisterServiceCtrlHandlerExW(s_ServiceName,
                                                     ServiceControlHandlerEx,
                                                     NULL);
    if (!s_hStatusHandle)
    {
        ERR("RegisterServiceCtrlHandlerExW failed: %lu\n", GetLastError());
        return;
    }

    UpdateServiceStatus(SERVICE_START_PENDING);

    InitializeCriticalSection(&g_Tapi.Lock);
    g_Tapi.dwNextAppHandle = 1;
    g_Tapi.dwNextLineHandle = 1;
    g_Tapi.dwNextCallHandle = 1;

    dwError = TapiLoadProviders();
    if (dwError != ERROR_SUCCESS)
    {
        ERR("TapiLoadProviders failed: %lu\n", dwError);
    }

    dwError = StartRpcServer();
    if (dwError != ERROR_SUCCESS)
    {
        ERR("StartRpcServer failed: %lu\n", dwError);
        UpdateServiceStatus(SERVICE_STOPPED);
        return;
    }

    UpdateServiceStatus(SERVICE_RUNNING);
    TRACE("TapiSrv running, %lu line device(s)\n", g_Tapi.dwTotalLines);
}

VOID
WINAPI
SvchostPushServiceGlobals(
    _In_ PSVCHOST_GLOBAL_DATA lpGlobals)
{
    g_pSvcHostGlobals = lpGlobals;
}

BOOL
WINAPI
DllMain(
    _In_ HINSTANCE hInstDLL,
    _In_ DWORD fdwReason,
    _In_ PVOID pvReserved)
{
    UNREFERENCED_PARAMETER(pvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstDLL);
            g_hInstance = hInstDLL;
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}