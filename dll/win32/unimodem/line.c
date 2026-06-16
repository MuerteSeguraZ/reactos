/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     TSPI line-level entry points
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(unimodem);

#define DIAL_TIMEOUT_MS 60000
#define AT_TIMEOUT_MS 5000

VOID UnimodemNotifyCallState(PUNIMODEM_CALL pCall,
                              DWORD dwNewState,
                              DWORD_PTR dwParam2,
                              DWORD_PTR dwParam3)
{
    PUNIMODEM_LINE pLine = pCall->pLine;

    pCall->dwCallState = dwNewState;

    TRACE("Call state -> %lu\n", dwNewState);

    if (pLine->pfnEventProc)
    {
        pLine->pfnEventProc(pLine->htLine,
                             pCall->htCall,
                             LINE_CALLSTATE,
                             (DWORD_PTR)dwNewState,
                             dwParam2,
                             dwParam3);
    }
}

static DWORD WINAPI DialThread(LPVOID lpParam)
{
    PUNIMODEM_CALL pCall = (PUNIMODEM_CALL)lpParam;
    PUNIMODEM_LINE pLine = pCall->pLine;
    CHAR szCmd[160];
    BOOL bOK;

    /* Open COM port */
    if (UnimodemOpenComm(pLine) != ERROR_SUCCESS)
    {
        UnimodemNotifyCallState(pCall, LINECALLSTATE_DISCONNECTED,
                                 LINEDISCONNECTMODE_UNAVAIL, 0);
        goto done;
    }

    /* Send init string */
    UnimodemNotifyCallState(pCall, LINECALLSTATE_DIALTONE, 0, 0);

    bOK = UnimodemSendATCommand(pLine->hComm, pLine->szInitStr,
                                 NULL, 0, AT_TIMEOUT_MS);
    if (!bOK)
    {
        WARN("Modem init failed\n");
        UnimodemNotifyCallState(pCall, LINECALLSTATE_DISCONNECTED,
                                 LINEDISCONNECTMODE_UNAVAIL, 0);
        UnimodemCloseComm(pLine);
        goto done;
    }

    StringCbPrintfA(szCmd, sizeof(szCmd),
                     "ATD%c%s",
                     pLine->bPulseDial ? 'P' : 'T',
                     pCall->szDialStr);

    UnimodemNotifyCallState(pCall, LINECALLSTATE_DIALING, 0, 0);

    {
        DWORD cbWritten;
        CHAR  szDialCR[160];
        StringCbPrintfA(szDialCR, sizeof(szDialCR), "%s\r", szCmd);
        WriteFile(pLine->hComm, szDialCR, (DWORD)strlen(szDialCR),
                  &cbWritten, NULL);
    }

    UnimodemNotifyCallState(pCall, LINECALLSTATE_RINGBACK, 0, 0);

    /* Wait for CONNECT */
    bOK = UnimodemWaitForConnect(pLine->hComm, DIAL_TIMEOUT_MS);
    if (bOK)
    {
        UnimodemNotifyCallState(pCall, LINECALLSTATE_CONNECTED,
                                 LINECONNECTEDMODE_ACTIVE, 0);

        /* Signal async completion to tapisrv */
        if (g_Provider.pfnCompletion)
        {
            g_Provider.pfnCompletion(pCall->dwRequestID, ERROR_SUCCESS);
        }
    }
    else
    {
        UnimodemNotifyCallState(pCall, LINECALLSTATE_DISCONNECTED,
                                 LINEDISCONNECTMODE_NOANSWER, 0);
        if (g_Provider.pfnCompletion)
        {
            g_Provider.pfnCompletion(pCall->dwRequestID, LINEERR_CALLUNAVAIL);
        }
        UnimodemCloseComm(pLine);
    }

done:
    return 0;
}

/***********************************************************************
 * TSPI_lineNegotiateTSPIVersion
 *
 * @param dwDeviceID Device ID of the line to negotiate version for.
 * @param dwLowVersion Lowest TSPI version tapisrv is willing to accept.
 * @param dwHighVersion Highest TSPI version tapisrv supports.
 * @param lpdwTSPIVersion Receives the negotiated TSPI version.
 *
 * @return ERROR_SUCCESS or LINEERR_INCOMPATIBLEAPIVERSION if dwHighVersion is below 2.0.
 */
