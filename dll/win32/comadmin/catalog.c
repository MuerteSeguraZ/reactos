/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     ICOMAdminCatalog2 implementation
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

typedef struct _COMADMIN_CATALOG
{
    ICOMAdminCatalog2 ICOMAdminCatalog2_iface;
    LONG RefCount;
} COMADMIN_CATALOG, *PCOMADMIN_CATALOG;

static
PCOMADMIN_CATALOG
impl_from_ICOMAdminCatalog2(
    _In_ ICOMAdminCatalog2 *iface)
{
    return CONTAINING_RECORD(iface, COMADMIN_CATALOG, ICOMAdminCatalog2_iface);
}

static
HRESULT
WINAPI
catalog_QueryInterface(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDispatch) ||
        IsEqualIID(riid, &IID_ICOMAdminCatalog) ||
        IsEqualIID(riid, &IID_ICOMAdminCatalog2))
    {
        *ppvObject = iface;
        ICOMAdminCatalog2_AddRef(iface);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static
ULONG
WINAPI
catalog_AddRef(
    _In_ ICOMAdminCatalog2 *iface)
{
    PCOMADMIN_CATALOG This = impl_from_ICOMAdminCatalog2(iface);

    return InterlockedIncrement(&This->RefCount);
}

static
ULONG
WINAPI
catalog_Release(
    _In_ ICOMAdminCatalog2 *iface)
{
    PCOMADMIN_CATALOG This = impl_from_ICOMAdminCatalog2(iface);
    LONG RefCount = InterlockedDecrement(&This->RefCount);

    if (RefCount == 0)
    {
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&g_ObjectCount);
    }

    return RefCount;
}

static
HRESULT
WINAPI
catalog_GetTypeInfoCount(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ UINT *pctinfo)
{
    *pctinfo = 1;
    return S_OK;
}

static
HRESULT
WINAPI
catalog_GetTypeInfo(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ UINT iTInfo,
    _In_ LCID lcid,
    _Outptr_ ITypeInfo **ppTInfo)
{
    if (iTInfo != 0)
        return DISP_E_BADINDEX;

    return ComAdminGetTypeInfo(&IID_ICOMAdminCatalog2, ppTInfo);
}

static
HRESULT
WINAPI
catalog_GetIDsOfNames(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ REFIID riid,
    _In_reads_(cNames) LPOLESTR *rgszNames,
    _In_ UINT cNames,
    _In_ LCID lcid,
    _Out_writes_(cNames) DISPID *rgDispId)
{
    return ComAdminGetIDsOfNames(&IID_ICOMAdminCatalog2, rgszNames, cNames, rgDispId);
}

