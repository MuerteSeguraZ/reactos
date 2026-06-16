/*
 * PROJECT:     ReactOS TAPI Service
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Precompiled header
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#ifndef _TAPISRV_PCH_
#define _TAPISRV_PCH_

#include <stdarg.h>
#include <limits.h>

#define WIN32_NO_STATUS
#define _INC_WINDOWS
#define COM_NO_WINDOWS_H

#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <winsvc.h>
#include <winnls.h>
#include <strsafe.h>
#include <tapi.h>
#include <tspi.h>
#include <svc.h>

#define NTOS_MODE_USER
#include <ndk/rtlfuncs.h>
#include <wine/debug.h>
#include <tapisrv_s.h>

typedef struct _TAPI_PROVIDER
{
    HMODULE hModule;
    DWORD dwPermanentProviderID;
    DWORD dwLineDeviceIDBase;
    DWORD dwNumLines;
    PROC pfnProviderEnumDevices;
    PROC pfnProviderInit;
    PROC pfnProviderShutdown;
    PROC pfnLineNegotiateTSPIVersion;
    PROC pfnLineOpen;
    PROC pfnLineClose;
    PROC pfnLineGetDevCaps;
    PROC pfnLineGetAddressCaps;
    PROC pfnLineMakeCall;
    PROC pfnLineDrop;
    PROC pfnLineAnswer;
    PROC pfnLineCloseCall;
    PROC pfnLineGetCallStatus;
} TAPI_PROVIDER, *PTAPI_PROVIDER;

#define TAPI_MAX_EVENTS 64

typedef struct _TAPI_EVENT
{
    DWORD dwMsg;
    DWORD dwParam1;
    DWORD dwParam2;
    DWORD dwParam3;
} TAPI_EVENT, *PTAPI_EVENT;

typedef struct _TAPI_APP
{
    DWORD dwAppHandle; /* unique ID returned to client */
    TAPI_EVENT Events[TAPI_MAX_EVENTS];
    DWORD dwHead; /* next slot to read */
    DWORD dwTail; /* next slot to write */
    CRITICAL_SECTION Lock;
} TAPI_APP, *PTAPI_APP;

typedef struct _TAPI_LINE
{
    DWORD dwLineHandle;   /* opaque ID returned to client */
    DWORD dwDeviceID;
    PTAPI_PROVIDER pProvider;
    HDRVLINE hdLine;         /* TSP driver handle */
    PTAPI_APP pApp;
} TAPI_LINE, *PTAPI_LINE;

typedef struct _TAPI_CALL
{
    DWORD dwCallHandle;
    PTAPI_LINE pLine;
    HDRVCALL hdCall;
} TAPI_CALL, *PTAPI_CALL;

typedef struct _TAPI_GLOBALS
{
    CRITICAL_SECTION Lock;
    PTAPI_PROVIDER pProviders;
    DWORD dwNumProviders;
    DWORD dwTotalLines;

    /* Flat array for open calls / lines / whatever. TODO: Too simple. */
    PTAPI_APP pApps[16];
    PTAPI_LINE pLines[64];
    PTAPI_CALL pCalls[64];
    DWORD dwNextAppHandle;
    DWORD dwNextLineHandle;
    DWORD dwNextCallHandle;
} TAPI_GLOBALS, *PTAPI_GLOBALS;

extern TAPI_GLOBALS       g_Tapi;
extern HINSTANCE          g_hInstance;
extern PSVCHOST_GLOBAL_DATA g_pSvcHostGlobals;

_Must_inspect_result_
DWORD
TapiLoadProviders(VOID);

VOID
TapiUnloadProviders(VOID);

_Must_inspect_result_
PTAPI_PROVIDER
TapiGetProviderForDevice(
    _In_ DWORD dwDeviceID);

VOID CALLBACK
TapiLineEventProc(
    _In_ HTAPILINE  htLine,
    _In_ HTAPICALL  htCall,
    _In_ DWORD      dwMsg,
    _In_ DWORD_PTR  dwParam1,
    _In_ DWORD_PTR  dwParam2,
    _In_ DWORD_PTR  dwParam3);

VOID CALLBACK
TapiAsyncCompletion(
    _In_ DRV_REQUESTID dwRequestID,
    _In_ LONG          lResult);

_Must_inspect_result_
DWORD
StartRpcServer(VOID);

DWORD
StopRpcServer(VOID);

_Must_inspect_result_
PTAPI_APP
TapiFindApp(
    _In_ DWORD dwAppHandle);

_Must_inspect_result_
PTAPI_LINE
TapiFindLine(
    _In_ DWORD dwLineHandle);

_Must_inspect_result_
PTAPI_CALL
TapiFindCall(
    _In_ DWORD dwCallHandle);

DWORD
TapiAllocApp(
    _Outptr_ PTAPI_APP *ppApp);

DWORD
TapiAllocLine(
    _In_     PTAPI_APP      pApp,
    _In_     DWORD          dwDeviceID,
    _In_     PTAPI_PROVIDER pProvider,
    _In_     HDRVLINE        hdLine,
    _Outptr_ PTAPI_LINE    *ppLine);

DWORD
TapiAllocCall(
    _In_     PTAPI_LINE  pLine,
    _In_     HDRVCALL    hdCall,
    _Outptr_ PTAPI_CALL *ppCall);

VOID
TapiFreeApp(
    _In_ PTAPI_APP pApp);

VOID
TapiFreeLine(
    _In_ PTAPI_LINE pLine);

VOID
TapiFreeCall(
    _In_ PTAPI_CALL pCall);

VOID
TapiPostEvent(
    _In_ PTAPI_APP pApp,
    _In_ DWORD     dwMsg,
    _In_ DWORD     dwParam1,
    _In_ DWORD     dwParam2,
    _In_ DWORD     dwParam3);

#endif /* _TAPISRV_PCH_ */