LONG TSPIAPI TSPI_lineNegotiateTSPIVersion(
    DWORD dwDeviceID,
    DWORD dwLowVersion,
    DWORD dwHighVersion,
    LPDWORD lpdwTSPIVersion)
{
    TRACE("(%lu, %08lx, %08lx)\n", dwDeviceID, dwLowVersion, dwHighVersion);

    /* We support TSPI 2.0 */
    if (dwHighVersion < 0x00020000)
    {
        return LINEERR_INCOMPATIBLEAPIVERSION;
    }

    *lpdwTSPIVersion = 0x00020000;
    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineOpen
 *
 * tapisrv is opening a line device. We store the TAPI handle and
 * event callback, and hand back our own opaque HDRVLINE (= pLine ptr).
 *
 * @param dwDeviceID Device ID of the line to open.
 * @param htLine TAPI's handle for the line.
 * @param lphdLine Receives our opaque driver line handle.
 * @param dwTSPIVersion Negotiated TSPI version.
 * @param lpfnEventProc Callback used to notify TAPI of line events.
 *
 * @return ERROR_SUCCESS or LINEERR_BADDEVICEID if dwDeviceID is invalid.
 */
LONG TSPIAPI TSPI_lineOpen(
    DWORD dwDeviceID,
    HTAPILINE htLine,
    LPHDRVLINE lphdLine,
    DWORD dwTSPIVersion,
    LINEEVENT lpfnEventProc)
{
    PUNIMODEM_LINE pLine;

    TRACE("(%lu, %p)\n", dwDeviceID, htLine);

    pLine = UnimodemGetLine(dwDeviceID);

    if (!pLine)
    {
        return LINEERR_BADDEVICEID;
    }

    EnterCriticalSection(&pLine->Lock);

    pLine->htLine = htLine;
    pLine->pfnEventProc = lpfnEventProc;
    pLine->State = MODEM_STATE_IDLE;

    *lphdLine = (HDRVLINE)(ULONG_PTR)pLine;

    LeaveCriticalSection(&pLine->Lock);

    TRACE("Opened line %lu (%s), hdLine=%p\n",
          dwDeviceID, pLine->szPort, *lphdLine);
    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineClose
 *
 * @param hdLine Driver handle to the line to close.
 *
 * @return ERROR_SUCCESS or LINEERR_INVALLINEHANDLE if hdLine is invalid.
 */
LONG TSPIAPI TSPI_lineClose(HDRVLINE hdLine)
{
    PUNIMODEM_LINE pLine = UnimodemGetLineByHandle(hdLine);

    TRACE("(%p)\n", hdLine);

    if (!pLine)
    {
        return LINEERR_INVALLINEHANDLE;
    }

    EnterCriticalSection(&pLine->Lock);
    UnimodemCloseComm(pLine);
    pLine->htLine = NULL;
    pLine->pfnEventProc = NULL;
    pLine->State = MODEM_STATE_IDLE;
    LeaveCriticalSection(&pLine->Lock);

    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineGetDevCaps
 *
 * Fill in LINEDEVCAPS so tapisrv/TAPI know what we support.
 *
 * @param dwDeviceID Device ID of the line to query.
 * @param dwTSPIVersion Negotiated TSPI version.
 * @param dwExtVersion Extension version (unused).
 * @param lpLineDevCaps Caller-supplied structure to fill in.
 *
 * @return ERROR_SUCCESS or LINEERR_BADDEVICEID if dwDeviceID is invalid.
 */
LONG TSPIAPI TSPI_lineGetDevCaps(
    DWORD dwDeviceID,
    DWORD dwTSPIVersion,
    DWORD dwExtVersion,
    LPLINEDEVCAPS lpLineDevCaps)
{
    PUNIMODEM_LINE pLine;
    DWORD dwNeeded;
    CHAR szProvName[] = "Unimodem";
    DWORD cbProvName = sizeof(szProvName);

    TRACE("(%lu)\n", dwDeviceID);

    pLine = UnimodemGetLine(dwDeviceID);
    if (!pLine)
    {
        return LINEERR_BADDEVICEID;
    }

    dwNeeded = sizeof(LINEDEVCAPS) + cbProvName;

    if (lpLineDevCaps->dwTotalSize < dwNeeded)
    {
        lpLineDevCaps->dwNeededSize = dwNeeded;
        return ERROR_SUCCESS;
    }

    ZeroMemory(lpLineDevCaps, sizeof(LINEDEVCAPS));
    lpLineDevCaps->dwTotalSize = lpLineDevCaps->dwTotalSize;
    lpLineDevCaps->dwNeededSize = dwNeeded;
    lpLineDevCaps->dwUsedSize = dwNeeded;
    lpLineDevCaps->dwProviderInfoSize = cbProvName;
    lpLineDevCaps->dwProviderInfoOffset = sizeof(LINEDEVCAPS);
    lpLineDevCaps->dwNumAddresses = 1;
    lpLineDevCaps->dwBearerModes = LINEBEARERMODE_VOICE |
                                   LINEBEARERMODE_DATA;
    lpLineDevCaps->dwMediaModes = LINEMEDIAMODE_DATAMODEM |
                                  LINEMEDIAMODE_INTERACTIVEVOICE;
    lpLineDevCaps->dwMaxNumActiveCalls = 1;
    lpLineDevCaps->dwStringFormat = STRINGFORMAT_ASCII;
    lpLineDevCaps->dwDevCapFlags = LINEDEVCAPFLAGS_CLOSEDROP;

    CopyMemory((LPBYTE)lpLineDevCaps + sizeof(LINEDEVCAPS),
               szProvName, cbProvName);

    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineGetAddressCaps
 *
 * @param dwDeviceID Device ID of the line to query.
 * @param dwAddressID Address ID to query (must be 0).
 * @param dwTSPIVersion Negotiated TSPI version.
 * @param dwExtVersion Extension version (unused).
 * @param lpAddressCaps Caller-supplied structure to fill in.
 *
 * @return ERROR_SUCCESS or LINEERR_INVALADDRESSID if dwAddressID is not 0.
 */
LONG TSPIAPI TSPI_lineGetAddressCaps(
    DWORD dwDeviceID,
    DWORD dwAddressID,
    DWORD dwTSPIVersion,
    DWORD dwExtVersion,
    LPLINEADDRESSCAPS lpAddressCaps)
{
    TRACE("(%lu, addr=%lu)\n", dwDeviceID, dwAddressID);

    if (dwAddressID != 0)
    {
        return LINEERR_INVALADDRESSID;
    }

    if (lpAddressCaps->dwTotalSize < sizeof(LINEADDRESSCAPS))
    {
        lpAddressCaps->dwNeededSize = sizeof(LINEADDRESSCAPS);
        return ERROR_SUCCESS;
    }

    ZeroMemory(lpAddressCaps, sizeof(LINEADDRESSCAPS));
    lpAddressCaps->dwTotalSize = lpAddressCaps->dwTotalSize;
    lpAddressCaps->dwNeededSize = sizeof(LINEADDRESSCAPS);
    lpAddressCaps->dwUsedSize = sizeof(LINEADDRESSCAPS);
    lpAddressCaps->dwLineDeviceID = dwDeviceID;
    lpAddressCaps->dwAddressSharing = LINEADDRESSSHARING_PRIVATE;
    lpAddressCaps->dwMaxNumActiveCalls = 1;
    lpAddressCaps->dwMaxNumOnHoldCalls = 0;
    lpAddressCaps->dwCallFeatures = LINECALLFEATURE_DIAL |
                                    LINECALLFEATURE_DROP;
    lpAddressCaps->dwAddrCapFlags = LINEADDRCAPFLAGS_DIALED;

    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineMakeCall
 *
 * Allocates a call context and starts the dial thread.
 * This is an async operation: we return ASYNC_REQUESTID and fire
 * LINE_REPLY when the dial completes or fails.
 *
 * @param dwRequestID Async request ID to pass back on completion.
 * @param hdLine Driver handle to the line to call on.
 * @param htCall TAPI's handle for the new call.
 * @param lphdCall Receives our opaque driver call handle.
 * @param lpszDestAddress Destination address to dial.
 * @param dwCountryCode Country code (unused).
 * @param lpCallParams Optional call parameters (unused).
 *
 * @return dwRequestID on success or a LINEERR_* code on failure.
 */
LONG TSPIAPI TSPI_lineMakeCall(
    DRV_REQUESTID dwRequestID,
    HDRVLINE hdLine,
    HTAPICALL htCall,
    LPHDRVCALL lphdCall,
    LPCWSTR lpszDestAddress,
    DWORD dwCountryCode,
    LPLINECALLPARAMS const lpCallParams)
{
    PUNIMODEM_LINE pLine = UnimodemGetLineByHandle(hdLine);
    PUNIMODEM_CALL pCall;

    TRACE("(%p, htCall=%p, dest=%s)\n",
          hdLine, htCall, wine_dbgstr_w(lpszDestAddress));

    if (!pLine)
    {
        return LINEERR_INVALLINEHANDLE;
    }

    EnterCriticalSection(&pLine->Lock);

    if (pLine->pCall)
    {
        LeaveCriticalSection(&pLine->Lock);
        return LINEERR_ALLOCATED;
    }

    pCall = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(UNIMODEM_CALL));
    if (!pCall)
    {
        LeaveCriticalSection(&pLine->Lock);
        return LINEERR_NOMEM;
    }

    pCall->pLine = pLine;
    pCall->htCall = htCall;
    pCall->hdCall = (HDRVCALL)(ULONG_PTR)pCall;
    pCall->dwRequestID = dwRequestID;
    pCall->dwCallState = LINECALLSTATE_IDLE;

    /* Convert wide dest address to ANSI */
    WideCharToMultiByte(CP_ACP, 0, lpszDestAddress, -1,
                         pCall->szDialStr, sizeof(pCall->szDialStr),
                         NULL, NULL);

    pLine->pCall = pCall;
    pLine->State = MODEM_STATE_DIALING;

    *lphdCall = pCall->hdCall;

    pCall->hDialThread = CreateThread(NULL, 0, DialThread, pCall, 0, NULL);
    if (!pCall->hDialThread)
    {
        pLine->pCall = NULL;
        pLine->State = MODEM_STATE_IDLE;
        LeaveCriticalSection(&pLine->Lock);
        HeapFree(GetProcessHeap(), 0, pCall);
        return LINEERR_OPERATIONFAILED;
    }

    LeaveCriticalSection(&pLine->Lock);

    return (LONG)dwRequestID;
}

/***********************************************************************
 * TSPI_lineDrop
 *
 * Hang up: send ATH, close COM port, notify DISCONNECTED.
 *
 * @param dwRequestID Async request ID to pass back on completion.
 * @param hdCall Driver handle to the call to drop.
 * @param lpsUserUserInfo User-to-user info (unused).
 * @param dwSize Size of lpsUserUserInfo (unused).
 *
 * @return dwRequestID on success or LINEERR_INVALCALLHANDLE if hdCall is invalid.
 */
LONG TSPIAPI TSPI_lineDrop(
    DRV_REQUESTID dwRequestID,
    HDRVCALL hdCall,
    LPCSTR lpsUserUserInfo,
    DWORD dwSize)
{
    PUNIMODEM_CALL pCall = UnimodemGetCallByHandle(hdCall);
    PUNIMODEM_LINE pLine;

    TRACE("(%p)\n", hdCall);

    if (!pCall)
    {
        return LINEERR_INVALCALLHANDLE;
    }

    pLine = pCall->pLine;

    EnterCriticalSection(&pLine->Lock);

    if (pCall->hDialThread)
    {
        TerminateThread(pCall->hDialThread, 0);
        CloseHandle(pCall->hDialThread);
        pCall->hDialThread = NULL;
    }

    /* Send ATH to hang up */
    if (pLine->hComm != INVALID_HANDLE_VALUE)
    {
        Sleep(1100);
        UnimodemSendATCommand(pLine->hComm, "ATH", NULL, 0, AT_TIMEOUT_MS);
    }

    UnimodemNotifyCallState(pCall, LINECALLSTATE_DISCONNECTED,
                             LINEDISCONNECTMODE_NORMAL, 0);
    UnimodemCloseComm(pLine);
    pLine->State = MODEM_STATE_IDLE;

    LeaveCriticalSection(&pLine->Lock);

    if (g_Provider.pfnCompletion)
    {
        g_Provider.pfnCompletion(dwRequestID, ERROR_SUCCESS);
    }

    return (LONG)dwRequestID;
}

/***********************************************************************
 * TSPI_lineAnswer
 *
 * Answer an incoming call with ATA.
 *
 * @param dwRequestID Async request ID to pass back on completion.
 * @param hdCall Driver handle to the call to answer.
 * @param lpsUserUserInfo User-to-user info (unused).
 * @param dwSize Size of lpsUserUserInfo (unused).
 *
 * @return dwRequestID on success or a LINEERR_* code on failure.
 */
LONG TSPIAPI TSPI_lineAnswer(
    DRV_REQUESTID dwRequestID,
    HDRVCALL hdCall,
    LPCSTR lpsUserUserInfo,
    DWORD dwSize)
{
    PUNIMODEM_CALL pCall = UnimodemGetCallByHandle(hdCall);
    PUNIMODEM_LINE pLine;
    BOOL bOK;

    TRACE("(%p)\n", hdCall);

    if (!pCall)
    {
        return LINEERR_INVALCALLHANDLE;
    }

    pLine = pCall->pLine;

    if (UnimodemOpenComm(pLine) != ERROR_SUCCESS)
    {
        return LINEERR_OPERATIONFAILED;
    }

    bOK = UnimodemWaitForConnect(pLine->hComm, DIAL_TIMEOUT_MS);
    if (bOK)
    {
        UnimodemNotifyCallState(pCall, LINECALLSTATE_CONNECTED,
                                 LINECONNECTEDMODE_ACTIVE, 0);
        if (g_Provider.pfnCompletion)
        {
            g_Provider.pfnCompletion(dwRequestID, ERROR_SUCCESS);
        }
    }
    else
    {
        UnimodemNotifyCallState(pCall, LINECALLSTATE_DISCONNECTED,
                                 LINEDISCONNECTMODE_NOANSWER, 0);
        UnimodemCloseComm(pLine);
        if (g_Provider.pfnCompletion)
        {
            g_Provider.pfnCompletion(dwRequestID, LINEERR_CALLUNAVAIL);
        }
    }

    return (LONG)dwRequestID;
}

/***********************************************************************
 * TSPI_lineCloseCall
 *
 * tapisrv has finished with the call handle; free our context.
 *
 * @param hdCall Driver handle to the call to close.
 *
 * @return ERROR_SUCCESS or LINEERR_INVALCALLHANDLE if hdCall is invalid.
 */
LONG TSPIAPI TSPI_lineCloseCall(HDRVCALL hdCall)
{
    PUNIMODEM_CALL pCall = UnimodemGetCallByHandle(hdCall);
    PUNIMODEM_LINE pLine;

    TRACE("(%p)\n", hdCall);

    if (!pCall)
    {
        return LINEERR_INVALCALLHANDLE;
    }

    pLine = pCall->pLine;

    EnterCriticalSection(&pLine->Lock);

    if (pCall->hDialThread)
    {
        WaitForSingleObject(pCall->hDialThread, 2000);
        CloseHandle(pCall->hDialThread);
        pCall->hDialThread = NULL;
    }

    pLine->pCall = NULL;

    LeaveCriticalSection(&pLine->Lock);

    HeapFree(GetProcessHeap(), 0, pCall);
    return ERROR_SUCCESS;
}

/***********************************************************************
 * TSPI_lineGetCallStatus
 *
 * @param hdCall Driver handle to the call to query.
 * @param lpCallStatus Caller-supplied structure to fill in.
 *
 * @return ERROR_SUCCESS or LINEERR_INVALCALLHANDLE if hdCall is invalid.
 */
LONG TSPIAPI TSPI_lineGetCallStatus(
    HDRVCALL hdCall,
    LPLINECALLSTATUS lpCallStatus)
{
    PUNIMODEM_CALL pCall = UnimodemGetCallByHandle(hdCall);

    TRACE("(%p)\n", hdCall);

    if (!pCall)
    {
        return LINEERR_INVALCALLHANDLE;
    }

    if (lpCallStatus->dwTotalSize < sizeof(LINECALLSTATUS))
    {
        lpCallStatus->dwNeededSize = sizeof(LINECALLSTATUS);
        return ERROR_SUCCESS;
    }

    lpCallStatus->dwNeededSize = sizeof(LINECALLSTATUS);
    lpCallStatus->dwUsedSize = sizeof(LINECALLSTATUS);
    lpCallStatus->dwCallState = pCall->dwCallState;
    lpCallStatus->dwCallStateMode = 0;
    lpCallStatus->dwCallFeatures = (pCall->dwCallState == LINECALLSTATE_CONNECTED)
                                    ? LINECALLFEATURE_DROP
                                    : 0;
    return ERROR_SUCCESS;
}