static
HRESULT
WINAPI
catalog_Invoke(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ DISPID dispIdMember,
    _In_ REFIID riid,
    _In_ LCID lcid,
    _In_ WORD wFlags,
    _In_ DISPPARAMS *pDispParams,
    _Out_opt_ VARIANT *pVarResult,
    _Out_opt_ EXCEPINFO *pExcepInfo,
    _Out_opt_ UINT *puArgErr)
{
    return ComAdminInvoke(iface, &IID_ICOMAdminCatalog2, dispIdMember, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static
HRESULT
WINAPI
catalog_GetCollection(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrCollName,
    _Outptr_ IDispatch **ppCatalogCollection)
{
    if (bstrCollName == NULL || ppCatalogCollection == NULL)
        return E_INVALIDARG;

    return ComAdminGetTopLevelCollection(bstrCollName, ppCatalogCollection);
}

static
HRESULT
WINAPI
catalog_Connect(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrCatalogServerName,
    _Outptr_ IDispatch **ppCatalogCollection)
{
    ICatalogCollection *Root;
    HRESULT hr;

    if (ppCatalogCollection == NULL)
        return E_INVALIDARG;

    *ppCatalogCollection = NULL;

    if (bstrCatalogServerName != NULL && bstrCatalogServerName[0] != L'\0')
    {
        WCHAR LocalName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD Size = ARRAYSIZE(LocalName);

        if (!GetComputerNameW(LocalName, &Size) || lstrcmpiW(bstrCatalogServerName, LocalName) != 0)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);
    }

    hr = ComAdminCreateCollection(CollectionRoot, L"Root", NULL, &Root);
    if (FAILED(hr))
        return hr;

    hr = ICatalogCollection_QueryInterface(Root, &IID_IDispatch, (PVOID *)ppCatalogCollection);
    ICatalogCollection_Release(Root);
    return hr;
}

static
HRESULT
WINAPI
catalog_get_MajorVersion(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ LONG *plMajorVersion)
{
    if (plMajorVersion == NULL)
        return E_INVALIDARG;

    *plMajorVersion = COMADMIN_CATALOG_MAJOR_VERSION;
    return S_OK;
}

static
HRESULT
WINAPI
catalog_get_MinorVersion(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ LONG *plMinorVersion)
{
    if (plMinorVersion == NULL)
        return E_INVALIDARG;

    *plMinorVersion = COMADMIN_CATALOG_MINOR_VERSION;
    return S_OK;
}

static
HRESULT
WINAPI
catalog_get_Is64BitCatalogServer(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ VARIANT_BOOL *pbIs64Bit)
{
    if (pbIs64Bit == NULL)
        return E_INVALIDARG;

#ifdef _WIN64
    *pbIs64Bit = VARIANT_TRUE;
#else
    *pbIs64Bit = VARIANT_FALSE;
#endif // _WIN64
    return S_OK;
}

static
HRESULT
WINAPI
catalog_GetCollectionByQuery(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrCollName,
    _In_ SAFEARRAY **ppsaVarQuery,
    _Outptr_ IDispatch **ppCatalogCollection)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ImportComponent(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIDOrName,
    _In_ BSTR bstrCLSIDOrProgID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallComponent(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIDOrName,
    _In_ BSTR bstrDLL,
    _In_ BSTR bstrTLB,
    _In_ BSTR bstrPSDLL)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ShutdownApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ExportApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIDOrName,
    _In_ BSTR bstrApplicationFile,
    _In_ LONG lOptions)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationFile,
    _In_opt_ BSTR bstrDestinationDirectory,
    _In_opt_ LONG lOptions,
    _In_opt_ BSTR bstrUserId,
    _In_opt_ BSTR bstrPassword,
    _In_opt_ BSTR bstrRSN)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_StopRouter(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_RefreshRouter(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_StartRouter(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_Reserved1(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_Reserved2(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallMultipleComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIDOrName,
    _In_ SAFEARRAY **ppsaVarFileNames,
    _In_ SAFEARRAY **ppsaVarCLSIDs)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetMultipleComponentsInfo(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIdOrName,
    _In_ SAFEARRAY **ppsaVarFileNames,
    _Outptr_ SAFEARRAY **ppsaVarCLSIDs,
    _Outptr_ SAFEARRAY **ppsaVarClassNames,
    _Outptr_ SAFEARRAY **ppsaVarFileFlags,
    _Outptr_ SAFEARRAY **ppsaVarComponentFlags)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_RefreshComponents(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_BackupREGDB(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrBackupFilePath)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_RestoreREGDB(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrBackupFilePath)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_QueryApplicationFile(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationFile,
    _Out_ BSTR *pbstrApplicationName,
    _Out_ BSTR *pbstrApplicationDescription,
    _Out_ VARIANT_BOOL *pbHasUsers,
    _Out_ VARIANT_BOOL *pbIsProxy,
    _Outptr_ SAFEARRAY **ppsaVarFileNames)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_StartApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIdOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ServiceCheck(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ LONG lService,
    _Out_ LONG *plStatus)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallMultipleEventClasses(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIdOrName,
    _In_ SAFEARRAY **ppsaVarFileNames,
    _In_ SAFEARRAY **ppsaVarCLSIDS)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallEventClass(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplIdOrName,
    _In_ BSTR bstrDLL,
    _In_ BSTR bstrTLB,
    _In_ BSTR bstrPSDLL)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetEventClassesForIID(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrIID,
    _Outptr_ SAFEARRAY **ppsaVarCLSIDs,
    _Outptr_ SAFEARRAY **ppsaVarProgIDs,
    _Outptr_ SAFEARRAY **ppsaVarDescriptions)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetCollectionByQuery2(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrCollectionName,
    _In_ VARIANT *pVarQueryStrings,
    _Outptr_ IDispatch **ppCatalogCollection)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetApplicationInstanceIDFromProcessID(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ LONG lProcessID,
    _Out_ BSTR *pbstrApplicationInstanceID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ShutdownApplicationInstances(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ VARIANT *pVarApplicationInstanceID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_PauseApplicationInstances(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ VARIANT *pVarApplicationInstanceID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ResumeApplicationInstances(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ VARIANT *pVarApplicationInstanceID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_RecycleApplicationInstances(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ VARIANT *pVarApplicationInstanceID,
    _In_ LONG lReasonCode)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_AreApplicationInstancesPaused(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ VARIANT *pVarApplicationInstanceID,
    _Out_ VARIANT_BOOL *pVarBoolPaused)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_DumpApplicationInstance(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationInstanceID,
    _In_ BSTR bstrDirectory,
    _In_ LONG lMaxImages,
    _Out_ BSTR *pbstrDumpFile)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_get_IsApplicationInstanceDumpSupported(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ VARIANT_BOOL *pVarBoolDumpSupported)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_CreateServiceForApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _In_ BSTR bstrServiceName,
    _In_ BSTR bstrStartType,
    _In_ BSTR bstrErrorControl,
    _In_ BSTR bstrDependencies,
    _In_ BSTR bstrRunAs,
    _In_ BSTR bstrPassword,
    _In_ VARIANT_BOOL bDesktopOk)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_DeleteServiceForApplication(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetPartitionID(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _Out_ BSTR *pbstrPartitionID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetPartitionName(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _Out_ BSTR *pbstrPartitionName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_put_CurrentPartition(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrPartitionIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_get_CurrentPartitionID(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ BSTR *pbstrPartitionID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_get_CurrentPartitionName(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ BSTR *pbstrPartitionName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_get_GlobalPartitionID(
    _In_ ICOMAdminCatalog2 *iface,
    _Out_ BSTR *pbstrGlobalPartitionID)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_FlushPartitionCache(
    _In_ ICOMAdminCatalog2 *iface)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_CopyApplications(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrSourcePartitionIDOrName,
    _In_ VARIANT *pVarApplicationID,
    _In_ BSTR bstrDestinationPartitionIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_CopyComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrSourceApplicationIDOrName,
    _In_ VARIANT *pVarCLSIDOrProgID,
    _In_ BSTR bstrDestinationApplicationIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_MoveComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrSourceApplicationIDOrName,
    _In_ VARIANT *pVarCLSIDOrProgID,
    _In_ BSTR bstrDestinationApplicationIDOrName)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_AliasComponent(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrSrcApplicationIDOrName,
    _In_ BSTR bstrCLSIDOrProgID,
    _In_ BSTR bstrDestApplicationIDOrName,
    _In_ BSTR bstrNewProgId,
    _In_ BSTR bstrNewClsid)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_IsSafeToDelete(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrDllName,
    _Out_ COMAdminInUse *pCOMAdminInUse)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ImportUnconfiguredComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _In_ VARIANT *pVarCLSIDOrProgID,
    _In_opt_ VARIANT *pVarComponentType)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_PromoteUnconfiguredComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _In_ VARIANT *pVarCLSIDOrProgID,
    _In_opt_ VARIANT *pVarComponentType)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ImportComponents(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationIDOrName,
    _In_ VARIANT *pVarCLSIDOrProgID,
    _In_opt_ VARIANT *pVarComponentType)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_ExportPartition(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrPartitionIDOrName,
    _In_ BSTR bstrPartitionFileName,
    _In_ LONG lOptions)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_InstallPartition(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrFileName,
    _In_ BSTR bstrDestDirectory,
    _In_ LONG lOptions,
    _In_ BSTR bstrUserID,
    _In_ BSTR bstrPassword,
    _In_ BSTR bstrRSN)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_QueryApplicationFile2(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrApplicationFile,
    _Outptr_ IDispatch **ppFilesForImport)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
catalog_GetComponentVersionCount(
    _In_ ICOMAdminCatalog2 *iface,
    _In_ BSTR bstrCLSIDOrProgID,
    _Out_ LONG *plVersionCount)
{
    return E_NOTIMPL;
}

static const ICOMAdminCatalog2Vtbl g_CatalogVtbl =
{
    catalog_QueryInterface,
    catalog_AddRef,
    catalog_Release,
    catalog_GetTypeInfoCount,
    catalog_GetTypeInfo,
    catalog_GetIDsOfNames,
    catalog_Invoke,
    catalog_GetCollection,
    catalog_Connect,
    catalog_get_MajorVersion,
    catalog_get_MinorVersion,
    catalog_GetCollectionByQuery,
    catalog_ImportComponent,
    catalog_InstallComponent,
    catalog_ShutdownApplication,
    catalog_ExportApplication,
    catalog_InstallApplication,
    catalog_StopRouter,
    catalog_RefreshRouter,
    catalog_StartRouter,
    catalog_Reserved1,
    catalog_Reserved2,
    catalog_InstallMultipleComponents,
    catalog_GetMultipleComponentsInfo,
    catalog_RefreshComponents,
    catalog_BackupREGDB,
    catalog_RestoreREGDB,
    catalog_QueryApplicationFile,
    catalog_StartApplication,
    catalog_ServiceCheck,
    catalog_InstallMultipleEventClasses,
    catalog_InstallEventClass,
    catalog_GetEventClassesForIID,
    catalog_GetCollectionByQuery2,
    catalog_GetApplicationInstanceIDFromProcessID,
    catalog_ShutdownApplicationInstances,
    catalog_PauseApplicationInstances,
    catalog_ResumeApplicationInstances,
    catalog_RecycleApplicationInstances,
    catalog_AreApplicationInstancesPaused,
    catalog_DumpApplicationInstance,
    catalog_get_IsApplicationInstanceDumpSupported,
    catalog_CreateServiceForApplication,
    catalog_DeleteServiceForApplication,
    catalog_GetPartitionID,
    catalog_GetPartitionName,
    catalog_put_CurrentPartition,
    catalog_get_CurrentPartitionID,
    catalog_get_CurrentPartitionName,
    catalog_get_GlobalPartitionID,
    catalog_FlushPartitionCache,
    catalog_CopyApplications,
    catalog_CopyComponents,
    catalog_MoveComponents,
    catalog_AliasComponent,
    catalog_IsSafeToDelete,
    catalog_ImportUnconfiguredComponents,
    catalog_PromoteUnconfiguredComponents,
    catalog_ImportComponents,
    catalog_get_Is64BitCatalogServer,
    catalog_ExportPartition,
    catalog_InstallPartition,
    catalog_QueryApplicationFile2,
    catalog_GetComponentVersionCount
};

HRESULT
ComAdminCreateCatalog(
    _In_ REFIID Riid,
    _Outptr_ PVOID *Object)
{
    PCOMADMIN_CATALOG This;
    HRESULT hr;

    *Object = NULL;

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*This));
    if (This == NULL)
        return E_OUTOFMEMORY;

    This->ICOMAdminCatalog2_iface.lpVtbl = &g_CatalogVtbl;
    This->RefCount = 1;
    InterlockedIncrement(&g_ObjectCount);

    hr = ICOMAdminCatalog2_QueryInterface(&This->ICOMAdminCatalog2_iface, Riid, Object);
    ICOMAdminCatalog2_Release(&This->ICOMAdminCatalog2_iface);
    return hr;
}