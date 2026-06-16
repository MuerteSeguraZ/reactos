/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Precompiled header
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#ifndef _UNIMODEM_PCH_
#define _UNIMODEM_PCH_

#define WIN32_NO_STATUS
#define _INC_WINDOWS
#define COM_NO_WINDOWS_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <winnls.h>
#include <strsafe.h>
#include <tapi.h>
#include <tspi.h>

#include <wine/debug.h>

typedef enum _MODEM_STATE
{
    MODEM_STATE_IDLE = 0,
    MODEM_STATE_INITIALIZING,
    MODEM_STATE_DIALING,
    MODEM_STATE_CONNECTED,
    MODEM_STATE_ANSWERING,
    MODEM_STATE_DROPPING,
} MODEM_STATE;

typedef struct _UNIMODEM_LINE UNIMODEM_LINE, *PUNIMODEM_LINE;
typedef struct _UNIMODEM_CALL UNIMODEM_CALL, *PUNIMODEM_CALL;

struct _UNIMODEM_LINE
{
    DWORD dwDeviceID;
    HANDLE hComm;
    MODEM_STATE State;
    HTAPILINE htLine;
    LINEEVENT pfnEventProc;
    DWORD_PTR dwCallbackInstance;
    PUNIMODEM_CALL pCall;
    CHAR szPort[16];
    CHAR szInitStr[128];
    DWORD dwSpeakerVolume;
    BOOL bPulseDial;
    CRITICAL_SECTION Lock;
};

struct _UNIMODEM_CALL
{
    PUNIMODEM_LINE pLine;
    HTAPICALL htCall;
    HDRVCALL hdCall;
    DWORD dwCallState;
    CHAR szDialStr[128];
    HANDLE hDialThread;
    DRV_REQUESTID dwRequestID;
};

typedef struct _UNIMODEM_PROVIDER
{
    DWORD dwPermanentProviderID;
    DWORD dwLineDeviceIDBase;
    DWORD dwNumLines;
    PUNIMODEM_LINE pLines;
    ASYNC_COMPLETION pfnCompletion;
} UNIMODEM_PROVIDER, *PUNIMODEM_PROVIDER;

extern UNIMODEM_PROVIDER g_Provider;
extern HINSTANCE g_hInstance;

LONG UnimodemEnumeratePorts(VOID);

LONG UnimodemOpenComm(PUNIMODEM_LINE pLine);

VOID UnimodemCloseComm(PUNIMODEM_LINE pLine);

BOOL UnimodemSendATCommand(HANDLE hComm, LPCSTR pszCmd,
                           LPSTR pszResponse, DWORD cbResponse,
                           DWORD dwTimeoutMs);

BOOL UnimodemWaitForConnect(HANDLE hComm, DWORD dwTimeoutMs);

VOID UnimodemNotifyCallState(PUNIMODEM_CALL pCall,
                             DWORD dwNewState,
                             DWORD_PTR dwParam2,
                             DWORD_PTR dwParam3);

PUNIMODEM_LINE UnimodemGetLine(DWORD dwDeviceID);

PUNIMODEM_LINE UnimodemGetLineByHandle(HDRVLINE hdLine);

PUNIMODEM_CALL UnimodemGetCallByHandle(HDRVCALL hdCall);

#endif /* _UNIMODEM_PCH_ */