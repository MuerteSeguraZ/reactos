/*
 * PROJECT:     ReactOS Unimodem TAPI Service Provider
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     TSPI provider-level entry points
 * COPYRIGHT:   Copyright 2026 Alex Mendoza
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(unimodem);

/* TSPI version we support (2.0) */
#define UNIMODEM_TSPI_VERSION 0x00020000

/*
 * TSPI_providerEnumDevices
 *
 * @param dwPermanentProviderID Permanent provider ID assigned by TAPI.
 * @param lpdwNumLines Receives the number of line devices exposed.
 * @param lpdwNumPhones Receives the number of phone devices exposed (always 0).
 * @param hProvider Handle to the provider instance.
 * @param lpfnLineCreateProc Callback for dynamic line device creation (unused).
 * @param lpfnPhoneCreateProc Callback for dynamic phone device creation (unused).
 *
 * @return ERROR_SUCCESS.
 */

LONG TSPIAPI TSPI_providerEnumDevices(
    DWORD dwPermanentProviderID,
    LPDWORD lpdwNumLines,
    LPDWORD lpdwNumPhones,
    HPROVIDER hProvider,
    LINEEVENT lpfnLineCreateProc,
    PHONEEVENT lpfnPhoneCreateProc)
{
    LONG lCount;

    TRACE("(%lu, %p, %p, %p, %p, %p)\n",
          dwPermanentProviderID, lpdwNumLines, lpdwNumPhones,
          hProvider, lpfnLineCreateProc, lpfnPhoneCreateProc);

    g_Provider.dwPermanentProviderID = dwPermanentProviderID;

    lCount = UnimodemEnumeratePorts();

    *lpdwNumLines = (DWORD)lCount;
    *lpdwNumPhones = 0;

    TRACE("Reporting %lu line device(s)\n", *lpdwNumLines);
    return ERROR_SUCCESS;
}

/*
 * TSPI_providerInit
 *
 * @param dwTSPIVersion TSPI version requested by TAPI.
 * @param dwPermanentProviderID Permanent provider ID assigned by TAPI.
 * @param dwLineDeviceIDBase First device ID to assign to line devices.
 * @param dwPhoneDeviceIDBase First device ID to assign to phone devices (unused).
 * @param dwNumLines Number of line devices to initialize.
 * @param dwNumPhones Number of phone devices to initialize (unused).
 * @param lpfnCompletionProc Callback to signal async operation completion.
 * @param lpdwTSPIOptions Receives provider option flags (set to 0).
 *
 * @return ERROR_SUCCESS, or LINEERR_INCOMPATIBLEAPIVERSION if dwTSPIVersion is too old.
 */
LONG TSPIAPI TSPI_providerInit(
    DWORD dwTSPIVersion,
    DWORD dwPermanentProviderID,
    DWORD dwLineDeviceIDBase,
    DWORD dwPhoneDeviceIDBase,
    DWORD_PTR dwNumLines,
    DWORD_PTR dwNumPhones,
    ASYNC_COMPLETION lpfnCompletionProc,
    LPDWORD lpdwTSPIOptions)
{
    DWORD i;

    TRACE("(%08lx, %lu, base=%lu, lines=%Iu)\n",
          dwTSPIVersion, dwPermanentProviderID,
          dwLineDeviceIDBase, dwNumLines);

    if (dwTSPIVersion < UNIMODEM_TSPI_VERSION)
    {
        WARN("tapisrv requested TSPI version %08lx, we need >= %08x\n",
             dwTSPIVersion, UNIMODEM_TSPI_VERSION);
        return LINEERR_INCOMPATIBLEAPIVERSION;
    }

    g_Provider.dwPermanentProviderID = dwPermanentProviderID;
    g_Provider.dwLineDeviceIDBase    = dwLineDeviceIDBase;
    g_Provider.pfnCompletion         = lpfnCompletionProc;

    UnimodemEnumeratePorts();

    for (i = 0; i < g_Provider.dwNumLines; i++)
    {
        g_Provider.pLines[i].dwDeviceID = dwLineDeviceIDBase + i;
    }

    if (lpdwTSPIOptions)
    {
        *lpdwTSPIOptions = 0;
    }

    TRACE("providerInit OK, %lu line(s)\n", g_Provider.dwNumLines);
    return ERROR_SUCCESS;
}

/*
 * TSPI_providerShutdown
 *
 * @param dwTSPIVersion TSPI version negotiated at init time.
 * @param dwPermanentProviderID Permanent provider ID assigned by TAPI.
 *
 * @return ERROR_SUCCESS.
 */
LONG TSPIAPI TSPI_providerShutdown(
    DWORD dwTSPIVersion,
    DWORD dwPermanentProviderID)
{
    DWORD i;

    TRACE("(%08lx, %lu)\n", dwTSPIVersion, dwPermanentProviderID);

    if (g_Provider.pLines)
    {
        for (i = 0; i < g_Provider.dwNumLines; i++)
        {
            PUNIMODEM_LINE pLine = &g_Provider.pLines[i];
            UnimodemCloseComm(pLine);
            DeleteCriticalSection(&pLine->Lock);
        }
        HeapFree(GetProcessHeap(), 0, g_Provider.pLines);
        g_Provider.pLines = NULL;
        g_Provider.dwNumLines = 0;
    }

    g_Provider.pfnCompletion = NULL;
    return ERROR_SUCCESS;
}