/*
 * TSPI (Telephony Service Provider Interface) definitions
 *
 * Copyright (c) 2026 Alex Mendoza
 * 
 * LICENSE: MIT
 */

#ifndef __WINE_TSPI_H
#define __WINE_TSPI_H

#include <tapi.h>

#define LINEADDRESSSHARING_PRIVATE         0x00000001
#define LINEADDRESSSHARING_MONITORED       0x00000002
#define LINEADDRESSSHARING_BRIDGEDEXCL     0x00000004
#define LINEADDRESSSHARING_BRIDGEDNEW      0x00000008
#define LINEADDRESSSHARING_BRIDGEDSHARED   0x00000010

#define LINEADDRCAPFLAGS_FWDNUMRINGS       0x00000001
#define LINEADDRCAPFLAGS_PICKUPGROUPID     0x00000002
#define LINEADDRCAPFLAGS_PREDICTIVEDIALER  0x00000004
#define LINEADDRCAPFLAGS_DIALED            0x00000008
#define LINEADDRCAPFLAGS_ORIGOFFHOOK       0x00000010
#define LINEADDRCAPFLAGS_DESTOFFHOOK       0x00000020
#define LINEADDRCAPFLAGS_FWDCONSULT        0x00000040
#define LINEADDRCAPFLAGS_SETUPCONFNULL     0x00000080
#define LINEADDRCAPFLAGS_AUTORECONNECT     0x00000100
#define LINEADDRCAPFLAGS_COMPLETIONID      0x00000200
#define LINEADDRCAPFLAGS_TRANSFERHELD      0x00000400
#define LINEADDRCAPFLAGS_TRANSFERMAKE      0x00000800
#define LINEADDRCAPFLAGS_CONFERENCEHELD    0x00001000
#define LINEADDRCAPFLAGS_CONFERENCEMAKE    0x00002000
#define LINEADDRCAPFLAGS_PARTIALDIAL       0x00004000
#define LINEADDRCAPFLAGS_FWDSTATUSVALID    0x00008000
#define LINEADDRCAPFLAGS_FWDINTEXTADDR     0x00010000
#define LINEADDRCAPFLAGS_FWDBUSYNAADDR     0x00020000
#define LINEADDRCAPFLAGS_ACCEPTTOALERT     0x00040000
#define LINEADDRCAPFLAGS_CONFDROP          0x00080000
#define LINEADDRCAPFLAGS_PICKUPCALLWAIT    0x00100000

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles used between tapisrv and a TSP */
typedef HANDLE HTAPILINE;
typedef HANDLE HTAPICALL;
typedef HANDLE HDRVCALL,  *LPHDRVCALL;
typedef HANDLE HDRVLINE,  *LPHDRVLINE;
typedef HANDLE HDRVPHONE, *LPHDRVPHONE;
typedef HANDLE HPROVIDER;
typedef DWORD  DRV_REQUESTID;

#define TSPIAPI WINAPI

typedef void (CALLBACK *LINEEVENT)(
    HTAPILINE   htLine,
    HTAPICALL   htCall,
    DWORD       dwMsg,
    DWORD_PTR   dwParam1,
    DWORD_PTR   dwParam2,
    DWORD_PTR   dwParam3);

typedef void (CALLBACK *PHONEEVENT)(
    HTAPILINE   htPhone,
    DWORD       dwMsg,
    DWORD_PTR   dwParam1,
    DWORD_PTR   dwParam2,
    DWORD_PTR   dwParam3);

typedef void (CALLBACK *ASYNC_COMPLETION)(
    DRV_REQUESTID   dwRequestID,
    LONG            lResult);

/* TSPI provider management */
LONG TSPIAPI TSPI_providerInit(
    DWORD            dwTSPIVersion,
    DWORD            dwPermanentProviderID,
    DWORD            dwLineDeviceIDBase,
    DWORD            dwPhoneDeviceIDBase,
    DWORD_PTR        dwNumLines,
    DWORD_PTR        dwNumPhones,
    ASYNC_COMPLETION lpfnCompletionProc,
    LPDWORD          lpdwTSPIOptions);

LONG TSPIAPI TSPI_providerShutdown(
    DWORD dwTSPIVersion,
    DWORD dwPermanentProviderID);

LONG TSPIAPI TSPI_providerEnumDevices(
    DWORD      dwPermanentProviderID,
    LPDWORD    lpdwNumLines,
    LPDWORD    lpdwNumPhones,
    HPROVIDER  hProvider,
    LINEEVENT  lpfnLineCreateProc,
    PHONEEVENT lpfnPhoneCreateProc);

/* TSPI line services */
LONG TSPIAPI TSPI_lineNegotiateTSPIVersion(
    DWORD   dwDeviceID,
    DWORD   dwLowVersion,
    DWORD   dwHighVersion,
    LPDWORD lpdwTSPIVersion);

LONG TSPIAPI TSPI_lineOpen(
    DWORD      dwDeviceID,
    HTAPILINE  htLine,
    LPHDRVLINE lphdLine,
    DWORD      dwTSPIVersion,
    LINEEVENT  lpfnEventProc);

LONG TSPIAPI TSPI_lineClose(
    HDRVLINE hdLine);

LONG TSPIAPI TSPI_lineGetDevCaps(
    DWORD         dwDeviceID,
    DWORD         dwTSPIVersion,
    DWORD         dwExtVersion,
    LPLINEDEVCAPS lpLineDevCaps);

LONG TSPIAPI TSPI_lineGetAddressCaps(
    DWORD             dwDeviceID,
    DWORD             dwAddressID,
    DWORD             dwTSPIVersion,
    DWORD             dwExtVersion,
    LPLINEADDRESSCAPS lpAddressCaps);

LONG TSPIAPI TSPI_lineMakeCall(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    HTAPICALL           htCall,
    LPHDRVCALL          lphdCall,
    LPCWSTR             lpszDestAddress,
    DWORD               dwCountryCode,
    LPLINECALLPARAMS    const lpCallParams);

LONG TSPIAPI TSPI_lineDrop(
    DRV_REQUESTID dwRequestID,
    HDRVCALL      hdCall,
    LPCSTR        lpsUserUserInfo,
    DWORD         dwSize);

LONG TSPIAPI TSPI_lineAnswer(
    DRV_REQUESTID dwRequestID,
    HDRVCALL      hdCall,
    LPCSTR        lpsUserUserInfo,
    DWORD         dwSize);

LONG TSPIAPI TSPI_lineCloseCall(
    HDRVCALL hdCall);

LONG TSPIAPI TSPI_lineGetCallStatus(
    HDRVCALL         hdCall,
    LPLINECALLSTATUS lpCallStatus);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_TSPI_H */