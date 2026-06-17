/*
 * PROJECT:     ReactOS TAPI Service
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Handle table (apps, lines, calls)
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(tapisrv);

_Must_inspect_result_
PTAPI_APP
TapiFindApp(
    _In_ DWORD dwAppHandle)
{
    DWORD i;
    for (i = 0; i < ARRAYSIZE(g_Tapi.pApps); i++)
    {
        if (g_Tapi.pApps[i] &&
            g_Tapi.pApps[i]->dwAppHandle == dwAppHandle)
            return g_Tapi.pApps[i];
    }
    return NULL;
}

DWORD
TapiAllocApp(
    _Outptr_ PTAPI_APP *ppApp)
{
    DWORD i;
    PTAPI_APP pApp;

    *ppApp = NULL;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pApps); i++)
    {
        if (!g_Tapi.pApps[i])
            break;
    }

    if (i == ARRAYSIZE(g_Tapi.pApps))
    {
        return ERROR_TOO_MANY_OPEN_FILES;
    }

    pApp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TAPI_APP));

    if (!pApp)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pApp->dwAppHandle = g_Tapi.dwNextAppHandle++;
    InitializeCriticalSection(&pApp->Lock);

    g_Tapi.pApps[i] = pApp;
    *ppApp = pApp;
    return ERROR_SUCCESS;
}

VOID
TapiFreeApp(
    _In_ PTAPI_APP pApp)
{
    DWORD i;
    for (i = 0; i < ARRAYSIZE(g_Tapi.pApps); i++)
    {
        if (g_Tapi.pApps[i] == pApp)
        {
            g_Tapi.pApps[i] = NULL;
            break;
        }
    }
    DeleteCriticalSection(&pApp->Lock);
    HeapFree(GetProcessHeap(), 0, pApp);
}

_Must_inspect_result_
PTAPI_LINE
TapiFindLine(
    _In_ DWORD dwLineHandle)
{
    DWORD i;
    for (i = 0; i < ARRAYSIZE(g_Tapi.pLines); i++)
    {
        if (g_Tapi.pLines[i] &&
            g_Tapi.pLines[i]->dwLineHandle == dwLineHandle)
            return g_Tapi.pLines[i];
    }
    return NULL;
}

DWORD
TapiAllocLine(
    _In_     PTAPI_APP pApp,
    _In_     DWORD dwDeviceID,
    _In_     PTAPI_PROVIDER pProvider,
    _In_     HDRVLINE hdLine,
    _Outptr_ PTAPI_LINE *ppLine)
{
    DWORD i;
    PTAPI_LINE pLine;

    *ppLine = NULL;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pLines); i++)
    {
        if (!g_Tapi.pLines[i])
            break;
    }

    if (i == ARRAYSIZE(g_Tapi.pLines))
    {
        return ERROR_TOO_MANY_OPEN_FILES;
    }

    pLine = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TAPI_LINE));

    if (!pLine)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pLine->dwLineHandle = g_Tapi.dwNextLineHandle++;
    pLine->dwDeviceID = dwDeviceID;
    pLine->pProvider = pProvider;
    pLine->hdLine = hdLine;
    pLine->pApp = pApp;

    g_Tapi.pLines[i] = pLine;
    *ppLine = pLine;
    return ERROR_SUCCESS;
}

VOID
TapiFreeLine(
    _In_ PTAPI_LINE pLine)
{
    DWORD i;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pLines); i++)
    {
        if (g_Tapi.pLines[i] == pLine)
        {
            g_Tapi.pLines[i] = NULL;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, pLine);
}

_Must_inspect_result_
PTAPI_CALL
TapiFindCall(
    _In_ DWORD dwCallHandle)
{
    DWORD i;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pCalls); i++)
    {
        if (g_Tapi.pCalls[i] &&
            g_Tapi.pCalls[i]->dwCallHandle == dwCallHandle)
            return g_Tapi.pCalls[i];
    }

    return NULL;
}

DWORD
TapiAllocCall(
    _In_ PTAPI_LINE pLine,
    _In_ HDRVCALL hdCall,
    _Outptr_ PTAPI_CALL *ppCall)
{
    DWORD i;
    PTAPI_CALL pCall;

    *ppCall = NULL;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pCalls); i++)
    {
        if (!g_Tapi.pCalls[i])
        {
            break;
        }
    }

    if (i == ARRAYSIZE(g_Tapi.pCalls))
    {
        return ERROR_TOO_MANY_OPEN_FILES;
    }

    pCall = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TAPI_CALL));

    if (!pCall)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCall->dwCallHandle = g_Tapi.dwNextCallHandle++;
    pCall->pLine = pLine;
    pCall->hdCall = hdCall;

    g_Tapi.pCalls[i] = pCall;
    *ppCall = pCall;
    return ERROR_SUCCESS;
}

VOID
TapiFreeCall(
    _In_ PTAPI_CALL pCall)
{
    DWORD i;

    for (i = 0; i < ARRAYSIZE(g_Tapi.pCalls); i++)
    {
        if (g_Tapi.pCalls[i] == pCall)
        {
            g_Tapi.pCalls[i] = NULL;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, pCall);
}

VOID
TapiPostEvent(
    _In_ PTAPI_APP pApp,
    _In_ DWORD dwMsg,
    _In_ DWORD dwParam1,
    _In_ DWORD dwParam2,
    _In_ DWORD dwParam3)
{
    DWORD dwNext;

    EnterCriticalSection(&pApp->Lock);

    dwNext = (pApp->dwTail + 1) % TAPI_MAX_EVENTS;
    if (dwNext == pApp->dwHead)
    {
        WARN("Event ring full, dropping oldest event\n");
        pApp->dwHead = (pApp->dwHead + 1) % TAPI_MAX_EVENTS;
    }

    pApp->Events[pApp->dwTail].dwMsg    = dwMsg;
    pApp->Events[pApp->dwTail].dwParam1 = dwParam1;
    pApp->Events[pApp->dwTail].dwParam2 = dwParam2;
    pApp->Events[pApp->dwTail].dwParam3 = dwParam3;
    pApp->dwTail = dwNext;

    LeaveCriticalSection(&pApp->Lock);
}