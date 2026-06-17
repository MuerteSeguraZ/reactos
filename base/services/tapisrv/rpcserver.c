/*
 * PROJECT:     ReactOS TAPI Service
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     RPC server, implements ITapSrv interface
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"
#include <tapisrv_s.h>

WINE_DEFAULT_DEBUG_CHANNEL(tapisrv);

void __RPC_FAR * __RPC_USER midl_user_allocate(SIZE_T len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}

void __RPC_USER midl_user_free(void __RPC_FAR *ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

_Must_inspect_result_
DWORD
StartRpcServer(VOID)
{
    NTSTATUS Status;
    Status = g_pSvcHostGlobals->StartRpcServer(L"TapiSrv", ITapSrv_v1_0_s_ifspec);
    TRACE("StartRpcServer -> 0x%08lx\n", Status);
    return RtlNtStatusToDosError(Status);
}

DWORD
StopRpcServer(VOID)
{
    NTSTATUS Status;
    Status = g_pSvcHostGlobals->StopRpcServer(ITapSrv_v1_0_s_ifspec);
    TRACE("StopRpcServer -> 0x%08lx\n", Status);
    return RtlNtStatusToDosError(Status);
}

DWORD __stdcall
TapSrvInitialize(
    _In_  handle_t hBinding,
    _In_  DWORD dwAPIVersion,
    _Out_ DWORD *lpdwNumDevs,
    _Out_ DWORD *lpdwAppHandle)
{
    PTAPI_APP pApp;
    DWORD dwError;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvInitialize(ver=%08lx)\n", dwAPIVersion);

    *lpdwNumDevs = 0;
    *lpdwAppHandle = 0;

    EnterCriticalSection(&g_Tapi.Lock);
    dwError = TapiAllocApp(&pApp);
    if (dwError == ERROR_SUCCESS)
    {
        *lpdwNumDevs   = g_Tapi.dwTotalLines;
        *lpdwAppHandle = pApp->dwAppHandle;
    }
    LeaveCriticalSection(&g_Tapi.Lock);

    return dwError;
}

DWORD __stdcall
TapSrvShutdown(
    _In_ handle_t hBinding,
    _In_ DWORD dwAppHandle)
{
    PTAPI_APP pApp;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvShutdown(app=%lu)\n", dwAppHandle);

    EnterCriticalSection(&g_Tapi.Lock);
    pApp = TapiFindApp(dwAppHandle);

    if (pApp)
    {
        TapiFreeApp(pApp);
    }

    LeaveCriticalSection(&g_Tapi.Lock);

    return pApp ? ERROR_SUCCESS : ERROR_INVALID_HANDLE;
}

DWORD __stdcall
TapSrvNegotiateAPIVersion(
    _In_  handle_t hBinding,
    _In_  DWORD dwDeviceID,
    _In_  DWORD dwAPILowVersion,
    _In_  DWORD dwAPIHighVersion,
    _Out_ DWORD *lpdwAPIVersion)
{
    PTAPI_PROVIDER pProv;
    LONG lRes;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvNegotiateAPIVersion(dev=%lu)\n", dwDeviceID);

    *lpdwAPIVersion = 0;

    pProv = TapiGetProviderForDevice(dwDeviceID);

    if (!pProv)
    {
        return LINEERR_BADDEVICEID;
    }

    {
        typedef LONG (TSPIAPI *PFN)(DWORD, DWORD, DWORD, LPDWORD);
        lRes = ((PFN)pProv->pfnLineNegotiateTSPIVersion)(
            dwDeviceID, dwAPILowVersion, dwAPIHighVersion, lpdwAPIVersion);
    }

    return (lRes == ERROR_SUCCESS) ? ERROR_SUCCESS : (DWORD)lRes;
}

DWORD __stdcall
TapSrvGetDevCaps(
    _In_ handle_t hBinding,
    _In_ DWORD dwDeviceID,
    _In_ DWORD dwAPIVersion,
    _In_ DWORD dwExtVersion,
    _In_ DWORD dwTotalSize,
    _Out_writes_bytes_(dwTotalSize) BYTE *lpDevCaps)
{
    PTAPI_PROVIDER pProv;
    LONG lRes;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvGetDevCaps(dev=%lu)\n", dwDeviceID);

    pProv = TapiGetProviderForDevice(dwDeviceID);

    if (!pProv)
    {
        return LINEERR_BADDEVICEID;
    }

    ((LPLINEDEVCAPS)lpDevCaps)->dwTotalSize = dwTotalSize;

    {
        typedef LONG (TSPIAPI *PFN)(DWORD, DWORD, DWORD, LPLINEDEVCAPS);
        lRes = ((PFN)pProv->pfnLineGetDevCaps)(
            dwDeviceID, dwAPIVersion, dwExtVersion,
            (LPLINEDEVCAPS)lpDevCaps);
    }

    return (lRes == ERROR_SUCCESS) ? ERROR_SUCCESS : (DWORD)lRes;
}

DWORD __stdcall
TapSrvGetAddressCaps(
    _In_ handle_t hBinding,
    _In_ DWORD dwDeviceID,
    _In_ DWORD dwAddressID,
    _In_ DWORD dwAPIVersion,
    _In_ DWORD dwExtVersion,
    _In_ DWORD dwTotalSize,
    _Out_writes_bytes_(dwTotalSize) BYTE *lpAddressCaps)
{
    PTAPI_PROVIDER pProv; 
    LONG lRes;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvGetAddressCaps(dev=%lu addr=%lu)\n", dwDeviceID, dwAddressID);

    pProv = TapiGetProviderForDevice(dwDeviceID);

    if (!pProv)
    {
        return LINEERR_BADDEVICEID;
    }

    ((LPLINEADDRESSCAPS)lpAddressCaps)->dwTotalSize = dwTotalSize;

    {
        typedef LONG (TSPIAPI *PFN)(DWORD, DWORD, DWORD, DWORD, LPLINEADDRESSCAPS);
        lRes = ((PFN)pProv->pfnLineGetAddressCaps)(
            dwDeviceID, dwAddressID, dwAPIVersion, dwExtVersion,
            (LPLINEADDRESSCAPS)lpAddressCaps);
    }

    return (lRes == ERROR_SUCCESS) ? ERROR_SUCCESS : (DWORD)lRes;
}

DWORD __stdcall
TapSrvOpenLine(
    _In_  handle_t hBinding,
    _In_  DWORD dwAppHandle,
    _In_  DWORD dwDeviceID,
    _In_  DWORD dwAPIVersion,
    _In_  DWORD dwMediaModes,
    _In_  DWORD dwPrivileges,
    _Out_ DWORD *lpdwLineHandle)
{
    PTAPI_PROVIDER pProv;
    PTAPI_APP pApp;
    PTAPI_LINE pLine;
    HDRVLINE hdLine = NULL;
    LONG lRes;
    DWORD dwError;

    UNREFERENCED_PARAMETER(hBinding);
    UNREFERENCED_PARAMETER(dwMediaModes);
    UNREFERENCED_PARAMETER(dwPrivileges);
    TRACE("TapSrvOpenLine(dev=%lu app=%lu)\n", dwDeviceID, dwAppHandle);

    *lpdwLineHandle = 0;

    EnterCriticalSection(&g_Tapi.Lock);

    pApp = TapiFindApp(dwAppHandle);

    if (!pApp)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return ERROR_INVALID_HANDLE;
    }

    pProv = TapiGetProviderForDevice(dwDeviceID);

    if (!pProv)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return LINEERR_BADDEVICEID;
    }

    dwError = TapiAllocLine(pApp, dwDeviceID, pProv, NULL, &pLine);

    if (dwError != ERROR_SUCCESS)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return dwError;
    }

    {
        typedef LONG (TSPIAPI *PFN)(DWORD, HTAPILINE, LPHDRVLINE,
                                     DWORD, LINEEVENT);
        lRes = ((PFN)pProv->pfnLineOpen)(
            dwDeviceID,
            (HTAPILINE)(ULONG_PTR)pLine,
            &hdLine,
            dwAPIVersion,
            TapiLineEventProc);
    }

    if (lRes != ERROR_SUCCESS)
    {
        TapiFreeLine(pLine);
        LeaveCriticalSection(&g_Tapi.Lock);
        return (DWORD)lRes;
    }

    pLine->hdLine = hdLine;
    *lpdwLineHandle = pLine->dwLineHandle;

    LeaveCriticalSection(&g_Tapi.Lock);
    return ERROR_SUCCESS;
}

DWORD __stdcall
TapSrvCloseLine(
    _In_ handle_t hBinding,
    _In_ DWORD dwLineHandle)
{
    PTAPI_LINE pLine;
    PTAPI_PROVIDER pProv;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvCloseLine(line=%lu)\n", dwLineHandle);

    EnterCriticalSection(&g_Tapi.Lock);

    pLine = TapiFindLine(dwLineHandle);

    if (!pLine)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return ERROR_INVALID_HANDLE;
    }

    pProv = pLine->pProvider;

    if (pProv && pProv->pfnLineClose)
    {
        typedef LONG (TSPIAPI *PFN)(HDRVLINE);
        ((PFN)pProv->pfnLineClose)(pLine->hdLine);
    }

    TapiFreeLine(pLine);
    LeaveCriticalSection(&g_Tapi.Lock);
    return ERROR_SUCCESS;
}

DWORD __stdcall
TapSrvMakeCall(
    _In_ handle_t hBinding,
    _In_ DWORD dwLineHandle,
    _In_z_ WCHAR *lpszDestAddress,
    _In_ DWORD dwCountryCode,
    _Out_ DWORD *lpdwCallHandle)
{
    PTAPI_LINE pLine;
    PTAPI_PROVIDER pProv;
    PTAPI_CALL pCall;
    HDRVCALL hdCall = NULL;
    LONG lRes;
    DWORD dwError;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvMakeCall(line=%lu dest=%S)\n", dwLineHandle, lpszDestAddress);

    *lpdwCallHandle = 0;

    EnterCriticalSection(&g_Tapi.Lock);

    pLine = TapiFindLine(dwLineHandle);

    if (!pLine)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return ERROR_INVALID_HANDLE;
    }

    pProv = pLine->pProvider;

    dwError = TapiAllocCall(pLine, NULL, &pCall);

    if (dwError != ERROR_SUCCESS)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return dwError;
    }

    {
        typedef LONG (TSPIAPI *PFN)(DRV_REQUESTID, HDRVLINE, HTAPICALL,
                                     LPHDRVCALL, LPCWSTR, DWORD,
                                     LPLINECALLPARAMS);
        lRes = ((PFN)pProv->pfnLineMakeCall)(
            (DRV_REQUESTID)pCall->dwCallHandle,
            pLine->hdLine,
            (HTAPICALL)(ULONG_PTR)pCall,
            &hdCall,
            lpszDestAddress,
            dwCountryCode,
            NULL);
    }

    if (lRes != ERROR_SUCCESS && lRes != (LONG)pCall->dwCallHandle)
    {
        TapiFreeCall(pCall);
        LeaveCriticalSection(&g_Tapi.Lock);
        return (DWORD)lRes;
    }

    pCall->hdCall = hdCall;
    *lpdwCallHandle = pCall->dwCallHandle;

    LeaveCriticalSection(&g_Tapi.Lock);
    return ERROR_SUCCESS;
}

DWORD __stdcall
TapSrvDrop(
    _In_ handle_t hBinding,
    _In_ DWORD dwCallHandle)
{
    PTAPI_CALL pCall;
    PTAPI_PROVIDER pProv;
    LONG lRes = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvDrop(call=%lu)\n", dwCallHandle);

    EnterCriticalSection(&g_Tapi.Lock);

    pCall = TapiFindCall(dwCallHandle);

    if (!pCall)
    {
        LeaveCriticalSection(&g_Tapi.Lock);
        return ERROR_INVALID_HANDLE;
    }

    pProv = pCall->pLine->pProvider;

    if (pProv && pProv->pfnLineDrop)
    {
        typedef LONG (TSPIAPI *PFN)(DRV_REQUESTID, HDRVCALL, LPCSTR, DWORD);
        lRes = ((PFN)pProv->pfnLineDrop)(
            (DRV_REQUESTID)dwCallHandle,
            pCall->hdCall, NULL, 0);
    }

    TapiFreeCall(pCall);
    LeaveCriticalSection(&g_Tapi.Lock);
    return (lRes == ERROR_SUCCESS || lRes == (LONG)dwCallHandle)
           ? ERROR_SUCCESS : (DWORD)lRes;
}

DWORD __stdcall
TapSrvGetCallStatus(
    _In_ handle_t hBinding,
    _In_ DWORD dwCallHandle,
    _In_ DWORD dwTotalSize,
    _Out_writes_bytes_(dwTotalSize) BYTE *lpCallStatus)
{
    PTAPI_CALL pCall;
    PTAPI_PROVIDER pProv;
    LONG lRes = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(hBinding);
    TRACE("TapSrvGetCallStatus(call=%lu)\n", dwCallHandle);

    pCall = TapiFindCall(dwCallHandle);

    if (!pCall)
    {
        return ERROR_INVALID_HANDLE;
    }

    pProv = pCall->pLine->pProvider;

    if (pProv && pProv->pfnLineGetCallStatus)
    {
        typedef LONG (TSPIAPI *PFN)(HDRVCALL, LPLINECALLSTATUS);
        ((LPLINECALLSTATUS)lpCallStatus)->dwTotalSize = dwTotalSize;
        lRes = ((PFN)pProv->pfnLineGetCallStatus)(
            pCall->hdCall, (LPLINECALLSTATUS)lpCallStatus);
    }

    return (lRes == ERROR_SUCCESS) ? ERROR_SUCCESS : (DWORD)lRes;
}

DWORD __stdcall
TapSrvGetEvent(
    _In_ handle_t hBinding,
    _In_ DWORD dwAppHandle,
    _Out_ DWORD *lpdwMsg,
    _Out_ DWORD *lpdwParam1,
    _Out_ DWORD *lpdwParam2,
    _Out_ DWORD *lpdwParam3)
{
    PTAPI_APP pApp;
    DWORD dwError = ERROR_NO_MORE_ITEMS;

    UNREFERENCED_PARAMETER(hBinding);

    *lpdwMsg = 0;
    *lpdwParam1 = 0;
    *lpdwParam2 = 0;
    *lpdwParam3 = 0;

    EnterCriticalSection(&g_Tapi.Lock);
    pApp = TapiFindApp(dwAppHandle);
    LeaveCriticalSection(&g_Tapi.Lock);

    if (!pApp)
    {
        return ERROR_INVALID_HANDLE;
    }

    EnterCriticalSection(&pApp->Lock);
    if (pApp->dwHead != pApp->dwTail)
    {
        *lpdwMsg = pApp->Events[pApp->dwHead].dwMsg;
        *lpdwParam1 = pApp->Events[pApp->dwHead].dwParam1;
        *lpdwParam2 = pApp->Events[pApp->dwHead].dwParam2;
        *lpdwParam3 = pApp->Events[pApp->dwHead].dwParam3;
        pApp->dwHead = (pApp->dwHead + 1) % TAPI_MAX_EVENTS;
        dwError = ERROR_SUCCESS;
    }
    LeaveCriticalSection(&pApp->Lock);

    return dwError;
}